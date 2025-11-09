
#include "mediaxx.h"
#include "analyse/audio_visualization.h"
#include "analyse/hwanalyse.h"
#include "analyse/media_info_reader.h"
#include "simdjson.h"
#include "util/log.h"
#include "util/string_util.h"
#include "util/utilxx.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

FFI_PLUGIN_EXPORT void* mediaxx_malloc(unsigned long long size) {
    return malloc(size);
}

FFI_PLUGIN_EXPORT void mediaxx_free(const void* ptr) {
    LXX_DEBEG("mediaxx_free : {}", ptr);
    // 如果此处出错，也可能是在此之前 ptr 已经越界访问，释放时 debug 检查出存在越界写入
    free(const_cast<void*>(ptr));
}

FFI_PLUGIN_EXPORT int mediaxx_get_log_level() {
    auto ver = av_log_get_level();
    return ver;
}

FFI_PLUGIN_EXPORT void mediaxx_set_log_level(int level) {
    av_log_set_level(level);
}

FFI_PLUGIN_EXPORT const char* mediaxx_get_label_malloc() {
    auto       str    = (char*)mediaxx_malloc(128);
    const auto target = std::string_view{"libmediaxx by coolight"};
    memcpy(str, target.data(), target.length() + 1);
    return str;
}

FFI_PLUGIN_EXPORT int mediaxx_get_media_info_malloc(
    const char*  filepath,
    const char*  headers,
    const char*  pictureOutputPath,
    const char*  picture96OutputPath,
    const char** outResult,
    const char** outLog
) {
    assert(nullptr != filepath);
    assert(nullptr != headers);
    assert(nullptr != pictureOutputPath);
    assert(nullptr != picture96OutputPath);
    assert(nullptr != outResult);
    assert(nullptr != outLog);
    LXX_DEBEG("mediaxx_get_media_info_malloc : {} ......", filepath);

    auto item  = MediaInfoItem_c{std::string_view{filepath}, outLog};
    int  ret   = 0;
    *outResult = nullptr;
    if (MediaInfoReader_c::instance.openFile(item, headers)) {
        // 读取信息
        auto             jsonsb    = MediaInfoReader_c::instance.toInfoMap(item);
        auto             pOutput   = std::string_view{pictureOutputPath};
        auto             p96Output = std::string_view{picture96OutputPath};
        std::string_view json      = jsonsb.view().value_unsafe();
        *outResult                 = StringUtilxx_c::stringCopyMalloc(json).data();

        if (false == pOutput.empty()) {
            // 读取图片
            ret = MediaInfoReader_c::instance.savePicture(item, pOutput, p96Output);
        } else {
            ret = 0;
        }
    } else {
        *outResult = nullptr;
        ret        = -1;
    }
    item.dispose();
    LXX_DEBEG("mediaxx_get_media_info_malloc done: {}", (void*)(*outResult));
    return ret;
}

FFI_PLUGIN_EXPORT int mediaxx_get_media_picture(
    const char*  filepath,
    const char*  headers,
    const char*  pictureOutputPath,
    const char*  picture96OutputPath,
    const char** log
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

FFI_PLUGIN_EXPORT const char* mediaxx_get_available_hwcodec_list() {
    auto             jsonsb = HWAnalyse_c::findAvailHW();
    std::string_view json   = jsonsb.view().value_unsafe();
    return StringUtilxx_c::stringCopyMalloc(json).data();
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