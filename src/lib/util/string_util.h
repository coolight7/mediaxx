#pragma once

#include <mediaxx.h>
#include <string>

class StringUtilxx_c {
public:

    static inline char* stringCopyMalloc(
        const std::string_view data1,
        const std::string_view data2 = "",
        const std::string_view data3 = "",
        const std::string_view data4 = ""
    ) {
        auto size   = data1.size() + data2.size() + data3.size() + 1;
        auto result = (char*)mediaxx_malloc(size);
        memcpy(result, data1.data(), data1.size());
        memcpy(result + data1.size(), data2.data(), data2.size());
        memcpy(
            result + data1.size() + data2.size(),
            data3.data(),
            data3.size()
        );
        memcpy(
            result + data1.size() + data2.size() + data3.size(),
            data4.data(),
            data4.size()
        );
        result[size - 1] = '\0';
        return result;
    }
};