#include "mediaxx.h"
#include "simdjson.h"
#include "util/string_util.h"

// 这份 ffmpeg 头文件没有自带 C++ 链接声明，必须包在 extern "C" 里，
// 否则引用的 av_log_* 会带 C++ 修饰名，链接时找不到符号
extern "C" {
#include "libavutil/log.h"
}

#include <mutex>
#include <vector>
#include <string>
#include <string_view>
#include <cstring>
#include <cstdio>
#include <filesystem>
#include <random>

#if defined(_WIN32)
#include <io.h>
#define mediaxx_dup _dup
#define mediaxx_dup2 _dup2
#define mediaxx_close _close
#define mediaxx_fileno _fileno
#else
#include <unistd.h>
#define mediaxx_dup dup
#define mediaxx_dup2 dup2
#define mediaxx_close close
#define mediaxx_fileno fileno
#endif

extern "C" {
int ffmpeg_exec(int argc, char **argv);
int ffprobe_exec(int argc, char **argv);
}

// 统一保护 ffmpeg 和 ffprobe 调用的递归互斥锁，确保多线程并发调用的线程安全
static std::recursive_mutex s_fftools_mutex;

namespace {

/// 标准输出与标准错误重定向捕获类
class StdStreamCapture {
public:
    StdStreamCapture(bool capture_out, bool capture_err)
        : m_capture_out(capture_out), m_capture_err(capture_err) {
        ::fflush(stdout);
        ::fflush(stderr);

        if (m_capture_out) {
            m_saved_out = mediaxx_dup(mediaxx_fileno(stdout));
            m_file_out = tmpfile();
            if (!m_file_out) {
                m_path_out = std::filesystem::temp_directory_path() /
                    ("mediaxx_stdout_" + std::to_string(std::random_device{}()) + ".tmp");
                m_file_out = fopen(m_path_out.string().c_str(), "w+b");
            }
            if (m_file_out) {
                mediaxx_dup2(mediaxx_fileno(m_file_out), mediaxx_fileno(stdout));
            }
        }

        if (m_capture_err) {
            m_saved_err = mediaxx_dup(mediaxx_fileno(stderr));
            m_file_err = tmpfile();
            if (!m_file_err) {
                m_path_err = std::filesystem::temp_directory_path() /
                    ("mediaxx_stderr_" + std::to_string(std::random_device{}()) + ".tmp");
                m_file_err = fopen(m_path_err.string().c_str(), "w+b");
            }
            if (m_file_err) {
                mediaxx_dup2(mediaxx_fileno(m_file_err), mediaxx_fileno(stderr));
            }
        }
    }

    ~StdStreamCapture() {
        stop();
    }

    void stop() {
        if (m_stopped) {
            return;
        }
        m_stopped = true;

        ::fflush(stdout);
        ::fflush(stderr);

        if (m_saved_out >= 0) {
            mediaxx_dup2(m_saved_out, mediaxx_fileno(stdout));
            mediaxx_close(m_saved_out);
            m_saved_out = -1;
        }
        if (m_saved_err >= 0) {
            mediaxx_dup2(m_saved_err, mediaxx_fileno(stderr));
            mediaxx_close(m_saved_err);
            m_saved_err = -1;
        }

        if (m_file_out) {
            fseek(m_file_out, 0, SEEK_END);
            long len = ftell(m_file_out);
            if (len > 0) {
                fseek(m_file_out, 0, SEEK_SET);
                m_out_str.resize(len);
                size_t read_bytes = fread(&m_out_str[0], 1, len, m_file_out);
                (void)read_bytes;
            }
            fclose(m_file_out);
            m_file_out = nullptr;
            if (!m_path_out.empty()) {
                std::error_code ec;
                std::filesystem::remove(m_path_out, ec);
            }
        }

        if (m_file_err) {
            fseek(m_file_err, 0, SEEK_END);
            long len = ftell(m_file_err);
            if (len > 0) {
                fseek(m_file_err, 0, SEEK_SET);
                m_err_str.resize(len);
                size_t read_bytes = fread(&m_err_str[0], 1, len, m_file_err);
                (void)read_bytes;
            }
            fclose(m_file_err);
            m_file_err = nullptr;
            if (!m_path_err.empty()) {
                std::error_code ec;
                std::filesystem::remove(m_path_err, ec);
            }
        }
    }

    const std::string& getStdout() const { return m_out_str; }
    const std::string& getStderr() const { return m_err_str; }

private:
    bool m_capture_out = false;
    bool m_capture_err = false;
    bool m_stopped = false;
    int m_saved_out = -1;
    int m_saved_err = -1;
    FILE* m_file_out = nullptr;
    FILE* m_file_err = nullptr;
    std::filesystem::path m_path_out;
    std::filesystem::path m_path_err;
    std::string m_out_str;
    std::string m_err_str;
};

/// 保证一次调用在结束时把 libav 的全局日志状态还原，不影响后面的调用。
///
/// fftools 会改动 libav 的进程级日志设置，而且只还原一部分：
/// - `-v` / `-loglevel` 改的是全局日志级别，ffmpeg 和 ffprobe 都不还原，
///   下一次调用不带参数时就会继续沿用（上一次用了 `-v error`，之后就再也没有日志）
/// - `-version` / `-h` / `-buildconf` 这类打印完就退出的选项会把日志回调换成
///   `log_callback_help`，它把日志写到 stdout 而且不看级别，ffmpeg 侧同样不还原，
///   之后所有调用的日志都会混进 stdout
///
/// 构造时先把回调换回默认回调（写 stderr、按级别过滤），让每次调用都从干净状态开始；
/// 析构时再换一次，并把进入前的日志级别还原。
class LogStateGuard {
public:
    LogStateGuard()
        : m_saved_level(av_log_get_level()) {
        av_log_set_callback(av_log_default_callback);
    }

    ~LogStateGuard() {
        av_log_set_callback(av_log_default_callback);
        av_log_set_level(m_saved_level);
    }

    LogStateGuard(const LogStateGuard&) = delete;
    LogStateGuard& operator=(const LogStateGuard&) = delete;

private:
    int m_saved_level;
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

    // 日志状态是进程级的，必须在锁里保存/还原，避免并发调用互相覆盖
    LogStateGuard log_state_guard;

    StdStreamCapture capture(true, true);
    int ret = is_ffmpeg ? ffmpeg_exec(argc, argv.data()) : ffprobe_exec(argc, argv.data());
    capture.stop();

    if (out_stdout != nullptr) {
        *out_stdout = mediaxx::util::stringCopyMalloc(capture.getStdout()).data();
    }
    if (out_stderr != nullptr) {
        *out_stderr = mediaxx::util::stringCopyMalloc(capture.getStderr()).data();
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
