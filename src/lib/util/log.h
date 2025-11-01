#pragma once

#include <format>
#include <iostream>

#define LXX_DEBEG(str, ...) (std::cerr << std::format(str, ##__VA_ARGS__) << std::endl);

#define LXX_INFO(str, ...) (std::cerr << std::format(str, ##__VA_ARGS__) << std::endl);

#define LXX_WARN(str, ...) (std::cerr << std::format(str, ##__VA_ARGS__) << std::endl);

#define LXX_ERR(str, ...) (std::cerr << std::format(str, ##__VA_ARGS__) << std::endl);