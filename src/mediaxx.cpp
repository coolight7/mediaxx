#include "mediaxx.h"
#include "analyse/media_info_reader.h"
#include "util/ffmpeg_ext.h"
#include <iostream>
#include <string>
#include <string_view>

FFI_PLUGIN_EXPORT void* mediaxx_malloc(int size) {
    return malloc(size);
}

FFI_PLUGIN_EXPORT void mediaxx_free(void* ptr) {
    free(ptr);
}

FFI_PLUGIN_EXPORT int get_libav_version() {
    auto ver = av_log_get_level();
    std::cout << "current av log level " << ver << std::endl;
    return ver;
}

FFI_PLUGIN_EXPORT void get_media_info(const char* filepath) {
    auto item = MediaInfoItem_c{std::string_view{filepath}, nullptr, nullptr};
    if (MediaInfoReader_c::instance.openFile(item)) {
        std::cout << "读取文件成功" << std::endl;
        MediaInfoReader_c::instance.printMediaInfo(item.fmtCtx);
    } else {
        std::cout << "读取文件失败" << std::endl;
    }
    item.dispose();
}