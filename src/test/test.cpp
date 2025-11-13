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
    {
        std::map<std::string, std::string> data{
            {"name",     "simdjson"    },
            {"type",     "parser"      },
            {"language", "C++23"       },
            {"version",  "版本 4.0.7"}
        };
        simdjson::builder::string_builder sb;
        sb.append(data);
        std::string_view json = sb.view().value_unsafe();
        std::cout << "Generated JSON: " << json << std::endl;
    }

    {
        assert(stringxx::utf8IsAvail(nullptr) == false);
        assert(stringxx::utf8IsAvail("\0") == false);
        assert(stringxx::utf8IsAvail("1"));
        assert(stringxx::utf8IsAvail("ww 测试 cc"));
        // assert(stringxx::isAvailUtf8("ww 测试 cc �") == false);
        assert(stringxx::utf8IsAvail(std::vector<char>{-49, -7, -43, -59}.data()) == false);
        // 多字节后续字节格式错误，后续字节固定为 10xxxxxx
        assert(stringxx::utf8IsAvail("12 \xE4\x28\x01 fd") == false);
        // 非最短编码检查
        assert(stringxx::utf8IsAvail("12 \xE0\x80\xAF fd") == false);
        assert(stringxx::utf8IsAvail("12 \xC0\x80 fd") == false);
        // 5、6字节编码无效
        assert(stringxx::utf8IsAvail("12 \xF8\xF7 fd") == false);
        assert(stringxx::utf8IsAvail("12 \xFC\xFD fd") == false);
    }

    mediaxx_set_log_level(AV_LOG_TRACE);

    auto result = mediaxx_get_available_hwcodec_list();
    if (nullptr != result) {
        std::cout << "硬件加速编解码器：" << result << std::endl;
    }

    {
        const char* result = nullptr;
        const char* log    = nullptr;
        auto        ret    = mediaxx_get_media_info_malloc(
            "./temp/李艺皓+-+嚣张.wav",
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
            std::cout << std::endl
                      << "## analyzePictureColorFromData: " << file.good()
                      << " size:" << buffer.size() << std::endl;
            file.close();
            analyse_tool::analyzePictureColorFromData(buffer.data(), buffer.size());
        } else {
            std::cout << "无法打开文件进行二进制读取" << std::endl;
        }
    }
}

int main(int argn, char** argv) {
    logxx::signal_error(argv[0]);
    std::cout << "======= Test Start =======" << std::endl;
    test();
    std::cout << "======= Test Done =======" << std::endl;
    std::cout << ">>>";
    int num = 0;
    cin >> num;
    return 0;
}