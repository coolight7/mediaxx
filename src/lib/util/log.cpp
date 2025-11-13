#include "log.h"
#include <csignal>
#include <execinfo.h>
#include <format>

static std::string _exe_path{};

void logxx::printStack() {

#define _printToConsoleAndFile(str, ...) printf(str, ##__VA_ARGS__);

    {
        char*  buffer[64];
        char** strings = nullptr;

        auto size = backtrace((void**)buffer, 64);

        _printToConsoleAndFile("======= Dump stack start =======\n");
        {
            strings = backtrace_symbols((void**)buffer, size);
            if (strings == nullptr) {
                _printToConsoleAndFile("backtrace_symbols return nullptr");
            }
        }
        for (int i = 0; i < size; i++) {
            if (nullptr == strings[i]) {
                _printToConsoleAndFile("[%02d] %p\n", i, buffer[i]);
            } else {
                _printToConsoleAndFile("[%02d] %s\n", i, strings[i]);
            }
            if (buffer[i] != NULL) {
                char addr2line_cmd[256];
                sprintf(addr2line_cmd, "addr2line -f -e %s %p", _exe_path.c_str(), buffer[i]);
                FILE* addr2line_fp = popen(addr2line_cmd, "r");
                if (addr2line_fp != NULL) {
                    char line[256]{};
                    while (fgets(line, sizeof(line), addr2line_fp) != NULL) {
                        _printToConsoleAndFile("%s", line);
                    }
                    pclose(addr2line_fp);
                }
            } else {
                _printToConsoleAndFile("(unknown)\n");
            }
        }
        _printToConsoleAndFile("======= Dump stack end =======\n");
        free(strings);
    }
#undef _printToConsoleAndFile
}

void signal_handler(int signo) {
    const std::string filename = std::format("crash-{}.log", std::time(nullptr));

#define _printToConsoleAndFile(fp, str, ...) \
    printf(str, ##__VA_ARGS__);              \
    fprintf(fp, str, ##__VA_ARGS__);

    {
        char*  buffer[64];
        char** strings = nullptr;

        auto size = backtrace((void**)buffer, 64);

        FILE* fp = fopen(filename.c_str(), "w");
        if (fp != NULL) {
            _printToConsoleAndFile(fp, "\n======= xx catch signal %d =======\n", signo);
            _printToConsoleAndFile(fp, "======= Dump stack start =======\n");
            {
                strings = backtrace_symbols((void**)buffer, size);
                if (strings == nullptr) {
                    _printToConsoleAndFile(fp, "backtrace_symbols return nullptr");
                }
            }
            for (int i = 0; i < size; i++) {
                if (nullptr == strings[i]) {
                    _printToConsoleAndFile(fp, "[%02d] %p\n", i, buffer[i]);
                } else {
                    _printToConsoleAndFile(fp, "[%02d] %s\n", i, strings[i]);
                }
                if (buffer[i] != NULL) {
                    char addr2line_cmd[256];
                    sprintf(addr2line_cmd, "addr2line -f -e %s %p", _exe_path.c_str(), buffer[i]);
                    FILE* addr2line_fp = popen(addr2line_cmd, "r");
                    if (addr2line_fp != NULL) {
                        char line[256]{};
                        while (fgets(line, sizeof(line), addr2line_fp) != NULL) {
                            _printToConsoleAndFile(fp, "%s", line);
                        }
                        pclose(addr2line_fp);
                    }
                } else {
                    _printToConsoleAndFile(fp, "(unknown)\n");
                }
            }
            _printToConsoleAndFile(fp, "======= Dump stack end =======\n");
            fclose(fp);
            free(strings);
        }
    }
    printf("\n# See file: %s\n", filename.c_str());
    signal(signo, SIG_DFL);
    raise(signo);
#undef _printToConsoleAndFile
}

void logxx::signal_error(std::string_view exepath) {
    _exe_path = exepath;
    signal(SIGSEGV, signal_handler);
}