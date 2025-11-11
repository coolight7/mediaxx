#pragma once

#include <iostream>
#include <mediaxx.h>
#include <string>
#include <tuple>
#include <unordered_set>

namespace stringxx {

    inline size_t utf8GetLength(std::string_view in_str) {
        size_t length = 0;
        for (size_t i = 0, step = 0; i < in_str.size(); i += step) {
            unsigned char byte = in_str[i];
            // lenght 6
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

    inline size_t utf8GetLengthCheckLenAvail(const char* str) {
        size_t length = 0;
        for (size_t i = 0, step = 0;; i += step) {
            unsigned char ch = str[i];
            // lenght 6
            if (ch >= 0xFC) {
                step = 6;
            } else if (ch >= 0xF8) {
                step = 5;
            } else if (ch >= 0xF0) {
                step = 4;
            } else if (ch >= 0xE0) {
                step = 3;
            } else if (ch >= 0xC0) {
                step = 2;
            } else {
                step = 1;
            }
            for (int j = 0; j < step; j++) {
                if (str[i + j] == '\0') {
                    // 不合规
                    return 0;
                }
            }
            length++;
        }
        return length;
    }

    inline bool isAvailUtf8(const char* str) {
        if (nullptr != str && str[0] != '\0' && utf8GetLengthCheckLenAvail(str) == 0) {
            return false;
        }
        return true;
    }

    inline std::vector<std::string> strSplit(const std::string& in_str, char in_char) {
        std::vector<std::string> re_strlist{};
        std::istringstream       iss{in_str};                         // 输入流
        for (std::string item{}; std::getline(iss, item, in_char);) { // 以split为分隔符
            re_strlist.push_back(item);
        }
        return re_strlist;
    }

    inline void strEliminate(std::string& in_str, char in_char) {
        in_str.erase(std::remove(in_str.begin(), in_str.end(), in_char), in_str.end());
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
            LXX_INFO("[]");
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
}; // namespace stringxx