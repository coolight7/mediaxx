#include "analyse/chorus_analyse.h"

#include "util/string_util.h"

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <limits>

namespace mediaxx {

    namespace {

        /// 频点值（0~255）→ dB 的查表（-80 ~ 0 dB）
        [[nodiscard]] const std::array<float, 256>& binDbTable() {
            static const std::array<float, 256> table = [] {
                std::array<float, 256> result{};
                for (int i = 0; i < 256; ++i) {
                    result[static_cast<size_t>(i)]
                        = static_cast<float>(static_cast<double>(i) * (80.0 / 255.0) + (-80.0));
                }
                return result;
            }();
            return table;
        }

        /// 响度值（0~255）→ dB 的查表（-60 ~ +20 dB）
        [[nodiscard]] const std::array<float, 256>& waveDbTable() {
            static const std::array<float, 256> table = [] {
                std::array<float, 256> result{};
                for (int i = 0; i < 256; ++i) {
                    result[static_cast<size_t>(i)]
                        = static_cast<float>(static_cast<double>(i) * (80.0 / 255.0) + (-60.0));
                }
                return result;
            }();
            return table;
        }

        /// Dart 的 `num.clamp`
        [[nodiscard]] double clampDouble(double value, double minValue, double maxValue) {
            if (value < minValue) {
                return minValue;
            }
            if (value > maxValue) {
                return maxValue;
            }
            return value;
        }

        /// 打包成 `{"type":"float32","base64":"..."}`（小端，与 Dart 的 Float32List 内存布局一致）
        void appendFloatArray(
            simdjson::builder::string_builder& sb,
            const char*                        key,
            const std::vector<float>&          data
        ) {
            sb.escape_and_append_with_quotes(key);
            sb.append_colon();
            sb.start_object();
            sb.append_key_value<"type">(std::string_view{"float32"});
            sb.append_comma();
            sb.escape_and_append_with_quotes("base64");
            sb.append_colon();
            sb.escape_and_append_with_quotes(
                util::base64Encode(data.data(), data.size() * sizeof(float))
            );
            sb.end_object();
        }

        void appendInt32Array(
            simdjson::builder::string_builder& sb,
            const char*                        key,
            const std::vector<int32_t>&        data
        ) {
            sb.escape_and_append_with_quotes(key);
            sb.append_colon();
            sb.start_object();
            sb.append_key_value<"type">(std::string_view{"int32"});
            sb.append_comma();
            sb.escape_and_append_with_quotes("base64");
            sb.append_colon();
            sb.escape_and_append_with_quotes(
                util::base64Encode(data.data(), data.size() * sizeof(int32_t))
            );
            sb.end_object();
        }

        /// 从 JSON 对象里取一个数字（不是数字就当没给）
        [[nodiscard]] std::optional<double>
            numberAt(const simdjson::dom::object& object, const char* key) {
            double value = 0;
            if (object[key].get_double().get(value) == simdjson::SUCCESS) {
                return value;
            }
            int64_t integer = 0;
            if (object[key].get_int64().get(integer) == simdjson::SUCCESS) {
                return static_cast<double>(integer);
            }
            uint64_t unsignedInteger = 0;
            if (object[key].get_uint64().get(unsignedInteger) == simdjson::SUCCESS) {
                return static_cast<double>(unsignedInteger);
            }
            return std::nullopt;
        }

        constexpr double DEF_LINEAR_HZ        = 30;
        constexpr double DEF_LINEAR_TOP_HZ    = 3840;
        constexpr double DEF_MID_BAND_RATIO   = 1.0149078;
        constexpr double DEF_MID_TOP_HZ       = 12000;
        constexpr double DEF_HIGH_BAND_RATIO  = 1.0100666;
        constexpr double DEF_MAX_FREQUENCY_HZ = 20000;
        constexpr int    DEF_LINEAR_BANDS     = 3840 / 30;
        constexpr int    DEF_MID_BANDS        = 77;

    } // namespace

    // ===================================================================
    // AudioChorusConfig_c
    // ===================================================================

    bool AudioChorusConfig_c::fromJson(std::string_view json, AudioChorusConfig_c& out) {
        out                           = AudioChorusConfig_c{};
        const std::string_view trimed = util::strTrim(json);
        if (trimed.empty() || trimed == "null") {
            return true;
        }
        simdjson::dom::parser  parser{};
        simdjson::dom::element document{};
        if (parser.parse(json.data(), json.size()).get(document) != simdjson::SUCCESS) {
            return false;
        }
        simdjson::dom::object object{};
        if (document.get_object().get(object) != simdjson::SUCCESS) {
            return false;
        }
        if (const auto value = numberAt(object, "bandCount"); value.has_value()) {
            out.bandCount = static_cast<int>(*value);
        }
        if (const auto value = numberAt(object, "hopFrames"); value.has_value()) {
            out.hopFrames = static_cast<int>(*value);
        }
        if (const auto value = numberAt(object, "windowFrames"); value.has_value()) {
            out.windowFrames = static_cast<int>(*value);
        }
        if (const auto value = numberAt(object, "minRepeatLagSeconds"); value.has_value()) {
            out.minRepeatLagSeconds = *value;
        }
        if (const auto value = numberAt(object, "segmentSimilarity"); value.has_value()) {
            out.segmentSimilarity = *value;
        }
        if (const auto value = numberAt(object, "minSegmentSeconds"); value.has_value()) {
            out.minSegmentSeconds = *value;
        }
        if (const auto value = numberAt(object, "maxSegmentSeconds"); value.has_value()) {
            out.maxSegmentSeconds = *value;
        }
        if (const auto value = numberAt(object, "mergeGapSeconds"); value.has_value()) {
            out.mergeGapSeconds = *value;
        }
        if (const auto value = numberAt(object, "maxSegments"); value.has_value()) {
            out.maxSegments = static_cast<int>(*value);
        }
        return true;
    }

    // ===================================================================
    // JSON 输出
    // ===================================================================

    void AudioChorusStructure_c::toJson(simdjson::builder::string_builder& sb) const {
        sb.start_object();
        sb.append_key_value<"totalFrames">(totalFrames);
        sb.append_comma();
        sb.append_key_value<"hopFrames">(hopFrames);
        sb.append_comma();
        sb.append_key_value<"windowFrames">(windowFrames);
        sb.append_comma();
        sb.append_key_value<"coarseCount">(coarseCount);
        sb.append_comma();
        sb.append_key_value<"bandCount">(bandCount);
        sb.append_comma();
        sb.append_key_value<"seedIndex">(seedIndex);
        sb.append_comma();
        sb.append_key_value<"regionStart">(regionStart);
        sb.append_comma();
        sb.append_key_value<"regionLength">(regionLength);
        sb.append_comma();
        sb.append_key_value<"regionSimilarity">(regionSimilarity);
        sb.append_comma();
        sb.append_key_value<"isFallback">(isFallback);
        sb.append_comma();

        sb.escape_and_append_with_quotes("occurrences");
        sb.append_colon();
        sb.start_array();
        bool isFirst = true;
        for (const auto& item : occurrences) {
            if (false == isFirst) {
                sb.append_comma();
            }
            isFirst = false;
            sb.start_object();
            sb.append_key_value<"start">(item.start);
            sb.append_comma();
            sb.append_key_value<"length">(item.length);
            sb.append_comma();
            sb.append_key_value<"similarity">(item.similarity);
            sb.append_comma();
            sb.append_key_value<"energy">(item.energy);
            sb.end_object();
        }
        sb.end_array();
        sb.append_comma();

        appendFloatArray(sb, "levelDb", levelDb);
        sb.append_comma();
        appendFloatArray(sb, "rawFeature", rawFeature);
        sb.append_comma();
        appendFloatArray(sb, "energy", energy);
        sb.append_comma();
        appendFloatArray(sb, "energySmooth", energySmooth);
        sb.append_comma();
        appendFloatArray(sb, "repeatBest", repeatBest);
        sb.append_comma();
        appendInt32Array(sb, "repeatBestIndex", repeatBestIndex);
        sb.append_comma();
        appendFloatArray(sb, "repeatSmooth", repeatSmooth);
        sb.end_object();
    }

    void AudioChorusResult_c::toJson(simdjson::builder::string_builder& sb) const {
        sb.start_object();
        sb.escape_and_append_with_quotes("sections");
        sb.append_colon();
        sb.start_array();
        bool isFirst = true;
        for (const auto& item : sections) {
            if (false == isFirst) {
                sb.append_comma();
            }
            isFirst = false;
            sb.start_object();
            sb.append_key_value<"start">(item.startMs);
            sb.append_comma();
            sb.append_key_value<"end">(item.endMs);
            sb.append_comma();
            sb.append_key_value<"confidence">(item.confidence);
            sb.append_comma();
            sb.append_key_value<"energy">(item.energy);
            sb.append_comma();
            sb.append_key_value<"repeat">(item.repeatCount);
            sb.append_comma();
            sb.append_key_value<"similarity">(item.similarity);
            sb.end_object();
        }
        sb.end_array();
        sb.append_comma();
        sb.append_key_value<"totalDuration">(totalDurationMs);
        sb.append_comma();
        sb.append_key_value<"analyseTimeMs">(analyseTimeMs);
        sb.end_object();
    }

    // ===================================================================
    // 压带表
    // ===================================================================

    int AudioChorusAnalyzer_c::binOfFrequencyRaw(double hz) {
        if (hz <= 0) {
            return 0;
        }
        if (hz < DEF_LINEAR_TOP_HZ) {
            return static_cast<int>(std::floor(hz / DEF_LINEAR_HZ));
        }
        if (hz < DEF_MID_TOP_HZ) {
            return DEF_LINEAR_BANDS
                   + static_cast<int>(
                       std::floor(std::log(hz / DEF_LINEAR_TOP_HZ) / std::log(DEF_MID_BAND_RATIO))
                   );
        }
        return DEF_LINEAR_BANDS + DEF_MID_BANDS
               + static_cast<int>(
                   std::floor(std::log(hz / DEF_MID_TOP_HZ) / std::log(DEF_HIGH_BAND_RATIO))
               );
    }

    std::vector<int> AudioChorusAnalyzer_c::bandRanges(int bandCount) {
        const int count
            = bandCount < 1 ? 1 : (bandCount > DEF_SPECTRUM_SIZE ? DEF_SPECTRUM_SIZE : bandCount);
        std::vector<int> ranges{};
        if (count <= 1) {
            ranges.push_back(0);
            ranges.push_back(DEF_SPECTRUM_SIZE - 1);
            return ranges;
        }
        ranges.reserve(static_cast<size_t>(count) * 2);
        const double ratio = std::pow(DEF_MAX_FREQUENCY_HZ / DEF_LINEAR_HZ, 1.0 / count);
        int          from  = 0;
        for (int i = 1; i < count; ++i) {
            int cut = binOfFrequencyRaw(DEF_LINEAR_HZ * std::pow(ratio, static_cast<double>(i)));
            // 一份至少占一个频点，并且给后面每一份都留一个频点
            const int first = from + 1;
            const int last  = DEF_SPECTRUM_SIZE - 1 - (count - 1 - i);
            if (cut < first) {
                cut = first;
            }
            if (cut > last) {
                cut = last;
            }
            ranges.push_back(from);
            ranges.push_back(cut - 1);
            from = cut;
        }
        ranges.push_back(from);
        ranges.push_back(DEF_SPECTRUM_SIZE - 1);
        return ranges;
    }

    // ===================================================================
    // 工具函数
    // ===================================================================

    int AudioChorusAnalyzer_c::coarseCountOf(int frames, int hop, int win) {
        if (frames < win) {
            return 0;
        }
        return (frames - win) / hop + 1;
    }

    AudioChorusAnalyzer_c::FrameFeatures_c AudioChorusAnalyzer_c::frameFeatures(
        const uint8_t* spectrums,
        const uint8_t* waveform,
        size_t         waveformSize,
        int            frames,
        int            bandCount
    ) {
        const std::vector<int> ranges = bandRanges(bandCount);
        const bool  hasWave   = waveform != nullptr && waveformSize >= static_cast<size_t>(frames);
        const auto& binTable  = binDbTable();
        const auto& waveTable = waveDbTable();

        FrameFeatures_c result{};
        result.bandDb.assign(static_cast<size_t>(frames) * bandCount, 0.0f);
        result.levelDb.assign(static_cast<size_t>(frames), 0.0f);

        for (int f = 0; f < frames; ++f) {
            const size_t base = static_cast<size_t>(f) * DEF_SPECTRUM_SIZE;
            for (int k = 0; k < bandCount; ++k) {
                const size_t first = base + static_cast<size_t>(ranges[static_cast<size_t>(k) * 2]);
                const size_t last
                    = base + static_cast<size_t>(ranges[static_cast<size_t>(k) * 2 + 1]);
                double sum = 0;
                for (size_t b = first; b <= last; ++b) {
                    sum += static_cast<double>(binTable[spectrums[b]]);
                }
                result.bandDb[static_cast<size_t>(f) * bandCount + static_cast<size_t>(k)]
                    = static_cast<float>(sum / static_cast<double>(last - first + 1));
            }
            if (hasWave) {
                result.levelDb[static_cast<size_t>(f)] = waveTable[waveform[f]];
            } else {
                // 没有响度数据时用频谱能量粗估
                double energy = 0;
                for (size_t b = base; b < base + DEF_SPECTRUM_SIZE; ++b) {
                    energy += std::pow(10.0, static_cast<double>(binTable[spectrums[b]]) / 10.0);
                }
                // Dart 的 ln10 常量
                constexpr double LN10 = 2.302585092994046;
                result.levelDb[static_cast<size_t>(f)]
                    = static_cast<float>(10.0 * std::log(std::max(energy, 1e-9)) / LN10);
            }
        }
        return result;
    }

    std::vector<float> AudioChorusAnalyzer_c::coarseFeatures(
        const std::vector<float>& bandDb,
        const std::vector<float>& levelDb,
        int                       bandCount,
        int                       frames,
        int                       hop,
        int                       win,
        int                       coarseCount
    ) {
        const int          dim = bandCount + 1;
        std::vector<float> result(
            static_cast<size_t>(coarseCount) * static_cast<size_t>(dim),
            0.0f
        );
        for (int i = 0; i < coarseCount; ++i) {
            const int start = i * hop;
            const int end   = std::min(frames, start + win);
            const int span  = end - start;
            if (span <= 0) {
                continue;
            }
            const size_t base = static_cast<size_t>(i) * static_cast<size_t>(dim);
            for (int k = 0; k < bandCount; ++k) {
                double sum = 0;
                for (int f = start; f < end; ++f) {
                    sum += static_cast<double>(
                        bandDb[static_cast<size_t>(f) * bandCount + static_cast<size_t>(k)]
                    );
                }
                result[base + static_cast<size_t>(k)] = static_cast<float>(sum / span);
            }
            double sum = 0;
            for (int f = start; f < end; ++f) {
                sum += static_cast<double>(levelDb[static_cast<size_t>(f)]);
            }
            result[base + static_cast<size_t>(bandCount)] = static_cast<float>(sum / span);
        }
        return result;
    }

    void AudioChorusAnalyzer_c::zScore(std::vector<float>& data, int rows, int dim) {
        if (rows <= 1) {
            return;
        }
        for (int k = 0; k < dim; ++k) {
            double meanValue = 0;
            for (int i = 0; i < rows; ++i) {
                meanValue += static_cast<double>(
                    data[static_cast<size_t>(i) * dim + static_cast<size_t>(k)]
                );
            }
            meanValue       /= rows;
            double variance  = 0;
            for (int i = 0; i < rows; ++i) {
                const double diff = static_cast<double>(
                                        data[static_cast<size_t>(i) * dim + static_cast<size_t>(k)]
                                    )
                                    - meanValue;
                variance += diff * diff;
            }
            variance              /= rows;
            const double stdValue  = std::sqrt(variance);
            // 整段都相同的维度（例如静音的频带）直接置 0，不参与相似度
            const double scale = stdValue > 1e-6 ? 1.0 / stdValue : 0.0;
            for (int i = 0; i < rows; ++i) {
                const size_t index = static_cast<size_t>(i) * dim + static_cast<size_t>(k);
                data[index]
                    = static_cast<float>((static_cast<double>(data[index]) - meanValue) * scale);
            }
        }
    }

    void AudioChorusAnalyzer_c::normalizeRows(
        const std::vector<float>& data,
        int                       rows,
        int                       dim,
        std::vector<float>&       result
    ) {
        result.assign(data.size(), 0.0f);
        for (int i = 0; i < rows; ++i) {
            const size_t base   = static_cast<size_t>(i) * dim;
            double       square = 0;
            for (int k = 0; k < dim; ++k) {
                const double value  = data[base + static_cast<size_t>(k)];
                square             += value * value;
            }
            const double length = std::sqrt(square);
            const double scale  = length > 1e-9 ? 1.0 / length : 0.0;
            for (int k = 0; k < dim; ++k) {
                result[base + static_cast<size_t>(k)] = static_cast<float>(
                    static_cast<double>(data[base + static_cast<size_t>(k)]) * scale
                );
            }
        }
    }

    std::pair<double, double> AudioChorusAnalyzer_c::percentileRange(
        const std::vector<float>& data,
        double                    loPercent,
        double                    hiPercent
    ) {
        if (data.empty()) {
            return std::make_pair(0.0, 0.0);
        }
        std::vector<float> sorted = data;
        std::sort(sorted.begin(), sorted.end());
        const auto indexAt = [&sorted](double percent) {
            const long long index
                = static_cast<long long>(std::floor(static_cast<double>(sorted.size()) * percent));
            return static_cast<size_t>(
                std::clamp(index, 0LL, static_cast<long long>(sorted.size()) - 1)
            );
        };
        return std::make_pair(
            static_cast<double>(sorted[indexAt(loPercent)]),
            static_cast<double>(sorted[indexAt(hiPercent)])
        );
    }

    std::vector<float> AudioChorusAnalyzer_c::energyCurve(const std::vector<float>& levelDb) {
        const auto [levelLo, levelHi] = percentileRange(levelDb, 0.05, 0.9);
        const double       range      = levelHi - levelLo;
        std::vector<float> result(levelDb.size(), 0.0f);
        for (size_t i = 0; i < levelDb.size(); ++i) {
            result[i] = (range <= 1e-3) ? 0.5f
                                        : static_cast<float>(clampDouble(
                                              (static_cast<double>(levelDb[i]) - levelLo) / range,
                                              0.0,
                                              1.0
                                          ));
        }
        return result;
    }

    std::vector<float> AudioChorusAnalyzer_c::smooth(const std::vector<float>& data, int window) {
        if (window <= 1 || data.empty()) {
            return data;
        }
        const int           rows = static_cast<int>(data.size());
        const int           half = window / 2;
        std::vector<float>  result(data.size(), 0.0f);
        std::vector<double> prefix(data.size() + 1, 0.0);
        for (int i = 0; i < rows; ++i) {
            prefix[static_cast<size_t>(i) + 1]
                = prefix[static_cast<size_t>(i)] + data[static_cast<size_t>(i)];
        }
        for (int i = 0; i < rows; ++i) {
            const int start = i - half;
            const int end   = i + half + 1;
            if (start >= 0 && end <= rows) {
                const double sum
                    = prefix[static_cast<size_t>(end)] - prefix[static_cast<size_t>(start)];
                result[static_cast<size_t>(i)] = static_cast<float>(sum / (end - start));
                continue;
            }
            // 端点延展：越界部分按最近端点值计入
            double sum   = 0;
            int    count = 0;
            for (int j = start; j < end; ++j) {
                const int index  = std::clamp(j, 0, rows - 1);
                sum             += data[static_cast<size_t>(index)];
                ++count;
            }
            result[static_cast<size_t>(i)] = static_cast<float>(sum / count);
        }
        return result;
    }

    void AudioChorusAnalyzer_c::repeatBest(
        const std::vector<float>& featureNorm,
        int                       rows,
        int                       dim,
        int                       minLag,
        std::vector<float>&       outBest,
        std::vector<int32_t>&     outBestIndex
    ) {
        outBest.assign(static_cast<size_t>(rows), -1.0f);
        outBestIndex.assign(static_cast<size_t>(rows), -1);
        if (rows <= minLag) {
            return;
        }
        // 长音频降低采样密度（重复匹配本身对 1 秒左右的偏差不敏感）
        const int stride = rows > 400 ? 2 : 1;
        for (int i = minLag; i < rows; ++i) {
            double maxSimilarity = -1;
            int    maxIndex      = -1;
            for (int j = i - minLag; j >= 0; j -= stride) {
                const double similarity = dot(featureNorm, i, j, dim);
                if (similarity > maxSimilarity) {
                    maxSimilarity = similarity;
                    maxIndex      = j;
                }
            }
            outBest[static_cast<size_t>(i)]      = static_cast<float>(maxSimilarity);
            outBestIndex[static_cast<size_t>(i)] = maxIndex;
        }
    }

    AudioChorusAnalyzer_c::EnergyPlateau_c AudioChorusAnalyzer_c::energyPlateau(
        int                       seed,
        const std::vector<float>& energy,
        double                    drop,
        int                       hole,
        int                       maxLength
    ) {
        EnergyPlateau_c result{};
        const int       rows = static_cast<int>(energy.size());
        if (rows <= 0) {
            return result;
        }
        const int    index = std::clamp(seed, 0, rows - 1);
        const double limit
            = std::max(0.0, static_cast<double>(energy[static_cast<size_t>(index)]) - drop);
        int end   = index + 1;
        int holes = 0;
        while (end < rows) {
            if (static_cast<double>(energy[static_cast<size_t>(end)]) >= limit) {
                holes = 0;
                ++end;
            } else if (++holes > hole) {
                break;
            } else {
                ++end;
            }
        }
        int start = index;
        holes     = 0;
        while (start > 0) {
            if (static_cast<double>(energy[static_cast<size_t>(start - 1)]) >= limit) {
                holes = 0;
                --start;
            } else if (++holes > hole) {
                break;
            } else {
                --start;
            }
        }
        if (end - start > maxLength) {
            const int half = maxLength / 2;
            start          = std::max(start, index - half);
            end            = std::min(rows, start + maxLength);
        }
        result.start  = start;
        result.length = end - start;
        return result;
    }

    int AudioChorusAnalyzer_c::argMaxRange(const std::vector<float>& data, int start, int end) {
        const int rows  = std::min(end, static_cast<int>(data.size()));
        const int begin = std::max(0, std::min(start, rows - 1));
        int       best  = begin;
        for (int i = begin + 1; i < rows; ++i) {
            if (data[static_cast<size_t>(i)] > data[static_cast<size_t>(best)]) {
                best = i;
            }
        }
        return best;
    }

    int AudioChorusAnalyzer_c::maxWindowStart(const std::vector<float>& energy, int length) {
        const int rows  = static_cast<int>(energy.size());
        const int begin = 0;
        const int span  = std::min(length, rows - begin);
        if (span <= 0) {
            return begin;
        }
        double bestSum   = -std::numeric_limits<double>::infinity();
        int    bestStart = begin;
        double sum       = 0;
        for (int i = begin; i < rows; ++i) {
            sum += energy[static_cast<size_t>(i)];
            if (i >= begin + span) {
                sum -= energy[static_cast<size_t>(i - span)];
            }
            if (i >= begin + span - 1) {
                if (sum > bestSum) {
                    bestSum   = sum;
                    bestStart = i - span + 1;
                }
            }
        }
        return bestStart;
    }

    void AudioChorusAnalyzer_c::windowSimilarity(
        const std::vector<float>& featureNorm,
        int                       dim,
        int                       regionStart,
        int                       regionLength,
        double                    coreRatio,
        int                       alignRadius,
        std::vector<float>&       outSimilarity,
        std::vector<int32_t>&     outOffset
    ) {
        const int rows = dim > 0 ? static_cast<int>(featureNorm.size()) / dim : 0;
        outSimilarity.assign(static_cast<size_t>(std::max(0, rows)), 0.0f);
        outOffset.assign(static_cast<size_t>(std::max(0, rows)), 0);
        const int maxStart = rows - regionLength;
        if (maxStart < 0) {
            return;
        }
        const int margin     = static_cast<int>(std::floor(regionLength * (1 - coreRatio) / 2));
        const int coreStart  = regionStart + margin;
        const int coreLength = std::max(1, regionLength - margin * 2);
        for (int align = -alignRadius; align <= alignRadius; ++align) {
            for (int t = 0; t <= maxStart; ++t) {
                const int start = t + align + margin;
                if (start < 0 || start + coreLength > rows) {
                    continue;
                }
                // 逐帧对齐：window[start + k] 对模板[coreStart + k]
                double sum = 0;
                for (int k = 0; k < coreLength; ++k) {
                    sum += dot(featureNorm, start + k, coreStart + k, dim);
                }
                const double value = sum / coreLength;
                if (value > static_cast<double>(outSimilarity[static_cast<size_t>(t)])) {
                    outSimilarity[static_cast<size_t>(t)] = static_cast<float>(value);
                    outOffset[static_cast<size_t>(t)]     = align;
                }
            }
        }
    }

    double AudioChorusAnalyzer_c::dot(const std::vector<float>& data, int rowA, int rowB, int dim) {
        const size_t baseA = static_cast<size_t>(rowA) * dim;
        const size_t baseB = static_cast<size_t>(rowB) * dim;
        double       sum   = 0;
        for (int k = 0; k < dim; ++k) {
            sum += static_cast<double>(data[baseA + static_cast<size_t>(k)])
                   * static_cast<double>(data[baseB + static_cast<size_t>(k)]);
        }
        return sum;
    }

    double AudioChorusAnalyzer_c::mean(const std::vector<float>& data, int start, int end) {
        const int begin = start;
        const int stop  = std::min(end, static_cast<int>(data.size()));
        if (stop <= begin) {
            return 0;
        }
        double sum = 0;
        for (int i = begin; i < stop; ++i) {
            sum += static_cast<double>(data[static_cast<size_t>(i)]);
        }
        return sum / (stop - begin);
    }

    // ===================================================================
    // 结构分析
    // ===================================================================

    std::optional<AudioChorusStructure_c> AudioChorusAnalyzer_c::analyseStructure(
        const uint8_t*             spectrums,
        size_t                     spectrumsSize,
        const uint8_t*             waveform,
        size_t                     waveformSize,
        int64_t                    durationMs,
        const AudioChorusConfig_c& config
    ) {
        (void)durationMs; // 总时长不参与结构分析
        if (nullptr == spectrums) {
            return std::nullopt;
        }
        const int totalFrames = static_cast<int>(spectrumsSize / DEF_SPECTRUM_SIZE);
        const int bandCount   = config.bandCount;
        if (totalFrames < 30 || bandCount <= 0 || bandCount > DEF_SPECTRUM_SIZE) {
            // 音频过短（< 3 秒）或参数非法
            return std::nullopt;
        }

        // 长歌曲放大步长，避免重复匹配开销随长度平方增长
        int hop = config.hopFrames > 0 ? config.hopFrames : 10;
        int win = config.windowFrames > hop ? config.windowFrames : hop;
        while (coarseCountOf(totalFrames, hop, win) > MAX_COARSE_FRAMES) {
            hop *= 2;
            win  = (win > hop) ? win : hop * 2;
        }
        const int coarse = coarseCountOf(totalFrames, hop, win);
        if (coarse < 8) {
            return std::nullopt;
        }

        // ── 1. 帧级特征：频带 dB 均值 + 帧响度 dB ──
        const FrameFeatures_c features
            = frameFeatures(spectrums, waveform, waveformSize, totalFrames, bandCount);
        // 整首歌都接近静音时没有副歌可言
        const auto levelRange = percentileRange(features.levelDb, 0.0, 0.9);
        if (levelRange.second < MIN_LEVEL_DB) {
            return std::nullopt;
        }

        // ── 2. 特征降采样 + z-score + 行归一化 ──
        const int          dim     = bandCount + 1;
        std::vector<float> feature = coarseFeatures(
            features.bandDb,
            features.levelDb,
            bandCount,
            totalFrames,
            hop,
            win,
            coarse
        );
        AudioChorusStructure_c structure{};
        // 保留一份未归一化的特征，便于调试/可视化/离线分析
        structure.rawFeature = feature;

        zScore(feature, coarse, dim);
        for (int i = 0; i < coarse; ++i) {
            const size_t index = static_cast<size_t>(i) * dim + static_cast<size_t>(bandCount);
            feature[index] = static_cast<float>(static_cast<double>(feature[index]) * LOUD_WEIGHT);
        }
        std::vector<float> featureNorm{};
        normalizeRows(feature, coarse, dim, featureNorm);

        // ── 3. 能量曲线（0~1）：用粗粒度响度均值（与特征同一时间轴） ──
        std::vector<float> coarseLevel(static_cast<size_t>(coarse), 0.0f);
        for (int i = 0; i < coarse; ++i) {
            coarseLevel[static_cast<size_t>(i)]
                = structure
                      .rawFeature[static_cast<size_t>(i) * dim + static_cast<size_t>(bandCount)];
        }
        const std::vector<float> energy       = energyCurve(coarseLevel);
        const std::vector<float> energySmooth = smooth(
            energy,
            std::max(1, static_cast<int>(std::llround(ENERGY_SMOOTH_SECONDS * DEF_FRAME_HZ / hop)))
        );

        // ── 4. 重复匹配：每帧找最相似的"另一帧" ──
        const int minLagCoarse = std::max(
            1,
            static_cast<int>(std::llround(config.minRepeatLagSeconds * DEF_FRAME_HZ / hop))
        );
        std::vector<float>   repeatBestValue{};
        std::vector<int32_t> repeatBestIndex{};
        repeatBest(featureNorm, coarse, dim, minLagCoarse, repeatBestValue, repeatBestIndex);
        const std::vector<float> repeatSmooth = smooth(
            repeatBestValue,
            std::max(1, static_cast<int>(std::llround(REPEAT_SMOOTH_SECONDS * DEF_FRAME_HZ / hop)))
        );

        // ── 5. 候选种子：平滑重复度 × 能量最高的若干帧 ──
        std::vector<std::pair<int, double>> scored{};
        scored.reserve(static_cast<size_t>(coarse));
        for (int i = 0; i < coarse; ++i) {
            const double repeat
                = std::max(static_cast<double>(repeatSmooth[static_cast<size_t>(i)]), 0.0);
            if (repeat <= 0) {
                continue;
            }
            scored.emplace_back(
                i,
                repeat
                    * (1 - SEED_ENERGY_WEIGHT
                       + SEED_ENERGY_WEIGHT
                             * static_cast<double>(energySmooth[static_cast<size_t>(i)]))
            );
        }
        std::stable_sort(scored.begin(), scored.end(), [](const auto& left, const auto& right) {
            return left.second > right.second;
        });
        std::vector<int> seeds{};
        for (const auto& item : scored) {
            bool farEnough = true;
            for (const int picked : seeds) {
                if (std::abs(picked - item.first) < SEED_SPACING_FRAMES) {
                    farEnough = false;
                    break;
                }
            }
            if (farEnough) {
                seeds.push_back(item.first);
                if (static_cast<int>(seeds.size()) >= SEED_CANDIDATE_COUNT) {
                    break;
                }
            }
        }

        const int maxSegmentCoarse = std::max(
            1,
            static_cast<int>(std::llround(config.maxSegmentSeconds * DEF_FRAME_HZ / hop))
        );
        const int minSegmentCoarse = std::max(
            1,
            static_cast<int>(std::llround(config.minSegmentSeconds * DEF_FRAME_HZ / hop))
        );

        // 填好结构结果里与"区段"无关的部分
        structure.totalFrames     = totalFrames;
        structure.hopFrames       = hop;
        structure.windowFrames    = win;
        structure.coarseCount     = coarse;
        structure.bandCount       = bandCount;
        structure.levelDb         = features.levelDb;
        structure.energy          = energy;
        structure.energySmooth    = energySmooth;
        structure.repeatBest      = repeatBestValue;
        structure.repeatBestIndex = repeatBestIndex;
        structure.repeatSmooth    = repeatSmooth;

        // 没有找到重复段落时的退化结果：最响亮的一段
        auto buildFallback = [&structure, &energySmooth, coarse, hop]() {
            const int length = std::min(
                coarse,
                std::max(1, static_cast<int>(std::llround(FALLBACK_SECONDS * DEF_FRAME_HZ / hop)))
            );
            const int               start = maxWindowStart(energySmooth, length);
            AudioChorusOccurrence_c item{};
            item.start                 = start;
            item.length                = length;
            item.similarity            = 0;
            item.energy                = mean(structure.energy, start, start + length);
            structure.seedIndex        = -1;
            structure.regionStart      = start;
            structure.regionLength     = length;
            structure.regionSimilarity = 0;
            structure.occurrences      = {item};
            structure.isFallback       = true;
            return structure;
        };

        // ── 6. 逐个候选种子按能量平台生长，取"长度 × 重复度 × 能量"最大者 ──
        double bestScore      = -1.0;
        int    bestSeed       = -1;
        int    bestStart      = 0;
        int    bestLength     = 0;
        double bestSimilarity = 0.0;
        double bestEnergy     = 0.0;
        for (const int seed : seeds) {
            // 种子对齐到邻域内能量最高的帧，避免落在副歌的上升沿
            const int peak
                = argMaxRange(energySmooth, std::max(0, seed - 2), std::min(coarse, seed + 3));
            const EnergyPlateau_c plateau = energyPlateau(
                peak,
                energySmooth,
                ENERGY_DROP_TOLERANCE,
                GROW_HOLE_TOLERANCE,
                maxSegmentCoarse
            );
            if (plateau.length < minSegmentCoarse) {
                continue;
            }
            const double meanRepeat
                = mean(repeatBestValue, plateau.start, plateau.start + plateau.length);
            if (meanRepeat < REGION_MIN_REPEAT) {
                continue;
            }
            const double meanEnergy = mean(energy, plateau.start, plateau.start + plateau.length);
            if (meanEnergy < REGION_MIN_ENERGY) {
                continue;
            }
            const double score
                = std::min(plateau.length, SCORE_LENGTH_CAP_SEC) * meanRepeat * meanEnergy;
            if (score > bestScore) {
                bestScore      = score;
                bestSeed       = seed;
                bestStart      = plateau.start;
                bestLength     = plateau.length;
                bestSimilarity = meanRepeat;
                bestEnergy     = meanEnergy;
            }
        }
        if (bestSeed < 0) {
            return buildFallback();
        }
        const int regionStart  = bestStart;
        const int regionLength = bestLength;

        // ── 7. 模板扫描：找出副歌的其他出现位置 ──
        std::vector<float>   scanSimilarity{};
        std::vector<int32_t> scanOffset{};
        windowSimilarity(
            featureNorm,
            dim,
            regionStart,
            regionLength,
            CORE_RATIO,
            ALIGN_SEARCH_RADIUS,
            scanSimilarity,
            scanOffset
        );

        std::vector<AudioChorusOccurrence_c> occurrences{};
        AudioChorusOccurrence_c              mainOccurrence{};
        mainOccurrence.start      = regionStart;
        mainOccurrence.length     = regionLength;
        mainOccurrence.similarity = bestSimilarity;
        mainOccurrence.energy     = bestEnergy;
        occurrences.push_back(mainOccurrence);

        const int    maxStart    = coarse - regionLength;
        const double minEnergy   = std::max(REGION_MIN_ENERGY, bestEnergy - ENERGY_DROP_TOLERANCE);
        int          groupStart  = -1;
        int          bestInGroup = -1;

        auto flushGroup = [&]() {
            if (groupStart < 0 || bestInGroup < 0) {
                groupStart  = -1;
                bestInGroup = -1;
                return;
            }
            groupStart = -1;
            int start  = bestInGroup + scanOffset[static_cast<size_t>(bestInGroup)];
            int length = regionLength;
            if (start >= 0) {
                // 用该出现位置自身的能量峰值重新确定起止
                // * 必须以窗口内能量最高的帧为基准：取窗口起点会落在副歌的上升沿上，
                //   基准能量偏低会让生长越过主歌/间奏，区段被拉长
                const int peak = argMaxRange(energySmooth, start, std::min(coarse, start + length));
                const EnergyPlateau_c plateau = energyPlateau(
                    peak,
                    energySmooth,
                    ENERGY_DROP_TOLERANCE,
                    GROW_HOLE_TOLERANCE,
                    maxSegmentCoarse
                );
                if (plateau.length >= minSegmentCoarse
                    && mean(energy, plateau.start, plateau.start + plateau.length) >= minEnergy) {
                    start  = plateau.start;
                    length = plateau.length;
                }
                bool overlapping = false;
                for (const auto& item : occurrences) {
                    if (start < item.start + item.length && item.start < start + length) {
                        overlapping = true;
                        break;
                    }
                }
                if (false == overlapping) {
                    AudioChorusOccurrence_c item{};
                    item.start      = start;
                    item.length     = length;
                    item.similarity = scanSimilarity[static_cast<size_t>(bestInGroup)];
                    item.energy     = mean(energy, start, start + length);
                    occurrences.push_back(item);
                }
            }
            bestInGroup = -1;
        };

        for (int t = 0; t <= maxStart; ++t) {
            const bool matched
                = scanSimilarity[static_cast<size_t>(t)] >= config.segmentSimilarity
                  // 与主区段重叠的帧不是"另一次出现"
                  && false == (t + regionLength > regionStart && t < regionStart + regionLength);
            if (false == matched) {
                flushGroup();
                continue;
            }
            if (groupStart < 0) {
                groupStart  = t;
                bestInGroup = t;
            } else if (scanSimilarity[static_cast<size_t>(t)]
                       > scanSimilarity[static_cast<size_t>(bestInGroup)]) {
                bestInGroup = t;
            }
        }
        flushGroup();

        std::stable_sort(
            occurrences.begin(),
            occurrences.end(),
            [](const auto& left, const auto& right) {
                return left.start < right.start;
            }
        );

        structure.seedIndex        = bestSeed;
        structure.regionStart      = regionStart;
        structure.regionLength     = regionLength;
        structure.regionSimilarity = bestSimilarity;
        structure.occurrences      = std::move(occurrences);
        structure.isFallback       = false;
        return structure;
    }

    // ===================================================================
    // 区段分析
    // ===================================================================

    AudioChorusResult_c AudioChorusAnalyzer_c::analyse(
        const uint8_t*             spectrums,
        size_t                     spectrumsSize,
        const uint8_t*             waveform,
        size_t                     waveformSize,
        int64_t                    durationMs,
        const AudioChorusConfig_c& config
    ) {
        const auto beginTime = std::chrono::steady_clock::now();

        const int totalFrames
            = (nullptr == spectrums) ? 0 : static_cast<int>(spectrumsSize / DEF_SPECTRUM_SIZE);
        AudioChorusResult_c result{};
        result.totalDurationMs
            = durationMs > 0 ? durationMs : static_cast<int64_t>(totalFrames) * 1000 / DEF_FRAME_HZ;

        const auto structure = analyseStructure(
            spectrums,
            spectrumsSize,
            waveform,
            waveformSize,
            result.totalDurationMs,
            config
        );
        if (false == structure.has_value() || structure->occurrences.empty()) {
            result.analyseTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                       std::chrono::steady_clock::now() - beginTime
            )
                                       .count();
            return result;
        }

        const int    hop             = structure->hopFrames;
        const int    occurrenceCount = static_cast<int>(structure->occurrences.size());
        const double countScore      = std::min(occurrenceCount, 3) / 3.0;
        const int    mergeGapCoarse  = std::max(
            1,
            static_cast<int>(std::llround(config.mergeGapSeconds * DEF_FRAME_HZ / hop))
        );
        const int64_t minSegmentMs
            = static_cast<int64_t>(std::llround(config.minSegmentSeconds * 1000));
        const int64_t maxSegmentMs
            = static_cast<int64_t>(std::llround(config.maxSegmentSeconds * 1000));

        // 先按出现位置的时长过滤（合并相邻出现后可能超过单段上限，这里不丢）
        std::vector<AudioChorusOccurrence_c> filtered{};
        for (const auto& item : structure->occurrences) {
            const int64_t lengthMs = static_cast<int64_t>(item.length) * hop * 1000 / DEF_FRAME_HZ;
            if (lengthMs < minSegmentMs) {
                continue;
            }
            if (maxSegmentMs > 0 && lengthMs > maxSegmentMs) {
                continue;
            }
            filtered.push_back(item);
        }
        if (filtered.empty()) {
            filtered = structure->occurrences;
        }

        // 合并相邻/重叠的出现位置
        std::vector<AudioChorusOccurrence_c> merged{};
        for (const auto& item : filtered) {
            if (false == merged.empty()
                && item.start - (merged.back().start + merged.back().length) <= mergeGapCoarse) {
                AudioChorusOccurrence_c last = merged.back();
                merged.pop_back();
                const int end   = std::max(last.start + last.length, item.start + item.length);
                last.length     = end - last.start;
                last.similarity = std::max(last.similarity, item.similarity);
                last.energy     = std::max(last.energy, item.energy);
                merged.push_back(last);
            } else {
                merged.push_back(item);
            }
        }

        std::vector<AudioChorusSection_c> sections{};
        for (const auto& item : merged) {
            const int64_t startFrame = static_cast<int64_t>(item.start) * hop;
            int64_t       endFrame   = startFrame + static_cast<int64_t>(item.length) * hop;
            if (endFrame > totalFrames) {
                endFrame = totalFrames;
            }
            if (endFrame <= startFrame) {
                continue;
            }
            AudioChorusSection_c section{};
            section.startMs = startFrame * 1000 / DEF_FRAME_HZ;
            section.endMs   = endFrame * 1000 / DEF_FRAME_HZ;
            // 相似度越高、能量越强、出现次数越多 → 越确定是副歌
            section.confidence = clampDouble(
                0.15 + 0.35 * item.similarity + 0.25 * item.energy + 0.15 * countScore,
                0.05,
                structure->isFallback ? 0.40 : 0.95
            );
            section.energy      = item.energy;
            section.repeatCount = structure->isFallback ? 1 : occurrenceCount;
            section.similarity  = structure->isFallback ? 0 : item.similarity;
            sections.push_back(section);
        }

        if (false == sections.empty()) {
            std::stable_sort(
                sections.begin(),
                sections.end(),
                [](const auto& left, const auto& right) {
                    return left.startMs < right.startMs;
                }
            );
            if (config.maxSegments > 0 && static_cast<int>(sections.size()) > config.maxSegments) {
                sections.resize(static_cast<size_t>(config.maxSegments));
            }
        }

        result.sections      = std::move(sections);
        result.analyseTimeMs = std::chrono::duration_cast<std::chrono::milliseconds>(
                                   std::chrono::steady_clock::now() - beginTime
        )
                                   .count();
        return result;
    }

} // namespace mediaxx
