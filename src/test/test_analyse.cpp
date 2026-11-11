// 一致性测试：mediaxx 的 C++ 实现 vs musicxx 的 Dart 实现
//
// 用例与期望值由 musicxx 的 `test/tool/mediaxx_analyse_fixture_dump.dart` 从 Dart 运行期导出，
// 放在 `src/test/data/analyse/`。这里只做纯计算，不读音频文件，所以放在 test() 的最前面跑。
//
// 比对规则：
// * 整数、字符串、布尔：完全相等；
// * 浮点：允许 1e-9 的绝对/相对误差（两个语言的 libm（pow/log）可能差最后一位；
//   有响度数据时全程只有 +-*\/ 与 sqrt，实际逐位相等，误差统计会打印出来）；
// * 集合类字段（歌手列表、别名列表）两边都排序后比较：Dart 的 HashSet 迭代顺序不保证。
#include "analyse/chorus_analyse.h"
#include "analyse/media_name.h"
#include "analyse/media_name_symbol.h"
#include "mediaxx.h"
#include "simdjson.h"
#include "util/string_util.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace {

    int    g_failed        = 0;
    int    g_checked       = 0;
    double g_maxDoubleDiff = 0;

    void reportFail(const std::string& label, const std::string& detail) {
        ++g_failed;
        if (g_failed <= 40) {
            std::cout << "    [FAIL] " << label << " | " << detail << std::endl;
        }
    }

    void expectTrue(const std::string& label, bool condition, const std::string& detail = "") {
        ++g_checked;
        if (false == condition) {
            reportFail(label, detail.empty() ? "条件不成立" : detail);
        }
    }

    void expectInt(const std::string& label, long long actual, long long expected) {
        ++g_checked;
        if (actual != expected) {
            reportFail(
                label,
                "actual=" + std::to_string(actual) + " expected=" + std::to_string(expected)
            );
        }
    }

    void expectString(
        const std::string& label,
        const std::string& actual,
        const std::string& expected
    ) {
        ++g_checked;
        if (actual != expected) {
            reportFail(label, "actual=\"" + actual + "\" expected=\"" + expected + "\"");
        }
    }

    void expectDouble(const std::string& label, double actual, double expected) {
        ++g_checked;
        const double diff      = std::abs(actual - expected);
        const double tolerance = 1e-9 * std::max(1.0, std::abs(expected));
        if (diff > tolerance) {
            std::ostringstream stream{};
            stream << "actual=" << actual << " expected=" << expected << " diff=" << diff;
            reportFail(label, stream.str());
            return;
        }
        g_maxDoubleDiff = std::max(g_maxDoubleDiff, diff);
    }

    // ── simdjson 取值（element 与 simdjson_result<element> 都能传进来） ──

    template<typename T>
    std::optional<double> numberOf(const T& value) {
        double number = 0;
        if (value.get_double().get(number) == simdjson::SUCCESS) {
            return number;
        }
        int64_t integer = 0;
        if (value.get_int64().get(integer) == simdjson::SUCCESS) {
            return static_cast<double>(integer);
        }
        uint64_t unsignedInteger = 0;
        if (value.get_uint64().get(unsignedInteger) == simdjson::SUCCESS) {
            return static_cast<double>(unsignedInteger);
        }
        return std::nullopt;
    }

    template<typename T>
    std::string stringOf(const T& value) {
        std::string_view text{};
        if (value.get_string().get(text) == simdjson::SUCCESS) {
            return std::string{text};
        }
        return {};
    }

    template<typename T>
    bool boolOf(const T& value, bool fallback = false) {
        bool result = false;
        if (value.get_bool().get(result) == simdjson::SUCCESS) {
            return result;
        }
        return fallback;
    }

    template<typename T>
    bool isObjectValue(const T& value) {
        simdjson::dom::object object{};
        return value.get_object().get(object) == simdjson::SUCCESS;
    }

    template<typename T>
    bool isArrayValue(const T& value) {
        simdjson::dom::array array{};
        return value.get_array().get(array) == simdjson::SUCCESS;
    }

    template<typename T>
    bool isStringValue(const T& value) {
        std::string_view text{};
        return value.get_string().get(text) == simdjson::SUCCESS;
    }

    template<typename T>
    int intOf(const T& value, int fallback = 0) {
        const auto number = numberOf(value);
        return number.has_value() ? static_cast<int>(*number) : fallback;
    }

    template<typename T>
    long long longOf(const T& value, long long fallback = 0) {
        const auto number = numberOf(value);
        return number.has_value() ? static_cast<long long>(*number) : fallback;
    }

    template<typename T>
    simdjson::dom::array arrayOf(const T& value) {
        simdjson::dom::array array{};
        if (value.get_array().get(array) != simdjson::SUCCESS) {
            reportFail("simdjson", "取值不是数组");
        }
        return array;
    }

    template<typename T>
    simdjson::dom::object objectOf(const T& value) {
        simdjson::dom::object object{};
        if (value.get_object().get(object) != simdjson::SUCCESS) {
            reportFail("simdjson", "取值不是对象");
        }
        return object;
    }

    simdjson::dom::element elementAt(simdjson::dom::array array, size_t index) {
        simdjson::dom::element element{};
        if (array.at(index).get(element) != simdjson::SUCCESS) {
            reportFail("simdjson", "数组下标越界");
        }
        return element;
    }

    /// 解析 JSON 文本（解析失败返回空）
    std::optional<simdjson::dom::element>
        parseJson(simdjson::dom::parser& parser, const std::string& text) {
        simdjson::dom::element document{};
        if (parser.parse(text).get(document) != simdjson::SUCCESS) {
            return std::nullopt;
        }
        return document;
    }

    /// 用例文件位置（按相对路径依次尝试，兼容从仓库根或 build 目录运行）
    std::string fixturePath(const std::string& name) {
        const std::vector<std::string> candidates = {
            "./src/test/data/analyse/" + name,
            "../src/test/data/analyse/" + name,
            "./data/analyse/" + name,
            "./test/data/analyse/" + name,
        };
        for (const auto& path : candidates) {
            std::ifstream file{path};
            if (file.good()) {
                return path;
            }
        }
        return {};
    }

    std::string readAll(const std::string& path) {
        std::ifstream     file{path, std::ios::binary};
        std::stringstream buffer{};
        buffer << file.rdbuf();
        return buffer.str();
    }

    /// 重建 config 的 JSON（把用例里的数字原样写回去，用来覆盖 fromJson 的解析）
    template<typename T>
    std::string buildConfigJson(const T& value) {
        if (false == isObjectValue(value)) {
            return {};
        }
        const simdjson::dom::object object = objectOf(value);
        const char*                 keys[] = {
            "bandCount",
            "hopFrames",
            "windowFrames",
            "minRepeatLagSeconds",
            "segmentSimilarity",
            "minSegmentSeconds",
            "maxSegmentSeconds",
            "mergeGapSeconds",
            "maxSegments",
        };
        std::string json    = "{";
        bool        isFirst = true;
        for (const char* key : keys) {
            const auto number = numberOf(object[key]);
            if (false == number.has_value()) {
                continue;
            }
            if (false == isFirst) {
                json += ",";
            }
            isFirst = false;
            char buffer[128];
            std::snprintf(buffer, sizeof(buffer), "\"%s\":%.17g", key, *number);
            json += buffer;
        }
        json += "}";
        return json;
    }

    /// 与 Dart 生成脚本相同的噪声（xorshift32，只有整数运算，两边逐位一致）
    class Noise_c {
    public:

        explicit Noise_c(uint32_t seed) :
            state(0 == seed ? 1 : seed) {}

        double next() {
            state ^= (state << 13) & 0xFFFFFFFFu;
            state ^= state >> 17;
            state ^= (state << 5) & 0xFFFFFFFFu;
            state &= 0xFFFFFFFFu;
            return static_cast<double>(state) / 4294967296.0;
        }

    private:

        uint32_t state;
    };

    struct FakeSection_c {
        int                 start = 0;
        int                 end   = 0;
        std::vector<double> bandDb{};
        double              levelDb = 0;
    };

    /// 按区段填频带 dB 与响度（与 Dart 生成脚本同一套公式），未覆盖的帧保持 0
    void buildWave(
        int                               totalFrames,
        const std::vector<FakeSection_c>& sections,
        uint32_t                          seed,
        std::vector<uint8_t>&             spectrums,
        std::vector<uint8_t>&             waveform
    ) {
        const int bandCount = 32;
        spectrums.assign(
            static_cast<size_t>(totalFrames) * mediaxx::AudioChorusAnalyzer_c::DEF_SPECTRUM_SIZE,
            0
        );
        waveform.assign(static_cast<size_t>(totalFrames), 0);
        const std::vector<int> ranges = mediaxx::AudioChorusAnalyzer_c::bandRanges(bandCount);
        Noise_c                noise{seed};
        for (const auto& section : sections) {
            const int start = std::max(0, section.start);
            const int end   = std::min(totalFrames, section.end);
            for (int f = start; f < end; ++f) {
                for (int k = 0; k < bandCount; ++k) {
                    const double db
                        = section.bandDb[static_cast<size_t>(k)] + (noise.next() - 0.5) * 3;
                    const long long value
                        = std::clamp<long long>(std::llround((db + 80) * 255 / 80), 0, 255);
                    for (int b = ranges[static_cast<size_t>(k) * 2];
                         b <= ranges[static_cast<size_t>(k) * 2 + 1];
                         ++b) {
                        spectrums
                            [static_cast<size_t>(f)
                                 * mediaxx::AudioChorusAnalyzer_c::DEF_SPECTRUM_SIZE
                             + static_cast<size_t>(b)]
                            = static_cast<uint8_t>(value);
                    }
                }
                const long long level = std::clamp<long long>(
                    std::llround((section.levelDb + 60) * 255 / 80),
                    0,
                    255
                );
                waveform[static_cast<size_t>(f)] = static_cast<uint8_t>(level);
            }
        }
    }

    // ===================================================================
    // 副歌
    // ===================================================================

    void compareChorusResult(
        const std::string&    label,
        simdjson::dom::object expected,
        simdjson::dom::object actual
    ) {
        const simdjson::dom::array expectedSections = arrayOf(expected["sections"]);
        const simdjson::dom::array actualSections   = arrayOf(actual["sections"]);
        expectInt(
            label + " 段数",
            static_cast<long long>(actualSections.size()),
            static_cast<long long>(expectedSections.size())
        );
        if (actualSections.size() != expectedSections.size()) {
            return;
        }
        for (size_t index = 0; index < expectedSections.size(); ++index) {
            const simdjson::dom::element expectedSection = elementAt(expectedSections, index);
            const simdjson::dom::element actualSection   = elementAt(actualSections, index);
            const std::string            itemLabel       = label + " 段" + std::to_string(index);
            for (const char* key : {"start", "end", "repeat"}) {
                expectInt(
                    itemLabel + " " + key,
                    longOf(actualSection[key], -1),
                    longOf(expectedSection[key], -2)
                );
            }
            for (const char* key : {"confidence", "energy", "similarity"}) {
                expectDouble(
                    itemLabel + " " + key,
                    numberOf(actualSection[key]).value_or(0),
                    numberOf(expectedSection[key]).value_or(0)
                );
            }
        }
        expectInt(
            label + " 总时长",
            longOf(actual["totalDuration"], -1),
            longOf(expected["totalDuration"], -1)
        );
    }

    void compareChorusStructure(
        const std::string&    label,
        simdjson::dom::object expected,
        simdjson::dom::object actual,
        bool                  withArrays
    ) {
        for (const char* key :
             {"totalFrames",
              "hopFrames",
              "windowFrames",
              "coarseCount",
              "bandCount",
              "seedIndex",
              "regionStart",
              "regionLength"}) {
            expectInt(label + " 结构 " + key, longOf(actual[key], -1), longOf(expected[key], -2));
        }
        expectDouble(
            label + " 结构 regionSimilarity",
            numberOf(actual["regionSimilarity"]).value_or(0),
            numberOf(expected["regionSimilarity"]).value_or(0)
        );
        expectTrue(
            label + " 结构 isFallback",
            boolOf(actual["isFallback"]) == boolOf(expected["isFallback"])
        );

        const simdjson::dom::array expectedOccurrences = arrayOf(expected["occurrences"]);
        const simdjson::dom::array actualOccurrences   = arrayOf(actual["occurrences"]);
        expectInt(
            label + " 结构 出现次数",
            static_cast<long long>(actualOccurrences.size()),
            static_cast<long long>(expectedOccurrences.size())
        );
        if (actualOccurrences.size() == expectedOccurrences.size()) {
            for (size_t index = 0; index < expectedOccurrences.size(); ++index) {
                const simdjson::dom::element expectedItem = elementAt(expectedOccurrences, index);
                const simdjson::dom::element actualItem   = elementAt(actualOccurrences, index);
                const std::string            itemLabel = label + " 出现" + std::to_string(index);
                expectInt(
                    itemLabel + " start",
                    longOf(actualItem["start"], -1),
                    longOf(expectedItem["start"], -2)
                );
                expectInt(
                    itemLabel + " length",
                    longOf(actualItem["length"], -1),
                    longOf(expectedItem["length"], -2)
                );
                expectDouble(
                    itemLabel + " similarity",
                    numberOf(actualItem["similarity"]).value_or(0),
                    numberOf(expectedItem["similarity"]).value_or(0)
                );
                expectDouble(
                    itemLabel + " energy",
                    numberOf(actualItem["energy"]).value_or(0),
                    numberOf(expectedItem["energy"]).value_or(0)
                );
            }
        }

        if (false == withArrays) {
            return;
        }
        // 曲线按 base64 逐字节比较：有响度数据时全程只有 +-*/ 与 sqrt，两边 float32 应当完全相同
        for (const char* key :
             {"levelDb",
              "rawFeature",
              "energy",
              "energySmooth",
              "repeatBest",
              "repeatBestIndex",
              "repeatSmooth"}) {
            expectString(
                label + " 曲线 " + key,
                stringOf(actual[key]["base64"]),
                stringOf(expected[key]["base64"])
            );
        }
    }

    void runChorusCases() {
        const std::string path = fixturePath("chorus_cases.json");
        if (path.empty()) {
            reportFail("副歌用例", "找不到 chorus_cases.json");
            return;
        }
        simdjson::dom::parser parser{};
        const auto            document = parseJson(parser, readAll(path));
        if (false == document.has_value()) {
            reportFail("副歌用例", "chorus_cases.json 解析失败");
            return;
        }
        const simdjson::dom::array cases = arrayOf((*document)["cases"]);

        int caseCount = 0;
        for (const auto& item : cases) {
            ++caseCount;
            const std::string name            = stringOf(item["name"]);
            const int         totalFrames     = intOf(item["totalFrames"]);
            const uint32_t    noiseSeed       = static_cast<uint32_t>(intOf(item["noiseSeed"], 1));
            const bool        withWaveform    = boolOf(item["withWaveform"], true);
            const bool        structureArrays = boolOf(item["structureArrays"]);
            const bool        rawSpectrums    = boolOf(item["rawSpectrums"]);

            std::vector<FakeSection_c> sections{};
            for (const auto& sectionItem : arrayOf(item["sections"])) {
                FakeSection_c section{};
                section.start   = intOf(sectionItem["start"]);
                section.end     = intOf(sectionItem["end"]);
                section.levelDb = numberOf(sectionItem["levelDb"]).value_or(0);
                for (const auto& bandItem : arrayOf(sectionItem["bandDb"])) {
                    section.bandDb.push_back(numberOf(bandItem).value_or(0));
                }
                sections.push_back(std::move(section));
            }

            std::vector<uint8_t> spectrums{};
            std::vector<uint8_t> waveform{};
            if (rawSpectrums) {
                // 纯静音：输入全 0，不需要按区段填数据
                spectrums.assign(
                    static_cast<size_t>(totalFrames)
                        * mediaxx::AudioChorusAnalyzer_c::DEF_SPECTRUM_SIZE,
                    0
                );
                waveform.assign(static_cast<size_t>(totalFrames), 0);
            } else {
                buildWave(totalFrames, sections, noiseSeed, spectrums, waveform);
            }

            const std::string configJson  = buildConfigJson(item["config"]);
            const long long   durationMs  = longOf(item["durationMs"]);
            const char*       spectrumPtr = reinterpret_cast<const char*>(spectrums.data());
            const char*       waveformPtr
                = withWaveform ? reinterpret_cast<const char*>(waveform.data()) : nullptr;
            const size_t waveformSize = withWaveform ? waveform.size() : 0;

            // ── 区段结果 ──
            {
                const char* resultJson = nullptr;
                const char* log        = nullptr;
                const int   ret        = mediaxx_analyse_audio_chorus(
                    spectrumPtr,
                    spectrums.size(),
                    waveformPtr,
                    waveformSize,
                    durationMs,
                    configJson.empty() ? nullptr : configJson.c_str(),
                    &resultJson,
                    &log
                );
                expectInt(name + " analyse 返回", ret, 1);
                if (1 == ret && nullptr != resultJson) {
                    simdjson::dom::parser resultParser{};
                    const auto            result = parseJson(resultParser, resultJson);
                    expectTrue(name + " 结果 JSON 可解析", result.has_value());
                    if (result.has_value()) {
                        compareChorusResult(name, objectOf(item["expect"]), objectOf(*result));
                    }
                }
                if (nullptr != log) {
                    mediaxx_free(log);
                }
                if (nullptr != resultJson) {
                    mediaxx_free(resultJson);
                }
            }

            // ── 结构结果 ──
            {
                const char* structureJson = nullptr;
                const char* log           = nullptr;
                const int   ret           = mediaxx_analyse_audio_chorus_structure(
                    spectrumPtr,
                    spectrums.size(),
                    waveformPtr,
                    waveformSize,
                    durationMs,
                    configJson.empty() ? nullptr : configJson.c_str(),
                    &structureJson,
                    &log
                );
                const bool hasExpected = isObjectValue(item["expectStructure"]);
                expectInt(name + " structure 返回", ret, hasExpected ? 1 : 0);
                if (1 == ret && nullptr != structureJson && hasExpected) {
                    simdjson::dom::parser structureParser{};
                    const auto            structure = parseJson(structureParser, structureJson);
                    expectTrue(name + " 结构 JSON 可解析", structure.has_value());
                    if (structure.has_value()) {
                        compareChorusStructure(
                            name,
                            objectOf(item["expectStructure"]),
                            objectOf(*structure),
                            structureArrays
                        );
                    }
                }
                if (nullptr != log) {
                    mediaxx_free(log);
                }
                if (nullptr != structureJson) {
                    mediaxx_free(structureJson);
                }
            }
        }
        std::cout << "  副歌用例 " << caseCount << " 个" << std::endl;
    }

    void runBandRanges() {
        const std::string path = fixturePath("band_ranges.json");
        if (path.empty()) {
            reportFail("压带表", "找不到 band_ranges.json");
            return;
        }
        simdjson::dom::parser parser{};
        const auto            document = parseJson(parser, readAll(path));
        if (false == document.has_value()) {
            reportFail("压带表", "band_ranges.json 解析失败");
            return;
        }
        expectInt(
            "压带表 频点数",
            longOf((*document)["spectrumSize"]),
            mediaxx::AudioChorusAnalyzer_c::DEF_SPECTRUM_SIZE
        );
        int count = 0;
        for (const auto& field : objectOf((*document)["ranges"])) {
            ++count;
            const int              bandCount = std::atoi(std::string{field.key}.c_str());
            const std::vector<int> actual = mediaxx::AudioChorusAnalyzer_c::bandRanges(bandCount);
            const simdjson::dom::array expected = arrayOf(field.value);
            if (expected.size() != actual.size()) {
                reportFail("压带表 bandCount=" + std::to_string(bandCount), "项数不一致");
                continue;
            }
            for (size_t index = 0; index < expected.size(); ++index) {
                expectInt(
                    "压带表 bandCount=" + std::to_string(bandCount) + " 第" + std::to_string(index),
                    actual[index],
                    longOf(expected.at(index), -1)
                );
            }
        }
        std::cout << "  压带表 " << count << " 组" << std::endl;
    }

    // ===================================================================
    // 智能识别
    // ===================================================================

    std::vector<std::string>
        readStringArray(const simdjson::dom::array& array, bool sorted = true) {
        std::vector<std::string> result{};
        for (const auto& item : array) {
            result.push_back(stringOf(item));
        }
        if (sorted) {
            std::sort(result.begin(), result.end());
        }
        return result;
    }

    std::string joinStrings(const std::vector<std::string>& list) {
        std::string text{};
        for (const auto& item : list) {
            text += item + " | ";
        }
        return text;
    }

    void compareStringSets(
        const std::string&              label,
        const std::vector<std::string>& actual,
        std::vector<std::string>        expected
    ) {
        std::sort(expected.begin(), expected.end());
        expectInt(
            label,
            static_cast<long long>(actual.size()),
            static_cast<long long>(expected.size())
        );
        if (actual.size() != expected.size()) {
            reportFail(label + " 内容", joinStrings(actual));
            return;
        }
        for (size_t index = 0; index < actual.size(); ++index) {
            expectString(label + " 第" + std::to_string(index), actual[index], expected[index]);
        }
    }

    /// 歌手的 JSON 字符串数组 → JSON 文本（用例里的数组元素只会是普通名字）
    std::string buildArtistListJson(const simdjson::dom::array& array) {
        if (0 == array.size()) {
            return {};
        }
        std::string json    = "[";
        bool        isFirst = true;
        for (const auto& item : array) {
            if (false == isFirst) {
                json += ",";
            }
            isFirst  = false;
            json    += "\"" + stringOf(item) + "\"";
        }
        json += "]";
        return json;
    }

    void runMediaNameCases() {
        const std::string path = fixturePath("media_name_cases.json");
        if (path.empty()) {
            reportFail("智能识别用例", "找不到 media_name_cases.json");
            return;
        }
        simdjson::dom::parser parser{};
        const auto            document = parseJson(parser, readAll(path));
        if (false == document.has_value()) {
            reportFail("智能识别用例", "media_name_cases.json 解析失败");
            return;
        }

        // ── 歌名解析 ──
        {
            int count = 0;
            for (const auto& item : arrayOf((*document)["mediaName"])) {
                ++count;
                const std::string name       = stringOf(item["name"]);
                const std::string artistJson = isArrayValue(item["artists"])
                                                   ? buildArtistListJson(arrayOf(item["artists"]))
                                                   : std::string{};
                const char*       resultJson = nullptr;
                const char*       log        = nullptr;
                const int         ret        = mediaxx_analyse_media_name(
                    name.c_str(),
                    artistJson.empty() ? nullptr : artistJson.c_str(),
                    &resultJson,
                    &log
                );
                expectInt("媒体名 返回 " + name, ret, 1);
                if (1 == ret && nullptr != resultJson) {
                    simdjson::dom::parser resultParser{};
                    const auto            result = parseJson(resultParser, resultJson);
                    expectTrue("媒体名 JSON " + name, result.has_value());
                    if (result.has_value()) {
                        expectString(
                            "媒体名 歌名「" + name + "」",
                            stringOf((*result)["name"]),
                            stringOf(item["expectName"])
                        );
                        compareStringSets(
                            "媒体名 歌手「" + name + "」",
                            readStringArray(arrayOf((*result)["artists"])),
                            readStringArray(arrayOf(item["expectArtists"]))
                        );
                    }
                }
                if (nullptr != log) {
                    mediaxx_free(log);
                }
                if (nullptr != resultJson) {
                    mediaxx_free(resultJson);
                }
            }
            std::cout << "  歌名解析用例 " << count << " 个" << std::endl;
        }

        // ── 歌手切分 ──
        {
            int count = 0;
            for (const auto& item : arrayOf((*document)["sliceArtist"])) {
                ++count;
                const std::string artist = stringOf(item["artist"]);
                // 用例里的 append 是 Dart 的入参：-2 = null（用映射值替换）、1 = true（原值 +
                // 映射值）、 0 与 -1 都是 false（只保留原值）。C 接口用 -1 / 0 / 1 对应这三种情况。
                const int   appendRaw  = intOf(item["append"], -2);
                const int   append     = (-2 == appendRaw) ? -1 : ((1 == appendRaw) ? 1 : 0);
                const int   weak       = boolOf(item["weak"]) ? 1 : 0;
                const char* resultJson = nullptr;
                const char* log        = nullptr;
                const int   ret
                    = mediaxx_slice_song_artist(artist.c_str(), append, weak, &resultJson, &log);
                expectInt("歌手切分 返回 " + artist, ret, 1);
                if (1 == ret && nullptr != resultJson) {
                    simdjson::dom::parser resultParser{};
                    const auto            result = parseJson(resultParser, resultJson);
                    expectTrue("歌手切分 JSON " + artist, result.has_value());
                    if (result.has_value()) {
                        compareStringSets(
                            "歌手切分「" + artist + "」append=" + std::to_string(append)
                                + " weak=" + std::to_string(weak),
                            readStringArray(arrayOf((*result)["artists"])),
                            readStringArray(arrayOf(item["expectArtists"]))
                        );
                        expectString(
                            "歌手切分 原串「" + artist + "」",
                            stringOf((*result)["crude"]),
                            stringOf(item["expectCrude"])
                        );
                    }
                }
                if (nullptr != log) {
                    mediaxx_free(log);
                }
                if (nullptr != resultJson) {
                    mediaxx_free(resultJson);
                }
            }
            std::cout << "  歌手切分用例 " << count << " 个" << std::endl;
        }

        // ── 歌词搜索串 ──
        {
            int count     = 0;
            int orderOnly = 0;
            for (const auto& item : arrayOf((*document)["lyricSearch"])) {
                ++count;
                const std::string name       = stringOf(item["name"]);
                const std::string artist     = stringOf(item["artist"]);
                const int         weak       = boolOf(item["weak"]) ? 1 : 0;
                const char*       resultJson = nullptr;
                const char*       log        = nullptr;
                const int         ret        = mediaxx_create_lyric_search_str(
                    name.c_str(),
                    artist.c_str(),
                    weak,
                    &resultJson,
                    &log
                );
                expectInt("歌词搜索串 返回 " + name, ret, 1);
                if (1 == ret && nullptr != resultJson) {
                    simdjson::dom::parser resultParser{};
                    const auto            result = parseJson(resultParser, resultJson);
                    if (result.has_value()) {
                        const std::string actual   = stringOf((*result)["str"]);
                        const std::string expected = stringOf(item["expect"]);
                        if (actual == expected) {
                            ++g_checked;
                        } else {
                            // 歌手集合的迭代顺序两边不保证一致：按 " - " 切开排序后再比一次
                            const auto splitTokens = [](const std::string& text) {
                                std::vector<std::string> tokens{};
                                size_t                   start = 0;
                                while (true) {
                                    const size_t pos = text.find(" - ", start);
                                    if (pos == std::string::npos) {
                                        tokens.push_back(text.substr(start));
                                        break;
                                    }
                                    tokens.push_back(text.substr(start, pos - start));
                                    start = pos + 3;
                                }
                                std::sort(tokens.begin(), tokens.end());
                                return tokens;
                            };
                            if (splitTokens(actual) == splitTokens(expected)) {
                                ++orderOnly;
                                ++g_checked;
                            } else {
                                expectString(
                                    "歌词搜索串「" + name + " / " + artist + "」",
                                    actual,
                                    expected
                                );
                            }
                        }
                    }
                }
                if (nullptr != log) {
                    mediaxx_free(log);
                }
                if (nullptr != resultJson) {
                    mediaxx_free(resultJson);
                }
            }
            std::cout << "  歌词搜索串用例 " << count << " 个（其中 " << orderOnly
                      << " 个只有歌手顺序不同）" << std::endl;
        }

        // ── 可用歌曲信息 ──
        {
            int count = 0;
            for (const auto& item : arrayOf((*document)["availInfo"])) {
                ++count;
                const bool        hasName     = isStringValue(item["name"]);
                const std::string name        = hasName ? stringOf(item["name"]) : std::string{};
                const bool        hasNotAvail = isStringValue(item["notAvail"]);
                const std::string notAvail
                    = hasNotAvail ? stringOf(item["notAvail"]) : std::string{};
                const char* resultJson = nullptr;
                const char* log        = nullptr;
                const int   ret        = mediaxx_get_avail_song_info(
                    hasName ? name.c_str() : nullptr,
                    hasNotAvail ? notAvail.c_str() : nullptr,
                    &resultJson,
                    &log
                );
                expectInt("可用性 返回", ret, 1);
                if (1 == ret && nullptr != resultJson) {
                    simdjson::dom::parser resultParser{};
                    const auto            result = parseJson(resultParser, resultJson);
                    if (result.has_value()) {
                        const std::string actual   = isStringValue((*result)["name"])
                                                         ? stringOf((*result)["name"])
                                                         : std::string{"<null>"};
                        const std::string expected = isStringValue(item["expect"])
                                                         ? stringOf(item["expect"])
                                                         : std::string{"<null>"};
                        expectString(
                            "可用性「" + (hasName ? name : std::string{"<null>"}) + "」",
                            actual,
                            expected
                        );
                    }
                }
                if (nullptr != log) {
                    mediaxx_free(log);
                }
                if (nullptr != resultJson) {
                    mediaxx_free(resultJson);
                }
            }
            std::cout << "  可用性用例 " << count << " 个" << std::endl;
        }

        // ── 歌词行 ──
        {
            int count = 0;
            for (const auto& item : arrayOf((*document)["lyricLine"])) {
                ++count;
                const std::string line     = stringOf(item["line"]);
                const bool        expected = boolOf(item["expect"]);
                const int         actual   = mediaxx_check_avail_lyric_line(line.c_str());
                expectInt("歌词行「" + line + "」", actual, expected ? 1 : 0);
            }
            std::cout << "  歌词行用例 " << count << " 个" << std::endl;
        }

        // ── 别名反查 ──
        {
            int count = 0;
            for (const auto& item : arrayOf((*document)["aliases"])) {
                ++count;
                const std::string artist     = stringOf(item["artist"]);
                const char*       resultJson = nullptr;
                const char*       log        = nullptr;
                const int ret = mediaxx_get_artist_aliases(artist.c_str(), &resultJson, &log);
                expectInt("别名 返回", ret, 1);
                if (1 == ret && nullptr != resultJson) {
                    simdjson::dom::parser resultParser{};
                    const auto            result = parseJson(resultParser, resultJson);
                    if (result.has_value()) {
                        compareStringSets(
                            "别名「" + artist + "」",
                            readStringArray(arrayOf((*result)["aliases"])),
                            readStringArray(arrayOf(item["expect"]))
                        );
                    }
                }
                if (nullptr != log) {
                    mediaxx_free(log);
                }
                if (nullptr != resultJson) {
                    mediaxx_free(resultJson);
                }
            }
            std::cout << "  别名反查用例 " << count << " 个" << std::endl;
        }
    }

    // ===================================================================
    // 符号合并
    // ===================================================================

    void runSymbolCases() {
        const std::string path = fixturePath("symbol_cases.json");
        if (path.empty()) {
            reportFail("符号用例", "找不到 symbol_cases.json");
            return;
        }
        simdjson::dom::parser parser{};
        const auto            document = parseJson(parser, readAll(path));
        if (false == document.has_value()) {
            reportFail("符号用例", "symbol_cases.json 解析失败");
            return;
        }
        int count = 0;
        for (const auto& item : arrayOf((*document)["cases"])) {
            ++count;
            const std::string    input = stringOf(item["input"]);
            const std::u16string value = mediaxx::util::utf8ToUtf16(input);
            const std::string    label = "符号「" + input + "」";
            expectString(
                label + " merge",
                mediaxx::util::utf16ToUtf8(mediaxx::MediaNameSymbol_c::mergeSymbols(value)),
                stringOf(item["merge"])
            );
            expectString(
                label + " strip",
                mediaxx::util::utf16ToUtf8(mediaxx::MediaNameSymbol_c::stripLeadingSymbols(value
                )),
                stringOf(item["strip"])
            );
            expectString(
                label + " collapse",
                mediaxx::util::utf16ToUtf8(mediaxx::MediaNameSymbol_c::collapseSpaces(value)),
                stringOf(item["collapse"])
            );
        }
        std::cout << "  符号用例 " << count << " 个" << std::endl;
    }

} // namespace

/// 跑全部一致性用例（纯计算，不依赖音频文件）
void runAnalyseTests() {
    std::cout << "======= Analyse Parity Start =======" << std::endl;

    // 编码转换与 base64 自检（后面所有比对都依赖它们）
    {
        const std::string    utf8  = "洛天依 🎵 test";
        const std::u16string utf16 = mediaxx::util::utf8ToUtf16(utf8);
        expectString("utf8→utf16→utf8", mediaxx::util::utf16ToUtf8(utf16), utf8);
        // 3 个汉字 + 空格 + 1 个非 BMP 字符（2 个码元）+ 空格 + 4 个字母
        expectInt("utf16 码元数", static_cast<long long>(utf16.size()), 11);
        const uint8_t bytes[]  = {1, 2, 3, 4};
        const uint8_t bytes2[] = {1, 2, 3};
        expectString("base64", mediaxx::util::base64Encode(bytes, 4), "AQIDBA==");
        expectString("base64 补一个 =", mediaxx::util::base64Encode(bytes2, 3), "AQID");
    }

    runBandRanges();
    runChorusCases();
    runSymbolCases();
    runMediaNameCases();

    std::cout << "  比对项 " << g_checked << " 个，浮点最大误差 " << g_maxDoubleDiff << std::endl;
    if (g_failed > 0) {
        std::cout << "======= Analyse Parity FAILED: " << g_failed << " 项 =======" << std::endl;
        std::abort();
    }
    std::cout << "======= Analyse Parity Done =======" << std::endl;
}
