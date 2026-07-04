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

        AVFormatContext* formatContext    = nullptr;
        AVCodecContext*  codecContext     = nullptr;
        SwrContext*      swrContext       = nullptr;
        int              audioStreamIndex = -1;

    public:

        inline static constexpr size_t        DEF_SPECTRUM_SIZE       = 256;
        inline static constexpr unsigned char DEF_WAVE_MAX_POINT      = 255;
        inline static constexpr size_t        DEF_HANNING_WINDOW_SIZE = DEF_SPECTRUM_SIZE * 2;
        inline static constexpr size_t        DEF_FPS                 = 10;
        inline static constexpr size_t        DEF_FFT_SIZE            = DEF_SPECTRUM_SIZE * 2;
        inline static constexpr size_t DEF_FFT_LOG2_SIZE = 9; // log2(512) = 9，位反转计算
        inline static constexpr float  DEF_2PI           = 2.0f * static_cast<float>(M_PI);
        // 波形振幅计算的分贝参数
        inline static constexpr float DEF_FREQUENCY_MIN_DB = -80.0f;
        inline static constexpr float DEF_FREQUENCY_MAX_DB = 0.0f;
        inline static constexpr float DEF_WAVE_MIN_DB      = -60.0f;
        inline static constexpr float DEF_WAVE_MAX_DB      = 20.0f;
        // UINT8音频采样的中心值（归一化用）
        inline static constexpr uint8_t DEF_U8_CENTER = 128;
        inline static constexpr float   DEF_U8_SCALE  = 128.0f;

        // ── 编译期预计算表 ──
        inline static constexpr std::array<float, DEF_HANNING_WINDOW_SIZE> makeHanningWindow() {
            std::array<float, DEF_HANNING_WINDOW_SIZE> arr{};
            for (size_t i = 0; i < DEF_HANNING_WINDOW_SIZE; i++) {
                arr[i] = 0.5f
                         * (1.0f
                            - std::cos(
                                DEF_2PI * static_cast<float>(i)
                                / static_cast<float>(DEF_HANNING_WINDOW_SIZE - 1)
                            ));
            }
            return arr;
        }

        inline static constexpr std::array<size_t, DEF_FFT_SIZE> makeBitRevTable() {
            std::array<size_t, DEF_FFT_SIZE> arr{};
            for (size_t i = 0; i < DEF_FFT_SIZE; ++i) {
                size_t bitRev = 0;
                for (size_t j = 0; j < DEF_FFT_LOG2_SIZE; ++j) {
                    bitRev |= ((i >> j) & 1) << (DEF_FFT_LOG2_SIZE - 1 - j);
                }
                arr[i] = bitRev;
            }
            return arr;
        }

        inline static constexpr std::array<float, DEF_FFT_SIZE / 2> makeTwiddleCos() {
            std::array<float, DEF_FFT_SIZE / 2> arr{};
            for (size_t k = 0; k < DEF_FFT_SIZE / 2; ++k) {
                arr[k]
                    = std::cos(DEF_2PI * static_cast<float>(k) / static_cast<float>(DEF_FFT_SIZE));
            }
            return arr;
        }

        inline static constexpr std::array<float, DEF_FFT_SIZE / 2> makeTwiddleSin() {
            std::array<float, DEF_FFT_SIZE / 2> arr{};
            for (size_t k = 0; k < DEF_FFT_SIZE / 2; ++k) {
                arr[k]
                    = -std::sin(DEF_2PI * static_cast<float>(k) / static_cast<float>(DEF_FFT_SIZE));
            }
            return arr;
        }

        inline static const std::array<float, DEF_FFT_SIZE / 2> fftTwiddleCos  = makeTwiddleCos();
        inline static const std::array<float, DEF_FFT_SIZE / 2> fftTwiddleSin  = makeTwiddleSin();
        inline static const std::array<size_t, DEF_FFT_SIZE>    fftBitRevTable = makeBitRevTable();
        inline static const std::array<float, DEF_HANNING_WINDOW_SIZE> hanningWindow
            = makeHanningWindow();

        // ── 预分配复用缓冲区，消除每帧 heap 分配 ──
        alignas(16) std::array<float, DEF_FFT_SIZE> fftReal{};
        alignas(16) std::array<float, DEF_FFT_SIZE> fftImag{};

        AudioSpectrumAnalyzer() {
            // avformat_network_init();
        }

        ~AudioSpectrumAnalyzer() {
            cleanup();
        }

        AudioSpectrumAnalyzer(const AudioSpectrumAnalyzer&)            = delete;
        AudioSpectrumAnalyzer& operator=(const AudioSpectrumAnalyzer&) = delete;

        // 快速傅里叶变换
        void fft(float* real, float* imag) {
            // 位反转重排
            for (size_t i = 0; i < DEF_FFT_SIZE; ++i) {
                const size_t j = fftBitRevTable[i];
                if (i < j) {
                    std::swap(real[i], real[j]);
                    std::swap(imag[i], imag[j]);
                }
            }

            // 迭代 FFT
            // 循环分组 len（2^1, 2^2, ..., 2^9=512）
            for (size_t len = 2; len <= DEF_FFT_SIZE; len <<= 1) {
                const size_t halfLen = len / 2;
                // 旋转因子步长
                const size_t step = DEF_FFT_SIZE / len;

                for (size_t i = 0; i < DEF_FFT_SIZE; i += len) {
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

        bool computeSpectrum(
            const size_t                                  dataSize,
            const uint8_t*                                dataStart,
            std::array<unsigned char, DEF_SPECTRUM_SIZE>& result,
            unsigned char&                                wave
        ) {
            result.fill(0);

            if (dataSize == 0) {
                XX_LOGW("computeSpectrum: input data size is 0");
                return false;
            }

            // ── 平均降采样替代线性插值 ──
            // 将 M 个原始采样均匀分成 N=512 段，每段求平均
            // 这等效于矩形低通滤波 + 降采样，消除了混叠失真
            float        framePointSum = 0.0f;
            const size_t blockSize     = dataSize / DEF_HANNING_WINDOW_SIZE;
            const size_t remainder     = dataSize % DEF_HANNING_WINDOW_SIZE;

            for (size_t i = 0; i < DEF_HANNING_WINDOW_SIZE; ++i) {
                // 每段长度：前 remainder 段多一个采样
                const size_t segLen = blockSize + (i < remainder ? 1 : 0);
                const size_t start  = i * blockSize + (i < remainder ? i : remainder);

                float sum = 0.0f;
                for (size_t j = 0; j < segLen; ++j) {
                    const float val
                        = static_cast<float>(dataStart[start + j] - DEF_U8_CENTER) / DEF_U8_SCALE;
                    sum           += val;
                    framePointSum += val * val; // RMS 基于原始采样
                }
                fftReal[i] = sum / segLen;
            }

            // ── 振幅计算（wave）──
            {
                const float rms        = std::sqrt(framePointSum / dataSize);
                const float db         = 20.0f * std::log10(rms + 1e-10f);
                const float normalized = std::clamp(
                    (db - DEF_WAVE_MIN_DB) / (DEF_WAVE_MAX_DB - DEF_WAVE_MIN_DB),
                    0.0f,
                    1.0f
                );
                wave = static_cast<unsigned char>(normalized * DEF_WAVE_MAX_POINT);
            }

            // ── 应用汉宁窗 ──
            for (size_t i = 0; i < DEF_HANNING_WINDOW_SIZE; ++i) {
                fftReal[i] *= hanningWindow[i];
                fftImag[i]  = 0.0f;
            }
            // 剩余部分清零（DEF_HANNING_WINDOW_SIZE ~ DEF_FFT_SIZE）
            for (size_t i = DEF_HANNING_WINDOW_SIZE; i < DEF_FFT_SIZE; ++i) {
                fftReal[i] = 0.0f;
                fftImag[i] = 0.0f;
            }

            // ── FFT ──
            fft(fftReal.data(), fftImag.data());

            // ── 幅度计算 + 分贝归一化 ──
            // DC(i=0) 和 Nyquist(i=N/2) 正确除以 N，其余除以 N/2
            for (size_t i = 0; i < DEF_SPECTRUM_SIZE; i++) {
                float magnitude = std::sqrt(fftReal[i] * fftReal[i] + fftImag[i] * fftImag[i]);

                // DC 和 Nyquist 分量除以 N，其他频率分量除以 N/2
                const float norm = (i == 0 || i == DEF_SPECTRUM_SIZE)
                                       ? magnitude / DEF_FFT_SIZE
                                       : magnitude / (DEF_FFT_SIZE / 2);

                const float db         = 20.0f * std::log10(norm + 1e-10f);
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
            AVSampleFormat  out_sample_fmt  = AV_SAMPLE_FMT_U8;

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

            XX_LOGD("openAudioFile success");
            return true;
        }

        bool decodeAudioFrame(
            std::vector<uint8_t>& data,
            AVPacket*             packet,
            AVFrame*              decodedFrame,
            uint8_t*&             resampledData,
            int&                  resampledDataSize
        ) {
            // packet can be nullptr
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
                            &resampledData,
                            nullptr,
                            1,
                            outSamplesSize,
                            AV_SAMPLE_FMT_U8,
                            0
                        );
                        if (ret < 0) {
                            return false;
                        }
                        resampledDataSize = outSamplesSize;
                    }

                    ret = swr_convert(
                        swrContext,
                        &resampledData,
                        outSamplesSize,
                        (const uint8_t**)decodedFrame->data,
                        decodedFrame->nb_samples
                    );
                    if (ret >= 0) {
                        haveDecoded = true;
                        // 校验数据
                        if (nullptr != resampledData) {
                            int outSize
                                = av_samples_get_buffer_size(nullptr, 1, ret, AV_SAMPLE_FMT_U8, 0);
                            data.insert(data.end(), resampledData, resampledData + outSize);
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
        bool processAudio(
            const char*                                                filepath,
            std::vector<std::array<unsigned char, DEF_SPECTRUM_SIZE>>& outputSpectrum,
            std::vector<unsigned char>&                                outputWaveform
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

            AVPacket* packet       = av_packet_alloc();
            AVFrame*  decodedFrame = av_frame_alloc();
            if (nullptr == packet || nullptr == decodedFrame) {
                XX_LOGE("processAudio: av_packet_alloc or av_frame_alloc failed");
                av_packet_free(&packet);
                av_frame_free(&decodedFrame);
                return false;
            }
            uint8_t*             resampledData     = nullptr;
            int                  resampledDataSize = 0;
            std::vector<uint8_t> accumulatedAudio{};
            // ── 用偏移指针替代 erase，消除 O(N) memmove ──
            size_t processedOffset = 0;
            auto   usePacket       = packet;

            while (true) {
                int ret = av_read_frame(formatContext, usePacket);
                if (ret >= 0) {
                    assert(nullptr != usePacket);
                } else if (false == outputSpectrum.empty()) {
                    // 读取失败，但之前积累读取数据，可能是到达末尾，最后调用一次解码
                    usePacket = nullptr;
                } else {
                    // 读取失败
                    break;
                }
                if (nullptr == usePacket || usePacket->stream_index == audioStreamIndex) {
                    // 解码并积累音频数据
                    if (decodeAudioFrame(
                            accumulatedAudio,
                            usePacket,
                            decodedFrame,
                            resampledData,
                            resampledDataSize
                        )) {
                        // 用指针偏移代替 erase
                        while (accumulatedAudio.size() - processedOffset >= samplesPerFrame) {
                            const uint8_t* frameData = accumulatedAudio.data() + processedOffset;

                            // 计算频谱数据
                            outputSpectrum.emplace_back();
                            // 波形振幅数据
                            outputWaveform.emplace_back(0);
                            computeSpectrum(
                                samplesPerFrame,
                                frameData,
                                outputSpectrum.back(),
                                outputWaveform.back()
                            );
                            processedOffset += samplesPerFrame;

                            // ── 定期 compact，避免 offset 无限增长 ──
                            if (processedOffset >= samplesPerFrame * DEF_FPS * 2) {
                                accumulatedAudio.erase(
                                    accumulatedAudio.begin(),
                                    accumulatedAudio.begin() + processedOffset
                                );
                                processedOffset = 0;
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
            }
            av_packet_free(&packet);
            av_frame_free(&decodedFrame);
            av_freep(&resampledData);

            // 处理剩余数据,不足一帧时填充0
            const size_t remaining = accumulatedAudio.size() - processedOffset;
            if (remaining > 0) {
                // 拷贝剩余数据到开头并补齐
                std::vector<uint8_t> lastFrame(samplesPerFrame, DEF_U8_CENTER);
                for (size_t i = 0; i < remaining; ++i) {
                    lastFrame[i] = accumulatedAudio[processedOffset + i];
                }

                outputSpectrum.emplace_back();
                outputWaveform.emplace_back(0);
                computeSpectrum(
                    samplesPerFrame,
                    lastFrame.data(),
                    outputSpectrum.back(),
                    outputWaveform.back()
                );
            }

            cleanup();
            assert(outputSpectrum.size() == outputWaveform.size());
            return (outputSpectrum.size() > 0 && outputWaveform.size() > 0);
        }

        void cleanup() {
            XX_LOGD("AudioVisuallizationAnalyzer / cleanup ...");
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