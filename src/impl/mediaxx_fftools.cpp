#include "mediaxx.h"
#include "simdjson.h"
#include "util/string_util.h"

// 这份 ffmpeg 头文件没有自带 C++ 链接声明，必须包在 extern "C" 里，
// 否则引用的 av_log_* 会带 C++ 修饰名，链接时找不到符号
extern "C" {
#include "libavutil/log.h"
}

#include <mutex>
#include <thread>
#include <vector>
#include <string>
#include <string_view>
#include <cstdarg>
#include <cstring>
#include <cstdio>

extern "C" {
int ffmpeg_exec(int argc, char **argv);
int ffprobe_exec(int argc, char **argv);

/// 定义在 lib/fftools/ffmpeg.c：把取消请求落到 ffmpeg 侧"收到信号"的两个标志上
void ffmpeg_request_cancel(void);

// 定义在 lib/fftools/cmdutils.c：进程内调用的取消标志，以及 fftools 里"打印到
// 标准输出"的内容要写的流。
// 流为空表示直接用进程的 stdout，进程内调用时由 StdoutCapture 换成临时文件。
void  fftools_cancel_request(void);
FILE* fftools_stdout_get(void);
void  fftools_stdout_set(FILE* file);
}

// 统一保护 ffmpeg 和 ffprobe 调用的递归互斥锁，确保多线程并发调用的线程安全
static std::recursive_mutex s_fftools_mutex;

/// 单次调用最多收集多少字节的 stdout / stderr，超过的部分丢弃
static const size_t kMaxCaptureBytes = 4u * 1024u * 1024u;

namespace {

/// 返回不超过 len 字节的位置，并且保证不会把一个 UTF-8 字符切成两半
static size_t utf8SafeCut(const char* data, size_t len) {
    if (0 == len) {
        return 0;
    }
    // 从末尾往前跳过最多 3 个续字节（10xxxxxx），找到最后一个字符的起始字节
    size_t start = len;
    for (int i = 0; i < 3 && start > 0; ++i) {
        const unsigned char c = static_cast<unsigned char>(data[start - 1]);
        if ((c & 0xC0) != 0x80) {
            break;
        }
        --start;
    }
    if (0 == start) {
        return 0;
    }
    const unsigned char first = static_cast<unsigned char>(data[start - 1]);
    size_t charLen = 1;
    if ((first & 0xE0) == 0xC0) {
        charLen = 2;
    } else if ((first & 0xF0) == 0xE0) {
        charLen = 3;
    } else if ((first & 0xF8) == 0xF0) {
        charLen = 4;
    }
    if (start - 1 + charLen > len) {
        // 末尾那个字符没写全，丢掉
        return start - 1;
    }
    return len;
}

/// 按上限收集文本。被截断过就在末尾补一句说明，免得调用方以为输出本来就这么短。
class CaptureBuffer {
public:
    explicit CaptureBuffer(size_t maxBytes)
        : m_maxBytes(maxBytes) {}

    void append(const char* data, size_t size) {
        if (m_truncated || 0 == size) {
            return;
        }
        const size_t room = m_maxBytes - m_text.size();
        if (size <= room) {
            m_text.append(data, size);
            return;
        }
        m_text.append(data, utf8SafeCut(data, room));
        m_truncated = true;
    }

    bool truncated() const {
        return m_truncated;
    }

    /// 取出内容，被截断时末尾会多一句说明
    std::string result() const {
        if (!m_truncated) {
            return m_text;
        }
        std::string text = m_text;
        text.append("\n...[mediaxx] 输出超过 ");
        text.append(std::to_string(m_maxBytes));
        text.append(" 字节，后面的内容已丢弃\n");
        return text;
    }

private:
    std::string m_text;
    size_t      m_maxBytes;
    bool        m_truncated = false;
};

/// 收集一次调用期间 libav 写到 stderr 的日志，结束再还原全局日志状态。
///
/// 做法是临时装一个 av_log 回调把日志收进内存，不去重定向进程的 fd 2：
/// Windows 上 dup2 会关掉宿主进程原来的 stderr 句柄，宿主（比如 Dart VM）之后再写
/// 就会报"句柄无效"。顺带解决 fftools 的两个坑：
/// - `-v` / `-loglevel` 改的是全局日志级别，ffmpeg 和 ffprobe 都不还原，
///   下一次调用不带参数时会继续沿用（上一次用了 `-v error`，之后就再也没有日志）
/// - `-version` / `-h` / `-buildconf` 这类打印完就退出的选项会把日志回调换成
///   `log_callback_help`，ffmpeg 侧同样不还原，之后所有调用的日志都会混进 stdout
///
/// 所有线程的日志都收：ffmpeg 的复用器、解码器是开线程跑的，很多报错（比如输出文件名
/// 不合规、编码线程里失败）只在那里打，按线程过滤会漏掉。呼叫期间宿主其它组件（比如
/// 同时在放的 mpv）的 libav 日志也会被一同收进来，量很少，不影响使用。
///
/// 每行用 av_log_format_line2 拼，和默认回调一样会带上 `[组件名 @ 0x地址] ` 前缀、
/// 也会合并连续重复的日志。这一点不能省：切片任务靠 `[silencedetect ...]` 前缀解析日志。
///
/// 结束时只能恢复成 av_log_default_callback：libav 没有提供读当前回调的接口。
/// 宿主如果自己装过回调（mpv 就会），它的转发在这一次调用后会失效，和 ffmpeg 上游
/// 在处理 `-h` 时的做法一样，不影响本库自己的功能。
class LogCapture {
public:
    LogCapture()
        : m_saved_level(av_log_get_level())
        , m_out(kMaxCaptureBytes) {
        s_self = this;
        av_log_set_callback(captureLogCallback);
    }

    ~LogCapture() {
        s_self = nullptr;
        av_log_set_callback(av_log_default_callback);
        av_log_set_level(m_saved_level);
    }

    LogCapture(const LogCapture&) = delete;
    LogCapture& operator=(const LogCapture&) = delete;

    const CaptureBuffer& getStderr() const {
        return m_out;
    }

private:
    static void captureLogCallback(void* ptr, int level, const char* fmt, va_list vl) {
        LogCapture* self = s_self;
        if (nullptr == self) {
            av_log_default_callback(ptr, level, fmt, vl);
            return;
        }
        // 默认回调里也是这么过滤的，这里保持一致
        if (level > av_log_get_level()) {
            return;
        }
        // 用 av_log_format_line2 而不是 vsnprintf：它会像默认回调一样补上
        // `[组件名 @ 0x地址] ` 前缀，也负责合并“上一条重复了 N 次”。
        // 少了前缀，靠 `[silencedetect ...]` 之类开头做解析的调用方就认不出日志了。
        char stackBuffer[1024];
        int  printPrefix = self->m_printPrefix;
        va_list copy;
        va_copy(copy, vl);
        const int len = av_log_format_line2(
            ptr, level, fmt, copy, stackBuffer, sizeof(stackBuffer), &printPrefix
        );
        va_end(copy);
        if (len <= 0) {
            return;
        }
        if (static_cast<size_t>(len) < sizeof(stackBuffer)) {
            self->m_printPrefix = printPrefix;
            self->m_out.append(stackBuffer, static_cast<size_t>(len));
            return;
        }
        // 一行放不下，按需要的大小再来一次
        std::string big(static_cast<size_t>(len) + 1, '\0');
        int printPrefixBig = self->m_printPrefix;
        va_list copyBig;
        va_copy(copyBig, vl);
        av_log_format_line2(
            ptr, level, fmt, copyBig, &big[0], big.size(), &printPrefixBig
        );
        va_end(copyBig);
        self->m_printPrefix = printPrefixBig;
        self->m_out.append(big.data(), static_cast<size_t>(len));
    }

    static LogCapture* s_self;

    int           m_saved_level;
    /// 下一条日志是否需要打 `[组件 @ 地址]` 前缀，由 av_log_format_line2 维护
    int           m_printPrefix = 1;
    CaptureBuffer m_out;
};

LogCapture* LogCapture::s_self = nullptr;

/// 收集一次调用期间写到标准输出的内容（`-version`、`-h`、ffprobe 的 json 等）。
///
/// 只把 fftools 里打印到 stdout 的那几处（cmdutils.c 的 log_callback_help 和 printf、
/// textformat/tw_stdout.c 里的 writer）用的流换成临时文件，不动进程的 fd 1。
/// 注意 pipe 输出（输出目标写 `-`）走的是 avio 的 fd 1，不在这里收集，所以参数里
/// 出现单独的 `-` 会直接被拒掉。
class StdoutCapture {
public:
    StdoutCapture()
        : m_out(kMaxCaptureBytes) {
        m_file = tmpfile();
        if (nullptr != m_file) {
            fftools_stdout_set(m_file);
        }
    }

    ~StdoutCapture() {
        stop();
    }

    StdoutCapture(const StdoutCapture&) = delete;
    StdoutCapture& operator=(const StdoutCapture&) = delete;

    void stop() {
        if (m_stopped) {
            return;
        }
        m_stopped = true;
        if (nullptr == m_file) {
            return;
        }
        // 先把流换回来，再读临时文件
        fftools_stdout_set(nullptr);
        ::fflush(m_file);
        if (fseek(m_file, 0, SEEK_SET) == 0) {
            // 分块读，避免先把整个临时文件读进内存（输出上限由 CaptureBuffer 管）
            char chunk[8192];
            size_t readBytes = 0;
            while (!m_out.truncated()
                   && (readBytes = fread(chunk, 1, sizeof(chunk), m_file)) > 0) {
                m_out.append(chunk, readBytes);
            }
        }
        fclose(m_file);
        m_file = nullptr;
    }

    const CaptureBuffer& getStdout() const {
        return m_out;
    }

private:
    FILE*         m_file = nullptr;
    bool          m_stopped = false;
    CaptureBuffer m_out;
};

/// 把 ffmpeg / ffprobe 的退出码收敛成 mediaxx.h 里约定的那几种
static int normalizeExitCode(int ret) {
    if (MEDIAXX_FFTOOLS_OK == ret) {
        return MEDIAXX_FFTOOLS_OK;
    }
    if (MEDIAXX_FFTOOLS_CANCELED == ret) {
        return MEDIAXX_FFTOOLS_CANCELED;
    }
    if (MEDIAXX_FFTOOLS_ABORTED == ret) {
        return MEDIAXX_FFTOOLS_ABORTED;
    }
    // 剩下的都算执行失败：负数（AVERROR_*）和 ffmpeg 自己的 1、69 之类
    return MEDIAXX_FFTOOLS_ERROR;
}

static int execute_tool(
    bool is_ffmpeg,
    const char* args,
    size_t argsSize,
    const char** out_stdout,
    const char** out_stderr,
    const char** outLog
) {
    if (out_stdout != nullptr) {
        *out_stdout = nullptr;
    }
    if (out_stderr != nullptr) {
        *out_stderr = nullptr;
    }
    if (outLog != nullptr) {
        *outLog = nullptr;
    }

    if (args == nullptr) {
        if (out_stderr != nullptr) {
            *out_stderr = mediaxx::util::stringCopyMalloc("args is null").data();
        }
        return MEDIAXX_FFTOOLS_BAD_ARGS;
    }

    if (argsSize == 0) {
        argsSize = strlen(args);
    }
    if (argsSize == 0) {
        if (out_stderr != nullptr) {
            *out_stderr = mediaxx::util::stringCopyMalloc("args is empty").data();
        }
        return MEDIAXX_FFTOOLS_BAD_ARGS;
    }

    simdjson::padded_string json_data(args, argsSize);
    simdjson::dom::parser parser;
    simdjson::dom::element doc;
    if (parser.parse(json_data).get(doc) != simdjson::SUCCESS) {
        if (out_stderr != nullptr) {
            *out_stderr = mediaxx::util::stringCopyMalloc("Failed to parse args JSON").data();
        }
        return MEDIAXX_FFTOOLS_BAD_ARGS;
    }

    simdjson::dom::array arr;
    if (doc.get_array().get(arr) != simdjson::SUCCESS) {
        if (out_stderr != nullptr) {
            *out_stderr = mediaxx::util::stringCopyMalloc("args JSON is not an array").data();
        }
        return MEDIAXX_FFTOOLS_BAD_ARGS;
    }

    std::vector<std::string> arg_list;
    for (const auto& elem : arr) {
        switch (elem.type()) {
            case simdjson::dom::element_type::STRING: {
                std::string_view sv;
                if (elem.get_string().get(sv) == simdjson::SUCCESS) {
                    arg_list.emplace_back(sv);
                }
                break;
            }
            case simdjson::dom::element_type::INT64: {
                int64_t v;
                if (elem.get_int64().get(v) == simdjson::SUCCESS) {
                    arg_list.emplace_back(std::to_string(v));
                }
                break;
            }
            case simdjson::dom::element_type::UINT64: {
                uint64_t v;
                if (elem.get_uint64().get(v) == simdjson::SUCCESS) {
                    arg_list.emplace_back(std::to_string(v));
                }
                break;
            }
            case simdjson::dom::element_type::DOUBLE: {
                double v;
                if (elem.get_double().get(v) == simdjson::SUCCESS) {
                    arg_list.emplace_back(std::to_string(v));
                }
                break;
            }
            default:
                break;
        }
    }

    if (arg_list.empty()) {
        if (out_stderr != nullptr) {
            *out_stderr = mediaxx::util::stringCopyMalloc("args array is empty").data();
        }
        return MEDIAXX_FFTOOLS_BAD_ARGS;
    }

    // 单独的 `-` 是管道：ffmpeg 会直接往进程的 fd 0/1 读写，进程内调用拿不到那头，
    // 而且会跟宿主自己的标准输入输出搅在一起，所以在这里就拒掉
    for (const auto& arg : arg_list) {
        if (arg == "-") {
            if (out_stderr != nullptr) {
                *out_stderr = mediaxx::util::stringCopyMalloc(
                    "pipe input/output (a lone \"-\") is not supported in in-process calls"
                ).data();
            }
            return MEDIAXX_FFTOOLS_BAD_ARGS;
        }
    }

    const char* default_prog = is_ffmpeg ? "ffmpeg" : "ffprobe";
    // 如果首个选项以 - 开头，自动在前面补齐程序名
    if (!arg_list.empty() && !arg_list[0].empty() && arg_list[0][0] == '-') {
        arg_list.insert(arg_list.begin(), default_prog);
    }

    std::vector<char*> argv;
    argv.reserve(arg_list.size() + 1);
    for (auto& s : arg_list) {
        argv.push_back(s.data());
    }
    argv.push_back(nullptr);
    int argc = static_cast<int>(arg_list.size());

    std::lock_guard<std::recursive_mutex> lock(s_fftools_mutex);

    // 日志和标准输出都是靠"换流"收集的，必须在锁里做，避免并发调用互相覆盖
    LogCapture    log_capture;
    StdoutCapture stdout_capture;

    int ret = is_ffmpeg ? ffmpeg_exec(argc, argv.data()) : ffprobe_exec(argc, argv.data());
    stdout_capture.stop();

    if (out_stdout != nullptr) {
        *out_stdout = mediaxx::util::stringCopyMalloc(stdout_capture.getStdout().result()).data();
    }
    if (out_stderr != nullptr) {
        *out_stderr = mediaxx::util::stringCopyMalloc(log_capture.getStderr().result()).data();
    }
    if (outLog != nullptr) {
        *outLog = nullptr;
    }

    return normalizeExitCode(ret);
}

} // namespace

extern "C" {

FFI_PLUGIN_EXPORT int mediaxx_execute_ffmpeg(
    const char* args,
    size_t argsSize,
    const char** out_stdout,
    const char** out_stderr,
    const char** outLog
) {
    return execute_tool(true, args, argsSize, out_stdout, out_stderr, outLog);
}

FFI_PLUGIN_EXPORT int mediaxx_execute_ffprobe(
    const char* args,
    size_t argsSize,
    const char** out_stdout,
    const char** out_stderr,
    const char** outLog
) {
    return execute_tool(false, args, argsSize, out_stdout, out_stderr, outLog);
}

FFI_PLUGIN_EXPORT int mediaxx_execute_fftools_cancel(void) {
    // 两边都置一次：ffmpeg 认自己的"收到信号"标志，ffprobe 走中断回调读的共用标志
    fftools_cancel_request();
    ffmpeg_request_cancel();
    return 0;
}

}
