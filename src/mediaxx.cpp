#include "mediaxx.h"
#include "analyse/audio_visualization.h"
#include "analyse/media_info_reader.h"
#include "simdjson.h"
#include "util/string_util.h"
#include "util/utilxx.h"
#include <iostream>
#include <string>
#include <string_view>

FFI_PLUGIN_EXPORT void* mediaxx_malloc(unsigned long long size) {
    return malloc(size);
}

FFI_PLUGIN_EXPORT void mediaxx_free(void* ptr) {
    free(ptr);
}

FFI_PLUGIN_EXPORT int mediaxx_get_libav_version() {
    auto ver = av_log_get_level();
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
    const char* headers,
    const char* pictureOutputPath,
    const char* picture96OutputPath,
    char**      log
) {
    assert(nullptr != filepath);
    assert(nullptr != headers);
    assert(nullptr != pictureOutputPath);
    assert(nullptr != picture96OutputPath);
    LXX_DEBEG("mediaxx_get_media_info_malloc : {} ......", filepath);
    auto  item   = MediaInfoItem_c{std::string_view{filepath}, log};
    char* result = nullptr;
    if (MediaInfoReader_c::instance.openFile(item, headers)) {
        // 读取信息
        auto             jsonsb    = MediaInfoReader_c::instance.toInfoMap(item);
        auto             pOutput   = std::string_view{pictureOutputPath};
        auto             p96Output = std::string_view{picture96OutputPath};
        std::string_view json      = jsonsb.view();
        result                     = StringUtilxx_c::stringCopyMalloc(json);
        if (false == pOutput.empty()) {
            // 读取图片
            MediaInfoReader_c::instance.savePicture(item, pOutput, p96Output);
        }
    } else {
        result = nullptr;
    }
    LXX_DEBEG("mediaxx_get_media_info_malloc done: {}", (void*)(result));
    item.dispose();
    return result;
}

FFI_PLUGIN_EXPORT int mediaxx_get_media_picture(
    const char* filepath,
    const char* headers,
    const char* pictureOutputPath,
    const char* picture96OutputPath,
    char**      log
) {
    assert(nullptr != filepath);
    assert(nullptr != headers);
    assert(nullptr != pictureOutputPath);
    assert(nullptr != picture96OutputPath);
    auto item   = MediaInfoItem_c{std::string_view{filepath}, log};
    int  result = 0;
    // 完整封面路径必须非空
    auto pOutput   = std::string_view{pictureOutputPath};
    auto p96Output = std::string_view{picture96OutputPath};
    if (false == pOutput.empty() && MediaInfoReader_c::instance.openFile(item, headers)) {
        result = MediaInfoReader_c::instance.savePicture(item, pOutput, p96Output);
    } else {
        result = 0;
    }
    item.dispose();
    return result;
}

FFI_PLUGIN_EXPORT int mediaxx_get_audio_visualization(const char* filepath, const char* output) {
    assert(nullptr != filepath);
    assert(nullptr != output);
    auto ret = AudioVisualization_c::instance.analyse(filepath, output);
    if (ret) {
        return 1;
    }
    return 0;
}