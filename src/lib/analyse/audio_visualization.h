#pragma once

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
#include <libavutil/audio_fifo.h>
#include <libavutil/imgutils.h>
#include <libavutil/opt.h>
#include <libswresample/swresample.h>
}

#include "util/log.h"
#include <algorithm>
#include <array>
#include <assert.h>
#include <cmath>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace mediaxx {

    class AudioSpectrumAnalyzer {
    private:

        AVFormatContext* formatContext     = nullptr;
        AVCodecContext*  codecContext      = nullptr;
        SwrContext*      swrContext        = nullptr;
        int              audioStreamIndex  = -1;
        int16_t*         resampledData     = nullptr;
        int              resampledDataSize = 0;

        // 每一首曲子按采样率计算的表与工作缓冲（见 prepareTables）
        size_t               fftSize = 0;
        std::vector<size_t>  fftBitRevTable;
        std::vector<float>   fftTwiddleCos;
        std::vector<float>   fftTwiddleSin;
        std::vector<float>   hanningWindow;
        std::vector<float>   fftReal;
        std::vector<float>   fftImag;

    public:

        inline static constexpr size_t        DEF_SPECTRUM_SIZE  = 256;
        inline static constexpr unsigned char DEF_WAVE_MAX_POINT = 255;
        inline static constexpr size_t        DEF_FPS            = 10;
        inline static constexpr float         DEF_2PI = 2.0f * static_cast<float>(M_PI);
        // 波形振幅计算的分贝参数
        inline static constexpr float DEF_FREQUENCY_MIN_DB = -80.0f;
        inline static constexpr float DEF_FREQUENCY_MAX_DB = 0.0f;
        inline static constexpr float DEF_WAVE_MIN_DB      = -60.0f;
        inline static constexpr float DEF_WAVE_MAX_DB      = 20.0f;
        /// 输出频点的分段边界（Hz）：第 i 个频点覆盖
        /// [DEF_BAND_EDGES[i], DEF_BAND_EDGES[i + 1])，共 DEF_SPECTRUM_SIZE 段。
        ///
        /// 分段方式参考均衡器：低频段受分析窗能分辨的间隔限制，取固定 40 Hz
        /// （44.1 kHz 与 48 kHz 下都约 3~3.7 个 FFT 频点宽，段内频点数不会从 3 跳到 4，
        /// 电平因此不会有 1 dB 上下的锯齿），3.84 kHz 以上按每段固定 1.04% 递增到
        /// 20 kHz —— 也就是高频是"每个倍频程固定多少段"，而不是固定多少 Hz。
        ///
        /// 这张表必须与内核（mpv fork 的 player/audio.c 里 AUDIO_FFT_BAND_EDGES）保持一致，
        /// 否则整曲提取与实时频谱对不上。
        inline static constexpr std::array<int, DEF_SPECTRUM_SIZE + 1> DEF_BAND_EDGES = {
            0, 40, 80, 120, 160, 200, 240, 280,
            320, 360, 400, 440, 480, 520, 560, 600,
            640, 680, 720, 760, 800, 840, 880, 920,
            960, 1000, 1040, 1080, 1120, 1160, 1200, 1240,
            1280, 1320, 1360, 1400, 1440, 1480, 1520, 1560,
            1600, 1640, 1680, 1720, 1760, 1800, 1840, 1880,
            1920, 1960, 2000, 2040, 2080, 2120, 2160, 2200,
            2240, 2280, 2320, 2360, 2400, 2440, 2480, 2520,
            2560, 2600, 2640, 2680, 2720, 2760, 2800, 2840,
            2880, 2920, 2960, 3000, 3040, 3080, 3120, 3160,
            3200, 3240, 3280, 3320, 3360, 3400, 3440, 3480,
            3520, 3560, 3600, 3640, 3680, 3720, 3760, 3800,
            3840, 3880, 3920, 3960, 4000, 4040, 4090, 4130,
            4170, 4210, 4260, 4300, 4350, 4390, 4440, 4480,
            4530, 4580, 4620, 4670, 4720, 4770, 4820, 4870,
            4920, 4970, 5020, 5070, 5130, 5180, 5230, 5290,
            5340, 5400, 5450, 5510, 5570, 5620, 5680, 5740,
            5800, 5860, 5920, 5980, 6050, 6110, 6170, 6240,
            6300, 6370, 6430, 6500, 6570, 6630, 6700, 6770,
            6840, 6910, 6980, 7060, 7130, 7200, 7280, 7350,
            7430, 7510, 7590, 7660, 7740, 7820, 7900, 7990,
            8070, 8150, 8240, 8320, 8410, 8500, 8580, 8670,
            8760, 8850, 8950, 9040, 9130, 9230, 9320, 9420,
            9520, 9620, 9720, 9820, 9920, 10000, 10100, 10200,
            10300, 10400, 10600, 10700, 10800, 10900, 11000, 11100,
            11200, 11300, 11500, 11600, 11700, 11800, 11900, 12100,
            12200, 12300, 12400, 12600, 12700, 12800, 13000, 13100,
            13200, 13400, 13500, 13700, 13800, 13900, 14100, 14200,
            14400, 14500, 14700, 14800, 15000, 15100, 15300, 15500,
            15600, 15800, 15900, 16100, 16300, 16400, 16600, 16800,
            17000, 17100, 17300, 17500, 17700, 17900, 18000, 18200,
            18400, 18600, 18800, 19000, 19200, 19400, 19600, 19800,
            20000
        };
        // 音频采样的中心值（归一化用），随采样格式变化（16 位有符号时为 0）
        inline static constexpr int16_t DEF_SAMPLE_CENTER = 0;
        inline static constexpr float   DEF_SAMPLE_SCALE  = 32768.0f;

        /// 固定容量环形缓冲区，用于流式音频采样分析，消除动态 vector 的反复扩容与内存前移
        class AudioRingBuffer {
        private:

            std::vector<uint8_t> buffer;
            size_t               head  = 0;
            size_t               tail  = 0;
            size_t               count = 0;

        public:

            void init(size_t capacity) {
                buffer.assign(capacity, static_cast<uint8_t>(DEF_SAMPLE_CENTER));
                head  = 0;
                tail  = 0;
                count = 0;
            }

            [[nodiscard]] size_t size() const noexcept {
                return count;
            }

            [[nodiscard]] size_t capacity() const noexcept {
                return buffer.size();
            }

            [[nodiscard]] size_t available_write() const noexcept {
                return buffer.size() - count;
            }

            void write(const uint8_t* data, size_t n) {
                if (0 == n) {
                    return;
                }
                if (n > available_write()) {
                    const size_t         newCap = std::max(buffer.size() * 2, count + n);
                    std::vector<uint8_t> newBuf(
                        newCap,
                        static_cast<uint8_t>(DEF_SAMPLE_CENTER)
                    );
                    if (count > 0) {
                        const size_t firstChunk = std::min(count, buffer.size() - tail);
                        std::memcpy(newBuf.data(), buffer.data() + tail, firstChunk);
                        if (count > firstChunk) {
                            std::memcpy(
                                newBuf.data() + firstChunk,
                                buffer.data(),
                                count - firstChunk
                            );
                        }
                    }
                    buffer = std::move(newBuf);
                    tail   = 0;
                    head   = count;
                }
                const size_t cap        = buffer.size();
                const size_t firstChunk = std::min(n, cap - head);
                std::memcpy(buffer.data() + head, data, firstChunk);
                if (n > firstChunk) {
                    std::memcpy(buffer.data(), data + firstChunk, n - firstChunk);
                }
                head  = (head + n) % cap;
                count += n;
            }

            void read(uint8_t* dest, size_t n) {
                assert(n <= count);
                if (0 == n) {
                    return;
                }
                const size_t cap        = buffer.size();
                const size_t firstChunk = std::min(n, cap - tail);
                std::memcpy(dest, buffer.data() + tail, firstChunk);
                if (n > firstChunk) {
                    std::memcpy(dest + firstChunk, buffer.data(), n - firstChunk);
                }
                tail  = (tail + n) % cap;
                count -= n;
            }

            void clear() {
                head  = 0;
                tail  = 0;
                count = 0;
            }
        };

        // ── 建表：一帧的变换长度随采样率变化，所以表在打开文件时按长度算一次 ──
        //
        // fftSize 是能放进一帧（sampleRate / DEF_FPS 个采样）的最大 2 的幂。
        // 一帧按 fftSize 做一次 FFT，得到 fftSize/2 个频点，每个频点宽
        // sampleRate / fftSize Hz，覆盖到 sampleRate / 2。这样一帧的覆盖范围
        // 不再受输出频点数限制：44.1 kHz 下 fftSize = 4096，频点约 10.8 Hz 宽，
        // 覆盖 22 kHz，够到 20 kHz。
        void prepareTables(size_t sampleRate) {
            const size_t frameSize = sampleRate / DEF_FPS;

            size_t n = 1;
            while (n * 2 <= frameSize) {
                n *= 2;
            }
            if (n < 64) {
                n = 64;
            }
            if (n == fftSize && !fftBitRevTable.empty()) {
                return;
            }

            fftSize = n;
            fftBitRevTable.assign(n, 0);
            fftTwiddleCos.assign(n / 2, 0.0f);
            fftTwiddleSin.assign(n / 2, 0.0f);
            hanningWindow.assign(n, 0.0f);
            fftReal.assign(n, 0.0f);
            fftImag.assign(n, 0.0f);

            size_t bits = 0;
            while ((static_cast<size_t>(1) << bits) < n) {
                bits++;
            }
            for (size_t i = 0; i < n; ++i) {
                size_t bitRev = 0;
                for (size_t j = 0; j < bits; ++j) {
                    bitRev |= ((i >> j) & 1) << (bits - 1 - j);
                }
                fftBitRevTable[i] = bitRev;
            }

            for (size_t k = 0; k < n / 2; ++k) {
                fftTwiddleCos[k] = std::cos(DEF_2PI * static_cast<float>(k) / static_cast<float>(n));
                fftTwiddleSin[k]
                    = -std::sin(DEF_2PI * static_cast<float>(k) / static_cast<float>(n));
            }

            for (size_t i = 0; i < n; i++) {
                hanningWindow[i]
                    = 0.5f
                      * (1.0f - std::cos(DEF_2PI * static_cast<float>(i) / static_cast<float>(n - 1)));
            }
        }

        AudioSpectrumAnalyzer() {
            // avformat_network_init();
        }

        ~AudioSpectrumAnalyzer() {
            cleanup();
        }

        AudioSpectrumAnalyzer(const AudioSpectrumAnalyzer&)            = delete;
        AudioSpectrumAnalyzer& operator=(const AudioSpectrumAnalyzer&) = delete;

        // 快速傅里叶变换（长度用 prepareTables 算出的 fftSize）
        void fft(float* real, float* imag) {
            const size_t n = fftSize;

            // 位反转重排
            for (size_t i = 0; i < n; ++i) {
                const size_t j = fftBitRevTable[i];
                if (i < j) {
                    std::swap(real[i], real[j]);
                    std::swap(imag[i], imag[j]);
                }
            }

            // 迭代 FFT
            // 循环分组 len（2^1, 2^2, ..., 2^log2(n)）
            for (size_t len = 2; len <= n; len <<= 1) {
                const size_t halfLen = len / 2;
                // 旋转因子步长
                const size_t step = n / len;

                for (size_t i = 0; i < n; i += len) {
                    // 组内蝶形运算
                    for (size_t j = 0; j < halfLen; ++j) {
                        // 从预计算表中获取旋转因子（W_N^(j*step)）
                        const float wCos = fftTwiddleCos[j * step];
                        const float wSin = fftTwiddleSin[j * step];

                        // 蝶形索引
                        const size_t idxU = i + j;
                        const size_t idxV = idxU + halfLen;

                        // v * W
                        const float vwReal = real[idxV] * wCos - imag[idxV] * wSin;
                        const float vwImag = imag[idxV] * wCos + real[idxV] * wSin;

                        const float uReal = real[idxU];
                        const float uImag = imag[idxU];

                        real[idxU] = uReal + vwReal;
                        imag[idxU] = uImag + vwImag;
                        real[idxV] = uReal - vwReal;
                        imag[idxV] = uImag - vwImag;
                    }
                }
            }
        }

        // 一帧音频（dataSize 个采样）算出频谱与响度
        //
        // 频谱：取这一帧最新的 fftSize 个采样（fftSize 见 prepareTables）做变换，
        // 一个频点宽 sampleRate / fftSize Hz（44.1 kHz 下约 10.8 Hz），覆盖到
        // sampleRate / 2（约 22 kHz）。输出的 DEF_SPECTRUM_SIZE 个字节是显示频点，
        // 不是单个 FFT 频点：第 i 个字节覆盖 DEF_BAND_EDGES 里的一段频率（见下表），
        // 值取这一段里各频点的合成功率（按频点在段内的宽度比例累加，见下）。
        //
        // 早期版本先把一帧平均成 2 * DEF_SPECTRUM_SIZE 个采样再做 512 点变换，
        // 那样只能覆盖 DEF_SPECTRUM_SIZE * DEF_FPS * 2 = 5.1 kHz 的一半（约
        // 2.5 kHz），更高的内容会折叠回低频段，也会把频点整体拉偏约 4.5%。
        bool computeSpectrum(
            const size_t                                  dataSize,
            const int16_t*                                dataStart,
            std::array<unsigned char, DEF_SPECTRUM_SIZE>& result,
            unsigned char&                                wave
        ) {
            result.fill(0);

            if (dataSize == 0 || 0 == fftSize || dataStart == nullptr) {
                XX_LOGW("computeSpectrum: 输入数据为空（dataSize: {}, fftSize: {}）", dataSize, fftSize);
                return false;
            }

            // ── 响度（wave）：用整帧采样算 RMS ──
            {
                float framePointSum = 0.0f;
                for (size_t i = 0; i < dataSize; ++i) {
                    const float val
                        = static_cast<float>(dataStart[i] - DEF_SAMPLE_CENTER) / DEF_SAMPLE_SCALE;
                    framePointSum += val * val;
                }
                const float rms        = std::sqrt(framePointSum / static_cast<float>(dataSize));
                const float db         = 20.0f * std::log10(rms + 1e-10f);
                const float normalized = std::clamp(
                    (db - DEF_WAVE_MIN_DB) / (DEF_WAVE_MAX_DB - DEF_WAVE_MIN_DB),
                    0.0f,
                    1.0f
                );
                wave = static_cast<unsigned char>(normalized * DEF_WAVE_MAX_POINT);
            }

            // ── 取这一帧最新的 fftSize 个采样，套汉宁窗 ──
            // 一帧不足 fftSize 个采样时（低采样率）前面的位置补 0
            const size_t offset = dataSize > fftSize ? dataSize - fftSize : 0;
            for (size_t i = 0; i < fftSize; ++i) {
                const size_t idx = offset + i;
                const float  val = idx < dataSize
                                       ? static_cast<float>(dataStart[idx] - DEF_SAMPLE_CENTER)
                                             / DEF_SAMPLE_SCALE
                                       : 0.0f;
                fftReal[i] = val * hanningWindow[i];
                fftImag[i] = 0.0f;
            }

            // ── FFT ──
            fft(fftReal.data(), fftImag.data());

            // ── 幅度计算 + 分贝归一化 ──
            // 单个频点的归一化系数：满幅（振幅 1）正弦的幅度为 fftSize / 2，
            // 直流分量为 fftSize。
            const float scaleDc = 1.0f / static_cast<float>(fftSize);
            const float scaleAc = 2.0f / static_cast<float>(fftSize);
            const size_t usable  = fftSize / 2; // 变换给出的频点数
            const int    sampleRate = codecContext ? codecContext->sample_rate : 0;
            if (sampleRate <= 0) {
                XX_LOGW("computeSpectrum: 无效的采样率 {}", sampleRate);
                return false;
            }
            const double binHz = static_cast<double>(sampleRate) / static_cast<double>(fftSize);

            for (size_t i = 0; i < DEF_SPECTRUM_SIZE; i++) {
                const double f0 = static_cast<double>(DEF_BAND_EDGES[i]);
                const double f1 = static_cast<double>(DEF_BAND_EDGES[i + 1]);
                // 与这一段有重叠的频点，含两侧各一个（边界可能落在频点中间）
                int first = static_cast<int>(std::floor(f0 / binHz)) - 1;
                int last  = static_cast<int>(std::ceil(f1 / binHz)) + 1;
                if (first < 0) {
                    first = 0;
                }
                if (last > static_cast<int>(usable)) {
                    last = static_cast<int>(usable);
                }

                // ── 按频点在段内的宽度比例累加功率 ──
                // 频点 k 覆盖 [k - 0.5, k + 0.5] 个频点宽，取它与这一段重叠的比例。
                // 只数"中心落在段内"的整个频点会出现：段宽 40 Hz 时有时算 3 个频点、
                // 有时算 4 个，相邻段电平差约 1 dB，而且 44.1 kHz 与 48 kHz 结果还不一样。
                // 按比例累加后，每一段都恰好量到自己的那段频率宽度，与采样率无关。
                // 单个正弦仍保持"只输出它那一个频点"时的电平（窗口把它摊在 3 个频点上）。
                float power = 0.0f;
                for (int k = first; k < last; ++k) {
                    const double share = std::min((k + 0.5) * binHz, f1)
                                         - std::max((k - 0.5) * binHz, f0);
                    if (share <= 0.0) {
                        continue;
                    }
                    const float scale = (0 == k) ? scaleDc : scaleAc;
                    const float re    = fftReal[k] * scale;
                    const float im    = fftImag[k] * scale;
                    power += (re * re + im * im) * static_cast<float>(share / binHz);
                }

                const float magnitude  = std::sqrt(power);
                const float db         = 20.0f * std::log10(magnitude + 1e-10f);
                const float normalized = std::clamp(
                    (db - DEF_FREQUENCY_MIN_DB) / (DEF_FREQUENCY_MAX_DB - DEF_FREQUENCY_MIN_DB),
                    0.0f,
                    1.0f
                );
                result[i] = static_cast<unsigned char>(normalized * DEF_WAVE_MAX_POINT);
            }

            return true;
        }

        bool openAudioFile(const char* filepath) {
            XX_LOGD("AudioVisuallizationAnalyzer / openAudioFile... {}", filepath);

            // 重置资源
            cleanup();

            int ret = avformat_open_input(&formatContext, filepath, nullptr, nullptr);
            if (ret < 0) {
                XX_LOGE_AV("avformat_open_input failed");
                return false;
            }

            ret = avformat_find_stream_info(formatContext, nullptr);
            if (ret < 0) {
                XX_LOGE_AV("avformat_find_stream_info failed");
                return false;
            }

            audioStreamIndex
                = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
            if (audioStreamIndex < 0) {
                XX_LOGE("No audio stream found");
                return false;
            }

            AVCodecParameters* codecParameters = formatContext->streams[audioStreamIndex]->codecpar;
            const AVCodec*     codec           = avcodec_find_decoder(codecParameters->codec_id);
            if (!codec) {
                XX_LOGE("avcodec_find_decoder failed, id:  {}", int(codecParameters->codec_id));
                return false;
            }

            codecContext = avcodec_alloc_context3(codec);
            if (!codecContext) {
                XX_LOGE("avcodec_alloc_context3 failed");
                return false;
            }

            ret = avcodec_parameters_to_context(codecContext, codecParameters);
            if (ret < 0) {
                XX_LOGE_AV("avcodec_parameters_to_context failed");
                return false;
            }

            ret = avcodec_open2(codecContext, codec, nullptr);
            if (ret < 0) {
                XX_LOGE_AV("avcodec_open2 failed");
                return false;
            }

            AVChannelLayout in_ch_layout    = codecContext->ch_layout;
            auto            out_ch_layout   = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;
            int             in_sample_rate  = codecContext->sample_rate;
            int             out_sample_rate = codecContext->sample_rate;
            AVSampleFormat  in_sample_fmt   = codecContext->sample_fmt;
            // 16 位有符号采样：整数解码器（wav / flac）在 8 位下会被截断出约
            // -0.5 LSB 的直流，进而把第 0、1 个频点抬到 77 上下；8 位本身也只有
            // -48 dBFS 的量化底，安静的内容会整个消失。
            AVSampleFormat  out_sample_fmt  = AV_SAMPLE_FMT_S16;

            assert(nullptr == swrContext);
            ret = swr_alloc_set_opts2(
                &swrContext,
                &out_ch_layout,  // 输出声道布局
                out_sample_fmt,  // 输出采样格式
                out_sample_rate, // 输出采样率
                &in_ch_layout,   // 输入声道布局
                in_sample_fmt,   // 输入采样格式
                in_sample_rate,  // 输入采样率
                0,               // 日志级别
                nullptr          // 日志上下文
            );
            if (nullptr == swrContext) {
                XX_LOGE_AV("swr_alloc failed");
                return false;
            }

            ret = swr_init(swrContext);
            if (ret != 0) {
                XX_LOGE_AV("swr_init failed");
                return false;
            }

            // 预分配重采样缓冲区（至少能容纳两帧并设有底线容量），避免解码循环中反复分配
            resampledDataSize
                = std::max(out_sample_rate / static_cast<int>(DEF_FPS) * 2, 16384);
            ret = av_samples_alloc(
                reinterpret_cast<uint8_t**>(&resampledData),
                nullptr,
                1,
                resampledDataSize,
                AV_SAMPLE_FMT_S16,
                0
            );
            if (ret < 0) {
                resampledData     = nullptr;
                resampledDataSize = 0;
            }

            XX_LOGD("openAudioFile success");
            return true;
        }

        bool decodeAudioFrame(
            AudioRingBuffer& ringBuffer,
            AVPacket*        packet,
            AVFrame*         decodedFrame
        ) {
            // packet can be nullptr (flush)
            if (!codecContext || !decodedFrame) {
                XX_LOGE("decodeAudioFrame: invalid parameters");
                return false;
            }

            int ret = avcodec_send_packet(codecContext, packet);
            if (ret < 0) {
                XX_LOGE_AV("avcodec_send_packet failed");
                return false;
            }

            bool haveDecoded = false;
            do {
                ret = avcodec_receive_frame(codecContext, decodedFrame);
                if (ret == 0) {
                    // 重采样
                    int outSamplesSize = swr_get_out_samples(swrContext, decodedFrame->nb_samples);
                    if (resampledDataSize < outSamplesSize || nullptr == resampledData) {
                        av_freep(&resampledData);
                        ret = av_samples_alloc(
                            reinterpret_cast<uint8_t**>(&resampledData),
                            nullptr,
                            1,
                            outSamplesSize,
                            AV_SAMPLE_FMT_S16,
                            0
                        );
                        if (ret < 0) {
                            resampledData     = nullptr;
                            resampledDataSize = 0;
                            return false;
                        }
                        resampledDataSize = outSamplesSize;
                    }

                    ret = swr_convert(
                        swrContext,
                        reinterpret_cast<uint8_t* const*>(&resampledData),
                        outSamplesSize,
                        (const uint8_t**)decodedFrame->data,
                        decodedFrame->nb_samples
                    );
                    if (ret >= 0) {
                        haveDecoded = true;
                        if (nullptr != resampledData && ret > 0) {
                            int outSize = av_samples_get_buffer_size(
                                nullptr,
                                1,
                                ret,
                                AV_SAMPLE_FMT_S16,
                                0
                            );
                            if (outSize > 0) {
                                ringBuffer.write(
                                    reinterpret_cast<const uint8_t*>(resampledData),
                                    static_cast<size_t>(outSize)
                                );
                            }
                        } else {
                            XX_LOGW("swr_convert: no valid data (nb_samples: {})", ret);
                        }
                    } else {
                        XX_LOGE_AV("swr_convert failed");
                    }
                } else if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
                    XX_LOGE_AV("avcodec_receive_frame failed");
                }
            } while (ret >= 0);
            return haveDecoded;
        }

        // [outputSpectrum] 时间-频率-振幅
        // [outputWaveform] 时间-振幅数据
        // [maxDurationSeconds] 最大分析音频时长（秒），<= 0 时不限制全曲分析
        bool processAudio(
            const char*                                                filepath,
            std::vector<std::array<unsigned char, DEF_SPECTRUM_SIZE>>& outputSpectrum,
            std::vector<unsigned char>&                                outputWaveform,
            int                                                        maxDurationSeconds = 0
        ) {
            // 清空输出
            outputSpectrum.clear();
            outputWaveform.clear();

            if (false == openAudioFile(filepath)) {
                XX_LOGE("processAudio: openAudioFile failed");
                return false;
            }
            XX_LOGD("AudioVisuallizationAnalyzer / processAudio ...");

            const int    sampleRate      = codecContext->sample_rate;
            const size_t samplesPerFrame = size_t(sampleRate) / DEF_FPS;
            if (sampleRate <= 0 || samplesPerFrame == 0) {
                XX_LOGE(
                    "processAudio: invalid sampleRate ({}) or samplesPerFrame ({})",
                    sampleRate,
                    samplesPerFrame
                );
                return false;
            }

            // 按采样率算好这一首的变换长度与各张表（见 prepareTables）
            prepareTables(static_cast<size_t>(sampleRate));
            if (0 == fftSize) {
                XX_LOGE("processAudio: prepareTables failed");
                return false;
            }
            XX_LOGD(
                "processAudio: sampleRate {}, frame {}, fft {}, bin {:.2f} Hz",
                sampleRate,
                samplesPerFrame,
                fftSize,
                static_cast<double>(sampleRate) / static_cast<double>(fftSize)
            );

            // 初始化固定大小环形缓冲区（约 8 帧容量，至少 64 KB），消除动态扩容与大内存前移
            // 环形缓冲区按字节存放，所以一帧占 samplesPerFrame * sizeof(int16_t) 字节
            AudioRingBuffer ringBuffer;
            const size_t    frameBytes   = samplesPerFrame * sizeof(int16_t);
            const size_t    ringCapacity = std::max(frameBytes * 8, size_t(65536));
            ringBuffer.init(ringCapacity);

            // 预分配单帧缓冲区，避免在分析过程中频繁分配内存
            std::vector<int16_t> frameBuffer(samplesPerFrame, DEF_SAMPLE_CENTER);

            // 预估输出帧数并预留空间，避免 outputSpectrum 多次动态扩容与重分配
            int64_t durationUs = formatContext->duration;
            if (durationUs > 0) {
                double durationSec = static_cast<double>(durationUs) / AV_TIME_BASE;
                if (maxDurationSeconds > 0 && durationSec > maxDurationSeconds) {
                    durationSec = maxDurationSeconds;
                }
                size_t estimatedFrames = static_cast<size_t>(durationSec * DEF_FPS) + 10;
                outputSpectrum.reserve(estimatedFrames);
                outputWaveform.reserve(estimatedFrames);
            }

            AVPacket* packet       = av_packet_alloc();
            AVFrame*  decodedFrame = av_frame_alloc();
            if (nullptr == packet || nullptr == decodedFrame) {
                XX_LOGE("processAudio: av_packet_alloc or av_frame_alloc failed");
                av_packet_free(&packet);
                av_frame_free(&decodedFrame);
                return false;
            }

            auto usePacket    = packet;
            bool reachedLimit = false;

            while (true) {
                int ret = av_read_frame(formatContext, usePacket);
                if (ret >= 0) {
                    assert(nullptr != usePacket);
                } else if (false == outputSpectrum.empty() || ringBuffer.size() > 0) {
                    // 读取失败但之前积累读取数据，可能是到达末尾，最后调用一次解码冲洗
                    usePacket = nullptr;
                } else {
                    // 读取失败
                    break;
                }
                if (nullptr == usePacket || usePacket->stream_index == audioStreamIndex) {
                    // 解码并积累音频数据到环形缓冲
                    if (decodeAudioFrame(ringBuffer, usePacket, decodedFrame)) {
                        // 当环形缓冲区中数据达到一帧时，流式取出计算，无需保留全曲数据
                        while (ringBuffer.size() >= frameBytes) {
                            ringBuffer.read(
                                reinterpret_cast<uint8_t*>(frameBuffer.data()),
                                frameBytes
                            );

                            // 计算频谱数据与波形振幅数据
                            outputSpectrum.emplace_back();
                            outputWaveform.emplace_back(0);
                            computeSpectrum(
                                samplesPerFrame,
                                frameBuffer.data(),
                                outputSpectrum.back(),
                                outputWaveform.back()
                            );

                            if (maxDurationSeconds > 0
                                && outputSpectrum.size()
                                       >= static_cast<size_t>(maxDurationSeconds * DEF_FPS)) {
                                reachedLimit = true;
                                break;
                            }
                        }
                    }
                    av_frame_unref(decodedFrame);
                }
                if (nullptr != usePacket) {
                    av_packet_unref(usePacket);
                } else {
                    break;
                }

                if (reachedLimit) {
                    break;
                }
            }
            av_packet_free(&packet);
            av_frame_free(&decodedFrame);

            // 处理剩余数据，不足一帧时填充静音（DEF_SAMPLE_CENTER）完成最后一帧计算
            if (!reachedLimit && ringBuffer.size() > 0) {
                const size_t remaining = ringBuffer.size();
                ringBuffer.read(reinterpret_cast<uint8_t*>(frameBuffer.data()), remaining);
                const size_t remainingSamples = remaining / sizeof(int16_t);
                std::fill(
                    frameBuffer.begin() + remainingSamples,
                    frameBuffer.end(),
                    DEF_SAMPLE_CENTER
                );

                outputSpectrum.emplace_back();
                outputWaveform.emplace_back(0);
                computeSpectrum(
                    samplesPerFrame,
                    frameBuffer.data(),
                    outputSpectrum.back(),
                    outputWaveform.back()
                );
            }

            cleanup();
            assert(outputSpectrum.size() == outputWaveform.size());
            return (!outputSpectrum.empty() && !outputWaveform.empty());
        }

        void cleanup() {
            XX_LOGD("AudioVisuallizationAnalyzer / cleanup ...");
            if (resampledData) {
                av_freep(&resampledData);
                resampledData     = nullptr;
                resampledDataSize = 0;
            }
            if (swrContext) {
                swr_free(&swrContext);
                swrContext = nullptr;
            }
            if (codecContext) {
                avcodec_free_context(&codecContext);
                codecContext = nullptr;
            }
            if (formatContext) {
                avformat_close_input(&formatContext);
                formatContext = nullptr;
            }
            audioStreamIndex = -1;
        }
    };
}; // namespace mediaxx