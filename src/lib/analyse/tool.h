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
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include "simdjson.h"
#include "util/log.h"
#include "util/string_util.h"

namespace analyse_tool {
    class AnalyseLogItem_c {
    public:

        const char** log;
        size_t       logNum = 0;

        AnalyseLogItem_c(const char** in_log) :
            log(in_log) {
            // log 容器非空，但内容為空
            assert(nullptr != log && nullptr == *log);
        }

        void setLog(const std::string_view data) {
            ++logNum;
            auto temp = *log;
            if (nullptr == temp) {
                *log = stringxx::stringCopyMalloc(data).data();
            } else {
                // 已经有内容，附加
                *log = stringxx::stringCopyMalloc(*log, "\n\n", data).data();
            }
            mediaxx_free(temp);
        }

        template<typename... _Args>
        void setLog(std::format_string<_Args...> fmt, _Args&&... args) {
            setLog(std::format(fmt, std::forward<_Args>(args)...));
        }
    };

    struct Color {
        uint8_t r, g, b;
        int     count;
        double  brightness;

        void toJson(simdjson::builder::string_builder& sb) const {
            sb.start_object();
            {
                sb.append_key_value<"rgb">(
                    (unsigned int)r << 16 | (unsigned int)g << 8 | (unsigned int)b
                );
                sb.append_comma();
                sb.append_key_value<"count">(count);
                sb.append_comma();
                sb.append_key_value<"brightness">(brightness);
            }
            sb.end_object();
        }
    };

    class AnalysePictureColorResult {
    public:

        Color mainColor;
        Color lightColors[4];
        Color darkColors[4];
        Color dominantColors[4];

        simdjson::builder::string_builder toJson() const {
            LXX_DEBEG("AnalysePictureColorResult.toJson ......");
            simdjson::builder::string_builder sb{};

            sb.start_object();

            sb.escape_and_append_with_quotes("mainColor");
            sb.append_colon();
            mainColor.toJson(sb);
            sb.append_comma();

            sb.escape_and_append_with_quotes("lightColors");
            sb.append_colon();
            {
                sb.start_array();
                bool isFirst = true;
                for (const auto& c : lightColors) {
                    if (false == isFirst) {
                        sb.append_comma();
                    }
                    isFirst = false;
                    c.toJson(sb);
                }
                sb.end_array();
            }
            sb.append_comma();

            sb.escape_and_append_with_quotes("darkColors");
            sb.append_colon();
            {
                sb.start_array();
                bool isFirst = true;
                for (const auto& c : darkColors) {
                    if (false == isFirst) {
                        sb.append_comma();
                    }
                    isFirst = false;
                    c.toJson(sb);
                }
                sb.end_array();
            }
            sb.append_comma();

            sb.escape_and_append_with_quotes("dominantColors");
            sb.append_colon();
            {
                sb.start_array();
                bool isFirst = true;
                for (const auto& c : dominantColors) {
                    if (false == isFirst) {
                        sb.append_comma();
                    }
                    isFirst = false;
                    c.toJson(sb);
                }
                sb.end_array();
            }

            sb.end_object();
            return sb;
        }
    };

    struct BufferData {
        const uint8_t* ptr;  // 指向图片二进制数据的指针
        size_t         size; // 数据大小
        size_t         pos;  // 当前读取位置
    };

    inline bool compareColor(const Color& a, const Color& b) {
        return a.count > b.count;
    }

    // 计算颜色亮度
    inline double calculateBrightness(uint8_t r, uint8_t g, uint8_t b) {
        return 0.299 * r + 0.587 * g + 0.114 * b;
    }

    inline std::string colorToHex(uint8_t r, uint8_t g, uint8_t b) {
        std::stringstream ss{};
        ss << std::hex << std::setw(2) << std::setfill('0') << (int)r << std::setw(2) << (int)g
           << std::setw(2) << (int)b;
        return ss.str();
    }

    inline std::shared_ptr<AnalysePictureColorResult>
        analysePictureColor(AVFormatContext* formatCtx, analyse_tool::AnalyseLogItem_c& logItem) {
        LXX_DEBEG("analysePictureColor: ");
        if (avformat_find_stream_info(formatCtx, nullptr) < 0) {
            std::cout << "无法获取流信息" << std::endl;
            avformat_close_input(&formatCtx);
            return nullptr;
        }
        LXX_DEBEG("find picture stream: ");

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
            return nullptr;
        }

        LXX_DEBEG("decoder picture: ");
        auto codecPar = formatCtx->streams[videoStreamIndex]->codecpar;
        auto codec    = avcodec_find_decoder(codecPar->codec_id);
        if (!codec) {
            std::cout << "无法找到解码器" << std::endl;
            avformat_close_input(&formatCtx);
            return nullptr;
        }

        AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx) {
            std::cout << "无法分配解码器上下文" << std::endl;
            avformat_close_input(&formatCtx);
            return nullptr;
        }

        if (avcodec_parameters_to_context(codecCtx, codecPar) < 0) {
            std::cout << "无法将解码器参数复制到上下文" << std::endl;
            avcodec_free_context(&codecCtx);
            avformat_close_input(&formatCtx);
            return nullptr;
        }

        if (avcodec_open2(codecCtx, codec, nullptr) < 0) {
            std::cout << "无法打开解码器" << std::endl;
            avcodec_free_context(&codecCtx);
            avformat_close_input(&formatCtx);
            return nullptr;
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
            return nullptr;
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
            return nullptr;
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

        // 排序
        std::vector<Color> allColors;
        for (auto& pair : colorMap) {
            allColors.push_back(pair.second);
        }
        std::sort(allColors.begin(), allColors.end(), compareColor);

        // 分类主色调、亮色调、暗色调
        auto result = std::make_shared<AnalysePictureColorResult>();

        if (!allColors.empty()) {
            result->mainColor = allColors[0]; // 主色调：出现次数最多的颜色
            int index         = 0;
            int lightIndex    = 0;
            int darkIndex     = 0;
            for (auto& color : allColors) {
                if (index < 4) {
                    result->dominantColors[index] = color;
                }
                if (color.brightness > 180 && lightIndex < 4) {
                    result->lightColors[lightIndex] = color;
                    ++lightIndex;
                } else if (color.brightness < 80 && darkIndex < 4) {
                    result->darkColors[darkIndex] = color;
                    ++darkIndex;
                }

                if (lightIndex >= 4 && darkIndex >= 4) {
                    break;
                }
                ++index;
            }
        }

        av_free(rgbBuffer);
        sws_freeContext(swsCtx);
        av_frame_free(&frame);
        av_frame_free(&rgbFrame);
        avcodec_free_context(&codecCtx);
        avformat_close_input(&formatCtx);

        return result;
    }

    static inline int _packetRead(void* opaque, uint8_t* buf, int buf_size) {
        BufferData* bd = (BufferData*)opaque;
        buf_size       = int(std::min((size_t)buf_size, bd->size - bd->pos));

        if (buf_size <= 0) {
            // 文件结束
            return AVERROR_EOF;
        }

        memcpy(buf, bd->ptr + bd->pos, buf_size);
        bd->pos += buf_size;

        return buf_size;
    }

    inline std::shared_ptr<AnalysePictureColorResult> analyzePictureColorFromData(
        const char*                     data,
        size_t                          dataSize,
        analyse_tool::AnalyseLogItem_c& logItem
    ) {
        if (nullptr == data || dataSize == 0) {
            logItem.setLog("输入数据无效, dataPtr: {}, dataSize: {}", (void*)data, dataSize);
            return nullptr;
        }

        auto bd = BufferData{(const uint8_t*)data, dataSize, 0};

        const size_t avio_buffer_size = 4096;
        uint8_t*     avio_buffer      = (uint8_t*)av_malloc(avio_buffer_size);
        if (nullptr == avio_buffer) {
            logItem.setLog("无法分配 AVIO 缓冲区");
            return nullptr;
        }
        AVIOContext* avioCtx = avio_alloc_context(
            avio_buffer,
            avio_buffer_size,
            0,
            &bd,
            &_packetRead,
            nullptr,
            nullptr
        );
        if (nullptr == avioCtx) {
            logItem.setLog("无法分配 AVIO 上下文");
            av_free(avio_buffer);
            return nullptr;
        }

        auto formatCtx = avformat_alloc_context();
        if (nullptr == formatCtx) {
            avio_context_free(&avioCtx); // 同时释放 avio_buffer
            return nullptr;
        }
        formatCtx->pb = avioCtx;

        if (avformat_open_input(&formatCtx, NULL, NULL, NULL) != 0) {
            logItem.setLog("无法打开 avformat_open_input");
            avformat_free_context(formatCtx);
            avio_context_free(&avioCtx);
            av_free(avio_buffer);
            return nullptr;
        }

        return analysePictureColor(formatCtx, logItem);
    }

    inline std::shared_ptr<AnalysePictureColorResult> analysePictureColorFromPath(
        const char*                     picturePath,
        analyse_tool::AnalyseLogItem_c& logItem
    ) {
        AVFormatContext* formatCtx = nullptr;
        if (avformat_open_input(&formatCtx, picturePath, nullptr, nullptr) != 0) {
            logItem.setLog("无法打开图片文件");
            return nullptr;
        }
        return analysePictureColor(formatCtx, logItem);
    }
}; // namespace analyse_tool