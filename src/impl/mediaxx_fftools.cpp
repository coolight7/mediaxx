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

// 定义在 lib/fftools/cmdutils.c：fftools 里"打印到标准输出"的内容都写到这里。
// 为空表示直接用进程的 stdout，进程内调用时由 StdoutCapture 换成临时文件。
FILE* fftools_stdout_get(void);
void  fftools_stdout_set(FILE* file);
}

// 统一保护 ffmpeg 和 ffprobe 调用的递归互斥锁，确保多线程并发调用的线程安全
static std::recursive_mutex s_fftools_mutex;

namespace {

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
/// 别的线程（比如同时在跑的 mpv）的日志照旧走默认回调，不会被收进来。
class LogCapture {
public:
    LogCapture()
        : m_owner(std::this_thread::get_id())
        , m_saved_level(av_log_get_level()) {
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

    const std::string& getStderr() const {
        return m_text;
    }

private:
    static void captureLogCallback(void* ptr, int level, const char* fmt, va_list vl) {
        LogCapture* self = s_self;
        // 不是本次调用线程的日志（比如 mpv 在别的线程打的）按原来的方式输出
        if (nullptr == self || std::this_thread::get_id() != self->m_owner) {
            av_log_default_callback(ptr, level, fmt, vl);
            return;
        }
        // 默认回调里也是这么过滤的，这里保持一致
        if (level > av_log_get_level()) {
            return;
        }
        char stackBuffer[1024];
        va_list copy;
        va_copy(copy, vl);
        const int len = vsnprintf(stackBuffer, sizeof(stackBuffer), fmt, vl);
        if (len > 0) {
            if (static_cast<size_t>(len) < sizeof(stackBuffer)) {
                self->m_text.append(stackBuffer, static_cast<size_t>(len));
            } else {
                std::string big(static_cast<size_t>(len) + 1, '\0');
                vsnprintf(&big[0], big.size(), fmt, copy);
                self->m_text.append(big.data(), static_cast<size_t>(len));
            }
        }
        va_end(copy);
    }

    static LogCapture* s_self;

    std::thread::id m_owner;
    int             m_saved_level;
    std::string     m_text;
};

LogCapture* LogCapture::s_self = nullptr;

/// 收集一次调用期间写到标准输出的内容（`-version`、`-h`、ffprobe 的 json 等）。
///
/// 只把 fftools 里打印到 stdout 的那几处（cmdutils.c 的 log_callback_help、
/// textformat/tw_stdout.c 里的 writer）用的流换成临时文件，不动进程的 fd 1。
/// 注意 pipe 输出（输出目标写 `-`）走的是 avio 的 fd 1，不在这里收集。
class StdoutCapture {
public:
    StdoutCapture() {
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
        long size = -1;
        if (fseek(m_file, 0, SEEK_END) == 0) {
            size = ftell(m_file);
        }
        if (size > 0 && fseek(m_file, 0, SEEK_SET) == 0) {
            m_text.resize(static_cast<size_t>(size));
            const size_t readBytes = fread(&m_text[0], 1, m_text.size(), m_file);
            m_text.resize(readBytes);
        }
        fclose(m_file);
        m_file = nullptr;
    }

    const std::string& getStdout() const {
        return m_text;
    }

private:
    FILE*       m_file = nullptr;
    bool        m_stopped = false;
    std::string m_text;
};

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
        return -1;
    }

    if (argsSize == 0) {
        argsSize = strlen(args);
    }
    if (argsSize == 0) {
        if (out_stderr != nullptr) {
            *out_stderr = mediaxx::util::stringCopyMalloc("args is empty").data();
        }
        return -1;
    }

    simdjson::padded_string json_data(args, argsSize);
    simdjson::dom::parser parser;
    simdjson::dom::element doc;
    if (parser.parse(json_data).get(doc) != simdjson::SUCCESS) {
        if (out_stderr != nullptr) {
            *out_stderr = mediaxx::util::stringCopyMalloc("Failed to parse args JSON").data();
        }
        return -1;
    }

    simdjson::dom::array arr;
    if (doc.get_array().get(arr) != simdjson::SUCCESS) {
        if (out_stderr != nullptr) {
            *out_stderr = mediaxx::util::stringCopyMalloc("args JSON is not an array").data();
        }
        return -1;
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
        return -1;
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
        *out_stdout = mediaxx::util::stringCopyMalloc(stdout_capture.getStdout()).data();
    }
    if (out_stderr != nullptr) {
        *out_stderr = mediaxx::util::stringCopyMalloc(log_capture.getStderr()).data();
    }
    if (outLog != nullptr) {
        *outLog = nullptr;
    }

    return ret;
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

}
