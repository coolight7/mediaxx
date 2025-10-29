#pragma once

#include <mediaxx.h>
#include <string>
#include <tuple>

class StringUtilxx_c {
public:

    static inline std::string_view stringCopyMalloc(
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
};