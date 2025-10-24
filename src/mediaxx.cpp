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

#include "mediaxx.h"
#include "util/ffmpeg_ext.h"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

class MediaInfoReader {
public:

    MediaInfoReader() {
        // 初始化FFmpeg网络库，支持HTTP协议:cite[8]
        avformat_network_init();
    }

    ~MediaInfoReader() {
        cleanup();
        avformat_network_deinit();
    }

    bool openFile(const std::string& filename) {
        // 10秒超时（单位微秒）
        av_dict_set(&options, "timeout", "30000000", 0);
        av_dict_set(&options, "tls_verify", "0", 0);
        av_dict_set(&options, "verify", "0", 0);
        av_dict_set(&options, "max_redirects", "5", 0);
        av_dict_set(&options, "follow_redirects", "1", 0);
        // 打开输入文件:cite[9]
        int ret = avformat_open_input(
            &fmt_ctx,
            filename.c_str(),
            nullptr,
            &options
        );
        if (ret < 0) {
            std::cerr << "无法打开文件: " << filename
                      << ", 错误: " << Utilxx_c::av_err2str(ret) << std::endl;
            return false;
        }

        // 获取流信息:cite[9]
        ret = avformat_find_stream_info(fmt_ctx, nullptr);
        if (ret < 0) {
            std::cerr << "无法获取流信息, 错误: " << Utilxx_c::av_err2str(ret)
                      << std::endl;
            cleanup();
            return false;
        }

        return true;
    }

    void printMediaInfo() {
        if (!fmt_ctx) {
            std::cerr << "文件未打开" << std::endl;
            return;
        }

        // 打印格式信息
        std::cout << "=== 媒体文件信息 ===" << std::endl;
        std::cout << "格式: "
                  << (fmt_ctx->iformat->name ? fmt_ctx->iformat->name : "未知")
                  << std::endl;
        std::cout << "时长: "
                  << (fmt_ctx->duration != AV_NOPTS_VALUE
                          ? fmt_ctx->duration / AV_TIME_BASE
                          : 0)
                  << " 秒" << std::endl;
        std::cout << "比特率: " << fmt_ctx->bit_rate / 1000 << " kb/s"
                  << std::endl;
        std::cout << "流数量: " << fmt_ctx->nb_streams << std::endl;

        // 打印元数据
        printMetadata(fmt_ctx->metadata, "文件");

        // 遍历所有流:cite[1]
        for (unsigned int i = 0; i < fmt_ctx->nb_streams; i++) {
            AVStream*          stream    = fmt_ctx->streams[i];
            AVCodecParameters* codec_par = stream->codecpar;

            std::cout << "\n--- 流 #" << i << " ---" << std::endl;
            std::cout << "类型: "
                      << av_get_media_type_string(codec_par->codec_type)
                      << std::endl;
            std::cout << "编码: " << avcodec_get_name(codec_par->codec_id)
                      << std::endl;

            // 根据流类型打印具体信息
            switch (codec_par->codec_type) {
            case AVMEDIA_TYPE_VIDEO:
                printVideoInfo(stream, codec_par);
                tryGetPicture(stream);
                break;
            case AVMEDIA_TYPE_AUDIO:
                printAudioInfo(stream, codec_par);
                break;
            default:
                break;
            }

            printMetadata(stream->metadata, "流");
        }
    }

private:

    AVFormatContext* fmt_ctx = nullptr;
    AVDictionary*    options = nullptr;
    std::string      userAgent
        = "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/122.0.6261.95 Safari/537.36";

    void cleanup() {
        if (fmt_ctx) {
            avformat_close_input(&fmt_ctx);
        }
        if (options) {
            av_dict_free(&options);
        }
    }

    // 将AVFrame保存为JPEG文件
    int save_frame_as_jpeg(
        AVFrame*                     frame,
        const std::filesystem::path& outputPath,
        int                          clipWidth = -1,
        int                          clipHeight    = -1,
        int                          quality   = 2
    ) {
        const AVCodec*  encoder = NULL;
        AVCodecContext* enc_ctx = NULL;
        AVPacket*       pkt     = NULL;
        int             ret     = 0;

        do {
            // 查找JPEG编码器
            encoder = avcodec_find_encoder(AV_CODEC_ID_MJPEG);
            if (!encoder) {
                fprintf(stderr, "未找到JPEG编码器\n");
                return -1;
            }

            // 创建编码器上下文
            enc_ctx = avcodec_alloc_context3(encoder);
            if (!enc_ctx) {
                fprintf(stderr, "无法创建编码器上下文\n");
                return -1;
            }

            // 设置编码参数
            enc_ctx->width   = (clipWidth > 0) ? clipWidth : frame->width;
            enc_ctx->height  = (clipHeight > 0) ? clipHeight : frame->height;
            enc_ctx->pix_fmt = AV_PIX_FMT_YUVJ420P; // JPEG使用的像素格式
            enc_ctx->time_base = AVRational{1, 25}; // 对于单帧不重要
            enc_ctx->framerate = AVRational{25, 1};
            // 设置JPEG质量 (1-100, 越高越好)
            // 2对应高质量
            av_opt_set_int(enc_ctx->priv_data, "qscale", quality, 0);

            // 打开编码器
            ret = avcodec_open2(enc_ctx, encoder, NULL);
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
            ret = avcodec_send_frame(enc_ctx, frame);
            if (ret < 0) {
                fprintf(stderr, "发送帧到编码器失败\n");
                break;
            }
            // 接收编码后的包
            ret = avcodec_receive_packet(enc_ctx, pkt);
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

        if (pkt) {
            av_packet_free(&pkt);
        }
        if (enc_ctx) {
            avcodec_free_context(&enc_ctx);
        }

        return ret;
    }

    // 缩放并保存帧为JPEG
    int save_frame_as_jpeg_with_scale(
        AVFrame*                     frame,
        const std::filesystem::path& outputPath,
        int                          targetWidth,
        int                          targetHeight,
        int                          quality = 2
    ) {
        AVCodecContext*    enc_ctx     = NULL;
        const AVCodec*     encoder     = NULL;
        AVPacket*          pkt         = NULL;
        FILE*              file        = NULL;
        struct SwsContext* sws_ctx     = NULL;
        AVFrame*           scaledFrame = NULL;
        int                ret         = 0;

        // 如果不需要缩放，直接保存
        if (targetWidth <= 0 && targetHeight <= 0) {
            return save_frame_as_jpeg(frame, outputPath, -1, -1, quality);
        }

        do {
            // 计算缩放后的尺寸（保持宽高比）
            int src_width = frame->width;
            int srcHeight = frame->height;

            if (targetWidth <= 0) {
                // 只指定了高度，按比例计算宽度
                targetWidth
                    = (int)((float)src_width * targetHeight / srcHeight);
            } else if (targetHeight <= 0) {
                // 只指定了宽度，按比例计算高度
                targetHeight
                    = (int)((float)srcHeight * targetWidth / src_width);
            }

            printf(
                "缩放图像: %dx%d -> %dx%d\n",
                src_width,
                srcHeight,
                targetWidth,
                targetHeight
            );

            // 创建缩放上下文
            sws_ctx = sws_getContext(
                src_width,
                srcHeight,
                (AVPixelFormat)frame->format,
                targetWidth,
                targetHeight,
                AV_PIX_FMT_YUVJ420P,
                SWS_BILINEAR, // 平衡速度和质量
                NULL,
                NULL,
                NULL
            );

            if (!sws_ctx) {
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
                sws_ctx,
                (const uint8_t* const*)frame->data,
                frame->linesize,
                0,
                srcHeight,
                scaledFrame->data,
                scaledFrame->linesize
            );

            ret = save_frame_as_jpeg(scaledFrame, outputPath, -1, -1, quality);
        } while (false);

        if (file)
            fclose(file);
        if (pkt)
            av_packet_free(&pkt);
        if (enc_ctx)
            avcodec_free_context(&enc_ctx);
        if (sws_ctx)
            sws_freeContext(sws_ctx);
        if (scaledFrame)
            av_frame_free(&scaledFrame);

        return ret;
    }

    // 像素格式转换函数
    AVFrame*
        convert_frame_format(AVFrame* src_frame, AVPixelFormat dst_pix_fmt) {
        struct SwsContext* sws_ctx   = NULL;
        AVFrame*           dst_frame = NULL;

        // 创建目标帧
        dst_frame = av_frame_alloc();
        if (!dst_frame) {
            fprintf(stderr, "无法创建目标帧\n");
            return NULL;
        }

        dst_frame->width  = src_frame->width;
        dst_frame->height = src_frame->height;
        dst_frame->format = dst_pix_fmt;

        // 分配目标帧缓冲区
        int ret = av_frame_get_buffer(dst_frame, 0);
        if (ret < 0) {
            fprintf(stderr, "无法为目标帧分配缓冲区\n");
            av_frame_free(&dst_frame);
            return NULL;
        }

        // 创建转换上下文
        sws_ctx = sws_getContext(
            src_frame->width,
            src_frame->height,
            (AVPixelFormat)src_frame->format,
            dst_frame->width,
            dst_frame->height,
            (AVPixelFormat)dst_frame->format,
            SWS_BILINEAR,
            NULL,
            NULL,
            NULL
        );

        if (!sws_ctx) {
            fprintf(stderr, "无法创建像素格式转换上下文\n");
            av_frame_free(&dst_frame);
            return NULL;
        }

        // 执行转换
        sws_scale(
            sws_ctx,
            (const uint8_t* const*)src_frame->data,
            src_frame->linesize,
            0,
            src_frame->height,
            dst_frame->data,
            dst_frame->linesize
        );

        sws_freeContext(sws_ctx);
        return dst_frame;
    }

    void tryGetPicture(AVStream* stream) {
        const AVCodec*  decoder = nullptr;
        AVCodecContext* dec_ctx = nullptr;
        auto outputPath         = std::filesystem::path("./temp/tempicon.jpg");
        auto outputPath96 = std::filesystem::path("./temp/tempicon96.jpg");

        do {
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

            // 方法2: 检查流类型和编码器
            if (stream->codecpar->codec_type == AVMEDIA_TYPE_VIDEO) {
                // 检查是否是封面图片（通常编码为MJPG、PNG等）
                // 获取解码器并创建解码上下文
                decoder = avcodec_find_decoder(stream->codecpar->codec_id);
                if (!decoder) {
                    fprintf(stderr, "未找到解码器\n");
                    break;
                }

                dec_ctx = avcodec_alloc_context3(decoder);
                avcodec_parameters_to_context(dec_ctx, stream->codecpar);

                // 打开解码器
                if (avcodec_open2(dec_ctx, decoder, NULL) < 0) {
                    fprintf(stderr, "无法打开解码器\n");
                    break;
                }

                // 定位到第1秒（这里以秒为单位，转换为流的时间基准）
                int64_t target_ts = av_rescale_q(1, {1, 1}, stream->time_base);
                if (av_seek_frame(
                        fmt_ctx,
                        stream->index,
                        target_ts,
                        AVSEEK_FLAG_BACKWARD
                    )
                    < 0) {
                    fprintf(stderr, "定位失败\n");
                }

                // 解码帧
                AVPacket* pkt        = av_packet_alloc();
                AVFrame*  frame      = av_frame_alloc();
                AVFrame*  jpeg_frame = nullptr;
                int       ret        = 0;

                AVFrame* target_frame = nullptr;
                while (av_read_frame(fmt_ctx, pkt) >= 0) {
                    if (pkt->stream_index == stream->index) {
                        ret = avcodec_send_packet(dec_ctx, pkt);
                        if (ret < 0) {
                            break;
                        }

                        ret = avcodec_receive_frame(dec_ctx, frame);
                        if (ret == 0) {
                            target_frame = av_frame_clone(frame);
                            av_frame_unref(frame);
                            av_packet_unref(pkt);
                            break;
                        }
                    }
                    av_packet_unref(pkt);
                }

                if (target_frame) {

                    // 如果像素格式不是YUVJ420P，进行转换
                    if (target_frame->format != AV_PIX_FMT_YUVJ420P) {
                        printf(
                            "转换像素格式从 %d 到 YUVJ420P\n",
                            target_frame->format
                        );
                        jpeg_frame = convert_frame_format(
                            target_frame,
                            AV_PIX_FMT_YUVJ420P
                        );
                        av_frame_free(&target_frame); // 释放原始帧

                        if (!jpeg_frame) {
                            fprintf(stderr, "像素格式转换失败\n");
                            break;
                        }
                    }

                    int ret
                        = save_frame_as_jpeg(jpeg_frame, outputPath, -1, -1, 2);
                    if (ret == 0) {
                        printf("成功提取并保存到: %s\n", outputPath);
                        ret = save_frame_as_jpeg_with_scale(
                            jpeg_frame,
                            outputPath96,
                            96,
                            96,
                            8
                        );
                        if (ret == 0) {
                            printf(
                                "成功提取缩略图96并保存到: %s\n",
                                outputPath96
                            );
                        }
                    }
                }

                av_frame_free(&jpeg_frame);
                jpeg_frame = nullptr;
                av_frame_free(&frame);
                frame = nullptr;
                av_packet_free(&pkt);
                pkt = nullptr;
            }
        } while (false);
        if (dec_ctx) {
            avcodec_free_context(&dec_ctx);
            dec_ctx = nullptr;
        }
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

// A very short-lived native function.
//
// For very short-lived functions, it is fine to call them on the main isolate.
// They will block the Dart execution while running the native function, so
// only do this for native functions which are guaranteed to be short-lived.
FFI_PLUGIN_EXPORT int sum(int a, int b) {
    return a + b;
}

// A longer-lived native function, which occupies the thread calling it.
//
// Do not call these kind of native functions in the main isolate. They will
// block Dart execution. This will cause dropped frames in Flutter applications.
// Instead, call these native functions on a separate isolate.
FFI_PLUGIN_EXPORT int sum_long_running(int a, int b) {
    // Simulate work.
#if _WIN32
    Sleep(5000);
#else
    usleep(5000 * 1000);
#endif
    return a + b;
}

FFI_PLUGIN_EXPORT void* mediaxx_malloc(int size) {
    return malloc(size);
}

FFI_PLUGIN_EXPORT void mediaxx_free(void* ptr) {
    free(ptr);
}

FFI_PLUGIN_EXPORT void get_libav_version() {
    std::cout << "current av log level " << av_log_get_level() << std::endl;
}

FFI_PLUGIN_EXPORT void get_media_info(const char* filepath) {
    MediaInfoReader reader;
    if (reader.openFile(filepath)) {
        std::cout << "读取文件成功" << std::endl;
        reader.printMediaInfo();
    } else {
        std::cout << "读取文件失败" << std::endl;
    }
}