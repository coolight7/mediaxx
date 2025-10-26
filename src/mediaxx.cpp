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
    auto       str    = (char*)mediaxx_malloc(128);
    const auto target = std::string_view{"libmediaxx by coolight"};
    memcpy(str, target.data(), target.length() + 1);
    return str;
}

FFI_PLUGIN_EXPORT const char* mediaxx_get_media_info_malloc(
    const char* filepath,
    const char* pictureOutputPath,
    const char* picture96OutputPath
) {
    auto  item   = MediaInfoItem_c{std::string_view{filepath}};
    char* result = nullptr;
    if (MediaInfoReader_c::instance.openFile(item)) {
        auto jsonsb = MediaInfoReader_c::instance.toInfoMap(item.fmtCtx);
        std::string_view json = jsonsb.view();
        result                = (char*)mediaxx_malloc(json.size() + 1);
        memcpy(result, json.data(), json.length() + 1);
        auto pOutput   = std::string_view{pictureOutputPath};
        auto p96Output = std::string_view{picture96OutputPath};
        if (false == pOutput.empty()) {
            MediaInfoReader_c::instance
                .savePicture(item.fmtCtx, pOutput, p96Output);
        }
    } else {
        result = nullptr;
    }
    item.dispose();
    return result;
}

FFI_PLUGIN_EXPORT int
    mediaxx_get_audio_visualization(const char* filepath, const char* output) {
    auto ret = AudioVisualization_c::instance.analyse(filepath, output);
    if (ret) {
        return 1;
    }
    return 0;
}