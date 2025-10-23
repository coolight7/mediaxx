extern "C" {
#include "libavcodec/avcodec.h"
#include "libavcodec/codec_par.h"
#include "libavformat/avformat.h"
#include "libavutil/avutil.h"
#include "libavutil/dict.h"
#include "libavutil/error.h"
#include "libavutil/log.h"
#include "libavutil/pixdesc.h"
}

#include "mediaxx.h"
#include <iostream>
#include <memory>
#include <string>
#include <string_view>

class MediaInfoReader {
public:

    MediaInfoReader() :
        fmt_ctx(nullptr) {
        // 初始化FFmpeg网络库，支持HTTP协议:cite[8]
        avformat_network_init();
    }

    ~MediaInfoReader() {
        cleanup();
        avformat_network_deinit();
    }

    bool openFile(const std::string& filename) {
        // 设置最大分析时长（微秒）
        av_dict_set(&options, "analyzeduration", "1000000", 0); // 1秒
        // 设置最大分析字节数
        // av_dict_set(&options, "probesize", "5000000", 0); // 5MB
        // 10秒超时（单位微秒）
        av_dict_set(&options, "timeout", "30000000", 0);
        // 允许多次请求
        // av_dict_set(&options, "multiple_requests", "1", 0);
        // 打开输入文件:cite[9]
        int ret = avformat_open_input(
            &fmt_ctx,
            filename.c_str(),
            nullptr,
            &options
        );
        if (ret < 0) {
            std::cerr << "无法打开文件: " << filename
                      << ", 错误: " << av_err2str(ret) << std::endl;
            return false;
        }

        // 获取流信息:cite[9]
        ret = avformat_find_stream_info(fmt_ctx, nullptr);
        if (ret < 0) {
            std::cerr << "无法获取流信息, 错误: " << av_err2str(ret)
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

    void cleanup() {
        if (fmt_ctx) {
            avformat_close_input(&fmt_ctx);
        }
        if (options) {
            av_dict_free(&options);
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