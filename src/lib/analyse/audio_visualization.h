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
#include <cmath>
#include <complex>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

class AudioSpectrumAnalyzer {
private:

    AVFormatContext* formatContext;
    AVCodecContext*  codecContext;
    int              audioStreamIndex;
    SwrContext*      swrContext;

public:

    inline static const size_t DEF_SPECTRUM_SIZE = 256;
    inline static const size_t DEF_FPS           = 10;

    AudioSpectrumAnalyzer() :
        formatContext(nullptr),
        codecContext(nullptr),
        audioStreamIndex(-1),
        swrContext(nullptr) {
        // avformat_network_init();
    }

    ~AudioSpectrumAnalyzer() {
        cleanup();
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

        // 音频流
        audioStreamIndex
            = av_find_best_stream(formatContext, AVMEDIA_TYPE_AUDIO, -1, -1, nullptr, 0);
        if (audioStreamIndex < 0) {
            LXX_ERR("No audio stream found");
            cleanup();
            return false;
        }

        // 解码器参数
        AVCodecParameters* codecParameters = formatContext->streams[audioStreamIndex]->codecpar;
        const AVCodec*     codec           = avcodec_find_decoder(codecParameters->codec_id);
        if (!codec) {
            LXX_ERR("avcodec_find_decoder failed, id:  {}", int(codecParameters->codec_id));
            cleanup();
            return false;
        }

        // 初始化解码器上下文
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

        // 解码器
        if (avcodec_open2(codecContext, codec, nullptr) < 0) {
            LXX_ERR("avcodec_open2 failed");
            cleanup();
            return false;
        }

        // 初始化重采样器
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

    bool decodeAudioFrame(std::vector<float>& data, AVPacket* packet) {
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

        AVFrame* frame = av_frame_alloc();
        if (!frame) {
            LXX_AVERR("av_frame_alloc failed");
            return false;
        }

        ret = avcodec_receive_frame(codecContext, frame);
        if (ret == 0) {
            // 重采样
            AVFrame* resampledFrame = av_frame_alloc();
            if (!resampledFrame) {
                LXX_AVERR("av_frame_alloc (resampledFrame) failed");
                av_frame_free(&frame);
                return false;
            }

            resampledFrame->sample_rate = frame->sample_rate;
            resampledFrame->format      = AV_SAMPLE_FMT_FLT;
            resampledFrame->ch_layout   = AV_CHANNEL_LAYOUT_MONO;

            ret = swr_convert_frame(swrContext, resampledFrame, frame);
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

            av_frame_free(&resampledFrame);
        } else if (ret != AVERROR(EAGAIN) && ret != AVERROR_EOF) {
            LXX_AVERR("avcodec_receive_frame failed");
        }

        av_frame_free(&frame);
        return ret == 0;
    }

    // 汉宁窗口
    std::vector<float> createHanningWindow(int size) {
        std::vector<float> window(size, 0.0f);
        for (int i = 0; i < size; i++) {
            window[i] = 0.5f * (1.0f - std::cos(2.0f * M_PI * i / (size - 1)));
        }
        return window;
    }

    // 快速傅里叶变换,仅支持2的幂长度
    void fft(std::vector<std::complex<float>>& data) {
        int n = data.size();
        if (n <= 1 || (n & (n - 1)) != 0) { // 新增：校验长度为2的幂
            LXX_WARN("fft: data size ({}) is not power of 2", n);
            return;
        }

        for (int i = 1, j = 0; i < n; i++) {
            int bit = n >> 1;
            for (; j & bit; bit >>= 1) {
                j ^= bit;
            }
            j ^= bit;
            if (i < j) {
                std::swap(data[i], data[j]);
            }
        }

        for (int len = 2; len <= n; len <<= 1) {
            float               angle = -2.0f * M_PI / len;
            std::complex<float> wlen{std::cos(angle), std::sin(angle)};
            for (int i = 0; i < n; i += len) {
                std::complex<float> w{1};
                for (int j = 0; j < len / 2; j++) {
                    std::complex<float> u  = data[i + j];
                    std::complex<float> v  = data[i + j + len / 2] * w;
                    data[i + j]            = u + v;
                    data[i + j + len / 2]  = u - v;
                    w                     *= wlen;
                }
            }
        }
    }

    bool computeSpectrum(
        const size_t                                  dataSize,
        const std::vector<float>::iterator            dataStart,
        std::array<unsigned char, DEF_SPECTRUM_SIZE>& result
    ) {
        const size_t requiredSize = DEF_SPECTRUM_SIZE * 2;
        result.fill(0);

        if (dataSize < requiredSize) {
            LXX_WARN(
                "computeSpectrum: audio data size ({}) < required ({})",
                dataSize,
                requiredSize
            );
            return false;
        }

        auto window  = createHanningWindow(requiredSize);
        auto fftData = std::vector<std::complex<float>>(requiredSize);
        for (size_t i = 0; i < requiredSize; i++) {
            fftData[i] = std::complex<float>{*(dataStart + i) * window[i], 0.0f};
        }

        fft(fftData);

        for (size_t i = 0; i < DEF_SPECTRUM_SIZE; i++) {
            float magnitude  = std::abs(fftData[i]);
            float db         = 20.0f * std::log10(magnitude + 1e-6f);
            float normalized = (db + 80.0f) / 80.0f;
            normalized       = std::clamp(normalized, 0.0f, 1.0f);
            result[i]        = static_cast<unsigned char>(normalized * 255);
        }

        return true;
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

        std::vector<float> accumulatedAudio{};

        while (av_read_frame(formatContext, packet) >= 0) {
            if (packet->stream_index == audioStreamIndex) {
                // 解码并积累音频数据
                if (decodeAudioFrame(accumulatedAudio, packet)) {
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
            }
            av_packet_unref(packet);
        }
        av_packet_free(&packet);

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
        // 释放顺序：依赖资源先释放（swrContext依赖codecContext）
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