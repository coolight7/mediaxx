#pragma once

// 通用字符串工具 (字符分类/大小写、不区分大小写的容器、UTF-8 长度与修复、
// 字符集转换、UTF-16 互转、base64、分割与去空白) 全部来自 cxx_utilxx_base;
// 这里只留 mediaxx 自己的三个函数, 并把 utilxx_base 的名字带进 mediaxx::util,
// 这样原有调用点 (mediaxx::util::xxx) 一行都不用改。
#include "log.h"
#include <mediaxx.h>
#include <cstring>
#include <iostream>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utilxx_base/string_util.h>

namespace mediaxx {

    namespace util {

        /// 通用部分来自 cxx_utilxx_base (utilxx_base 命名空间)
        using namespace ::utilxx_base;

        /// 打印字符串每个字节的整数值 (排查非法编码时用)
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

        /// 分号分隔的子串里有没有重复
        /// - 每段先去掉两端空白, 空段忽略; 比较是大小写敏感的
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

        /// 把最多四段拼成一个以 '\0' 结尾的 C 字符串
        /// - 内存由 mediaxx_malloc 分配, 由调用方 (Dart 侧 mediaxx_free) 释放
        /// - 返回的视图指向这块内存, 长度不含结尾的 '\0'
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

    }; // namespace util
}; // namespace mediaxx
