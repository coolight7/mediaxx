#pragma once

extern "C" {
#include "libavcodec/avcodec.h"
#include "libavcodec/codec_par.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/dict.h"
#include "libavutil/log.h"
#include "libavutil/opt.h"
#include "libavutil/pixdesc.h"
#include "libswscale/swscale.h"
}

#include "simdjson.h"
#include "util/ffmpeg_ext.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <string>
#include <string_view>

class MediaInfoItem_c {
public:

    inline static const auto cDefUserAgent = std::string_view{
        "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.6261.95 Safari/537.36"
    };

    const std::string filepath;
    AVFormatContext*  fmtCtx  = nullptr;
    AVDictionary*     options = nullptr;

    MediaInfoItem_c(std::string_view in_filepath) :
        filepath(in_filepath) {}

    void setOptions() {
        av_dict_set(&options, "timeout", "30000000", 0);
        av_dict_set(&options, "tls_verify", "0", 0);
        av_dict_set(&options, "verify", "0", 0);
        av_dict_set(&options, "max_redirects", "5", 0);
        av_dict_set(&options, "follow_redirects", "1", 0);
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

    bool openFile(MediaInfoItem_c& item) {
        item.setOptions();
        int ret = avformat_open_input(
            &item.fmtCtx,
            item.filepath.c_str(),
            nullptr,
            &item.options
        );
        if (ret < 0) {
            std::cerr << "无法打开文件: " << item.filepath
                      << ", 错误: " << Utilxx_c::av_err2str(ret) << std::endl;
            return false;
        }

        ret = avformat_find_stream_info(item.fmtCtx, nullptr);
        if (ret < 0) {
            std::cerr << "无法获取流信息, 错误: " << Utilxx_c::av_err2str(ret)
                      << std::endl;
            return false;
        }

        return true;
    }

    simdjson::builder::string_builder toInfoMap(AVFormatContext* fmtCtx) {
        simdjson::builder::string_builder result{};

        if (!fmtCtx) {
            return result;
        }
        result.start_object();
        result.append_key_value<"format">(
            fmtCtx->iformat->name ? fmtCtx->iformat->name : ""
        );
        result.append_comma();
        result.append_key_value<"duration">(
            fmtCtx->duration != AV_NOPTS_VALUE
                ? double(fmtCtx->duration) / AV_TIME_BASE
                : 0
        );
        result.append_comma();
        result.append_key_value<"bit_rate">(fmtCtx->bit_rate);
        result.append_comma();

        result.escape_and_append_with_quotes("streams");
        result.append_colon();
        {
            result.start_array();
            for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
                result.start_object();
                AVStream*          stream   = fmtCtx->streams[i];
                AVCodecParameters* codecPar = stream->codecpar;

                result.append_key_value<"codec_type">(
                    av_get_media_type_string(codecPar->codec_type)
                );
                result.append_comma();

                result.append_key_value<"codec_id">(
                    avcodec_get_name(codecPar->codec_id)
                );
                result.append_comma();

                result.escape_and_append_with_quotes("metadata");
                result.append_colon();
                {
                    result.start_object();
                    auto               metadata = stream->metadata;
                    AVDictionaryEntry* tag      = nullptr;
                    auto               isFirst  = true;
                    while ((
                        tag
                        = av_dict_get(metadata, "", tag, AV_DICT_IGNORE_SUFFIX)
                    )) {
                        if (false == isFirst) {
                            result.append_comma();
                        }
                        isFirst = false;
                        result.append_key_value(tag->key, tag->value);
                    }
                    result.end_object();
                }

                switch (codecPar->codec_type) {
                case AVMEDIA_TYPE_VIDEO:
                    {
                        result.append_comma();
                        result.escape_and_append_with_quotes("video");
                        result.append_colon();
                        {
                            result.start_object();

                            result.append_key_value<"width">(codecPar->width);
                            result.append_comma();
                            result.append_key_value<"height">(codecPar->height);
                            result.append_comma();
                            result.append_key_value<"format">(
                                av_get_pix_fmt_name((AVPixelFormat
                                )codecPar->format)
                            );

                            if (stream->avg_frame_rate.den
                                && stream->avg_frame_rate.num) {
                                // 计算帧率
                                result.append_comma();
                                double fps = av_q2d(stream->avg_frame_rate);
                                result.append_key_value<"fps">(fps);
                            }

                            if (stream->time_base.den
                                && stream->time_base.num) {
                                result.append_comma();
                                result.append_key_value<"duration">((
                                    stream->duration * av_q2d(stream->time_base)
                                ));
                                // result.append_comma();
                            }

                            result.end_object();
                        }
                    }
                    break;
                case AVMEDIA_TYPE_AUDIO:
                    {
                        result.append_comma();
                        result.escape_and_append_with_quotes("audio");
                        result.append_colon();
                        {
                            result.start_object();

                            result.append_key_value<"sample_rate">(
                                codecPar->sample_rate
                            );
                            result.append_comma();
                            result.append_key_value<"channels">(
                                codecPar->ch_layout.nb_channels
                            );
                            result.append_comma();
                            result.append_key_value<"format">(
                                av_get_sample_fmt_name((AVSampleFormat
                                )codecPar->format)
                            );
                            if (stream->time_base.den
                                && stream->time_base.num) {
                                // 秒
                                result.append_comma();
                                result.append_key_value<"duration">((
                                    stream->duration * av_q2d(stream->time_base)
                                ));
                                // result.append_comma();
                            }

                            result.end_object();
                        }
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

    bool savePicture(
        AVFormatContext* fmtCtx,
        std::string_view outputStr,
        std::string_view output96Str
    ) {
        for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
            AVStream*          stream   = fmtCtx->streams[i];
            AVCodecParameters* codecPar = stream->codecpar;
            switch (codecPar->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                return tryGetPicture(fmtCtx, stream, outputStr, output96Str);
            case AVMEDIA_TYPE_AUDIO:
            default:
                break;
            }
        }
        return false;
    }

    void printMediaInfo(AVFormatContext* fmtCtx) {
        if (!fmtCtx) {
            std::cerr << "文件未打开" << std::endl;
            return;
        }

        // 打印格式信息
        std::cout << "=== 媒体文件信息 ===" << std::endl;
        std::cout << "格式: "
                  << (fmtCtx->iformat->name ? fmtCtx->iformat->name : "未知")
                  << std::endl;
        std::cout << "时长: "
                  << (fmtCtx->duration != AV_NOPTS_VALUE
                          ? fmtCtx->duration / AV_TIME_BASE
                          : 0)
                  << " 秒" << std::endl;
        std::cout << "比特率: " << fmtCtx->bit_rate / 1000 << " kb/s"
                  << std::endl;
        std::cout << "流数量: " << fmtCtx->nb_streams << std::endl;

        printMetadata(fmtCtx->metadata, "文件");

        for (unsigned int i = 0; i < fmtCtx->nb_streams; i++) {
            AVStream*          stream   = fmtCtx->streams[i];
            AVCodecParameters* codecPar = stream->codecpar;

            std::cout << "\n--- 流 #" << i << " ---" << std::endl;
            std::cout << "类型: "
                      << av_get_media_type_string(codecPar->codec_type)
                      << std::endl;
            std::cout << "编码: " << avcodec_get_name(codecPar->codec_id)
                      << std::endl;

            // 根据流类型打印具体信息
            switch (codecPar->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                printVideoInfo(stream, codecPar);
                tryGetPicture(
                    fmtCtx,
                    stream,
                    "./temp/output.jpg",
                    "./temp/output96.jpg"
                );
                break;
            case AVMEDIA_TYPE_AUDIO:
                printAudioInfo(stream, codecPar);
                break;
            default:
                break;
            }

            printMetadata(stream->metadata, "流");
        }
    }

    // 将AVFrame保存为JPEG文件
    int saveFrameAsJPEG(
        AVFrame*                     frame,
        const std::filesystem::path& outputPath,
        int                          clipWidth  = -1,
        int                          clipHeight = -1,
        int                          quality    = 2
    ) {
        const AVCodec*  encoder   = NULL;
        AVCodecContext* encodeCtx = NULL;
        AVPacket*       pkt       = NULL;
        int             ret       = 0;

        do {
            // 查找JPEG编码器
            encoder = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
            if (!encoder) {
                fprintf(stderr, "未找到JPEG编码器\n");
                return -1;
            }

            // 创建编码器上下文
            encodeCtx = avcodec_alloc_context3(encoder);
            if (!encodeCtx) {
                fprintf(stderr, "无法创建编码器上下文\n");
                return -1;
            }

            // 设置编码参数
            encodeCtx->width  = (clipWidth > 0) ? clipWidth : frame->width;
            encodeCtx->height = (clipHeight > 0) ? clipHeight : frame->height;
            encodeCtx->pix_fmt = AV_PIX_FMT_YUVJ420P; // JPEG使用的像素格式
            encodeCtx->time_base = AVRational{1, 25}; // 对于单帧不重要
            encodeCtx->framerate = AVRational{25, 1};
            // 设置JPEG质量 (1-100, 越高越好)
            // 2对应高质量
            av_opt_set_int(encodeCtx->priv_data, "qscale", quality, 0);

            // 打开编码器
            ret = avcodec_open2(encodeCtx, encoder, NULL);
            if (ret < 0) {
                fprintf(stderr, "无法打开JPEG编码器\n");
                break;
            }

            // 创建包
            pkt = av_packet_alloc();
            if (!pkt) {
                fprintf(stderr, "无法创建AVPacket\n");
                break;
            }

            // 发送帧到编码器
            ret = avcodec_send_frame(encodeCtx, frame);
            if (ret < 0) {
                fprintf(stderr, "发送帧到编码器失败\n");
                break;
            }

            // 接收编码后的包
            ret = avcodec_receive_packet(encodeCtx, pkt);
            if (ret < 0) {
                fprintf(stderr, "从编码器接收包失败\n");
                break;
            }

            // 写入文件
            std::ofstream file{outputPath, std::ios::binary};
            if (file.is_open()) {
                file.write(reinterpret_cast<const char*>(pkt->data), pkt->size);
                file.close();
                printf(
                    "成功保存JPEG图片: %s (大小: %d 字节)\n",
                    outputPath,
                    pkt->size
                );
            }
        } while (false);

        av_packet_free(&pkt);
        avcodec_free_context(&encodeCtx);

        return ret;
    }

    // 缩放并保存帧为JPEG
    int saveFrameAsJPEGWithScale(
        AVFrame*                     frame,
        const std::filesystem::path& outputPath,
        int                          targetWidth,
        int                          targetHeight,
        int                          quality = 2
    ) {
        struct SwsContext* swsCtx      = NULL;
        AVFrame*           scaledFrame = NULL;
        int                ret         = 0;

        // 如果不需要缩放，直接保存
        if (targetWidth <= 0 && targetHeight <= 0) {
            return saveFrameAsJPEG(frame, outputPath, -1, -1, quality);
        }

        do {
            // 计算缩放后的尺寸（保持宽高比）
            int srcWidth  = frame->width;
            int srcHeight = frame->height;

            if (targetWidth <= 0) {
                // 只指定了高度，按比例计算宽度
                targetWidth = (int)((float)srcWidth * targetHeight / srcHeight);
            } else if (targetHeight <= 0) {
                // 只指定了宽度，按比例计算高度
                targetHeight = (int)((float)srcHeight * targetWidth / srcWidth);
            }

            printf(
                "缩放图像: %dx%d -> %dx%d\n",
                srcWidth,
                srcHeight,
                targetWidth,
                targetHeight
            );

            // 创建缩放上下文
            swsCtx = sws_getContext(
                srcWidth,
                srcHeight,
                (AVPixelFormat)frame->format,
                targetWidth,
                targetHeight,
                AV_PIX_FMT_YUV420P,
                SWS_BILINEAR, // 平衡速度和质量
                NULL,
                NULL,
                NULL
            );

            if (!swsCtx) {
                fprintf(stderr, "无法创建缩放上下文\n");
                ret = -1;
                break;
            }

            // 创建缩放后的帧
            scaledFrame = av_frame_alloc();
            if (!scaledFrame) {
                fprintf(stderr, "无法创建缩放帧\n");
                ret = -1;
                break;
            }

            scaledFrame->width  = targetWidth;
            scaledFrame->height = targetHeight;
            scaledFrame->format = AV_PIX_FMT_YUVJ420P;

            // 为缩放帧分配缓冲区
            ret = av_frame_get_buffer(scaledFrame, 0);
            if (ret < 0) {
                fprintf(stderr, "无法为缩放帧分配缓冲区\n");
                break;
            }

            // 执行缩放
            sws_scale(
                swsCtx,
                (const uint8_t* const*)frame->data,
                frame->linesize,
                0,
                srcHeight,
                scaledFrame->data,
                scaledFrame->linesize
            );

            ret = saveFrameAsJPEG(scaledFrame, outputPath, -1, -1, quality);
        } while (false);

        sws_freeContext(swsCtx);
        av_frame_free(&scaledFrame);

        return ret;
    }

    // 像素格式转换函数
    AVFrame*
        convertFramePixelFormat(AVFrame* srcFrame, AVPixelFormat dstPixFmt) {
        struct SwsContext* swsCtx   = NULL;
        AVFrame*           dstFrame = NULL;

        // 创建目标帧
        dstFrame = av_frame_alloc();
        if (!dstFrame) {
            fprintf(stderr, "无法创建目标帧\n");
            return NULL;
        }

        dstFrame->width       = srcFrame->width;
        dstFrame->height      = srcFrame->height;
        dstFrame->format      = dstPixFmt;
        dstFrame->color_range = AVColorRange::AVCOL_RANGE_JPEG;

        // 分配目标帧缓冲区
        int ret = av_frame_get_buffer(dstFrame, 0);
        if (ret < 0) {
            fprintf(stderr, "无法为目标帧分配缓冲区\n");
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
            fprintf(stderr, "无法创建像素格式转换上下文\n");
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

    bool tryGetPicture(
        AVFormatContext*       fmtCtx,
        AVStream*              stream,
        const std::string_view outputStr,
        const std::string_view output96Str
    ) {
        if (outputStr.empty()) {
            return false;
        }

        int             ret          = 0;
        const AVCodec*  decoder      = nullptr;
        AVCodecContext* decodeCtx    = nullptr;
        auto            outputPath   = std::filesystem::path(outputStr);
        auto            outputPath96 = std::filesystem::path(output96Str);

        do {
            // 提取图片封面
            if (stream->disposition & AV_DISPOSITION_ATTACHED_PIC) {
                std::cout << "1 - 找到附加图片流 #" << std::endl;
                AVPacket pkt = stream->attached_pic;

                std::ofstream file(outputPath, std::ios::binary);
                if (file.is_open()) {
                    file.write(
                        reinterpret_cast<const char*>(pkt.data),
                        pkt.size
                    );
                    file.close();
                    std::cout << "成功提取封面到: " << outputPath << std::endl;
                }
                break;
            }

            // 提取视频封面
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                decoder = avcodec_find_decoder(stream->codecpar->codec_id);
                if (!decoder) {
                    fprintf(stderr, "未找到解码器\n");
                    break;
                }

                decodeCtx = avcodec_alloc_context3(decoder);
                avcodec_parameters_to_context(decodeCtx, stream->codecpar);

                // 打开解码器
                if (avcodec_open2(decodeCtx, decoder, NULL) < 0) {
                    fprintf(stderr, "无法打开解码器\n");
                    break;
                }

                // 解码帧
                AVPacket* pkt         = av_packet_alloc();
                AVFrame*  frame       = av_frame_alloc();
                AVFrame*  jpegFrame   = nullptr;
                AVFrame*  targetFrame = nullptr;

                while (av_read_frame(fmtCtx, pkt) >= 0) {
                    if (pkt->stream_index == stream->index) {
                        ret = avcodec_send_packet(decodeCtx, pkt);
                        if (ret < 0) {
                            break;
                        }

                        ret = avcodec_receive_frame(decodeCtx, frame);
                        if (ret == 0) {
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
                    if (targetFrame->format != AV_PIX_FMT_YUV420P) {
                        printf(
                            "转换像素格式从 %d 到 YUVJ420P\n",
                            targetFrame->format
                        );
                        jpegFrame = convertFramePixelFormat(
                            targetFrame,
                            AV_PIX_FMT_YUV420P
                        );

                        if (!jpegFrame) {
                            fprintf(stderr, "像素格式转换失败\n");
                            break;
                        }
                    }

                    AVFrame* useFrame = targetFrame;
                    if (nullptr != jpegFrame) {
                        useFrame = jpegFrame;
                    }

                    int ret = saveFrameAsJPEG(useFrame, outputPath, -1, -1, 2);
                    if (ret == 0) {
                        printf("成功提取并保存到: %s\n", outputPath);
                        if (false == output96Str.empty()) {
                            ret = saveFrameAsJPEGWithScale(
                                useFrame,
                                outputPath96,
                                96,
                                96,
                                8
                            );
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
        if (0 == ret) {
            return true;
        }
        return false;
    }

    void printMetadata(AVDictionary* metadata, const std::string& prefix) {
        AVDictionaryEntry* tag = nullptr;
        while ((tag = av_dict_get(metadata, "", tag, AV_DICT_IGNORE_SUFFIX))) {
            std::cout << prefix << "元数据 - " << tag->key << ": " << tag->value
                      << std::endl;
        }
    }

    void printVideoInfo(AVStream* stream, AVCodecParameters* codec_par) {
        std::cout << "分辨率: " << codec_par->width << "x" << codec_par->height
                  << std::endl;
        std::cout << "像素格式: "
                  << av_get_pix_fmt_name((AVPixelFormat)codec_par->format)
                  << std::endl;

        // 计算帧率
        if (stream->avg_frame_rate.den && stream->avg_frame_rate.num) {
            double fps = av_q2d(stream->avg_frame_rate);
            std::cout << "帧率: " << fps << " fps" << std::endl;
        }

        if (stream->time_base.den && stream->time_base.num) {
            std::cout << "时长: "
                      << (stream->duration * av_q2d(stream->time_base)) << " 秒"
                      << std::endl;
        }
    }

    void printAudioInfo(AVStream* stream, AVCodecParameters* codec_par) {
        std::cout << "采样率: " << codec_par->sample_rate << " Hz" << std::endl;
        std::cout << "声道数: " << codec_par->ch_layout.nb_channels
                  << std::endl;
        std::cout << "采样格式: "
                  << av_get_sample_fmt_name((AVSampleFormat)codec_par->format)
                  << std::endl;

        if (stream->time_base.den && stream->time_base.num) {
            std::cout << "时长: "
                      << (stream->duration * av_q2d(stream->time_base)) << " 秒"
                      << std::endl;
        }
    }
};