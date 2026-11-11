#pragma once

// 日志来自 cxx_utilxx_base (utilxx_base/log.h): XX_LOG* 宏、LogDispatcher/LogSink、
// 崩溃栈输出。这里只做两件事:
// - 沿用 mediaxx 原来的开关语义: 非 LUMENXX_BUILD_TYPE 构建里日志宏退化成空语句,
//   副歌分析、歌名识别这类热路径不再格式化字符串;
// - 保留 mediaxx 专用的 XX_LOGE_AV (ffmpeg 错误串) 与 mediaxx::util 命名空间。
#include "fmt/format.h"
#include "util/utilxx.h"
#include "utilxx_base/log.h"
#include <iostream>
#include <string_view>

#ifndef LUMENXX_BUILD_TYPE

// 发布/性能构建: 把 cxx_utilxx_base 的日志宏改成空语句
#undef XX_LOGT
#undef XX_LOGD
#undef XX_LOGI
#undef XX_LOGW
#undef XX_LOGE
#undef XX_OUT

#define XX_LOGT(str, ...) ;
#define XX_LOGD(str, ...) ;
#define XX_LOGI(str, ...) ;
#define XX_LOGW(str, ...) ;
#define XX_LOGE(str, ...) ;
#define XX_OUT(str, ...) ;

#define XX_LOGE_AV(str) ;

#else

#define XX_LOGE_AV(str) XX_LOGE("{}: {}/{}", str, ret, mediaxx::util::av_err2str(ret));

#endif

namespace mediaxx {
    namespace util {
        /// 输出当前线程的调用栈 (实现与崩溃处理都在 cxx_utilxx_base)
        using utilxx_base::printStack;

        /// 注册崩溃信号处理 (Linux 下输出 backtrace, 其它平台由基础库决定)
        using utilxx_base::signalError;
    }; // namespace util
}; // namespace mediaxx
