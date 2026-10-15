#pragma once

#if _WIN32
#include <windows.h>
#undef max
#undef min
#else
#include <pthread.h>
#include <unistd.h>
#endif

#if _WIN32
#define DllImport         __declspec(dllimport)
#define DllExport         __declspec(dllexport)
#define FFI_PLUGIN_EXPORT DllExport
#else
#define FFI_PLUGIN_EXPORT __attribute__((visibility("default")))
#endif

#if __cplusplus
extern "C" {
#endif

FFI_PLUGIN_EXPORT void* mediaxx_malloc(unsigned long long size);
FFI_PLUGIN_EXPORT void  mediaxx_free(const void* ptr);

FFI_PLUGIN_EXPORT int  mediaxx_get_log_level();
FFI_PLUGIN_EXPORT void mediaxx_set_log_level(int level);

FFI_PLUGIN_EXPORT const char* mediaxx_get_label_malloc();

/// # 获取音视频的信息和封面
///
/// ## Args:
/// - [filepath] 必要，音视频文件路径
/// - [pictureOutputPath] 可选，完整图片保存本地路径；指定才会提取图片
/// - [picture96OutputPath] 可选，缩略图保存本地路径; 指定 [pictureOutputPath]
/// 后这个参数才有效
///
/// ## Return:
/// - 返回 json 格式的音视频信息
FFI_PLUGIN_EXPORT int mediaxx_get_media_info_malloc(
    const char*  filepath,
    const char*  headers,
    const char*  pictureOutputPath,
    const char*  picture96OutputPath,
    const char** outResult,
    const char** outLog
);

/// # 获取音视频的封面
///
/// ## Args:
/// - [filepath] 必要，音视频文件路径
/// - [pictureOutputPath] 必要，完整图片保存本地路径
/// - [picture96OutputPath] 可选，缩略图保存本地路径; 指定 [pictureOutputPath]
/// 后这个参数才有效
/// - [pictureDisplayOutputPath] 可选，标准显示尺寸（512）缩放封面保存本地路径
///
/// ## Return:
/// - 返回操作结果位掩码 (bit 0: 原图成功, bit 1: 96缩略图成功, bit 2: 512显示图成功)
FFI_PLUGIN_EXPORT int mediaxx_get_media_picture(
    const char*  filepath,
    const char*  headers,
    const char*  pictureOutputPath,
    const char*  picture96OutputPath,
    const char*  pictureDisplayOutputPath,
    const char** outLog
);

FFI_PLUGIN_EXPORT int mediaxx_analyse_picture_color(
    const char*  filepath,
    const char*  data,
    const size_t dataSize,
    const char** outResult,
    const char** outLog
);

FFI_PLUGIN_EXPORT int mediaxx_analyse_picture_color_from_decoded_data(
    const char*  data,
    const size_t dataSize,
    const char** outResult,
    const char** outLog
);

/// # 分析音频频谱与波形数据
///
/// ## Args:
/// - [filepath] 必要，音频文件路径
/// - [outSpectrums] 输出频谱数据指针
/// - [outWavefrom] 输出波形振幅数据指针
/// - [outLog] 输出日志
/// - [maxDurationSeconds] 可选，最大分析时长限制（秒），<= 0 时不限制
///
/// ## Return:
/// - 返回生成的频谱字节长度
FFI_PLUGIN_EXPORT int mediaxx_get_audio_visualization(
    const char*  filepath,
    const char** outSpectrums,
    const char** outWavefrom,
    const char** outLog
#if __cplusplus
    , int maxDurationSeconds = 0
#else
    , int maxDurationSeconds
#endif
);

FFI_PLUGIN_EXPORT int
    mediaxx_convert_char_encoding(const char* str, const size_t dataSize, const char** out);

FFI_PLUGIN_EXPORT const char* mediaxx_get_available_hwcodec_list();

/// # 获取音频过滤器列表
///
/// 列出编译进来的 ffmpeg（libavfilter）里能直接写在 mpv 的 af 上的音频过滤器，
/// 判断规则与 mpv 源码 filters/f_lavfi.c 的 is_usable 相同。
///
/// ## Return:
/// - 返回 json 数组字符串（需调用 mediaxx_free 释放）
///   ```
///   [{"name":"volume","desc":"Volume adjustment.","flags":0}, ......]
///   ```
///   - [name] 过滤器名
///   - [desc] 过滤器说明，可能没有这个字段
///   - [flags] AVFILTER_FLAG_* 位掩码
///   - [mpv_name] 名字与 mpv 自带过滤器重名时才有的字段，内容为加 `lavfi-` 前缀后的名字
/// - 列表中不含 mpv 自带的过滤器（format、scaletempo 等），它们由 mpv 编译，
///   可以读 mpv 的 option-info/af/choices
FFI_PLUGIN_EXPORT const char* mediaxx_get_available_audio_filter_list();

/// # 获取视频过滤器列表
///
/// 列出编译进来的 ffmpeg（libavfilter）里能直接写在 mpv 的 vf 上的视频过滤器，
/// 字段含义同 [mediaxx_get_available_audio_filter_list]。
///
/// ## Return:
/// - 返回 json 数组字符串（需调用 mediaxx_free 释放）
FFI_PLUGIN_EXPORT const char* mediaxx_get_available_video_filter_list();

#if __cplusplus
}
#endif