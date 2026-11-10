#pragma once

#include "analyse/media_name_tables.h"
#include <algorithm>
#include <cstdint>
#include <iterator>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

namespace mediaxx {

    /// 媒体名称里「符号 / 空白」的分类与合并（对应 Dart 的 MediaInfoSymbol_c）
    ///
    /// Dart 那边这套逻辑是为了替换 `\p{P}\p{S}`、`\s` 这两类 Unicode 属性正则：
    /// 每个码点先用同一个正则引擎分类一次，之后走查表 + 线性扫描。这里把分类结果换成
    /// 数据表（由 musicxx 的生成脚本用 Dart 正则逐码点跑出来，见 media_name_tables.h 与
    /// media_name_tables.cpp），
    /// 行为与 Dart 完全一致，运行时不再碰正则。
    ///
    /// 输入输出都是 UTF-16 码元序列：Dart 的 String 就是码元序列，下标、子串、长度的语义
    /// 都按码元算，非 BMP 字符要按代理对整体处理。
    class MediaNameSymbol_c {
    public:

        /// 码点是否是 `\p{P}` 或 `\p{S}`（标点 / 符号）
        static bool isPunctuationOrSymbol(int32_t codePoint) {
            return hitRange(
                media_name_tables::kPunctuationSymbolRanges,
                std::size(media_name_tables::kPunctuationSymbolRanges),
                codePoint
            );
        }

        /// 码点是否是 `\s`（空白）
        static bool isSpace(int32_t codePoint) {
            return hitRange(
                media_name_tables::kSpaceRanges,
                std::size(media_name_tables::kSpaceRanges),
                codePoint
            );
        }

        /// 取 [index] 处的码点（[index] 必须是码元起点；孤立代理按单码元返回）
        static int32_t codePointAt(std::u16string_view str, size_t index) {
            const char16_t code = str[index];
            if (code >= 0xD800 && code <= 0xDBFF && index + 1 < str.size()) {
                const char16_t next = str[index + 1];
                if (next >= 0xDC00 && next <= 0xDFFF) {
                    return 0x10000 + ((code - 0xD800) << 10) + (next - 0xDC00);
                }
            }
            return code;
        }

        /// 码点占用的码元数
        static size_t codePointWidth(int32_t codePoint) {
            return (codePoint > 0xFFFF) ? 2 : 1;
        }

        /// [index] 处的码点占用的码元数（[index] 必须是码元起点）
        static size_t widthAt(std::u16string_view str, size_t index) {
            const char16_t code = str[index];
            if (code >= 0xD800 && code <= 0xDBFF && index + 1 < str.size()) {
                const char16_t next = str[index + 1];
                if (next >= 0xDC00 && next <= 0xDFFF) {
                    return 2;
                }
            }
            return 1;
        }

        /// 合并连续符号：保留「连续符号段」的首尾两个符号
        ///
        /// 等价于 Dart 的
        /// `replaceAllMapped(RegExp(r"(^|\p{P}|\p{S})(\s|\p{P}|\p{S})+(\p{P}|\p{S}|$)"), ...)`。
        static std::u16string mergeSymbols(const std::u16string& str) {
            const size_t length = str.size();
            if (0 == length) {
                return str;
            }
            std::u16string result{};
            result.reserve(length);
            size_t lastOut  = 0;
            size_t index    = 0;
            bool   hasMatch = false;
            while (index < length) {
                bool                                     group1Empty = false;
                size_t                                   group1End   = 0;
                std::optional<std::pair<size_t, size_t>> tail{};
                if (0 == index) {
                    // 候选 1：`^` 断言，group1 为空
                    tail = matchGroup23(str, 0);
                    if (tail.has_value()) {
                        group1Empty = true;
                    }
                }
                if (false == tail.has_value()) {
                    // 候选 2：group1 为当前位置的符号
                    const int32_t codePoint = codePointAt(str, index);
                    if (isPunctuationOrSymbol(codePoint)) {
                        const size_t width = codePointWidth(codePoint);
                        tail               = matchGroup23(str, index + width);
                        if (tail.has_value()) {
                            group1End = index + width;
                        }
                    }
                }
                if (false == tail.has_value()) {
                    index += widthAt(str, index);
                    continue;
                }
                const size_t matchEnd    = tail->first;
                const size_t group3Start = tail->second;
                hasMatch                 = true;
                result.append(str, lastOut, index - lastOut);
                if (false == group1Empty) {
                    result.append(str, index, group1End - index);
                }
                if (group3Start < matchEnd) {
                    result.append(str, group3Start, matchEnd - group3Start);
                }
                lastOut = matchEnd;
                index   = matchEnd;
            }
            if (false == hasMatch) {
                return str;
            }
            if (lastOut < length) {
                result.append(str, lastOut, length - lastOut);
            }
            return result;
        }

        /// 去掉开头的符号，等价于 `replaceFirst(RegExp(r"^(\p{P}|\p{S})+"), '')`
        static std::u16string stripLeadingSymbols(const std::u16string& str) {
            size_t index = 0;
            while (index < str.size()) {
                if (false == isPunctuationOrSymbol(codePointAt(str, index))) {
                    break;
                }
                index += widthAt(str, index);
            }
            return (0 == index) ? str : str.substr(index);
        }

        /// 合并连续空白，等价于 `replaceAll(RegExp(r"\s{2,}"), ' ')`
        static std::u16string collapseSpaces(const std::u16string& str) {
            const size_t length = str.size();
            if (length < 2) {
                return str;
            }
            std::u16string result{};
            result.reserve(length);
            size_t lastOut  = 0;
            size_t index    = 0;
            bool   hasMatch = false;
            while (index < length) {
                if (false == isSpace(codePointAt(str, index))) {
                    index += widthAt(str, index);
                    continue;
                }
                size_t runEnd   = index;
                int    runCount = 0;
                while (runEnd < length) {
                    const int32_t codePoint = codePointAt(str, runEnd);
                    if (false == isSpace(codePoint)) {
                        break;
                    }
                    runEnd += codePointWidth(codePoint);
                    ++runCount;
                }
                if (runCount < 2) {
                    index = runEnd;
                    continue;
                }
                hasMatch = true;
                result.append(str, lastOut, index - lastOut);
                result.push_back(u' ');
                lastOut = runEnd;
                index   = runEnd;
            }
            if (false == hasMatch) {
                return str;
            }
            if (lastOut < length) {
                result.append(str, lastOut, length - lastOut);
            }
            return result;
        }

        /// 等价于原实现的两条正则：先合并连续符号，再去掉开头的符号
        static std::u16string mergeAndStripSymbols(const std::u16string& str) {
            return stripLeadingSymbols(mergeSymbols(str));
        }

    private:

        /// 有序区间表上的二分查找
        static bool hitRange(
            const media_name_tables::CodePointRange_c* list,
            size_t                                     size,
            int32_t                                    codePoint
        ) {
            size_t begin = 0;
            size_t end   = size;
            while (begin < end) {
                const size_t middle = begin + (end - begin) / 2;
                if (codePoint < list[middle].start) {
                    end = middle;
                } else if (codePoint > list[middle].end) {
                    begin = middle + 1;
                } else {
                    return true;
                }
            }
            return false;
        }

        /// [end] 处（码元边界）前一个码点占用的码元数
        static size_t prevWidth(const std::u16string& str, size_t end) {
            if (end >= 2) {
                const char16_t prev = str[end - 1];
                if (prev >= 0xDC00 && prev <= 0xDFFF) {
                    const char16_t before = str[end - 2];
                    if (before >= 0xD800 && before <= 0xDBFF) {
                        return 2;
                    }
                }
            }
            return 1;
        }

        /// 匹配 `group2`（`(\s|\p{P}|\p{S})+`）与 `group3`（`(\p{P}|\p{S}|$)`）
        ///
        /// 成功返回 `(匹配结束位置, group3 起始位置)`；group3 为 `$` 的零宽匹配时两者相等。
        static std::optional<std::pair<size_t, size_t>>
            matchGroup23(const std::u16string& str, size_t start) {
            const size_t length = str.size();
            if (start >= length) {
                return std::nullopt;
            }
            // 贪婪扫描 group2
            size_t runEnd = start;
            while (runEnd < length) {
                const int32_t codePoint = codePointAt(str, runEnd);
                if (false == isPunctuationOrSymbol(codePoint) && false == isSpace(codePoint)) {
                    break;
                }
                runEnd += codePointWidth(codePoint);
            }
            if (runEnd == start) {
                return std::nullopt;
            }
            // 回溯 group2 的末尾，寻找 group3
            for (size_t end = runEnd; end > start; end -= prevWidth(str, end)) {
                if (end == length) {
                    // `$` 零宽匹配
                    return std::make_pair(end, end);
                }
                const int32_t codePoint = codePointAt(str, end);
                if (isPunctuationOrSymbol(codePoint)) {
                    return std::make_pair(end + codePointWidth(codePoint), end);
                }
            }
            return std::nullopt;
        }
    };

} // namespace mediaxx
