extern "C" {
#include "libavcodec/codec.h"
}

#include "analyse/codec_info.h"
#include "analyse/tool.h"
#include "mediaxx.h"
#include "simdjson.h"
#include <fstream>
#include <iostream>
#include <map>

using namespace std;

void test() {
    mediaxx_set_log_level(AV_LOG_TRACE);

    auto result = mediaxx_get_available_hwcodec_list();
    if (nullptr != result) {
        std::cout << "硬件加速编解码器：" << result << std::endl;
    }

    {
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
    }

    {
        const char* result = nullptr;
        const char* log    = nullptr;
        auto        ret    = mediaxx_get_media_info_malloc(
            // "C:/0Acoolight/Music/Chinese/林宥嘉 - 浪费.flac",
            // "C:/0Acoolight/Music/English/Animals.flac",
            // "C:/0Acoolight/Music/only/安静的午后_Pianoboy高至豪.flac",
            "D:\\0Acoolight\\Download\\chrome\\李艺皓+-+嚣张.wav",
            "",
            "./temp/output.jpg",
            "./temp/output96.jpg",
            &result,
            &log
        );
        std::cout << "mediaxx info ret: " << ret << std::endl;
        std::cout << ((nullptr != result) ? result : "nullptr") << std::endl;
        std::cout << "log: " << ((nullptr != log) ? log : "nullptr") << std::endl;
        mediaxx_free(result);
        mediaxx_free(log);
    }

    { analyse_tool::analysePictureColorFromPath("./temp/output.jpg"); }

    {
        auto file = std::ifstream{"./temp/output.jpg", std::ios::binary};
        if (file.is_open()) {
            file.seekg(0, ios::end);
            size_t fileSize = file.tellg();
            file.seekg(0, ios::beg);
            vector<char> buffer{};
            buffer.resize(fileSize);
            file.read(buffer.data(), fileSize);
            file.close();
            std::cout << std::endl
                      << "## analyzePictureColorFromData: " << buffer.size() << std::endl;
            analyse_tool::analyzePictureColorFromData(buffer.data(), buffer.size());
        } else {
            std::cout << "无法打开文件进行二进制读取" << std::endl;
        }
    }
}

int main(int argn, char** argv) {
    std::cout << "======= Test Start =======" << std::endl;
    test();
    std::cout << "======= Test Done =======" << std::endl;
    std::cout << ">>>";
    int num = 0;
    cin >> num;
    return 0;
}