#pragma once
extern "C" {
#include "libavcodec/avcodec.h"
#include "libavformat/avformat.h"
#include "libavutil/imgutils.h"
#include <libswscale/swscale.h>
}

#include <algorithm>
#include <iomanip>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "util/log.h"

namespace analyse_tool {
    struct Color {
        uint8_t r, g, b;
        int     count;
        double  brightness;
    };

    struct BufferData {
        const uint8_t* ptr;  // 指向图片二进制数据的指针
        size_t         size; // 数据大小
        size_t         pos;  // 当前读取位置
    };

    // 比较函数：用于颜色排序（按出现次数降序）
    bool compareColor(const Color& a, const Color& b) {
        return a.count > b.count;
    }

    // 计算颜色亮度
    double calculateBrightness(uint8_t r, uint8_t g, uint8_t b) {
        return 0.299 * r + 0.587 * g + 0.114 * b;
    }

    // 颜色转十六进制字符串
    std::string colorToHex(uint8_t r, uint8_t g, uint8_t b) {
        std::stringstream ss{};
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)r << std::setw(2) << (int)g
           << std::setw(2) << (int)b;
        return ss.str();
    }

    void analysePictureColor(AVFormatContext* formatCtx) {
        LXX_DEBEG("analysePictureColor: ");
        if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
            std::cout << "无法获取流信息" << std::endl;
            avformat_close_input(&formatCtx);
            return;
        }
        LXX_DEBEG("find picture stream: ");

        // 查找视频流（图片只有一个视频流）
        int videoStreamIndex = -1;
        for (unsigned int i = 0; i < formatCtx->nb_streams; i++) {
            if (formatCtx->streams[i]->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                videoStreamIndex = i;
                break;
            }
        }

        if (videoStreamIndex < 0) {
            std::cout << "未找到视频流" << std::endl;
            avformat_close_input(&formatCtx);
            return;
        }

        // 获取解码器参数
        LXX_DEBEG("decoder picture: ");
        auto codecPar = formatCtx->streams[videoStreamIndex]->codecpar;
        auto codec    = avcodec_find_decoder(codecPar->codec_id);
        if (!codec) {
            std::cout << "无法找到解码器" << std::endl;
            avformat_close_input(&formatCtx);
            return;
        }

        AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx) {
            std::cout << "无法分配解码器上下文" << std::endl;
            avformat_close_input(&formatCtx);
            return;
        }

        if (avcodec_parameters_to_context(codecCtx, codecPar) < 0) {
            std::cout << "无法将解码器参数复制到上下文" << std::endl;
            avcodec_free_context(&codecCtx);
            avformat_close_input(&formatCtx);
            return;
        }

        if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
            std::cout << "无法打开解码器" << std::endl;
            avcodec_free_context(&codecCtx);
            avformat_close_input(&formatCtx);
            return;
        }
        LXX_DEBEG("decoder picture...");

        // 解码图片
        AVFrame*  frame    = av_frame_alloc();
        AVFrame*  rgbFrame = av_frame_alloc();
        AVPacket* pkt      = av_packet_alloc();

        int  ret;
        bool frameDecoded = false;
        while (av_read_frame(formatCtx, pkt) >= 0) {
            if (pkt->stream_index == videoStreamIndex) {
                ret = avcodec_send_packet(codecCtx, pkt);
                if (ret < 0) {
                    std::cout << "发送数据包失败" << std::endl;
                    break;
                }

                ret = avcodec_receive_frame(codecCtx, frame);
                if (ret == 0) {
                    frameDecoded = true;
                    break;
                }
            }
            av_packet_unref(pkt);
        }
        av_packet_unref(pkt);
        av_packet_free(&pkt);

        if (!frameDecoded) {
            std::cout << "解码图片失败" << std::endl;
            av_frame_free(&frame);
            av_frame_free(&rgbFrame);
            avcodec_free_context(&codecCtx);
            avformat_close_input(&formatCtx);
            return;
        }

        // 转换为RGB格式
        LXX_DEBEG("to RGB...");
        struct SwsContext* swsCtx = sws_getContext(
            codecCtx->width,
            codecCtx->height,
            codecCtx->pix_fmt,
            codecCtx->width,
            codecCtx->height,
            AV_PIX_FMT_RGB24,
            SWS_BILINEAR,
            nullptr,
            nullptr,
            nullptr
        );

        if (!swsCtx) {
            std::cout << "无法创建颜色转换上下文" << std::endl;
            av_frame_free(&frame);
            av_frame_free(&rgbFrame);
            avcodec_free_context(&codecCtx);
            avformat_close_input(&formatCtx);
            return;
        }

        int numBytes
            = av_image_get_buffer_size(AV_PIX_FMT_RGB24, codecCtx->width, codecCtx->height, 1);
        uint8_t* rgbBuffer = (uint8_t*)av_malloc(numBytes * sizeof(uint8_t));
        av_image_fill_arrays(
            rgbFrame->data,
            rgbFrame->linesize,
            rgbBuffer,
            AV_PIX_FMT_RGB24,
            codecCtx->width,
            codecCtx->height,
            1
        );

        sws_scale(
            swsCtx,
            frame->data,
            frame->linesize,
            0,
            codecCtx->height,
            rgbFrame->data,
            rgbFrame->linesize
        );

        // 统计颜色
        LXX_DEBEG("analyse color...");
        std::map<uint32_t, Color> colorMap; // key: RGB值(0xRRGGBB), value: 颜色信息
        int                       width  = codecCtx->width;
        int                       height = codecCtx->height;

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                uint8_t* pixel = rgbFrame->data[0] + y * rgbFrame->linesize[0] + x * 3;
                uint8_t  r     = pixel[0];
                uint8_t  g     = pixel[1];
                uint8_t  b     = pixel[2];

                uint32_t key        = (r << 16) | (g << 8) | b;
                double   brightness = calculateBrightness(r, g, b);

                if (colorMap.find(key) != colorMap.end()) {
                    colorMap[key].count++;
                } else {
                    colorMap[key] = {r, g, b, 1, brightness};
                }
            }
        }

        // 转换为vector并排序
        std::vector<Color> allColors;
        for (auto& pair : colorMap) {
            allColors.push_back(pair.second);
        }
        std::sort(allColors.begin(), allColors.end(), compareColor);

        // 分类主色调、亮色调、暗色调
        std::vector<Color> brightColors;                // 亮色调 (亮度 > 180)
        std::vector<Color> darkColors;                  // 暗色调 (亮度 < 80)
        Color              mainColor = {0, 0, 0, 0, 0}; // 主色调

        if (!allColors.empty()) {
            mainColor = allColors[0]; // 主色调：出现次数最多的颜色

            for (auto& color : allColors) {
                if (color.brightness > 180 && brightColors.size() < 4) {
                    brightColors.push_back(color);
                } else if (color.brightness < 80 && darkColors.size() < 4) {
                    darkColors.push_back(color);
                }

                if (brightColors.size() >= 4 && darkColors.size() >= 4) {
                    break;
                }
            }
        }

        // 输出结果
        std::cout << "图片尺寸: " << width << "x" << height << std::endl;
        std::cout << "\n主色调: " << colorToHex(mainColor.r, mainColor.g, mainColor.b)
                  << " (出现次数: " << mainColor.count << ")" << std::endl;

        std::cout << "\n亮色调 (Top 4):" << std::endl;
        for (int i = 0; i < brightColors.size(); i++) {
            std::cout << i + 1 << ". "
                      << colorToHex(brightColors[i].r, brightColors[i].g, brightColors[i].b)
                      << " (出现次数: " << brightColors[i].count
                      << ", 亮度: " << brightColors[i].brightness << ")" << std::endl;
        }

        std::cout << "\n暗色调 (Top 4):" << std::endl;
        for (int i = 0; i < darkColors.size(); i++) {
            std::cout << i + 1 << ". "
                      << colorToHex(darkColors[i].r, darkColors[i].g, darkColors[i].b)
                      << " (出现次数: " << darkColors[i].count
                      << ", 亮度: " << darkColors[i].brightness << ")" << std::endl;
        }

        // 释放资源
        av_free(rgbBuffer);
        sws_freeContext(swsCtx);
        av_frame_free(&frame);
        av_frame_free(&rgbFrame);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);
    }

    // FFmpeg 的读取回调函数
    // 当 FFmpeg 需要数据时，它会调用这个函数
    static int _readPacket(void* opaque, uint8_t* buf, int buf_size) {
        BufferData* bd = (BufferData*)opaque;
        buf_size       = int(std::min((size_t)buf_size, bd->size - bd->pos));

        if (buf_size <= 0)
            return AVERROR_EOF; // 文件结束

        // 从我们的缓冲区复制数据到 FFmpeg 的缓冲区
        memcpy(buf, bd->ptr + bd->pos, buf_size);
        bd->pos += buf_size;

        return buf_size;
    }

    void analysePictureColorFromPath(const char* picturePath) {
        AVFormatContext* formatCtx = nullptr;
        if (avformat_open_input(&formatCtx, picturePath, nullptr, nullptr) != 0) {
            std::cout << "无法打开图片文件" << std::endl;
            return;
        }
        analysePictureColor(formatCtx);
    }

    void analyzePictureColorFromData(const char* data, size_t dataSize) {
        if (nullptr == data || dataSize == 0) {
            std::cout << "无效的输入数据" << std::endl;
            return;
        }

        // 2. 设置缓冲区数据结构
        BufferData bd = {(const uint8_t*)data, dataSize, 0};

        // 3. 分配并初始化 AVIOContext
        // 我们使用一个小的内部缓冲区 (例如 4KB)，FFmpeg 会从这里读取
        const size_t avio_buffer_size = 4096;
        uint8_t*     avio_buffer      = (uint8_t*)av_malloc(avio_buffer_size);
        if (nullptr == avio_buffer) {
            std::cout << "无法分配 AVIO 缓冲区" << std::endl;
            return;
        }
        AVIOContext* avioCtx = avio_alloc_context(
            avio_buffer,
            avio_buffer_size,
            0,
            &bd,
            &_readPacket,
            nullptr,
            nullptr
        );
        if (!avioCtx) {
            std::cout << "无法分配 AVIO 上下文" << std::endl;
            av_free(avio_buffer); // 释放已分配的缓冲区
            return;
        }

        // 4. 分配并初始化 AVFormatContext
        AVFormatContext* formatCtx = avformat_alloc_context();
        if (nullptr == formatCtx) {
            avio_context_free(&avioCtx); // 这会同时释放 avio_buffer
            return;
        }
        formatCtx->pb                 = avioCtx;
        formatCtx->protocol_whitelist = av_strdup("file,data,pipe");
        analysePictureColor(formatCtx);
    }
}; // namespace analyse_tool