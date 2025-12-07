#pragma once

#include "fmt/format.h"
#include "util/utilxx.h"
#include <iostream>
#include <string_view>

#ifdef LUMENXX_BUILD_TYPE

#define XX_LOGD(str, ...) (std::cerr << fmt::format(str, ##__VA_ARGS__) << std::endl);

#define XX_LOGI(str, ...) (std::cerr << fmt::format(str, ##__VA_ARGS__) << std::endl);

#define XX_LOGW(str, ...) (std::cerr << fmt::format(str, ##__VA_ARGS__) << std::endl);

#define XX_LOGE(str, ...) (std::cerr << fmt::format(str, ##__VA_ARGS__) << std::endl);

#define XX_LOGE_AV(str) XX_LOGE("{}: {}/{}", str, ret, mediaxx::utilxx::av_err2str(ret));

#else

#define XX_LOGD(str, ...) ;

#define XX_LOGI(str, ...) ;

#define XX_LOGW(str, ...) ;

#define XX_LOGE(str, ...) ;

#define XX_LOGE_AV(str) ;

#endif

namespace mediaxx {
    namespace logxx {
        void printStack();

        void signalError(std::string_view exepath);
    }; // namespace logxx
}; // namespace mediaxx