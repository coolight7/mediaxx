#pragma once

#include "log.h"
#include <algorithm>
#include <assert.h>
#include <iostream>
#include <mediaxx.h>
#include <ranges>
#include <sstream>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mediaxx {

    namespace stringxx {

        inline static constexpr int CODE_0 = 48;
        inline static constexpr int CODE_9 = 57;
        inline static constexpr int CODE_A = 65;
        inline static constexpr int CODE_Z = 90;
        inline static constexpr int CODE_a = 97;
        inline static constexpr int CODE_z = 122;

        [[nodiscard]] constexpr char charToLower(char c) noexcept {
            return (c >= 'A' && c <= 'Z') ? static_cast<char>(c + ('a' - 'A')) : c;
        }

        [[nodiscard]] constexpr char charToUpper(char c) noexcept {
            return (c >= 'a' && c <= 'z') ? static_cast<char>(c - ('a' - 'A')) : c;
        }

        [[nodiscard]] constexpr bool charIsSpace(char c) noexcept {
            return c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f';
        }

        [[nodiscard]] inline constexpr bool isCode_num(int code) {
            return (code >= CODE_0 && code <= CODE_9);
        }

        [[nodiscard]] inline constexpr bool isCode_AZ(int code) {
            return (code >= CODE_A && code <= CODE_Z);
        }

        [[nodiscard]] inline constexpr bool isCode_az(int code) {
            return (code >= CODE_a && code <= CODE_z);
        }

        [[nodiscard]] inline constexpr bool isCode_AZaz(int code) {
            return (isCode_AZ(code) || isCode_az(code));
        }

        [[nodiscard]] inline constexpr std::optional<int> toCode_tryAZ(int code) {
            if (isCode_az(code)) {
                return code - (CODE_a - CODE_A);
            } else if (isCode_AZ(code)) {
                return code;
            }
            return std::nullopt;
        }

        [[nodiscard]] inline constexpr std::optional<int> toCode_tryaz(int code) {
            if (isCode_az(code)) {
                return code;
            } else if (isCode_AZ(code)) {
                return code + (CODE_a - CODE_A);
            }
            return std::nullopt;
        }

        [[nodiscard]] inline constexpr int toCode_mayAZ(int code) {
            auto result = toCode_tryAZ(code);
            return result.has_value() ? result.value() : code;
        }

        [[nodiscard]] inline constexpr int toCode_mayaz(int code) {
            auto result = toCode_tryaz(code);
            return result.has_value() ? result.value() : code;
        }

        [[nodiscard]] inline constexpr int toCode_AZ(int code) {
            if (isCode_az(code)) {
                return code - (CODE_a - CODE_A);
            }
            if (!std::is_constant_evaluated()) {
                assert(isCode_AZ(code));
            }
            return code;
        }

        [[nodiscard]] inline constexpr int toCode_az(int code) {
            if (isCode_AZ(code)) {
                return code + (CODE_a - CODE_A);
            }
            if (!std::is_constant_evaluated()) {
                assert(isCode_az(code));
            }
            return code;
        }

        inline constexpr void toUpperSelf(std::string& str) {
            std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
                return charToUpper(static_cast<char>(c));
            });
        }

        [[nodiscard]] inline constexpr std::string toUpper(std::string_view str) {
            auto result = std::string{str};
            toUpperSelf(result);
            return result;
        }

        inline constexpr void toLowerSelf(std::string& str) {
            std::transform(str.begin(), str.end(), str.begin(), [](unsigned char c) {
                return charToLower(static_cast<char>(c));
            });
        }

        [[nodiscard]] inline constexpr std::string toLower(std::string_view str) {
            auto result = std::string{str};
            toLowerSelf(result);
            return result;
        }

        // 不区分大小写哈希
        struct IgnoreCaseHash {
            using is_transparent = void;

            size_t operator()(const std::string& s) const {
                return std::hash<std::string>()(toLower(s));
            }

            size_t operator()(std::string_view s) const {
                return std::hash<std::string>()(toLower(s));
            }

            // 支持 const char* 透明查找 (字符串字面量), 否则字面量同时可转换到
            // std::string 与 std::string_view, 导致重载解析歧义无法编译
            size_t operator()(const char* s) const {
                return (*this)(std::string_view{s});
            }
        };

        // 不区分大小写相等判断
        struct IgnoreCaseEqual {
            using is_transparent = void;

            inline static constexpr bool equal(std::string_view a, std::string_view b) {
                if (a.size() != b.size()) {
                    return false;
                }
                for (size_t i = 0; i < a.size(); ++i) {
                    if (charToLower(static_cast<char>(static_cast<unsigned char>(a[i])))
                        != charToLower(static_cast<char>(static_cast<unsigned char>(b[i])))) {
                        return false;
                    }
                }
                return true;
            }

            template<typename T1, typename T2>
            constexpr bool operator()(const T1& a, const T2& b) const {
                return equal(a, b);
            }
        };

        template<typename V>
        using IgnoreCaseMap = std::unordered_map<std::string, V, IgnoreCaseHash, IgnoreCaseEqual>;

        using IgnoreCaseSet = std::unordered_set<std::string, IgnoreCaseHash, IgnoreCaseEqual>;

        [[nodiscard]] inline constexpr bool
            isIgnoreCaseEqual(std::string_view left, std::string_view right) {
            if (left.size() == right.size()) {
                return toLower(left) == toLower(right);
            }
            return false;
        }

        [[nodiscard]] inline constexpr bool
            isIgnoreCaseContains(std::string_view longStr, std::string_view shortStr) {
            std::string lowerLong  = toLower(longStr);
            std::string lowerShort = toLower(shortStr);
            return lowerLong.find(lowerShort) != std::string::npos;
        }

        [[nodiscard]] inline constexpr bool
            isIgnoreCaseContainsAny(std::string_view str1, std::string_view str2) {
            return (str1.size() >= str2.size()) ? isIgnoreCaseContains(str1, str2)
                                                : isIgnoreCaseContains(str2, str1);
        }

        [[nodiscard]] inline constexpr bool
            isNotEmptyAndIgnoreCaseContains(std::string_view str1, std::string_view str2) {
            if (str1.empty() || str2.empty()) {
                return false;
            }
            return isIgnoreCaseContains(str1, str2);
        }

        /// 计算 UTF-8 字符串的字符数 (按前导字节推断后续字节数并跳步计数)
        /// - 不校验编码合法性: 非法序列也按前导字节规则跳步 (想校验请用 utf8GetLengthCheckAvail)
        /// - 相关: [utf8GetLengthCheckAvail] / [utf8IsContinuationChar]
        [[nodiscard]] inline constexpr size_t utf8GetLength(std::string_view in_str) {
            size_t length = 0;
            for (size_t i = 0, step = 0; i < in_str.size(); i += step) {
                unsigned char byte = in_str[i];
                // 按首字节的高位模式确定整个序列的字节数 step (UTF-8 前导字节规则)
                if (byte >= 0xFC) {
                    step = 6;
                } else if (byte >= 0xF8) {
                    step = 5;
                } else if (byte >= 0xF0) {
                    step = 4;
                } else if (byte >= 0xE0) {
                    step = 3;
                } else if (byte >= 0xC0) {
                    step = 2;
                } else {
                    step = 1;
                }
                length++;
            }
            return length;
        }

        /// 按 UTF-8 字符数定位字节下标: 从 start 字节处开始扫描, 跳过 targetLen 个
        /// UTF-8 字符后返回对应字节偏移 (如按可见字符数截断文本)
        /// - `return` 定位到的字节下标; 不足 targetLen 个字符时返回 0
        [[nodiscard]] inline constexpr size_t
            findIndexByUtf8Length(std::string_view in_str, size_t targetLen, size_t start = 0) {
            size_t count = 0;
            size_t i = start, step = 0;
            for (; i < in_str.size();) {
                unsigned char byte = in_str[i];
                // 按首字节的高位模式确定整个序列的字节数 step (UTF-8 前导字节规则)
                if (byte >= 0xFC) {
                    step = 6;
                } else if (byte >= 0xF8) {
                    step = 5;
                } else if (byte >= 0xF0) {
                    step = 4;
                } else if (byte >= 0xE0) {
                    step = 3;
                } else if (byte >= 0xC0) {
                    step = 2;
                } else {
                    step = 1;
                }
                ++count;
                i += step;

                if (count >= targetLen) {
                    return i;
                }
            }
            return 0;
        }

        /// 同 findIndexByUtf8Length, 并附带统计已扫描行数 (换行符数) 与最后一个
        /// 换行符的字节下标 (供 UI 按字符数滚动/截断且需保留行信息时使用)
        /// - `return` tuple<目标字节下标, 行数, 最后一个换行符字节下标>;
        ///   文本不足 targetLen 个字符时返回 {0, 0, 0}
        [[nodiscard]] inline constexpr std::tuple<size_t, size_t, size_t>
            findIndexAndLastLineIndexByUtf8Length(std::string_view in_str, size_t targetLen) {
            if (in_str.size() >= targetLen) {
                size_t count = 0, lineCount = 0, lastLineIndex = 0;
                size_t i = 0, step = 0;
                for (; i < in_str.size();) {
                    const unsigned char byte = in_str[i];
                    // 按首字节的高位模式确定整个序列的字节数 step (UTF-8 前导字节规则)
                    if (byte >= 0xFC) {
                        step = 6;
                    } else if (byte >= 0xF8) {
                        step = 5;
                    } else if (byte >= 0xF0) {
                        step = 4;
                    } else if (byte >= 0xE0) {
                        step = 3;
                    } else if (byte >= 0xC0) {
                        step = 2;
                    } else {
                        step = 1;
                        if (byte == (unsigned char)'\n') {
                            ++lineCount;
                            lastLineIndex = i;
                        }
                    }
                    ++count;
                    i += step;

                    if (count >= targetLen) {
                        return std::tuple<size_t, size_t, size_t>{i, lineCount, lastLineIndex};
                    }
                }
            }
            // 文本不足 targetLen 个字符
            return std::tuple<size_t, size_t, size_t>{0, 0, 0};
        }

        /// 统计文本的行数: '\n' 数量 + 末尾不以 '\n' 结尾的最后一个不完整行
        /// 与 `agentxx_share_store` 按行分页取值的行划分方式一致 (逐行读取)
        [[nodiscard]] inline constexpr size_t countLines(std::string_view in_str) {
            if (in_str.empty()) {
                return 0;
            }
            size_t lineCount = 0;
            for (const char ch : in_str) {
                if ('\n' == ch) {
                    ++lineCount;
                }
            }
            if ('\n' != in_str.back()) {
                // 最后一行没有换行符, 也算一行
                ++lineCount;
            }
            return lineCount;
        }

        /// 判断字节是否为 UTF-8 延续字节 (10xxxxxx): 多字节序列中除首字节外的后续字节
        [[nodiscard]] inline constexpr bool utf8IsContinuationChar(unsigned char ch) {
            return (ch & 0xC0) == 0x80;
        }

        /// 校验 UTF-8 编码合法性并返回字符数; 非法时返回 0
        /// - 非法情况: 5/6 字节头 (0xF8~0xFF)、过短编码头 (0xC0/0xC1)、非最短编码
        ///   (0xE0 0x80~0x9F、0xF0 0x80~0x8F)、序列中途出现非延续字节、序列被截断、
        ///   孤立延续字节、串内 '\0'
        /// - `return` 合法时返回字符数 (空串为 0), 非法返回 0
        [[nodiscard]] inline constexpr size_t
            utf8GetLengthCheckAvail(std::string_view str, bool allowBackstop = true) {
            size_t     length = 0;
            const auto strLen = str.length();
            for (size_t i = 0, step = 0; i < strLen; i += step) {
                unsigned char ch = str[i];
                if (ch == '\0') {
                    break;
                } else if (ch >= 0xF8) {
                    // 5/6 字节头 (0xF8~0xFF) 不是合法 UTF-8 (最多 4 字节)
                    return 0;
                } else if (ch >= 0xF0) {
                    if (ch == 0xF0 && i + 1 < strLen
                        && (static_cast<unsigned char>(str[i + 1]) & 0xF0) == 0x80) {
                        // 检查非最短编码长度，对应 0~0xFFFF
                        return 0;
                    }
                    step = 4;
                } else if (ch >= 0xE0) {
                    if (ch == 0xE0 && i + 1 < strLen
                        && (static_cast<unsigned char>(str[i + 1]) & 0xE0) == 0x80) {
                        // 0xE0 0x80~0x9F 对应 0~0x7FF
                        return 0;
                    }
                    if (false == allowBackstop) {
                        if (ch == 0xEF && i + 2 < strLen) {
                            // 部分转换需要用 � 替代非法编码，因此这里放行
                            unsigned char ch1 = static_cast<unsigned char>(str[i + 1]);
                            unsigned char ch2 = static_cast<unsigned char>(str[i + 2]);
                            if (ch1 == 0xBF && ch2 == 0xBD) {
                                return 0; // 匹配�，判定为无效UTF-8
                            }
                        }
                    }
                    step = 3;
                } else if (ch >= 0xC0) {
                    if (ch == 0xC0 || ch == 0xC1) {
                        // 对应 0~0x3F
                        return 0;
                    }
                    step = 2;
                } else {
                    if (str[i] < 0) {
                        return 0;
                    }
                    step = 1;
                }

                if (step > 1) {
                    for (size_t j = 1; j < step; j++) {
                        if (i + j >= strLen || str[i + j] == '\0'
                            || false == utf8IsContinuationChar((unsigned char)str[i + j])) {
                            // 不合规
                            return 0;
                        }
                    }
                }
                length++;
            }
            return length;
        }

        [[nodiscard]] inline constexpr bool utf8IsAvail(std::string_view str, bool allowBackstop = false) {
            if (str.empty() || str[0] == '\0') {
                return false;
            }
            return utf8GetLengthCheckAvail(str, allowBackstop) != 0;
        }

        /// 修复非法 UTF-8: 将每一处非法字节序列 (按 Unicode 推荐的
        /// "maximal subpart" 规则划分的最大非法子部分) 替换为占位符 U+FFFD (EF BF BD)
        /// - 非法序列包括: 孤立延续字节 (10xxxxxx 无前导)、5/6 字节头 (0xF8~0xFF)、
        ///   过短编码头 (0xC0/0xC1)、非最短编码序列 (0xE0 0x80..、0xF0 0x80..)、
        ///   序列中途出现非延续字节、末尾被截断的多字节序列
        /// - 合法的多字节序列与 ASCII 原样保留; 非法序列只替换为一个 U+FFFD,
        ///   且其后紧跟的合法字符 (如 ASCII) 不会被吞掉
        /// - `return` true 表示输入含非法序列且已被修复, false 表示输入本就合法未做修改
        [[nodiscard]] inline constexpr bool utf8Repair(std::string& str) {
            constexpr char kReplacement[] = {'\xEF', '\xBF', '\xBD'}; // U+FFFD 的 UTF-8 编码
            const size_t   size           = str.size();

            // 快速路径: 输入本就合法则不做任何修改直接返回
            if (size == 0 || utf8GetLengthCheckAvail(str) != 0) {
                return false;
            }

            std::string result;
            result.reserve(size);

            for (size_t i = 0; i < size;) {
                const unsigned char byte = static_cast<unsigned char>(str[i]);

                if (byte < 0x80) {
                    // ASCII
                    result += str[i++];
                    continue;
                }

                if (utf8IsContinuationChar(byte)) {
                    // 孤立延续字节: 一个 maximal subpart 只含这一个字节,
                    // 后续字节可能是新的合法序列前导, 不能一并吞掉
                    result.append(kReplacement, 3);
                    ++i;
                    continue;
                }

                if (byte < 0xC2) {
                    // 0x80~0xBF 已在上方处理, 此处即 0xC0/0xC1:
                    // 过短编码 (只能表示 0~0x7F, 必须用 1 字节 ASCII 表示)
                    result.append(kReplacement, 3);
                    ++i;
                    continue;
                }

                if (byte >= 0xF8) {
                    // 0xF8~0xFF: 5/6 字节头或无效头, UTF-8 最多 4 字节, 单字节非法
                    result.append(kReplacement, 3);
                    ++i;
                    continue;
                }

                // 多字节前导: 计算期望序列长度与第二个字节范围要求,
                // 与 utf8GetLengthCheckAvail 的校验规则保持一致
                size_t step;
                if (byte >= 0xF0) {
                    step = 4;
                } else if (byte >= 0xE0) {
                    step = 3;
                } else {
                    step = 2; // 0xC2~0xDF
                }

                size_t j = 1;
                for (; j < step && i + j < size; ++j) {
                    const unsigned char c = static_cast<unsigned char>(str[i + j]);
                    if (byte == 0xE0 && j == 1 && c < 0xA0) {
                        // 0xE0 0x80~0x9F: 非最短编码 (对应 U+0000~U+07FF)
                        break;
                    }
                    if (byte == 0xF0 && j == 1 && c < 0x90) {
                        // 0xF0 0x80~0x8F: 非最短编码 (对应 U+0000~U+FFFF)
                        break;
                    }
                    if (false == utf8IsContinuationChar(c)) {
                        break;
                    }
                }

                if (j == step) {
                    // 序列合法, 原样拷贝
                    result.append(str.data() + i, step);
                    i += step;
                } else {
                    // 非法序列: 替换其 maximal subpart (已处理的 j 个字节)
                    result.append(kReplacement, 3);
                    i += j;
                }
            }

            str = std::move(result);
            return true;
        }

        [[nodiscard]] inline std::vector<std::string_view>
            strSplit(std::string_view in_str, char delim) noexcept {
            auto                          split = in_str | std::views::split(delim);
            std::vector<std::string_view> result;

            for (auto&& sub : split) {
                if (sub.empty()) {
                    result.emplace_back();
                } else {
                    result.emplace_back(&*sub.begin(), sub.size());
                }
            }
            return result;
        }

        /// 字符分割并拷贝为 std::string 列表
        [[nodiscard]] inline std::vector<std::string>
            strSplitCopied(std::string_view in_str, char delim) {
            auto                     split = in_str | std::views::split(delim);
            std::vector<std::string> result;

            for (const auto& sub : split) {
                result.emplace_back(sub.begin(), sub.end());
            }
            return result;
        }

        inline std::string_view strTrim(std::string_view sv) {
            while (!sv.empty() && std::isspace(sv.front())) {
                sv.remove_prefix(1);
            }
            while (!sv.empty() && std::isspace(sv.back())) {
                sv.remove_suffix(1);
            }
            return sv;
        }

        inline bool hasDuplicateSemicolonSubstr(std::string_view s) {
            std::unordered_set<std::string_view> seen{};

            size_t pos = 0;
            while (pos < s.size()) {
                size_t           next = s.find(';', pos);
                std::string_view part
                    = s.substr(pos, next == std::string_view::npos ? s.size() - pos : next - pos);
                part = strTrim(part);

                if (!part.empty()) {
                    if (seen.count(part)) {
                        // 找到重复
                        return true;
                    }
                    seen.insert(part);
                }

                if (next == std::string_view::npos) {
                    break;
                }
                pos = next + 1;
            }
            return false;
        }

        inline void printStringToIntList(const char* str) {
            if (nullptr == str) {
                XX_LOGI("[]");
                return;
            }
            std::cout << "[";
            for (int i = 0;; ++i) {
                if (str[i] == '\0') {
                    break;
                }
                if (i > 0) {
                    std::cout << ", ";
                }
                std::cout << int(str[i]);
            }
            std::cout << "]" << std::endl;
        }

        inline std::string_view toStringNotNull(const char* str) {
            if (nullptr == str) {
                return std::string_view{""};
            }
            return std::string_view{str};
        }

        inline std::string_view stringCopyMalloc(
            const std::string_view data1,
            const std::string_view data2 = "",
            const std::string_view data3 = "",
            const std::string_view data4 = ""
        ) {
            const auto len    = data1.size() + data2.size() + data3.size() + data4.size();
            auto       size   = len + 1;
            auto       result = (char*)mediaxx_malloc(size);
            memcpy(result, data1.data(), data1.size());
            if (false == data2.empty()) {
                memcpy(result + data1.size(), data2.data(), data2.size());
                if (false == data3.empty()) {
                    memcpy(result + data1.size() + data2.size(), data3.data(), data3.size());
                    if (false == data4.empty()) {
                        memcpy(
                            result + data1.size() + data2.size() + data3.size(),
                            data4.data(),
                            data4.size()
                        );
                    }
                }
            }
            result[len] = '\0';
            return std::string_view{result, len};
        }

        [[nodiscard]] std::tuple<bool, std::optional<std::string>> convertCharset(
            std::string_view src,
            std::string_view srcEncoding,
            std::string_view targetEncoding
        );

        [[nodiscard]] std::tuple<bool, std::optional<std::string>> autoConvertCharset(
            std::string_view str,
            std::string&     encoding,
            std::string_view targetEncoding
        );

        [[nodiscard]] std::tuple<bool, std::optional<std::string>>
            autoConvertToUtf8(std::string_view str, bool _);

        [[nodiscard]] inline std::tuple<bool, std::optional<std::string>>
            autoConvertToUtf8(std::string_view str, std::string& encoding) {
            return autoConvertCharset(str, encoding, "UTF-8");
        }

        bool autoConvertToUtf8(std::string& str);

        [[nodiscard]] std::string autoTryConvertToUtf8(std::string_view str);
    }; // namespace stringxx
}; // namespace mediaxx