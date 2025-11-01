#pragma once

extern "C" {
#include "libavutil/error.h"
#include "libavutil/timestamp.h"
}
#undef av_err2str
#undef av_ts2timestr
#undef av_ts2str

#include <string>

class Utilxx_c {
public:

    static int debugArg;

    static inline std::string av_err2str(int errnum) {
        char av_error[AV_ERROR_MAX_STRING_SIZE] = {0};
        return std::string{av_make_error_string(av_error, AV_ERROR_MAX_STRING_SIZE, errnum)};
    }

    static inline std::string av_ts2timestr(int64_t ts, const AVRational* tb) {
        char av_error[AV_TS_MAX_STRING_SIZE] = {0};
        return std::string{av_ts_make_time_string(av_error, ts, tb)};
    }

    static inline std::string av_ts2str(int64_t ts) {
        char av_error[AV_TS_MAX_STRING_SIZE] = {0};
        return std::string{av_ts_make_string(av_error, ts)};
    }
};