#include "mediaxx.h"
#include <iostream>
#include <memory>

extern "C" {
#include "third_part/ffmpeg/include/libavutil/log.h"
}

// A very short-lived native function.
//
// For very short-lived functions, it is fine to call them on the main isolate.
// They will block the Dart execution while running the native function, so
// only do this for native functions which are guaranteed to be short-lived.
FFI_PLUGIN_EXPORT int sum(int a, int b) {
    return a + b;
}

// A longer-lived native function, which occupies the thread calling it.
//
// Do not call these kind of native functions in the main isolate. They will
// block Dart execution. This will cause dropped frames in Flutter applications.
// Instead, call these native functions on a separate isolate.
FFI_PLUGIN_EXPORT int sum_long_running(int a, int b) {
    // Simulate work.
#if _WIN32
    Sleep(5000);
#else
    usleep(5000 * 1000);
#endif
    return a + b;
}

FFI_PLUGIN_EXPORT void* mediaxx_malloc(int size) {
    return malloc(size);
}

FFI_PLUGIN_EXPORT void mediaxx_free(void* ptr) {
    free(ptr);
}

FFI_PLUGIN_EXPORT void get_libav_version() {
    std::cout << "current av log level " << av_log_get_level() << std::endl;
}
