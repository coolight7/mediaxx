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
#include <complex>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

class AudioSpectrumAnalyzer {
private:

    AVFormatContext* formatContext    = nullptr;
    AVCodecContext*  codecContext     = nullptr;
    SwrContext*      swrContext       = nullptr;
    int              audioStreamIndex = -1;

public:

    inline static constexpr size_t DEF_SPECTRUM_SIZE       = 256;
    inline static constexpr size_t DEF_HANNING_WINDOW_SIZE = DEF_SPECTRUM_SIZE * 2;
    inline static constexpr size_t DEF_FPS                 = 10;
    inline static constexpr size_t DEF_FFT_SIZE            = DEF_SPECTRUM_SIZE * 2;
    inline static constexpr size_t DEF_FFT_LOG2_SIZE       = 9; // log2(512) = 9，位反转计算
    inline static constexpr float  DEF_2PI                 = 2.0f * static_cast<float>(M_PI);

    // 预计算的旋转因子表（W_N^k = cosθ - i*sinθ，θ=2kπ/N）
    std::array<float, DEF_FFT_SIZE / 2> fftTwiddleCos{}; // 实部（cosθ）
    std::array<float, DEF_FFT_SIZE / 2> fftTwiddleSin{}; // 虚部（-sinθ，提前存储负号）

    std::array<size_t, DEF_FFT_SIZE>           fftBitRevTable{};
    std::array<float, DEF_HANNING_WINDOW_SIZE> hanningWindow{};

    AudioSpectrumAnalyzer() {
        // avformat_network_init();
    }

    ~AudioSpectrumAnalyzer() {
        cleanup();
    }

    AudioSpectrumAnalyzer(const AudioSpectrumAnalyzer&)            = delete;
    AudioSpectrumAnalyzer& operator=(const AudioSpectrumAnalyzer&) = delete;

    // 汉宁窗口
    void createHanningWindow() {
        assert(hanningWindow.size() == DEF_HANNING_WINDOW_SIZE);
        hanningWindow.fill(0);
        for (size_t i = 0; i < DEF_HANNING_WINDOW_SIZE; i++) {
            hanningWindow[i]
                = 0.5f * (1.0f - std::cos(DEF_2PI * i / (DEF_HANNING_WINDOW_SIZE - 1)));
        }
    }

    void precomputeFFTTables() {
        // 预计算位反转表（针对 512 点 FFT，9 位地址）
        for (size_t i = 0; i < DEF_FFT_SIZE; ++i) {
            size_t bitRev = 0;
            for (size_t j = 0; j < DEF_FFT_LOG2_SIZE; ++j) {
                // 逐位反转：将 i 的第 j 位放到 bitRev 的第 (DEF_FFT_LOG2_SIZE-1-j) 位
                bitRev |= ((i >> j) & 1) << (DEF_FFT_LOG2_SIZE - 1 - j);
            }
            fftBitRevTable[i] = bitRev;
        }

        // 预计算旋转因子表（W_N^k = cos(2πk/N) - i*sin(2πk/N)）
        // 只需要计算前 N/2 个（后续可复用），提前存储虚部的负号
        for (size_t k = 0; k < DEF_FFT_SIZE / 2; ++k) {
            const float theta = DEF_2PI * k / DEF_FFT_SIZE;
            fftTwiddleCos[k]  = std::cos(theta);
            // 提前带负号，减少计算
            fftTwiddleSin[k] = -std::sin(theta);
        }

        LXX_DEBEG(
            "FFT tables precomputed: bitRevTable size={}, twiddleTable size={}",
            fftBitRevTable.size(),
            fftTwiddleCos.size()
        );
    }

    // 快速傅里叶变换
    void fft(std::vector<std::complex<float>>& data) const {
        if (data.size() != DEF_FFT_SIZE) {
            LXX_WARN("fft: only support size={}, current={}", DEF_FFT_SIZE, data.size());
            return;
        }

        // 位反转重排（使用预计算表，O(N) 时间）
        for (size_t i = 0; i < DEF_FFT_SIZE; ++i) {
            const size_t j = fftBitRevTable[i];
            if (i < j) {
                std::swap(data[i], data[j]);
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

                    // 获取蝶形的两个节点
                    const size_t               idxU = i + j;
                    const size_t               idxV = idxU + halfLen;
                    const std::complex<float>& u    = data[idxU];
                    const std::complex<float>& v    = data[idxV];

                    // 计算 v * W
                    // (v.real * wCos - v.imag * wSin) + i*(v.imag * wCos + v.real * wSin)
                    const float vwReal = v.real() * wCos - v.imag() * wSin;
                    const float vwImag = v.imag() * wCos + v.real() * wSin;

                    data[idxU] = std::complex<float>(u.real() + vwReal, u.imag() + vwImag);
                    data[idxV] = std::complex<float>(u.real() - vwReal, u.imag() - vwImag);
                }
            }
        }
    }

    void init() {
        createHanningWindow();
        // 预计算 FFT 旋转因子表和位反转表
        precomputeFFTTables();
    }

    bool computeSpectrum(
        const size_t                                  dataSize,
        const std::vector<float>::iterator            dataStart,
        std::array<unsigned char, DEF_SPECTRUM_SIZE>& result
    ) {
        result.fill(0);

        if (dataSize == 0) {
            LXX_WARN("computeSpectrum: input data size is 0");
            return false;
        }

        // 统一线性插值适配汉宁窗大小
        std::vector<float> windowInput(DEF_HANNING_WINDOW_SIZE, 0.0f);
        for (size_t i = 0; i < DEF_HANNING_WINDOW_SIZE; ++i) {
            // 将原始M个采样点映射到N=512个窗口点
            // 计算当前目标点在原始数据中的 [归一化位置]（0~M-1）
            const float srcPos
                = static_cast<float>(i) * (dataSize - 1) / (DEF_HANNING_WINDOW_SIZE - 1);
            // 左邻点索引
            const size_t srcIdx = static_cast<size_t>(srcPos);
            // 插值权重（0~1，表示距离左邻点的比例）
            const float alpha = srcPos - srcIdx;

            // 线性插值：保持原始信号的趋势，避免失真
            if (srcIdx < dataSize - 1) {
                // (1-alpha)*左邻点 + alpha*右邻点
                windowInput[i]
                    = (1.0f - alpha) * *(dataStart + srcIdx) + alpha * *(dataStart + srcIdx + 1);
            } else {
                // 最后一个目标点直接取原始数据最后一个值
                windowInput[i] = *(dataStart + dataSize - 1);
            }
        }

        std::vector<std::complex<float>> fftData(DEF_FFT_SIZE);
        for (size_t i = 0; i < DEF_HANNING_WINDOW_SIZE; ++i) {
            fftData[i] = std::complex<float>{windowInput[i] * hanningWindow[i], 0.0f};
        }

        fft(fftData);

        for (size_t i = 0; i < DEF_SPECTRUM_SIZE; i++) {
            float magnitude = std::abs(fftData[i]);

            // 实信号FFT共轭对称，正频率分量幅度×2
            if (i > 0 && i < DEF_SPECTRUM_SIZE) {
                magnitude *= 2.0f;
            }

            // 分贝转换，归一化 255
            const float db         = 20.0f * std::log10(magnitude + 1e-6f);
            float       normalized = (db + 80.0f) / 80.0f;
            normalized             = std::clamp(normalized, 0.0f, 1.0f);
            result[i]              = static_cast<unsigned char>(normalized * 255);
        }

        return true;
    }

    bool openAudioFile(const char* filepath) {
        LXX_DEBEG("AudioVisuallizationAnalyzer / openAudioFile... {}", filepath);

        // 重置资源
        cleanup();

        if (avformat_open_input(&formatContext, filepath, nullptr, nullptr) < 0) {
            LXX_ERR("avformat_open_input failed");
            return false;
        }

        if (avformat_find_stream_info(formatContext, nullptr) < 0) {
            LXX_ERR("avformat_find_stream_info failed");
            cleanup();
            return false;
        }

        audioStreamIndex
            = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (audioStreamIndex < 0) {
            LXX_ERR("No audio stream found");
            cleanup();
            return false;
        }

        AVCodecParameters* codecParameters = formatContext->streams[audioStreamIndex]->codecpar;
        const AVCodec*     codec           = avcodec_find_decoder(codecParameters->codec_id);
        if (!codec) {
            LXX_ERR("avcodec_find_decoder failed, id:  {}", int(codecParameters->codec_id));
            cleanup();
            return false;
        }

        codecContext = avcodec_alloc_context3(codec);
        if (!codecContext) {
            LXX_ERR("avcodec_alloc_context3 failed");
            cleanup();
            return false;
        }

        if (avcodec_parameters_to_context(codecContext, codecParameters) < 0) {
            LXX_ERR("avcodec_parameters_to_context failed");
            cleanup();
            return false;
        }

        if (avcodec_open2(codecContext, codec, nullptr) < 0) {
            LXX_ERR("avcodec_open2 failed");
            cleanup();
            return false;
        }

        swrContext = swr_alloc();
        if (!swrContext) {
            LXX_ERR("swr_alloc failed");
            cleanup();
            return false;
        }

        auto outputChLayout = (AVChannelLayout)AV_CHANNEL_LAYOUT_MONO;
        av_opt_set_chlayout(swrContext, "in_chlayout", &(codecContext->ch_layout), 0);
        av_opt_set_chlayout(swrContext, "out_chlayout", &outputChLayout, 0);
        av_opt_set_int(swrContext, "in_sample_rate", codecContext->sample_rate, 0);
        av_opt_set_int(swrContext, "out_sample_rate", codecContext->sample_rate, 0);
        av_opt_set_sample_fmt(swrContext, "in_sample_fmt", codecContext->sample_fmt, 0);
        av_opt_set_sample_fmt(swrContext, "out_sample_fmt", AV_SAMPLE_FMT_FLT, 0);

        int ret = swr_init(swrContext);
        if (ret != 0) {
            LXX_AVERR("swr_init failed");
            cleanup();
            return false;
        }

        LXX_DEBEG("openAudioFile success");
        return true;
    }

    bool decodeAudioFrame(
        std::vector<float>& data,
        AVPacket*           packet,
        AVFrame*            decodedFrame,
        AVFrame*            resampledFrame
    ) {
        if (!codecContext || !packet) {
            LXX_ERR("decodeAudioFrame: invalid parameters");
            return false;
        }

        int ret = avcodec_send_packet(codecContext, packet);
        if (ret < 0) {
            if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
                LXX_AVERR("avcodec_send_packet failed");
            }
            return false;
        }

        if (!decodedFrame) {
            LXX_AVERR("decodedFrame not available");
            return false;
        }

        ret = avcodec_receive_frame(codecContext, decodedFrame);
        if (ret == 0) {
            // 重采样
            if (!resampledFrame) {
                LXX_AVERR("resampledFrame not available");
                return false;
            }

            resampledFrame->sample_rate = decodedFrame->sample_rate;
            resampledFrame->format      = AV_SAMPLE_FMT_FLT;
            resampledFrame->ch_layout   = AV_CHANNEL_LAYOUT_MONO;

            ret = swr_convert_frame(swrContext, resampledFrame, decodedFrame);
            if (ret == 0) {
                // 校验数据
                if (resampledFrame->data[0] && resampledFrame->nb_samples > 0) {
                    float* samples = reinterpret_cast<float*>(resampledFrame->data[0]);
                    data.insert(data.end(), samples, samples + resampledFrame->nb_samples);
                } else {
                    LXX_WARN(
                        "swr_convert_frame: no valid data (nb_samples: {})",
                        resampledFrame->nb_samples
                    );
                }
            } else {
                LXX_AVERR("swr_convert_frame failed");
            }
        } else if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            LXX_AVERR("avcodec_receive_frame failed");
        }

        return ret == 0;
    }

    bool processAudio(
        const char*                                                filepath,
        std::vector<std::array<unsigned char, DEF_SPECTRUM_SIZE>>& outputSpectrum
    ) {
        outputSpectrum.clear();

        if (false == openAudioFile(filepath)) {
            LXX_ERR("processAudio: openAudioFile failed");
            return false;
        }
        LXX_DEBEG("AudioVisuallizationAnalyzer / processAudio ...");

        int    sampleRate      = codecContext->sample_rate;
        size_t samplesPerFrame = size_t(sampleRate) / DEF_FPS;
        if (sampleRate <= 0 || samplesPerFrame == 0) {
            LXX_ERR(
                "processAudio: invalid sampleRate ({}) or samplesPerFrame ({})",
                sampleRate,
                samplesPerFrame
            );
            cleanup();
            return false;
        }

        AVPacket* packet = av_packet_alloc();
        if (nullptr == packet) {
            LXX_ERR("processAudio: av_packet_alloc failed");
            cleanup();
            return false;
        }

        init();
        AVFrame*           decodedFrame   = av_frame_alloc();
        AVFrame*           resampledFrame = av_frame_alloc();
        std::vector<float> accumulatedAudio{};

        while (av_read_frame(formatContext, packet) >= 0) {
            if (packet->stream_index == audioStreamIndex) {
                // 解码并积累音频数据
                if (decodeAudioFrame(accumulatedAudio, packet, decodedFrame, resampledFrame)) {
                    while (accumulatedAudio.size() >= samplesPerFrame) {
                        outputSpectrum.emplace_back();
                        computeSpectrum(
                            samplesPerFrame,
                            accumulatedAudio.begin(),
                            outputSpectrum.back()
                        );

                        // 移除已处理的数据
                        accumulatedAudio.erase(
                            accumulatedAudio.begin(),
                            accumulatedAudio.begin() + samplesPerFrame
                        );
                    }
                }
                av_frame_unref(decodedFrame);
                av_frame_unref(resampledFrame);
            }
            av_packet_unref(packet);
        }
        av_packet_free(&packet);
        av_frame_free(&decodedFrame);
        av_frame_free(&resampledFrame);

        // 处理剩余数据,不足一帧时填充0
        if (false == accumulatedAudio.empty()) {
            accumulatedAudio.resize(samplesPerFrame, 0.0f);
            outputSpectrum.emplace_back();
            computeSpectrum(
                accumulatedAudio.size(),
                accumulatedAudio.begin(),
                outputSpectrum.back()
            );
        }

        cleanup();
        return true;
    }

    void cleanup() {
        LXX_DEBEG("AudioVisuallizationAnalyzer / cleanup ...");
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