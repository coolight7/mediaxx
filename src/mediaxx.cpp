#include "mediaxx.h"
#include "analyse/audio_visualization.h"
#include "analyse/media_info_reader.h"
#include "simdjson.h"
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

FFI_PLUGIN_EXPORT int mediaxx_get_libav_version() {
    auto ver = av_log_get_level();
    std::cout << "current av log level " << ver << std::endl;
    return ver;
}

FFI_PLUGIN_EXPORT const char* mediaxx_get_label_malloc() {
    auto str = (char*)mediaxx_malloc(128);
    strcpy(str, "libmediaxx by coolight");
    return str;
}

FFI_PLUGIN_EXPORT void mediaxx_get_media_info(const char* filepath) {
    auto item = MediaInfoItem_c{std::string_view{filepath}};
    if (MediaInfoReader_c::instance.openFile(item)) {
        std::cout << "读取文件成功" << std::endl;
        auto sb            = MediaInfoReader_c::instance.toInfoMap(item.fmtCtx);
        std::string_view p = sb.view();
        std::cout << "info json:" << std::endl << p << std::endl;
    } else {
        std::cout << "读取文件失败" << std::endl;
    }
    item.dispose();
}

FFI_PLUGIN_EXPORT int
    mediaxx_get_audio_visualization(const char* filepath, const char* output) {
    auto ret = AudioVisualization_c::instance.analyse(filepath, output);
    if (ret) {
        return 1;
    }
    return 0;
}