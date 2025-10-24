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

extern "C" {

FFI_PLUGIN_EXPORT void* mediaxx_malloc(int size);
FFI_PLUGIN_EXPORT void  mediaxx_free(void* ptr);

FFI_PLUGIN_EXPORT int get_libav_version();

FFI_PLUGIN_EXPORT void get_media_info(const char* filepath);
}