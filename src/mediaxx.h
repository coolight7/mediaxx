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
///
/// ## Return:
/// - 返回操作是否成功
FFI_PLUGIN_EXPORT int mediaxx_get_media_picture(
    const char*  filepath,
    const char*  headers,
    const char*  pictureOutputPath,
    const char*  picture96OutputPath,
    const char** log
);

FFI_PLUGIN_EXPORT const char* mediaxx_get_available_hwcodec_list();

FFI_PLUGIN_EXPORT int mediaxx_get_audio_visualization(const char* filepath, const char* output);

#if __cplusplus
}
#endif