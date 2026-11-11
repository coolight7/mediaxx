
#include "mediaxx.h"
#include "analyse/audio_visualization.h"
#include "analyse/chorus_analyse.h"
#include "analyse/codec_info.h"
#include "analyse/filter_info.h"
#include "analyse/image.h"
#include "analyse/media_info.h"
#include "analyse/media_name.h"
#include "simdjson.h"
#include "util/log.h"
#include "util/string_util.h"
#include "util/av_util.h"
#include <cstdlib>
#include <iostream>
#include <string>
#include <string_view>

using namespace mediaxx;

namespace {

    /// JSON 输出里的一个字符串字段（UTF-16 结果按 UTF-8 输出）
    void appendStringValue(simdjson::builder::string_builder& sb, std::u16string_view value) {
        sb.escape_and_append_with_quotes(util::utf16ToUtf8(value));
    }

    /// 输出字符串数组字段：`"key":["a","b"]`
    void appendStringArray(
        simdjson::builder::string_builder& sb,
        const std::string_view             key,
        const std::vector<std::u16string>& list
    ) {
        sb.escape_and_append_with_quotes(key);
        sb.append_colon();
        sb.start_array();
        bool isFirst = true;
        for (const auto& item : list) {
            if (false == isFirst) {
                sb.append_comma();
            }
            isFirst = false;
            appendStringValue(sb, item);
        }
        sb.end_array();
    }

    /// 解析歌手的 JSON 数组（NULL / 空串 / null / 非数组都按"没有歌手信息"处理）
    [[nodiscard]] std::vector<std::u16string> parseArtistList(const char* json) {
        std::vector<std::u16string> result{};
        if (nullptr == json) {
            return result;
        }
        const auto text = util::strTrim(std::string_view{json});
        if (text.empty() || text == "null") {
            return result;
        }
        simdjson::dom::parser  parser{};
        simdjson::dom::element document{};
        if (parser.parse(text.data(), text.size()).get(document) != simdjson::SUCCESS) {
            return result;
        }
        simdjson::dom::array array{};
        if (document.get_array().get(array) != simdjson::SUCCESS) {
            return result;
        }
        for (const auto& item : array) {
            std::string_view value{};
            if (item.get_string().get(value) == simdjson::SUCCESS) {
                result.emplace_back(util::utf8ToUtf16(value));
            }
        }
        return result;
    }

    /// 输出 `{"sections":[...]}` 之外的通用错误日志
    void setErrorLog(mediaxx::AnalyseLogItem_c& logItem, std::string_view message) {
        logItem.setLog(message);
    }

} // namespace

FFI_PLUGIN_EXPORT void* mediaxx_malloc(unsigned long long size) {
    return malloc(size);
}

FFI_PLUGIN_EXPORT void mediaxx_free(const void* ptr) {
    XX_LOGD("mediaxx_free : {}", ptr);
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
    XX_LOGD("mediaxx_get_media_info_malloc : {} ......", filepath);

    auto item  = mediaxx::MediaInfoEntity_c{std::string_view{filepath}, outLog};
    int  ret   = 0;
    *outResult = nullptr;
    if (mediaxx::MediaInfoReader_c::instance.openFile(item, headers)) {
        // 读取信息
        auto jsonsb    = mediaxx::MediaInfoReader_c::instance.getInfoMap(item);
        auto pOutput   = std::string_view{pictureOutputPath};
        auto p96Output = std::string_view{picture96OutputPath};
        *outResult     = mediaxx::util::stringCopyMalloc(jsonsb.view().value_unsafe()).data();

        if (false == pOutput.empty()) {
            // 读取图片
            ret = mediaxx::MediaInfoReader_c::instance.savePicture(item, pOutput, p96Output);
        } else {
            ret = 0;
        }
    } else {
        *outResult = nullptr;
        ret        = -1;
    }
    item.dispose();
    XX_LOGD("mediaxx_get_media_info_malloc done: {}", (void*)(*outResult));
    return ret;
}

FFI_PLUGIN_EXPORT int mediaxx_get_media_picture(
    const char*  filepath,
    const char*  headers,
    const char*  pictureOutputPath,
    const char*  picture96OutputPath,
    const char*  pictureDisplayOutputPath,
    const char** outLog
) {
    assert(nullptr != filepath);
    assert(nullptr != headers);
    assert(nullptr != pictureOutputPath);
    assert(nullptr != picture96OutputPath);
    assert(nullptr != outLog);
    auto item   = mediaxx::MediaInfoEntity_c{std::string_view{filepath}, outLog};
    int  result = 0;
    // 完整封面路径必须非空
    auto pOutput        = std::string_view{pictureOutputPath};
    auto p96Output      = std::string_view{picture96OutputPath};
    auto pDisplayOutput = (nullptr != pictureDisplayOutputPath)
                              ? std::string_view{pictureDisplayOutputPath}
                              : std::string_view{};
    if (false == pOutput.empty() && mediaxx::MediaInfoReader_c::instance.openFile(item, headers)) {
        result = mediaxx::MediaInfoReader_c::instance
                     .savePicture(item, pOutput, p96Output, pDisplayOutput);
    } else {
        result = 0;
    }
    item.dispose();
    return result;
}

FFI_PLUGIN_EXPORT int mediaxx_analyse_picture_color(
    const char*  filepath,
    const char*  data,
    const size_t dataSize,
    const char** outResult,
    const char** outLog
) {
    assert(nullptr != data || nullptr != filepath);
    assert(nullptr != outResult);
    assert(nullptr != outLog);
    // 失败分支也要给出确定值，不能依赖调用方预先初始化出参
    *outResult = nullptr;

    auto logItem = mediaxx::AnalyseLogItem_c{outLog};
    if (nullptr != data) {
        auto result = mediaxx::analyzePictureColorFromData(data, dataSize, logItem);
        if (nullptr != result) {
            *outResult = mediaxx::util::stringCopyMalloc(result->toJson().view().value_unsafe())
                             .data();
            return 1;
        }
    } else if (nullptr != filepath) {
        auto result = mediaxx::analysePictureColorFromPath(filepath, logItem);
        if (nullptr != result) {
            *outResult = mediaxx::util::stringCopyMalloc(result->toJson().view().value_unsafe())
                             .data();
            return 1;
        }
    }

    return 0;
}

FFI_PLUGIN_EXPORT int mediaxx_analyse_picture_color_from_decoded_data(
    const char*  data,
    const size_t dataSize,
    const char** outResult,
    const char** outLog
) {
    assert(nullptr != data && dataSize > 0);
    assert(nullptr != outResult);
    assert(nullptr != outLog);
    *outResult = nullptr;

    // auto logItem = mediaxx::AnalyseLogItem_c{outLog};
    if (nullptr != data) {
        auto result = analysePictureColorFromDecodedData(
            (const uint8_t*)data,
            dataSize,
            int(dataSize / 4),
            1,
            int(dataSize),
            4
        );
        if (nullptr != result) {
            *outResult = mediaxx::util::stringCopyMalloc(result->toJson().view().value_unsafe())
                             .data();
            return 1;
        }
    }

    return 0;
}

FFI_PLUGIN_EXPORT int mediaxx_get_audio_visualization(
    const char*  filepath,
    const char** outSpectrums,
    const char** outWavefrom,
    const char** outLog,
    int          maxDurationSeconds
) {
    assert(nullptr != filepath);
    assert(nullptr != outSpectrums);
    assert(nullptr != outWavefrom);
    assert(nullptr != outLog);
    *outSpectrums = nullptr;
    *outWavefrom  = nullptr;

    mediaxx::AudioSpectrumAnalyzer analyzer{};
    std::vector<std::array<uint8_t, mediaxx::AudioSpectrumAnalyzer::DEF_SPECTRUM_SIZE>>
                               spectrumData{};
    std::vector<unsigned char> waveformData{};
    if (analyzer.processAudio(filepath, spectrumData, waveformData, maxDurationSeconds)) {
        assert(spectrumData.size() == waveformData.size());
        const auto len = mediaxx::AudioSpectrumAnalyzer::DEF_SPECTRUM_SIZE * spectrumData.size();

        auto resultWaveform = (char*)mediaxx_malloc(waveformData.size());
        std::memcpy((void*)resultWaveform, waveformData.data(), waveformData.size());

        auto   resultSpectrums = (char*)mediaxx_malloc(len);
        size_t index           = 0;
        for (auto& spectrum : spectrumData) {
            std::memcpy(
                (void*)(resultSpectrums + index),
                spectrum.data(),
                mediaxx::AudioSpectrumAnalyzer::DEF_SPECTRUM_SIZE
            );
            index += mediaxx::AudioSpectrumAnalyzer::DEF_SPECTRUM_SIZE;
        }

        *outWavefrom  = resultWaveform;
        *outSpectrums = resultSpectrums;
        return len;
    }
    *outSpectrums = nullptr;
    *outWavefrom  = nullptr;
    return 0;
}

FFI_PLUGIN_EXPORT int
    mediaxx_convert_char_encoding(const char* str, const size_t dataSize, const char** out) {
    assert(nullptr != out);
    *out = nullptr;
    if (nullptr == str || dataSize <= 0) {
        return 0;
    }
    auto [isSucc, result]
        = mediaxx::util::autoConvertToUtf8(std::string_view{str, dataSize}, true);
    if (isSucc) {
        if (false == result.has_value()) {
            auto resultPtr = (char*)mediaxx_malloc(dataSize + 1);
            std::memcpy((void*)resultPtr, str, dataSize);
            resultPtr[dataSize] = '\0';
            *out                = resultPtr;
            return dataSize;
        }
        const auto& resultStr = result.value();
        auto        resultPtr = (char*)mediaxx_malloc(resultStr.length() + 1);
        std::memcpy((void*)resultPtr, resultStr.data(), resultStr.length());
        resultPtr[resultStr.length()] = '\0';
        *out                          = resultPtr;
        return resultStr.length();
    }
    return 0;
}

FFI_PLUGIN_EXPORT const char* mediaxx_get_available_hwcodec_list() {
    auto jsonsb = mediaxx::CodecInfo_c::findAvailCodec();
    return mediaxx::util::stringCopyMalloc(jsonsb.view().value_unsafe()).data();
}

FFI_PLUGIN_EXPORT const char* mediaxx_get_available_audio_filter_list() {
    auto jsonsb = mediaxx::FilterInfo_c::findAvailFilter(AVMEDIA_TYPE_AUDIO);
    return mediaxx::util::stringCopyMalloc(jsonsb.view().value_unsafe()).data();
}

FFI_PLUGIN_EXPORT const char* mediaxx_get_available_video_filter_list() {
    auto jsonsb = mediaxx::FilterInfo_c::findAvailFilter(AVMEDIA_TYPE_VIDEO);
    return mediaxx::util::stringCopyMalloc(jsonsb.view().value_unsafe()).data();
}

FFI_PLUGIN_EXPORT int mediaxx_analyse_audio_chorus(
    const char*  spectrums,
    size_t       spectrumsSize,
    const char*  waveform,
    size_t       waveformSize,
    long long    durationMs,
    const char*  configJson,
    const char** outResult,
    const char** outLog
) {
    assert(nullptr != outResult);
    assert(nullptr != outLog);
    *outResult   = nullptr;
    auto logItem = mediaxx::AnalyseLogItem_c{outLog};

    mediaxx::AudioChorusConfig_c config{};
    if (nullptr != configJson
        && false == mediaxx::AudioChorusConfig_c::fromJson(configJson, config)) {
        setErrorLog(logItem, "configJson 不是 JSON 对象");
        return 0;
    }
    if (nullptr == spectrums || 0 == spectrumsSize
        || 0 != spectrumsSize % mediaxx::AudioChorusAnalyzer_c::DEF_SPECTRUM_SIZE) {
        setErrorLog(logItem, "spectrums 长度不合法（应为每帧 256 字节的整数倍）");
        return 0;
    }

    const auto result = mediaxx::AudioChorusAnalyzer_c::analyse(
        reinterpret_cast<const uint8_t*>(spectrums),
        spectrumsSize,
        (nullptr != waveform && waveformSize > 0) ? reinterpret_cast<const uint8_t*>(waveform)
                                                  : nullptr,
        waveformSize,
        durationMs,
        config
    );
    simdjson::builder::string_builder sb{};
    result.toJson(sb);
    *outResult = mediaxx::util::stringCopyMalloc(sb.view().value_unsafe()).data();
    return 1;
}

FFI_PLUGIN_EXPORT int mediaxx_analyse_audio_chorus_structure(
    const char*  spectrums,
    size_t       spectrumsSize,
    const char*  waveform,
    size_t       waveformSize,
    long long    durationMs,
    const char*  configJson,
    const char** outResult,
    const char** outLog
) {
    assert(nullptr != outResult);
    assert(nullptr != outLog);
    *outResult   = nullptr;
    auto logItem = mediaxx::AnalyseLogItem_c{outLog};

    mediaxx::AudioChorusConfig_c config{};
    if (nullptr != configJson
        && false == mediaxx::AudioChorusConfig_c::fromJson(configJson, config)) {
        setErrorLog(logItem, "configJson 不是 JSON 对象");
        return 0;
    }
    if (nullptr == spectrums || 0 == spectrumsSize
        || 0 != spectrumsSize % mediaxx::AudioChorusAnalyzer_c::DEF_SPECTRUM_SIZE) {
        setErrorLog(logItem, "spectrums 长度不合法（应为每帧 256 字节的整数倍）");
        return 0;
    }

    const auto structure = mediaxx::AudioChorusAnalyzer_c::analyseStructure(
        reinterpret_cast<const uint8_t*>(spectrums),
        spectrumsSize,
        (nullptr != waveform && waveformSize > 0) ? reinterpret_cast<const uint8_t*>(waveform)
                                                  : nullptr,
        waveformSize,
        durationMs,
        config
    );
    if (false == structure.has_value()) {
        setErrorLog(logItem, "没有结构结果（音频过短、参数非法或整首静音）");
        return 0;
    }
    simdjson::builder::string_builder sb{};
    structure->toJson(sb);
    *outResult = mediaxx::util::stringCopyMalloc(sb.view().value_unsafe()).data();
    return 1;
}

FFI_PLUGIN_EXPORT int mediaxx_analyse_media_name(
    const char*  name,
    const char*  artistListJson,
    const char** outResult,
    const char** outLog
) {
    assert(nullptr != outResult);
    assert(nullptr != outLog);
    *outResult            = nullptr;
    const auto artistlist = parseArtistList(artistListJson);
    const auto result     = mediaxx::MediaInfoNameAnalyse_c::instance().getNameArtist(
        mediaxx::util::utf8ToUtf16(mediaxx::util::toStringNotNull(name)),
        artistlist
    );

    simdjson::builder::string_builder sb{};
    sb.start_object();
    sb.escape_and_append_with_quotes("name");
    sb.append_colon();
    appendStringValue(sb, result.first);
    sb.append_comma();
    appendStringArray(sb, "artists", result.second);
    sb.end_object();
    *outResult = mediaxx::util::stringCopyMalloc(sb.view().value_unsafe()).data();
    return 1;
}

FFI_PLUGIN_EXPORT int mediaxx_slice_song_artist(
    const char*  artist,
    int          append,
    int          weakArtist,
    const char** outResult,
    const char** outLog
) {
    assert(nullptr != outResult);
    assert(nullptr != outLog);
    *outResult = nullptr;
    std::u16string crude{};
    const auto     list = mediaxx::MediaNameAnalyseBase_c::sliceSongArtist(
        mediaxx::util::utf8ToUtf16(mediaxx::util::toStringNotNull(artist)),
        append,
        0 != weakArtist,
        crude
    );

    simdjson::builder::string_builder sb{};
    sb.start_object();
    appendStringArray(sb, "artists", list);
    sb.append_comma();
    sb.escape_and_append_with_quotes("crude");
    sb.append_colon();
    appendStringValue(sb, crude);
    sb.end_object();
    *outResult = mediaxx::util::stringCopyMalloc(sb.view().value_unsafe()).data();
    return 1;
}

FFI_PLUGIN_EXPORT int mediaxx_create_lyric_search_str(
    const char*  name,
    const char*  artist,
    int          weakArtist,
    const char** outResult,
    const char** outLog
) {
    assert(nullptr != outResult);
    assert(nullptr != outLog);
    *outResult        = nullptr;
    const auto result = mediaxx::LyricSearchStrAnalyse_c::instance().createLyricSearchStr(
        mediaxx::util::utf8ToUtf16(mediaxx::util::toStringNotNull(name)),
        mediaxx::util::utf8ToUtf16(mediaxx::util::toStringNotNull(artist)),
        0 != weakArtist
    );

    simdjson::builder::string_builder sb{};
    sb.start_object();
    sb.escape_and_append_with_quotes("str");
    sb.append_colon();
    appendStringValue(sb, result);
    sb.end_object();
    *outResult = mediaxx::util::stringCopyMalloc(sb.view().value_unsafe()).data();
    return 1;
}

FFI_PLUGIN_EXPORT int mediaxx_get_avail_song_info(
    const char*  name,
    const char*  notAvailResult,
    const char** outResult,
    const char** outLog
) {
    assert(nullptr != outResult);
    assert(nullptr != outLog);
    *outResult = nullptr;
    std::optional<std::u16string> nameValue{};
    if (nullptr != name) {
        nameValue = mediaxx::util::utf8ToUtf16(name);
    }
    std::optional<std::u16string> notAvailValue{};
    if (nullptr != notAvailResult) {
        notAvailValue = mediaxx::util::utf8ToUtf16(notAvailResult);
    }
    const auto value = mediaxx::MediaNameAnalyseBase_c::getAvailSongInfo(nameValue, notAvailValue);

    simdjson::builder::string_builder sb{};
    sb.start_object();
    sb.escape_and_append_with_quotes("name");
    sb.append_colon();
    if (value.has_value()) {
        appendStringValue(sb, *value);
    } else {
        sb.append_null();
    }
    sb.end_object();
    *outResult = mediaxx::util::stringCopyMalloc(sb.view().value_unsafe()).data();
    return 1;
}

FFI_PLUGIN_EXPORT int mediaxx_check_avail_lyric_line(const char* line) {
    if (nullptr == line) {
        return 0;
    }
    return mediaxx::MediaNameAnalyseBase_c::checkAvailLyricLine(mediaxx::util::utf8ToUtf16(line)
           )
               ? 1
               : 0;
}

FFI_PLUGIN_EXPORT int
    mediaxx_get_artist_aliases(const char* artist, const char** outResult, const char** outLog) {
    assert(nullptr != outResult);
    assert(nullptr != outLog);
    *outResult      = nullptr;
    const auto list = mediaxx::MediaNameAnalyseBase_c::artistAliases(
        mediaxx::util::utf8ToUtf16(mediaxx::util::toStringNotNull(artist))
    );

    simdjson::builder::string_builder sb{};
    sb.start_object();
    appendStringArray(sb, "aliases", list);
    sb.end_object();
    *outResult = mediaxx::util::stringCopyMalloc(sb.view().value_unsafe()).data();
    return 1;
}
