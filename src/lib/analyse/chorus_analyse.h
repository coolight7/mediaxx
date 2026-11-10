#pragma once

#include "simdjson.h"
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <vector>

namespace mediaxx {

    /// 副歌（高潮）分析参数（对应 Dart 的 `AudioChorusConfig_c`）
    struct AudioChorusConfig_c {
        /// 频带数量（把 256 个频点压成这么多份，1 ~ 256）
        int bandCount = 32;
        /// 特征帧步长（帧，10 帧 = 1 秒）
        int hopFrames = 10;
        /// 特征窗长（帧）
        int windowFrames = 20;
        /// 重复匹配的最小时距（秒）
        double minRepeatLagSeconds = 20;
        /// 副歌出现位置匹配的余弦相似度阈值（0~1）
        double segmentSimilarity = 0.60;
        /// 副歌区段最短时长（秒）
        double minSegmentSeconds = 5;
        /// 副歌区段最长时长（秒）
        double maxSegmentSeconds = 40;
        /// 相邻副歌区段合并的最大间隔（秒）
        double mergeGapSeconds = 2;
        /// 输出的副歌区段数量上限（<= 0 不限制）
        int maxSegments = 6;

        /// 从 JSON 解析（键名与 Dart 相同，缺省用默认值）
        /// * 类型不对的键按"没给"处理，与 Dart 的 `_chorusConfigOf` 容错一致
        /// * 返回 false 表示整体不是 JSON 对象
        [[nodiscard]] static bool fromJson(std::string_view json, AudioChorusConfig_c& out);
    };

    /// 副歌的一次出现（结构分析的中间结果）
    struct AudioChorusOccurrence_c {
        /// 起点（粗粒度帧）
        int start = 0;
        /// 时长（粗粒度帧）
        int length = 0;
        /// 与副歌模板的对齐平均相似度（0~1）
        double similarity = 0;
        /// 平均能量（0~1）
        double energy = 0;
    };

    /// 副歌区段
    struct AudioChorusSection_c {
        int64_t startMs = 0;
        int64_t endMs   = 0;
        /// 置信度 0~1
        double confidence = 0;
        /// 区段能量 0~1
        double energy = 0;
        /// 该副歌在歌曲中出现的次数（含自身）
        int repeatCount = 1;
        /// 与副歌模板的相似度 0~1（退化为"最响亮片段"时为 0）
        double similarity = 0;
    };

    /// 副歌分析的结构中间结果（对应 Dart 的 `AudioChorusStructure_c`）
    class AudioChorusStructure_c {
    public:

        int totalFrames  = 0;
        int hopFrames    = 0;
        int windowFrames = 0;
        int coarseCount  = 0;
        int bandCount    = 0;

        /// 每帧的响度（dB）
        std::vector<float> levelDb{};
        /// 粗粒度特征矩阵（coarseCount × featureDim，未做 z-score / 归一化）
        std::vector<float> rawFeature{};
        /// 每帧能量（0~1）
        std::vector<float> energy{};
        /// 平滑后的能量
        std::vector<float> energySmooth{};
        /// 每帧与最佳匹配帧的余弦相似度（< 0 表示没有可匹配的帧）
        std::vector<float> repeatBest{};
        /// 每帧的最佳匹配帧下标（-1 表示没有）
        std::vector<int32_t> repeatBestIndex{};
        /// 平滑后的重复度
        std::vector<float> repeatSmooth{};

        /// 种子帧下标（-1 表示没有找到重复段落）
        int    seedIndex        = -1;
        int    regionStart      = 0;
        int    regionLength     = 0;
        double regionSimilarity = 0;

        std::vector<AudioChorusOccurrence_c> occurrences{};

        /// 是否退化为"最响亮片段"
        bool isFallback = false;

        [[nodiscard]] int featureDim() const {
            return bandCount + 1;
        }

        /// 序列化成 JSON（数组用 base64 打包，便于跨语言读取）
        void toJson(simdjson::builder::string_builder& sb) const;
    };

    /// 副歌分析结果（对应 Dart 的 `AudioChorusAnalyseResult_c`）
    class AudioChorusResult_c {
    public:

        std::vector<AudioChorusSection_c> sections{};
        int64_t                           totalDurationMs = 0;
        int64_t                           analyseTimeMs   = 0;

        void toJson(simdjson::builder::string_builder& sb) const;
    };

    /// 副歌（高潮）识别算法（对应 Dart 的 `AudioChorusAnalyzer_c`）
    ///
    /// 输入是 `mediaxx_get_audio_visualization` 产出的两份数据：
    /// * `spectrums`：每帧 [DEF_SPECTRUM_SIZE] 个频点，值为 -80~0 dB 的线性映射（0~255），10
    /// 帧/秒；
    /// * `waveform`：每帧 1 字节 RMS 响度，值为 -60~+20 dB 的线性映射（0~255），可以为空
    ///   （为空时用频谱能量粗估响度）。
    ///
    /// 算法步骤见 `analyseStructure` 的注释；参数与常量都跟 Dart 实现一一对应，
    /// 改动这里之前先跑 `src/test/test_analyse.cpp` 的一致性用例。
    class AudioChorusAnalyzer_c {
    public:

        /// 每帧频点数
        static constexpr int DEF_SPECTRUM_SIZE = 256;
        /// 帧率（帧/秒）
        static constexpr int DEF_FRAME_HZ = 10;

        /// 压带表：把 256 个频点按频率压成 [bandCount] 份
        /// * 返回每两项一份（起止频点，闭区间），首尾相接覆盖 0 ~ 255
        /// * [bandCount] 超出 1 ~ 256 时钳到范围内
        [[nodiscard]] static std::vector<int> bandRanges(int bandCount);

        /// 频率换算成频点序号（不夹取版本，可能超出 0~255）
        [[nodiscard]] static int binOfFrequencyRaw(double hz);

        /// 结构分析：特征 → 重复匹配 → 种子 → 区段生长 → 全部出现位置
        /// * 返回空 = 音频过短（< 3 秒）、参数非法、整首接近静音
        [[nodiscard]] static std::optional<AudioChorusStructure_c> analyseStructure(
            const uint8_t*             spectrums,
            size_t                     spectrumsSize,
            const uint8_t*             waveform,
            size_t                     waveformSize,
            int64_t                    durationMs,
            const AudioChorusConfig_c& config
        );

        /// 分析副歌区段
        /// * [durationMs] <= 0 时按帧数推算总时长
        /// * 总是返回结果：没有副歌时 `sections` 为空
        [[nodiscard]] static AudioChorusResult_c analyse(
            const uint8_t*             spectrums,
            size_t                     spectrumsSize,
            const uint8_t*             waveform,
            size_t                     waveformSize,
            int64_t                    durationMs,
            const AudioChorusConfig_c& config
        );

    private:

        static constexpr int    MAX_COARSE_FRAMES     = 800;
        static constexpr double LOUD_WEIGHT           = 0.3;
        static constexpr double ENERGY_DROP_TOLERANCE = 0.2;
        static constexpr double REGION_MIN_ENERGY     = 0.2;
        static constexpr double REGION_MIN_REPEAT     = 0.5;
        static constexpr int    GROW_HOLE_TOLERANCE   = 2;
        static constexpr int    SEED_CANDIDATE_COUNT  = 8;
        static constexpr int    SEED_SPACING_FRAMES   = 5;
        static constexpr double ENERGY_SMOOTH_SECONDS = 5;
        static constexpr double REPEAT_SMOOTH_SECONDS = 10;
        static constexpr double SEED_ENERGY_WEIGHT    = 0.65;
        static constexpr int    SCORE_LENGTH_CAP_SEC  = 25;
        static constexpr int    ALIGN_SEARCH_RADIUS   = 2;
        static constexpr double CORE_RATIO            = 0.8;
        static constexpr int    FALLBACK_SECONDS      = 20;
        static constexpr double MIN_LEVEL_DB          = -45;

        struct FrameFeatures_c {
            std::vector<float> bandDb{};
            std::vector<float> levelDb{};
        };

        struct EnergyPlateau_c {
            int start  = 0;
            int length = 0;
        };

        [[nodiscard]] static int coarseCountOf(int frames, int hop, int win);

        [[nodiscard]] static FrameFeatures_c frameFeatures(
            const uint8_t* spectrums,
            const uint8_t* waveform,
            size_t         waveformSize,
            int            frames,
            int            bandCount
        );

        [[nodiscard]] static std::vector<float> coarseFeatures(
            const std::vector<float>& bandDb,
            const std::vector<float>& levelDb,
            int                       bandCount,
            int                       frames,
            int                       hop,
            int                       win,
            int                       coarseCount
        );

        static void zScore(std::vector<float>& data, int rows, int dim);

        static void normalizeRows(
            const std::vector<float>& data,
            int                       rows,
            int                       dim,
            std::vector<float>&       result
        );

        [[nodiscard]] static std::vector<float> energyCurve(const std::vector<float>& levelDb);

        [[nodiscard]] static std::vector<float> smooth(const std::vector<float>& data, int window);

        static void repeatBest(
            const std::vector<float>& featureNorm,
            int                       rows,
            int                       dim,
            int                       minLag,
            std::vector<float>&       outBest,
            std::vector<int32_t>&     outBestIndex
        );

        [[nodiscard]] static EnergyPlateau_c energyPlateau(
            int                       seed,
            const std::vector<float>& energy,
            double                    drop,
            int                       hole,
            int                       maxLength
        );

        [[nodiscard]] static int argMaxRange(const std::vector<float>& data, int start, int end);

        [[nodiscard]] static int maxWindowStart(const std::vector<float>& energy, int length);

        static void windowSimilarity(
            const std::vector<float>& featureNorm,
            int                       dim,
            int                       regionStart,
            int                       regionLength,
            double                    coreRatio,
            int                       alignRadius,
            std::vector<float>&       outSimilarity,
            std::vector<int32_t>&     outOffset
        );

        [[nodiscard]] static double
            dot(const std::vector<float>& data, int rowA, int rowB, int dim);

        [[nodiscard]] static double mean(const std::vector<float>& data, int start, int end);

        [[nodiscard]] static std::pair<double, double>
            percentileRange(const std::vector<float>& data, double loPercent, double hiPercent);
    };

} // namespace mediaxx
