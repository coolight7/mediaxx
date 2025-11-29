#pragma once

#include "fmt/format.h"
#include <iostream>
#include <string_view>

#ifdef LUMENXX_BUILD_TYPE

#define LXX_DEBEG(str, ...) (std::cerr << fmt::format(str, ##__VA_ARGS__) << std::endl);

#define LXX_INFO(str, ...) (std::cerr << fmt::format(str, ##__VA_ARGS__) << std::endl);

#define LXX_WARN(str, ...) (std::cerr << fmt::format(str, ##__VA_ARGS__) << std::endl);

#define LXX_ERR(str, ...) (std::cerr << fmt::format(str, ##__VA_ARGS__) << std::endl);

#else

#define LXX_DEBEG(str, ...) ;

#define LXX_INFO(str, ...) ;

#define LXX_WARN(str, ...) ;

#define LXX_ERR(str, ...) ;

#endif

namespace mediaxx {
    namespace logxx {
        void printStack();

        void signal_error(std::string_view exepath);
    }; // namespace logxx
}; // namespace mediaxx