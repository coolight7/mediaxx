#pragma once

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavcodec/codec_par.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/bprint.h"
#include "libavutil/dict.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libswscale/swscale.h"
}

#include "simdjson.h"
#include "util/json_helper.h"
#include "util/log.h"
#include "util/string_util.h"
#include "util/utilxx.h"
#include <algorithm>
#include <filesystem>
#include <format>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>

// 常见图片格式的文件签名
struct SignatureInfo {
    const uint8_t* signature;
    size_t         length;
    AVCodecID      codec_id;
    const char*    description;
};

inline static const SignatureInfo cImgSignatureTable[] = {
    {(const uint8_t*)"\xFF\xD8\xFF",      3, AV_CODEC_ID_MJPEG, "JPEG"                },
    {(const uint8_t*)"\x89PNG\r\n\x1A\n", 8, AV_CODEC_ID_PNG,   "PNG"                 },
    {(const uint8_t*)"GIF8",              4, AV_CODEC_ID_GIF,   "GIF"                 },
    {(const uint8_t*)"BM",                2, AV_CODEC_ID_BMP,   "BMP"                 },
    {(const uint8_t*)"II*\x00",           4, AV_CODEC_ID_TIFF,  "TIFF (little endian)"},
    {(const uint8_t*)"MM\x00*",           4, AV_CODEC_ID_TIFF,  "TIFF (big endian)"   },
    {nullptr,                             0, AV_CODEC_ID_NONE,  nullptr               }  // 结束标记
};

class MediaInfoItem_c {
public:

    inline static const auto cDefUserAgent = std::string_view{
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.6261.95 Safari/537.36"
    };

    const std::string filepath;
    char**            log;
    size_t            logNum  = 0;
    AVFormatContext*  fmtCtx  = nullptr;
    AVDictionary*     options = nullptr;

    MediaInfoItem_c(const std::string_view in_filepath, char** in_log) :
        filepath(in_filepath),
        log(in_log) {
        // log 容器非空，但内容為空
        assert(nullptr != log && nullptr == *log);
    }

    void setLog(const std::string_view data) {
        ++logNum;
        auto temp = *log;
        if (nullptr == temp) {
            *log = StringUtilxx_c::stringCopyMalloc(data);
        } else {
            // 已经有内容，附加
            *log = StringUtilxx_c::stringCopyMalloc(*log, "\n\n", data);
        }
        mediaxx_free(temp);
    }

    void setOptions(const std::string_view headers) {
        av_dict_set(&options, "timeout", "30000000", 0);
        av_dict_set(&options, "tls_verify", "0", 0);
        av_dict_set(&options, "verify", "0", 0);
        av_dict_set(&options, "max_redirects", "5", 0);
        av_dict_set(&options, "follow_redirects", "1", 0);
        av_dict_set(&options, "chunked_post", "0", 0);
        // 微秒，限制分析时长
        av_dict_set(&options, "analyzeduration", "1000000", 0);
        av_dict_set(&options, "probesize", "5000000", 0);
        if (false == headers.empty()) {
            av_dict_set(&options, "headers", headers.data(), 0);
        }
    }

    void dispose() {
        avformat_close_input(&fmtCtx);
        av_dict_free(&options);
    }
};

class MediaInfoReader_c {
protected:
public:

    static MediaInfoReader_c instance;

    MediaInfoReader_c() {
        avformat_network_init();
    }

    ~MediaInfoReader_c() {
        avformat_network_deinit();
    }

    bool openFile(MediaInfoItem_c& item, const std::string_view headers) {
        if (item.filepath.empty()) {
            item.setLog("缺少文件路径");
            return false;
        }

        item.setOptions(headers);
        LXX_DEBEG("openFile ...... : {}", item.filepath);
        int ret = avformat_open_input(&item.fmtCtx, item.filepath.c_str(), nullptr, &item.options);
        if (ret != 0) {
            item.setLog(
                std::format("无法打开文件: {}, 错误: {}", item.filepath, Utilxx_c::av_err2str(ret))
            );
            return false;
        }

        LXX_DEBEG("openFile | find info ...... : {}", item.filepath);
        ret = avformat_find_stream_info(item.fmtCtx, nullptr);
        if (ret < 0) {
            item.setLog(std::format("无法获取流信息: {}", item.filepath, Utilxx_c::av_err2str(ret))
            );
            return false;
        }

        LXX_DEBEG("openFile success: {}", item.filepath);
        return true;
    }

    simdjson::builder::string_builder toInfoMap(MediaInfoItem_c& item) {
        LXX_DEBEG("toInfoMap ......");
        simdjson::builder::string_builder result{};

        auto const fmtCtx = item.fmtCtx;
        if (!fmtCtx) {
            item.setLog("未打开文件");
            return result;
        }
        result.start_object();

        result.escape_and_append_with_quotes("format");
        result.append_colon();
        {
            result.start_object();

            result.append_key_value<"filename">(item.filepath);
            result.append_comma();
            strBuilderAppendFixdKeyVPtr_d(result, "format_name", fmtCtx->iformat->name);
            result.append_key_value<"nb_streams">(fmtCtx->nb_streams);
            result.append_comma();
            result.append_key_value<"nb_programs">(fmtCtx->nb_programs);
            result.append_comma();
            result.append_key_value<"nb_stream_groups">(fmtCtx->nb_stream_groups);
            result.append_comma();
            result.append_key_value<"start_time">(fmtCtx->start_time);
            result.append_comma();
            result.append_key_value<"duration">(
                fmtCtx->duration != AV_NOPTS_VALUE ? double(fmtCtx->duration) / AV_TIME_BASE : 0
            );
            result.append_comma();
            if (nullptr != fmtCtx->pb) {
                result.append_key_value<"size">(avio_size(fmtCtx->pb));
                result.append_comma();
            }
            result.append_key_value<"bit_rate">(fmtCtx->bit_rate);
            result.append_comma();
            result.append_key_value<"probe_score">(fmtCtx->probe_score);
            result.append_comma();

            result.escape_and_append_with_quotes("tags");
            result.append_colon();
            result.start_object();
            auto ctxMetadata = fmtCtx->metadata;
            if (nullptr != ctxMetadata) {
                AVDictionaryEntry* tag     = nullptr;
                auto               isFirst = true;
                while ((tag = av_dict_get(ctxMetadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
                    if (nullptr != tag && nullptr != tag->key && nullptr != tag->value) {
                        if (false == isFirst) {
                            result.append_comma();
                        }
                        isFirst = false;
                        result.append_key_value(tag->key, tag->value);
                    }
                }
            }
            result.end_object();

            result.end_object();
        }
        result.append_comma();

        result.escape_and_append_with_quotes("streams");
        result.append_colon();
        {
            result.start_array();
            for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
                result.start_object();
                AVStream*          stream   = fmtCtx->streams[i];
                AVCodecParameters* codecPar = stream->codecpar;

                result.append_key_value<"index">(i);
                result.append_comma();
                result.append_key_value<"codec_id">(int(codecPar->codec_id));
                result.append_comma();
                strBuilderAppendFixdKeyVPtr_d(
                    result,
                    "codec_name",
                    avcodec_get_name(codecPar->codec_id)
                );
                auto avdesc = avcodec_descriptor_get(codecPar->codec_id);
                if (nullptr != avdesc) {
                    strBuilderAppendFixdKeyVPtr_d(result, "codec_long_name", avdesc->long_name);
                }
                strBuilderAppendFixdKeyVPtr_d(
                    result,
                    "codec_type",
                    av_get_media_type_string(codecPar->codec_type)
                );
                result.append_key_value<"codec_tag">(codecPar->codec_tag);
                result.append_comma();
                result.append_key_value<"bit_rate">(codecPar->bit_rate);
                result.append_comma();
                result.append_key_value<"bits_per_sample">(codecPar->bits_per_raw_sample);
                result.append_comma();
                result.append_key_value<"start_time">(stream->start_time);
                result.append_comma();
                result.append_key_value<"r_frame_rate">(
                    std::format("{}/{}", stream->r_frame_rate.num, stream->r_frame_rate.den)
                );
                result.append_comma();
                result.append_key_value<"avg_frame_rate">(
                    std::format("{}/{}", stream->avg_frame_rate.num, stream->avg_frame_rate.den)
                );
                result.append_comma();
                result.append_key_value<"time_base">(
                    std::format("{}/{}", stream->time_base.num, stream->time_base.den)
                );
                result.append_comma();
                if (stream->time_base.den && stream->time_base.num) {
                    // 秒
                    result.append_key_value<"duration">(
                        (stream->duration * av_q2d(stream->time_base))
                    );
                    result.append_comma();
                }
                result.escape_and_append_with_quotes("tags");
                result.append_colon();
                {
                    result.start_object();
                    auto               metadata = stream->metadata;
                    AVDictionaryEntry* tag      = nullptr;
                    auto               isFirst  = true;
                    while ((tag = av_dict_get(metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
                        if (nullptr != tag && nullptr != tag->key && nullptr != tag->value) {
                            if (false == isFirst) {
                                result.append_comma();
                            }
                            isFirst = false;
                            result.append_key_value(tag->key, tag->value);
                        }
                    }
                    result.end_object();
                }

                LXX_DEBEG(
                    "toInfoMap | append stream/metadata: {} ......",
                    int(codecPar->codec_type)
                );
                switch (codecPar->codec_type) {
                case AVMEDIA_TYPE_VIDEO:
                    {
                        result.append_comma();
                        result.append_key_value<"width">(codecPar->width);
                        result.append_comma();
                        result.append_key_value<"height">(codecPar->height);
                        result.append_comma();
                        result.append_key_value<"framerate">(
                            std::format("{}/{}", codecPar->framerate.num, codecPar->framerate.den)
                        );
                        result.append_comma();
                        result.append_key_value<"sample_aspect_ratio">(std::format(
                            "{}/{}",
                            stream->sample_aspect_ratio.num,
                            stream->sample_aspect_ratio.den
                        ));
                        result.append_comma();
                        strBuilderAppendFixdKeyVPtr_d(
                            result,
                            "color_range",
                            av_color_range_name(codecPar->color_range)
                        );
                        strBuilderAppendFixdKeyVPtr_d(
                            result,
                            "color_space",
                            av_color_space_name(codecPar->color_space)
                        );
                        strBuilderAppendFixdKeyVPtr_d(
                            result,
                            "chroma_location",
                            av_chroma_location_name(codecPar->chroma_location)
                        );
                        strBuilderAppendFixdKeyVPtr_d(
                            result,
                            "pix_fmt",
                            av_get_pix_fmt_name((AVPixelFormat)codecPar->format)
                        );
                        strBuilderAppendFixdKeyVPtr_d(
                            result,
                            "format",
                            av_get_pix_fmt_name((AVPixelFormat)codecPar->format)
                        );

                        if (stream->avg_frame_rate.den && stream->avg_frame_rate.num) {
                            // 计算帧率
                            double fps = av_q2d(stream->avg_frame_rate);
                            result.append_key_value<"fps">(fps);
                            result.append_comma();
                        }
                        // if 块不能结尾，否则可能多余逗号 [result.append_comma()]
                        result.append_key_value<"level">(codecPar->level);
                    }
                    break;
                case AVMEDIA_TYPE_AUDIO:
                    {
                        result.append_comma();
                        result.append_key_value<"sample_rate">(codecPar->sample_rate);
                        result.append_comma();
                        result.append_key_value<"channels">(codecPar->ch_layout.nb_channels);
                        result.append_comma();
                        {
                            AVBPrint bp{};
                            av_bprint_init(&bp, 0, AV_BPRINT_SIZE_UNLIMITED);
                            av_channel_layout_describe_bprint(&codecPar->ch_layout, &bp);
                            char* channel_name = nullptr;
                            av_bprint_finalize(&bp, &channel_name);
                            strBuilderAppendFixdKeyVPtr_d(result, "channel_layout", channel_name);
                            av_free(channel_name);
                        }
                        strBuilderAppendFixdKeyVPtr_d(
                            result,
                            "sample_fmt",
                            av_get_sample_fmt_name((AVSampleFormat)codecPar->format)
                        );
                        strBuilderAppendFixdKeyVPtr_d(
                            result,
                            "format",
                            av_get_sample_fmt_name((AVSampleFormat)codecPar->format)
                        );
                        result.append_key_value<"initial_padding">(codecPar->initial_padding);
                        result.append_comma();
                        result.append_key_value<"trailing_padding">(codecPar->trailing_padding);
                        // result.append_comma();
                    }
                    break;
                default:
                    break;
                }

                result.end_object();
                if (i < fmtCtx->nb_streams - 1) {
                    result.append_comma();
                }
            }
            result.end_array();
            // result.append_comma();
        }

        result.end_object();
        return result;
    }

    int convertQualityToQscale(int quality) {
        if (quality < 0)
            quality = 0;
        if (quality > 100)
            quality = 100;
        return (int)(2 + (100 - quality) * 29 / 100.0 + 0.5);
    }

    int savePicture(
        MediaInfoItem_c&       item,
        const std::string_view outputStr,
        const std::string_view output96Str
    ) {
        LXX_DEBEG("savePicture: {}", item.filepath);
        auto const fmtCtx = item.fmtCtx;
        if (!fmtCtx) {
            item.setLog("未打开文件");
            return 0;
        }
        for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
            AVStream*          stream   = fmtCtx->streams[i];
            AVCodecParameters* codecPar = stream->codecpar;
            switch (codecPar->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                return tryGetPicture(item, stream, outputStr, output96Str);
            case AVMEDIA_TYPE_AUDIO:
            default:
                break;
            }
        }
        return 0;
    }

    // 将AVFrame保存为JPEG文件
    bool saveFrameAsJPEG(
        MediaInfoItem_c&             item,
        AVFrame*                     frame,
        const std::filesystem::path& outputPath,
        int                          clipWidth  = -1,
        int                          clipHeight = -1,
        int                          quality    = 2
    ) {
        const AVCodec*  encoder   = NULL;
        AVCodecContext* encodeCtx = NULL;
        AVPacket*       pkt       = NULL;
        bool            result    = false;

        do {
            // 查找JPEG编码器
            encoder = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
            if (!encoder) {
                item.setLog("未找到JPEG编码器");
                return false;
            }

            // 创建编码器上下文
            encodeCtx = avcodec_alloc_context3(encoder);
            if (!encodeCtx) {
                item.setLog("无法创建编码器上下文");
                return false;
            }

            // 设置编码参数
            encodeCtx->width  = (clipWidth > 0) ? clipWidth : frame->width;
            encodeCtx->height = (clipHeight > 0) ? clipHeight : frame->height;
            {
                auto maxLine = std::max(encodeCtx->width, encodeCtx->height);
                if (maxLine <= 0) {
                    item.setLog(std::format(
                        "saveFrameAsJPEG: encodeCtx/maxLine <= 0: {}, reset to 96",
                        maxLine
                    ));
                    maxLine = 96;
                }
                if (encodeCtx->width <= 0) {
                    item.setLog(std::format(
                        "saveFrameAsJPEG: encodeCtx->width <= 0: {}, reset to maxLine: {}",
                        encodeCtx->width,
                        maxLine
                    ));
                    encodeCtx->width = maxLine;
                }
                if (encodeCtx->height <= 0) {
                    item.setLog(std::format(
                        "saveFrameAsJPEG: encodeCtx->height <= 0: {}, reset to maxLine: {}",
                        encodeCtx->height,
                        maxLine
                    ));
                    encodeCtx->height = maxLine;
                }
            }

            encodeCtx->codec_type  = AVMediaType ::AVMEDIA_TYPE_VIDEO;
            encodeCtx->pix_fmt     = AVPixelFormat::AV_PIX_FMT_YUV420P;
            encodeCtx->color_range = AVColorRange::AVCOL_RANGE_JPEG;
            encodeCtx->time_base   = AVRational{1, 25}; // 对于单帧不重要
            encodeCtx->framerate   = AVRational{25, 1};
            // 设置JPEG质量
            encodeCtx->global_quality = quality;
            // 启用固定质量模式
            encodeCtx->flags |= AV_CODEC_FLAG_QSCALE;

            // 打开编码器
            auto ret = avcodec_open2(encodeCtx, encoder, NULL);
            if (ret != 0) {
                item.setLog(std::format("无法打开JPEG编码器: {}/{}", ret, Utilxx_c::av_err2str(ret))
                );
                result = false;
                break;
            }

            // 创建包
            pkt = av_packet_alloc();
            if (!pkt) {
                item.setLog("无法创建AVPacket");
                result = false;
                break;
            }

            // 发送帧到编码器
            ret = avcodec_send_frame(encodeCtx, frame);
            if (ret != 0) {
                item.setLog(std::format("发送帧到编码器失败: {}/{}", ret, Utilxx_c::av_err2str(ret))
                );
                result = false;
                break;
            }

            // 接收编码后的包
            ret = avcodec_receive_packet(encodeCtx, pkt);
            if (ret != 0) {
                item.setLog(std::format("从编码器接收包失败: {}/{}", ret, Utilxx_c::av_err2str(ret))
                );
                result = false;
                break;
            }

            // 写入文件
            std::ofstream file{outputPath, std::ios::binary};
            if (file.is_open()) {
                file.write(reinterpret_cast<const char*>(pkt->data), pkt->size);
                file.close();
                result = true;
            } else {
                item.setLog(std::format("输出文件打开失败: {}", outputPath.generic_string()));
                result = true;
            }
        } while (false);

        av_packet_free(&pkt);
        avcodec_free_context(&encodeCtx);

        return result;
    }

    // 缩放并保存帧为JPEG
    bool saveFrameAsJPEGWithScale(
        MediaInfoItem_c&             item,
        AVFrame*                     frame,
        const std::filesystem::path& outputPath,
        int                          targetMinLineSize,
        int                          quality = 2
    ) {
        struct SwsContext* swsCtx      = NULL;
        AVFrame*           scaledFrame = NULL;
        bool               result      = false;

        // 计算缩放后的尺寸（保持宽高比）
        int srcWidth   = frame->width;
        int srcHeight  = frame->height;
        int srcMinLine = std::min(srcWidth, srcHeight);
        if (srcMinLine <= 0) {
            item.setLog(std::format(
                "saveFrameAsJPEGWithScale: src 最小宽度 <=0 : w {} / h {}",
                srcWidth,
                srcHeight
            ));
            srcMinLine = -1;
        }
        double scalePercent = (double)targetMinLineSize / srcMinLine;
        LXX_DEBEG(
            "saveFrameAsJPEGWithScale: quality {} | scale to {} %",
            quality,
            scalePercent * 100
        );

        // 如果不需要缩放，直接保存
        if (scalePercent <= 0 || scalePercent >= 1) {
            return saveFrameAsJPEG(item, frame, outputPath, -1, -1, quality);
        }

        do {
            auto targetWidth  = (int)((double)srcWidth * scalePercent);
            auto targetHeight = (int)((double)srcHeight * scalePercent);
            if (targetHeight <= 0 || targetWidth <= 0) {
                item.setLog(std::format(
                    "saveFrameAsJPEGWithScale: target 最小宽度 <=0 : w {} / h {} / scale {}",
                    srcWidth,
                    srcHeight,
                    scalePercent
                ));
            }

            // 创建缩放上下文
            swsCtx = sws_getContext(
                srcWidth,
                srcHeight,
                (AVPixelFormat)frame->format,
                targetWidth,
                targetHeight,
                AVPixelFormat::AV_PIX_FMT_YUV420P,
                SWS_BILINEAR, // 平衡速度和质量
                NULL,
                NULL,
                NULL
            );

            if (!swsCtx) {
                item.setLog("无法创建缩放上下文");
                result = false;
                break;
            }

            // 创建缩放后的帧
            scaledFrame = av_frame_alloc();
            if (!scaledFrame) {
                item.setLog("无法创建缩放帧");
                result = false;
                break;
            }

            scaledFrame->width       = targetWidth;
            scaledFrame->height      = targetHeight;
            scaledFrame->format      = AVPixelFormat::AV_PIX_FMT_YUV420P;
            scaledFrame->color_range = AVColorRange::AVCOL_RANGE_JPEG;

            // 为缩放帧分配缓冲区
            if (0 != av_frame_get_buffer(scaledFrame, 0)) {
                item.setLog("无法为缩放帧分配缓冲区");
                break;
            }
            LXX_DEBEG(
                "saveFrameAsJPEGWithScale: format {} | {}",
                frame->format,
                scaledFrame->format
            );
            // 执行缩放
            auto reHeight = sws_scale(
                swsCtx,
                (const uint8_t* const*)frame->data,
                frame->linesize,
                0,
                srcHeight,
                scaledFrame->data,
                scaledFrame->linesize
            );
            LXX_DEBEG(
                "saveFrameAsJPEGWithScale: scale result: reheight {} | fw {} | fh {}",
                reHeight,
                scaledFrame->width,
                scaledFrame->height
            );

            result = saveFrameAsJPEG(item, scaledFrame, outputPath, -1, -1, quality);
        } while (false);

        sws_freeContext(swsCtx);
        av_frame_free(&scaledFrame);

        return result;
    }

    // 像素格式转换函数
    AVFrame* convertFramePixelFormat(
        MediaInfoItem_c& item,
        AVFrame*         srcFrame,
        AVPixelFormat    dstPixFmt,
        AVColorRange     color_range
    ) {
        struct SwsContext* swsCtx   = NULL;
        AVFrame*           dstFrame = NULL;

        // 创建目标帧
        dstFrame = av_frame_alloc();
        if (!dstFrame) {
            item.setLog("无法创建目标帧");
            return NULL;
        }

        dstFrame->width       = srcFrame->width;
        dstFrame->height      = srcFrame->height;
        dstFrame->format      = dstPixFmt;
        dstFrame->color_range = color_range;

        // 分配目标帧缓冲区
        if (0 != av_frame_get_buffer(dstFrame, 0)) {
            item.setLog("无法为缩放帧分配缓冲区");
            av_frame_free(&dstFrame);
            return NULL;
        }

        // 创建转换上下文
        swsCtx = sws_getContext(
            srcFrame->width,
            srcFrame->height,
            (AVPixelFormat)srcFrame->format,
            dstFrame->width,
            dstFrame->height,
            (AVPixelFormat)dstFrame->format,
            SWS_BILINEAR,
            NULL,
            NULL,
            NULL
        );

        if (!swsCtx) {
            item.setLog("无法创建像素格式转换上下文");
            av_frame_free(&dstFrame);
            return NULL;
        }

        // 执行转换
        sws_scale(
            swsCtx,
            srcFrame->data,
            srcFrame->linesize,
            0,
            srcFrame->height,
            dstFrame->data,
            dstFrame->linesize
        );

        sws_freeContext(swsCtx);
        return dstFrame;
    }

    bool savePictureScaleByStream(
        MediaInfoItem_c&             item,
        AVPacket*                    pkt,
        AVStream*                    stream,
        const std::filesystem::path& outputPath,
        const int                    targetMinLine,
        const int                    quality    = 2,
        AVCodecID                    useCodecId = AVCodecID::AV_CODEC_ID_NONE
    ) {
        bool            result         = false;
        const bool      hasSetCodecId  = (AVCodecID::AV_CODEC_ID_NONE != useCodecId);
        bool            retryByCodecId = false;
        AVCodecContext* decCtx         = nullptr;
        AVFrame*        frame          = nullptr;
        do {
            // 查找解码器
            if (AVCodecID::AV_CODEC_ID_NONE == useCodecId) {
                useCodecId = stream->codecpar->codec_id;
            }
            LXX_DEBEG("savePictureScaleByStream: try decoder: {}", int(useCodecId));
            const AVCodec* decoder = avcodec_find_decoder(useCodecId);
            if (!decoder) {
                item.setLog(std::format("找不到解码器: {}", int(useCodecId)));
                retryByCodecId = true;
                result         = false;
                break;
            }

            // 初始化解码器上下文
            decCtx = avcodec_alloc_context3(decoder);
            if (!decCtx) {
                item.setLog(std::format("无法分配解码器上下文: {}", int(useCodecId)));
                result = false;
                break;
            }

            // 复制流参数到解码器上下文
            int ret = avcodec_parameters_to_context(decCtx, stream->codecpar);
            if (ret < 0) {
                item.setLog(std::format("复制流参数失败: {}/{}", ret, Utilxx_c::av_err2str(ret)));
                result = false;
                break;
            }
            if (hasSetCodecId) {
                // 明确指定解码器时
                // [decCtx] 从 [stream] 拷贝来的参数可能是错误的
                // 重置为解码器的参数
                decCtx->codec_id = decoder->id;
            }

            // 打开解码器
            ret = avcodec_open2(decCtx, decoder, nullptr);
            if (ret != 0) {
                item.setLog(std::format("无法打开解码器: {}/{}", ret, Utilxx_c::av_err2str(ret)));
                retryByCodecId = true;
                result         = false;
                break;
            }

            // 分配解码帧
            frame = av_frame_alloc();
            if (!frame) {
                item.setLog("无法分配解码帧");
                result = false;
                break;
            }

            // 发送数据包到解码器
            ret = avcodec_send_packet(decCtx, pkt);
            if (ret != 0) {
                item.setLog(
                    std::format("发送数据包到解码器失败: {}/{}", ret, Utilxx_c::av_err2str(ret))
                );
                retryByCodecId = true;
                result         = false;
                break;
            }

            // 接收解码后的帧（封面通常只有一帧）
            ret = avcodec_receive_frame(decCtx, frame);
            if (ret != 0) {
                item.setLog(std::format("接收解码帧失败: {}/{}", ret, Utilxx_c::av_err2str(ret)));
                retryByCodecId = true;
                result         = false;
                break;
            }

            result = saveFrameAsJPEGWithScale(item, frame, outputPath, targetMinLine, quality);
        } while (false);

        avcodec_free_context(&decCtx);
        av_frame_free(&frame);

        if (false == result && false == hasSetCodecId && retryByCodecId) {
            // 尝试寻找其他编码器
            const auto newId = findDecoderBySignature(pkt->data, pkt->size);
            if (AVCodecID::AV_CODEC_ID_NONE != newId && newId != useCodecId) {
                return savePictureScaleByStream(
                    item,
                    pkt,
                    stream,
                    outputPath,
                    targetMinLine,
                    quality,
                    newId
                );
            }
        }
        return result;
    }

    int tryGetPicture(
        MediaInfoItem_c&       item,
        AVStream*              stream,
        const std::string_view outputStr,
        const std::string_view output96Str
    ) {
        if (outputStr.empty()) {
            item.setLog("缺少输出路径");
            return 0;
        }
        auto const fmtCtx = item.fmtCtx;

        int             result       = 0;
        const AVCodec*  decoder      = nullptr;
        AVCodecContext* decodeCtx    = nullptr;
        auto            outputPath   = std::filesystem::path(outputStr);
        auto            outputPath96 = std::filesystem::path(output96Str);
        do {
            // 提取图片封面
            if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                LXX_DEBEG("tryGetPicture: ATTACHED_PIC");
                AVPacket pkt = stream->attached_pic;

                std::ofstream file{outputPath, std::ios::binary};
                if (file.is_open()) {
                    file.write(reinterpret_cast<const char*>(pkt.data), pkt.size);
                    file.close();
                    result = 1;
                    if (false == output96Str.empty()) {
                        LXX_DEBEG("tryGetPicture: savePictureScaleByStream-96");
                        if (savePictureScaleByStream(item, &pkt, stream, outputPath96, 96, 8)) {
                            result = 2;
                        }
                    }
                } else {
                    item.setLog(std::format("输出文件打开失败: {}", outputStr));
                    result = 0;
                }
                break;
            }

            // 提取视频封面
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                LXX_DEBEG("tryGetPicture: VIDEO");
                decoder = avcodec_find_decoder(stream->codecpar->codec_id);
                if (!decoder) {
                    item.setLog("未找到解码器");
                    break;
                }

                decodeCtx = avcodec_alloc_context3(decoder);
                avcodec_parameters_to_context(decodeCtx, stream->codecpar);

                // 打开解码器
                if (avcodec_open2(decodeCtx, decoder, NULL) != 0) {
                    item.setLog("无法打开解码器");
                    break;
                }

                // 解码帧
                AVPacket* pkt         = av_packet_alloc();
                AVFrame*  frame       = av_frame_alloc();
                AVFrame*  jpegFrame   = nullptr;
                AVFrame*  targetFrame = nullptr;

                while (av_read_frame(fmtCtx, pkt) == 0) {
                    if (pkt->stream_index == stream->index) {
                        if (avcodec_send_packet(decodeCtx, pkt) != 0) {
                            break;
                        }

                        if (avcodec_receive_frame(decodeCtx, frame) == 0) {
                            targetFrame = av_frame_clone(frame);
                            av_frame_unref(frame);
                            av_packet_unref(pkt);
                            break;
                        }
                    }
                    av_packet_unref(pkt);
                }

                if (targetFrame) {
                    // 如果像素格式不是YUVJ420P，进行转换
                    if (targetFrame->format != AVPixelFormat::AV_PIX_FMT_YUV420P) {
                        jpegFrame = convertFramePixelFormat(
                            item,
                            targetFrame,
                            AVPixelFormat::AV_PIX_FMT_YUV420P,
                            AVColorRange::AVCOL_RANGE_JPEG
                        );

                        if (!jpegFrame) {
                            item.setLog(std::format(
                                "转换像素格式从 {} 到 YUVJ420P 失败",
                                targetFrame->format
                            ));
                            break;
                        }
                    }

                    AVFrame* useFrame = targetFrame;
                    if (nullptr != jpegFrame) {
                        useFrame = jpegFrame;
                    }

                    if (saveFrameAsJPEG(item, useFrame, outputPath, -1, -1, 2)) {
                        result = 1;
                        if (false == output96Str.empty()) {
                            if (saveFrameAsJPEGWithScale(item, useFrame, outputPath96, 96, 8)) {
                                result = 2;
                            }
                        }
                    }
                }
                av_frame_free(&targetFrame);
                av_frame_free(&jpegFrame);
                av_frame_free(&frame);
                av_packet_free(&pkt);
                break;
            }
        } while (false);

        avcodec_free_context(&decodeCtx);
        return result;
    }

public:

    AVCodecID findDecoderBySignature(const uint8_t* data, size_t size) {
        LXX_DEBEG("尝试根据数据头特征[signature]寻找解码器");
        if (!data || size < 8) {
            return AVCodecID::AV_CODEC_ID_NONE;
        }

        for (const SignatureInfo* info = cImgSignatureTable; info->signature != nullptr; ++info) {
            if (size >= info->length && memcmp(data, info->signature, info->length) == 0) {
                LXX_DEBEG(
                    "检测到文件签名匹配，尝试使用解码器: {}/{}",
                    info->description,
                    int(info->codec_id)
                );
                return info->codec_id;
            }
        }

        return AVCodecID::AV_CODEC_ID_NONE;
    }
};