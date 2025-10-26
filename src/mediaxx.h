#if _WIN32
#include <windows.h>
#else
#include <pthread.h>
#include <unistd.h>
#endif

#if _WIN32
#define DllImport         __declspec(dllimport)
#define DllExport         __declspec(dllexport)
#define FFI_PLUGIN_EXPORT DllExport
#else
#define FFI_PLUGIN_EXPORT
#endif

#if __cplusplus
extern "C" {
#endif

FFI_PLUGIN_EXPORT void* mediaxx_malloc(int size);
FFI_PLUGIN_EXPORT void  mediaxx_free(void* ptr);

FFI_PLUGIN_EXPORT int mediaxx_get_libav_version();

FFI_PLUGIN_EXPORT const char* mediaxx_get_label_malloc();

FFI_PLUGIN_EXPORT const char* mediaxx_get_media_info_malloc(
    const char* filepath,
    const char* savePicture,
    const char* savePicture96
);

FFI_PLUGIN_EXPORT int
    mediaxx_get_audio_visualization(const char* filepath, const char* output);

#if __cplusplus
}
#endif