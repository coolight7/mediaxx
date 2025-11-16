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
#include <random>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

#include "simdjson.h"
#include "util/log.h"
#include "util/string_util.h"
#include "util/utilxx.h"

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
        int     brightness;

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

        bool operator==(const Color& other) const {
            return r == other.r && g == other.g && b == other.b;
        }
    };

    class AnalysePictureColorResult {
    public:

        Color                mainColor;
        std::array<Color, 4> lightColors{};
        std::array<Color, 4> darkColors{};
        std::array<Color, 8> dominantColors{};

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
        const uint8_t* ptr;  // 指向图片二进制数据
        size_t         size; // 数据大小
        size_t         pos;  // 当前读取位置
    };

    inline bool compareColor(const Color& a, const Color& b) {
        return a.count > b.count;
    }

    // RGB转HSL
    inline void rgbToHsl(uint8_t r, uint8_t g, uint8_t b, double& h, double& s, double& l) {
        double red   = r / 255.0;
        double green = g / 255.0;
        double blue  = b / 255.0;

        double max = std::max({red, green, blue});
        double min = std::min({red, green, blue});

        l = (max + min) / 2.0;

        if (max == min) {
            h = s = 0.0; // achromatic
        } else {
            double delta = max - min;
            s            = l > 0.5 ? delta / (2.0 - max - min) : delta / (max + min);

            if (max == red) {
                h = (green - blue) / delta + (green < blue ? 6.0 : 0.0);
            } else if (max == green) {
                h = (blue - red) / delta + 2.0;
            } else {
                h = (red - green) / delta + 4.0;
            }
            h /= 6.0;
        }
    }

    // 计算颜色差异
    inline double colorDistance(const Color& c1, const Color& c2) {
        double rmean = (c1.r + c2.r) / 2.0;
        double r     = c1.r - c2.r;
        double g     = c1.g - c2.g;
        double b     = c1.b - c2.b;

        return std::sqrt(
            (2 + rmean / 256.0) * r * r + 4 * g * g + (2 + (255 - rmean) / 256.0) * b * b
        );
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

    inline std::string colorToHex(const Color& color) {
        return colorToHex(color.r, color.g, color.b);
    }

    class KMeansCluster {
    private:

        std::vector<Color>              centroids{};
        std::vector<std::vector<Color>> clusters{};

    public:

        void cluster(const std::vector<Color>& colors, int k, int maxIterations = 100) {
            if (colors.empty() || k <= 0) {
                return;
            }

            // 初始化质心
            initializeCentroids(colors, k);

            for (int iter = 0; iter < maxIterations; ++iter) {
                // 清空聚类
                clusters.clear();
                clusters.resize(k);

                // 分配每个颜色到最近的质心
                for (const auto& color : colors) {
                    int    bestCluster  = 0;
                    double bestDistance = std::numeric_limits<double>::max();

                    for (int i = 0; i < k; ++i) {
                        double distance = colorDistance(color, centroids[i]);
                        if (distance < bestDistance) {
                            bestDistance = distance;
                            bestCluster  = i;
                        }
                    }

                    // 根据count添加多个副本
                    for (int j = 0; j < color.count; ++j) {
                        clusters[bestCluster].push_back(color);
                    }
                }

                // 更新质心
                std::vector<Color> newCentroids{};
                bool               changed = false;

                for (int i = 0; i < k; ++i) {
                    if (clusters[i].empty()) {
                        newCentroids.push_back(centroids[i]);
                        continue;
                    }

                    double totalR = 0, totalG = 0, totalB = 0;
                    int    totalCount = 0;

                    for (const auto& color : clusters[i]) {
                        totalR += color.r;
                        totalG += color.g;
                        totalB += color.b;
                        totalCount++;
                    }

                    auto newCentroid = Color{
                        static_cast<uint8_t>(totalR / totalCount),
                        static_cast<uint8_t>(totalG / totalCount),
                        static_cast<uint8_t>(totalB / totalCount),
                        1,
                        int(calculateBrightness(
                            static_cast<uint8_t>(totalR / totalCount),
                            static_cast<uint8_t>(totalG / totalCount),
                            static_cast<uint8_t>(totalB / totalCount)
                        ))
                    };

                    if (colorDistance(newCentroid, centroids[i]) > 1.0) {
                        changed = true;
                    }

                    newCentroids.push_back(newCentroid);
                }

                centroids = newCentroids;

                if (!changed) {
                    break;
                }
            }
        }

        const std::vector<Color>& getCentroids() const {
            return centroids;
        }

        const std::vector<std::vector<Color>>& getClusters() const {
            return clusters;
        }

    private:

        void initializeCentroids(const std::vector<Color>& colors, int k) {
            centroids.clear();

            if (colors.empty()) {
                return;
            }

            // 随机选择初始化质心
            std::random_device              rd{};
            std::mt19937                    gen{rd()};
            std::uniform_int_distribution<> dis{0, int(colors.size() - 1)};

            for (int i = 0; i < k; ++i) {
                centroids.push_back(colors[dis(gen)]);
            }
        }
    };

    class GradientColorAnalyzer {
    private:

        Color              primaryColor;
        std::vector<Color> dominantColors;

    public:

        struct AnalysisResult {
            Color              primary;
            std::vector<Color> allDominant;
        };

        AnalysisResult analyzeForGradient(const std::vector<Color>& colors, int numClusters = 12) {
            dominantColors.clear();

            // 聚类
            performWeightedClustering(colors, numClusters);

            // 提取主色调
            selectPrimaryColor();

            return AnalysisResult{primaryColor, dominantColors};
        }

    private:

        void performWeightedClustering(const std::vector<Color>& colors, int k) {
            if (colors.empty() || k <= 0) {
                return;
            }

            KMeansCluster clusterer{};
            clusterer.cluster(colors, k);

            dominantColors = clusterer.getCentroids();

            // 按视觉重要性排序（亮度、饱和度和流行度）
            std::sort(
                dominantColors.begin(),
                dominantColors.end(),
                [](const Color& a, const Color& b) {
                    return calculateVisualScore(a) > calculateVisualScore(b);
                }
            );
        }

        static double calculateVisualScore(const Color& color) {
            double h, s, l;
            rgbToHsl(color.r, color.g, color.b, h, s, l);

            // 综合考虑饱和度、亮度和颜色流行度
            double saturationScore = s * 0.4;
            // 偏好中等亮度
            double lightnessScore  = (1.0 - std::abs(l - 0.6)) * 0.3;
            double populationScore = std::log1p(color.count) * 0.3;

            return saturationScore + lightnessScore + populationScore;
        }

        void selectPrimaryColor() {
            if (dominantColors.empty()) {
                primaryColor = Color{0, 0, 0, 1, 128};
                return;
            }

            // 选择视觉评分最高的颜色作为主色调
            primaryColor = dominantColors[0];
        }
    };

    /// [dataSize] 如果指定 dataSize == 0，则不检查；否则应当比需要遍历的宽高乘积数据大
    inline std::shared_ptr<AnalysePictureColorResult> analysePictureColorFromDecodedData(
        const uint8_t* data,
        size_t         dataSize,
        int            width,
        int            height,
        int            lineSize,
        int            itemSize = 3
    ) {
        assert(lineSize >= width * itemSize);
        assert((dataSize == 0 || dataSize >= size_t(height * lineSize)));
        assert(itemSize >= 3);
        // 统计颜色
        LXX_DEBEG("analyse color...");
        // key: RGB值(0xRRGGBB), value: 颜色信息
        std::map<uint32_t, Color> colorMap{};

        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                const uint8_t* pixel = data + y * lineSize + x * itemSize;
                const uint8_t  r     = pixel[0];
                const uint8_t  g     = pixel[1];
                const uint8_t  b     = pixel[2];

                uint32_t key        = (r << 16) | (g << 8) | b;
                int      brightness = int(calculateBrightness(r, g, b));

                if (colorMap.find(key) != colorMap.end()) {
                    colorMap[key].count++;
                } else {
                    colorMap[key] = {r, g, b, 1, brightness};
                }
            }
        }

        // 排序
        std::vector<Color> allColors{};
        allColors.reserve(colorMap.size());
        for (const auto& pair : colorMap) {
            allColors.push_back(pair.second);
        }
        // 分类主色调、亮色调、暗色调
        if (!allColors.empty()) {
            GradientColorAnalyzer analyzer{};
            auto                  analyseResult = analyzer.analyzeForGradient(allColors, 8);

            auto result       = std::make_shared<AnalysePictureColorResult>();
            result->mainColor = analyseResult.primary;
            int index         = 0;
            int lightIndex    = 0;
            int darkIndex     = 0;
            for (const auto& color : analyseResult.allDominant) {
                if (index < int(result->dominantColors.size())) {
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
            return result;
        }
        return nullptr;
    }

    inline std::shared_ptr<AnalysePictureColorResult>
        analysePictureColor(AVFormatContext* formatCtx, analyse_tool::AnalyseLogItem_c& logItem) {
        LXX_DEBEG("analysePictureColor: ");
        int ret = avformat_find_stream_info(formatCtx, nullptr);
        if (ret < 0) {
            logItem.setLog(
                "avformat_find_stream_info: 无法获取流信息 | {}",
                utilxx::av_err2str(ret)
            );
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
            logItem.setLog("未找到视频流");
            avformat_close_input(&formatCtx);
            return nullptr;
        }

        LXX_DEBEG("decoder picture: ");
        auto codecPar = formatCtx->streams[videoStreamIndex]->codecpar;
        auto codec    = avcodec_find_decoder(codecPar->codec_id);
        if (!codec) {
            logItem.setLog("无法找到解码器");
            avformat_close_input(&formatCtx);
            return nullptr;
        }

        AVCodecContext* codecCtx = avcodec_alloc_context3(codec);
        if (!codecCtx) {
            logItem.setLog("无法分配解码器上下文");
            avformat_close_input(&formatCtx);
            return nullptr;
        }
        ret = avcodec_parameters_to_context(codecCtx, codecPar);
        if (ret < 0) {
            logItem.setLog(
                "avcodec_parameters_to_context: 解码器参数复制到上下文失败 | {}",
                utilxx::av_err2str(ret)
            );
            avcodec_free_context(&codecCtx);
            avformat_close_input(&formatCtx);
            return nullptr;
        }

        ret = avcodec_open2(codecCtx, codec, nullptr);
        if (ret < 0) {
            logItem.setLog("avcodec_open2: 打开解码器失败 | {}", utilxx::av_err2str(ret));
            avcodec_free_context(&codecCtx);
            avformat_close_input(&formatCtx);
            return nullptr;
        }
        LXX_DEBEG("decoder picture...");

        // 解码图片
        AVFrame*  frame    = av_frame_alloc();
        AVFrame*  rgbFrame = av_frame_alloc();
        AVPacket* pkt      = av_packet_alloc();

        bool frameDecoded = false;
        while ((ret = av_read_frame(formatCtx, pkt)) >= 0) {
            if (pkt->stream_index == videoStreamIndex) {
                ret = avcodec_send_packet(codecCtx, pkt);
                if (ret < 0) {
                    logItem.setLog(
                        "avcodec_send_packet: 发送数据包失败 | {}",
                        utilxx::av_err2str(ret)
                    );
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
            logItem.setLog("解码图片失败");
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
            logItem.setLog("无法创建颜色转换上下文");
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
        auto result = analysePictureColorFromDecodedData(
            rgbFrame->data[0],
            0,
            codecCtx->width,
            codecCtx->height,
            rgbFrame->linesize[0]
        );

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

        constexpr size_t avio_buffer_size = 4096;
        uint8_t*         avio_buffer      = (uint8_t*)av_malloc(avio_buffer_size);
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
        int ret       = avformat_open_input(&formatCtx, NULL, NULL, NULL);
        if (ret != 0) {
            logItem.setLog("avformat_open_input: 无法打开数据 | {}", utilxx::av_err2str(ret));
            avformat_free_context(formatCtx);
            avio_context_free(&avioCtx);
            return nullptr;
        }

        return analysePictureColor(formatCtx, logItem);
    }

    inline std::shared_ptr<AnalysePictureColorResult> analysePictureColorFromPath(
        const char*                     picturePath,
        analyse_tool::AnalyseLogItem_c& logItem
    ) {
        AVFormatContext* formatCtx = nullptr;
        auto             ret       = avformat_open_input(&formatCtx, picturePath, nullptr, nullptr);
        if (ret != 0) {
            logItem.setLog("avformat_open_input: 无法打开文件 | {}", utilxx::av_err2str(ret));
            return nullptr;
        }
        return analysePictureColor(formatCtx, logItem);
    }
}; // namespace analyse_tool