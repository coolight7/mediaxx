#pragma once

#include <format>
#include <iostream>
#include <string_view>

#ifdef LUMENXX_BUILD_TYPE

#define LXX_DEBEG(str, ...) (std::cerr << std::format(str, ##__VA_ARGS__) << std::endl);

#define LXX_INFO(str, ...) (std::cerr << std::format(str, ##__VA_ARGS__) << std::endl);

#define LXX_WARN(str, ...) (std::cerr << std::format(str, ##__VA_ARGS__) << std::endl);

#define LXX_ERR(str, ...) (std::cerr << std::format(str, ##__VA_ARGS__) << std::endl);

#else

#define LXX_DEBEG(str, ...) ;

#define LXX_INFO(str, ...) ;

#define LXX_WARN(str, ...) ;

#define LXX_ERR(str, ...) ;

#endif

namespace logxx {
    void printStack();

    void signal_error(std::string_view exepath);
}; // namespace logxx