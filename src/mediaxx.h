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

/// # 分析副歌（高潮）区段
///
/// 输入是 [mediaxx_get_audio_visualization] 产出的两份数据，不需要再读文件：
/// - [spectrums]：每帧 256 个频点，值为 -80~0 dB 的线性映射（0~255），默认 10 帧/秒
/// - [waveform]：每帧 1 字节 RMS 响度，值为 -60~+20 dB 的线性映射（0~255），可以不给
///   （不给时用频谱能量粗估响度）
///
/// ## Args:
/// - [spectrums] / [spectrumsSize]：频谱数据，长度必须是 256 的整数倍
/// - [waveform] / [waveformSize]：响度数据，可为 NULL / 0
/// - [durationMs]：歌曲时长（毫秒）；<= 0 时按帧数推算
/// - [configJson]：分析参数（键名与本项目 Dart 侧 `AudioChorusConfig_c` 相同：
///   bandCount / hopFrames / windowFrames / minRepeatLagSeconds / segmentSimilarity /
///   minSegmentSeconds / maxSegmentSeconds / mergeGapSeconds / maxSegments）；NULL 用默认值
/// - [outResult]：输出的 JSON，成功后用 mediaxx_free 释放
/// - [outLog]：失败原因，可为 NULL；有内容时用 mediaxx_free 释放
///
/// ## Return:
/// - 1：成功。[outResult] 为
///   `{"sections":[{"start":毫秒,"end":毫秒,"confidence":0~1,"energy":0~1,
///                   "repeat":出现次数,"similarity":0~1}],"totalDuration":毫秒,"analyseTimeMs":毫秒}`
///   空数组表示分析过了、这首歌没有副歌
/// - 0：输入不合法（[outLog] 给出原因）
FFI_PLUGIN_EXPORT int mediaxx_analyse_audio_chorus(
    const char*  spectrums,
    size_t       spectrumsSize,
    const char*  waveform,
    size_t       waveformSize,
    long long    durationMs,
    const char*  configJson,
    const char** outResult,
    const char** outLog
);

/// # 分析副歌的结构中间结果（调试、离线分析、一致性核对用）
///
/// 参数与 [mediaxx_analyse_audio_chorus] 相同。返回的 JSON 比区段结果多出：
/// `totalFrames` / `hopFrames` / `windowFrames` / `coarseCount` / `bandCount` /
/// `seedIndex` / `regionStart` / `regionLength` / `regionSimilarity` / `isFallback` /
/// `occurrences`，以及每帧曲线 `levelDb` / `rawFeature` / `energy` / `energySmooth` /
/// `repeatBest` / `repeatBestIndex` / `repeatSmooth`（前两类是
/// `{"type":"float32"|"int32","base64":"..."}`，小端）。
///
/// ## Return:
/// - 1：成功（音频过短 / 参数非法 / 整首静音时返回 0，不产生结果）
FFI_PLUGIN_EXPORT int mediaxx_analyse_audio_chorus_structure(
    const char*  spectrums,
    size_t       spectrumsSize,
    const char*  waveform,
    size_t       waveformSize,
    long long    durationMs,
    const char*  configJson,
    const char** outResult,
    const char** outLog
);

/// # 歌名 / 歌手智能识别
///
/// 等价于 Dart 侧的 `MediaInfoAnalyse_c.songNameAnalyse.getNameArtist`：
/// 去掉歌名里的推广词、括号标签，抽出歌手名并统一别名。
///
/// ## Args:
/// - [name]：原始歌名（UTF-8）
/// - [artistListJson]：已知歌手的 JSON 字符串数组，NULL / 空串 / `null` 表示没有
/// - [outResult]：`{"name":"解析后的歌名","artists":["歌手", ...]}`，用 mediaxx_free 释放
/// - [outLog]：失败原因，可为 NULL
///
/// ## Return:
/// - 1：成功
/// - 0：输入不合法
FFI_PLUGIN_EXPORT int mediaxx_analyse_media_name(
    const char*  name,
    const char*  artistListJson,
    const char** outResult,
    const char** outLog
);

/// # 歌手切分
///
/// 等价于 Dart 侧的 `MediaInfoAnalyseBase_c.sliceSongArtist`：按分隔符切开并统一别名。
///
/// ## Args:
/// - [artist]：歌手串（UTF-8）
/// - [append]：1 = 保留原值并追加别名，0 = 只保留原值，-1 = 用别名替换原值
/// - [weakArtist]：非 0 时只保留别名表里登记过的名字
/// - [outResult]：`{"artists":["..."],"crude":"去掉两端空白的原串"}`，用 mediaxx_free 释放
///
/// ## Return:
/// - 1：成功
FFI_PLUGIN_EXPORT int mediaxx_slice_song_artist(
    const char*  artist,
    int          append,
    int          weakArtist,
    const char** outResult,
    const char** outLog
);

/// # 生成歌词搜索串
///
/// 等价于 Dart 侧的 `LyricSearchStrAnalyse.createLyricSearchStr`：
/// 歌名 + 歌手 → 用于歌词搜索的字符串（去标签、补歌手、截断到 50 个码元字节内）。
///
/// ## Args:
/// - [name] 歌名，[artist] 歌手串，[weakArtist] 非 0 时丢弃别名表里没有的歌手
/// - [outResult]：`{"str":"..."}`，用 mediaxx_free 释放
///
/// ## Return:
/// - 1：成功
FFI_PLUGIN_EXPORT int mediaxx_create_lyric_search_str(
    const char*  name,
    const char*  artist,
    int          weakArtist,
    const char** outResult,
    const char** outLog
);

/// # 判断歌曲信息是否可用
///
/// 等价于 Dart 侧的 `MediaInfoAnalyseBase_c.getAvailSongInfo`：过滤推广词、网址、
/// 乱码（含代理码元的字符串，包括正常 emoji）与 `kuwo` / `kugou` 这类占位值。
///
/// ## Args:
/// - [name]：原始歌名，可为 NULL
/// - [notAvailResult]：不可用时返回的内容（可为 NULL，此时返回 JSON 里的 null）
/// - [outResult]：`{"name":"..."}` 或 `{"name":null}`，用 mediaxx_free 释放
///
/// ## Return:
/// - 1：成功（"不可用"也是成功，看 [outResult] 里的 name 是否为 null）
FFI_PLUGIN_EXPORT int mediaxx_get_avail_song_info(
    const char*  name,
    const char*  notAvailResult,
    const char** outResult,
    const char** outLog
);

/// # 判断歌词行是否可用
///
/// 等价于 Dart 侧的 `MediaInfoAnalyseBase_c.checkAvailLyricLine`：
/// 过滤 `//`、`翻译`、版权声明、音乐平台推广语这类行。
///
/// ## Return:
/// - 1：可用 / 0：不可用
FFI_PLUGIN_EXPORT int mediaxx_check_avail_lyric_line(const char* line);

/// # 歌手别名反查
///
/// 等价于 Dart 侧的 `MediaInfoAnalyseBase_c.artistTagWordVK[artist]`：
/// 给一个规范歌手名，返回别名表里映射到它的那些写法（按字符串排序）。
///
/// ## Args:
/// - [artist]：歌手名（UTF-8）
/// - [outResult]：`{"aliases":["..."]}`，用 mediaxx_free 释放
///
/// ## Return:
/// - 1：成功（没有别名时 aliases 为空数组）
FFI_PLUGIN_EXPORT int
    mediaxx_get_artist_aliases(const char* artist, const char** outResult, const char** outLog);

#if __cplusplus
}
#endif