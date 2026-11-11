#pragma once

// ffmpeg 的错误码 / 时间戳转字符串
// - ffmpeg 自己的 av_err2str / av_ts2str 是宏, 里面用了复合字面量 (C 的写法),
//   C++ 下不能直接用, 所以这里先 #undef 再给同名函数
// - 返回 std::string, 直接拿去拼日志, 不用管缓冲区
extern "C" {
#include "libavutil/error.h"
#include "libavutil/timestamp.h"
}
#undef av_err2str
#undef av_ts2timestr
#undef av_ts2str

#include <string>

namespace mediaxx {

    namespace util {
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
    }; // namespace util
}; // namespace mediaxx