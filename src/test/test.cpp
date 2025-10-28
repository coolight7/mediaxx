extern "C" {
#include "libavutil/log.h"
}

#include "mediaxx.h"
#include "simdjson.h"
#include <iostream>
#include <map>

using namespace std;

void test() {
    mediaxx_get_libav_version();
}

int main(int argn, char** argv) {
    av_log_set_level(AV_LOG_TRACE);
    std::cout << "======= Test Start =======" << std::endl;
    test();

    std::map<std::string, std::string> data{
        {"name",     "simdjson"},
        {"type",     "parser"  },
        {"language", "C++23"   },
        {"version",  "4.0.7"   }
    };

    simdjson::builder::string_builder sb;
    sb.append(data);
    std::string_view p{sb};

    std::cout << "Generated JSON: " << p << std::endl;

    char* log    = nullptr;
    auto  result = mediaxx_get_media_info_malloc(
        // "C:/0Acoolight/Music/Chinese/林宥嘉 - 浪费.flac",
        "C:/0Acoolight/Music/English/Animals.flac",
        // "C:/0Acoolight/Music/only/安静的午后_Pianoboy高至豪.flac",
        "",
        "./temp/output.jpg",
        "./temp/output96.jpg",
        &log
    );
    std::cout << "mediaxx info result: " << std::endl;
    std::cout << ((nullptr != result) ? result : "nullptr") << std::endl;
    std::cout << "log: " << ((nullptr != log) ? log : "nullptr") << std::endl;
    std::cout << "======= Test Done =======" << std::endl;
    std::cout << ">>>";
    int num = 0;
    cin >> num;
    return 0;
}