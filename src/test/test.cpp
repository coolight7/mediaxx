extern "C" {
#include "libavcodec/codec.h"
}

#include "analyse/audio_visualization.h"
#include "analyse/codec_info.h"
#include "analyse/image.h"
#include "analyse/media_info.h"
#include "mediaxx.h"
#include "simdjson.h"
#include "util/log.h"
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
        std::cout << "Generated JSON: " << sb.view().value_unsafe() << std::endl;
    }

    {
        // uchardet
        const auto textlist = std::vector<std::pair<std::string, std::string>>{
            {"./temp/test-utf8.txt",                      "UTF-8"   },
            {"./temp/test-utf16.txt",                     "UTF-16BE"},
            {"./temp/test-gbk.txt",                       "GB18030" },
            {"./temp/安静了.lrc",                      "GB18030" },
            {"./temp/不该 - 周杰伦、张惠妹.lrc", "GB18030" },
        };
        std::string encoding, result;
        for (const auto& item : textlist) {
            std::ifstream file(item.first, std::ios::binary);
            if (!file) {
                std::cout << "文件打开失败: " << item.first << std::endl;
                continue;
            }

            // 获取文件大小
            file.seekg(0, std::ios_base::end);
            const auto size = file.tellg();
            if (size <= 0) {
                std::cout << "文件为空: " << item.first << std::endl;
                continue;
            }
            file.seekg(0, std::ios_base::beg);

            std::string buf;
            buf.resize(static_cast<size_t>(size));
            file.read(buf.data(), size);

            std::cout << "预期字符编码: " << item.second << std::endl;
            if (mediaxx::stringxx::chardetConvertEncoding(buf, encoding, result)) {
                std::cout << item.first << " - " << encoding << std::endl
                          << result << std::endl
                          << std::endl;
            } else {
                std::cout << "转换失败: " << encoding << std::endl
                          << item.first << std::endl
                          << std::endl;
            }
            assert(item.second == encoding);
        }

        if (mediaxx::stringxx::chardetConvertEncoding("Hello \xB0\xA1\xC4\xE3", encoding, result)) {
            std::cout << "GBK: - " << encoding << std::endl << result << std::endl;
            assert(encoding == "GB18030");
        } else {
            std::cout << "GBK: - 失败" << std::endl;
        }
        if (mediaxx::stringxx::chardetConvertEncoding("Hello 世界", encoding, result)) {
            std::cout << "UTF8: - " << encoding << std::endl << result << std::endl;
            assert(encoding == "UTF-8");
        } else {
            std::cout << "UTF8: - 失败" << std::endl;
        }
        {
            std::string str = "Hello 世界";
            const char* out = nullptr;
            assert(mediaxx_convert_char_encoding(str.data(), str.size(), &out) == 12);
        }
    }

    {
        assert(mediaxx::stringxx::utf8IsAvail("\0") == false);
        assert(mediaxx::stringxx::utf8IsAvail("1"));
        assert(mediaxx::stringxx::utf8IsAvail("ww 测试 cc"));
        assert(mediaxx::stringxx::utf8IsAvail("ww 测试 cc �") == false);
        assert(
            mediaxx::stringxx::utf8IsAvail(std::string_view{
                std::vector<char>{-49, -7, -43, -59}
                .data(),
                4
        })
            == false
        );
        // 多字节后续字节格式错误，后续字节固定为 10xxxxxx
        assert(mediaxx::stringxx::utf8IsAvail("12 \xE4\x28\x01 fd") == false);
        // 非最短编码检查
        assert(mediaxx::stringxx::utf8IsAvail("12 \xE0\x80\xAF fd") == false);
        assert(mediaxx::stringxx::utf8IsAvail("12 \xC0\x80 fd") == false);
        // 5、6字节编码无效
        assert(mediaxx::stringxx::utf8IsAvail("12 \xF8\xF7 fd") == false);
        assert(mediaxx::stringxx::utf8IsAvail("12 \xFC\xFD fd") == false);
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
            "./temp/林力尧 - 初恋旧爱新欢.flac",
            "",
            "./temp/中文输出/输出out - put.jpg",
            "./temp/中文输出/输出out - put96.jpg",
            &result,
            &log
        );
        std::cout << "mediaxx info ret: " << ret << std::endl;
        std::cout << ((nullptr != result) ? result : "nullptr") << std::endl;
        std::cout << "log: " << ((nullptr != log) ? log : "nullptr") << std::endl;
        assert(ret == 2);
        mediaxx_free(result);
        mediaxx_free(log);
    }
    {
        const char* result = nullptr;
        const char* log    = nullptr;
        auto        ret    = mediaxx_get_media_info_malloc(
            // "./temp/要不要买菜-火红的萨日朗.wav",
            "./temp/李艺皓+-+嚣张.wav",
            // "./temp/林力尧 - 初恋旧爱新欢.flac",
            // "./temp/Great Voyage_洛天依.mp3",
            // "./temp/淋雨一直走-张韶涵.flac",
            // "./temp/爱情的骗子我问你 - 陈小云.mp3",
            "",
            "./temp/output.jpg",
            "./temp/output96.jpg",
            &result,
            &log
        );
        std::cout << "mediaxx info ret: " << ret << std::endl;
        std::cout << ((nullptr != result) ? result : "nullptr") << std::endl;
        std::cout << "log: " << ((nullptr != log) ? log : "nullptr") << std::endl;
        assert(ret == 2);
        mediaxx_free(result);
        mediaxx_free(log);
    }
    {
        std::cout << "AudioSpectrumAnalyzer ....... ====================" << std::endl;
        mediaxx::AudioSpectrumAnalyzer analyzer{};
        std::vector<std::array<unsigned char, mediaxx::AudioSpectrumAnalyzer::DEF_SPECTRUM_SIZE>>
                                   spectrumData{};
        std::vector<unsigned char> waveformData{};

        int rebool
            = analyzer.processAudio("./temp/不老不死_洛天依.flac", spectrumData, waveformData);
        assert(rebool == true);
        std::cout << "result: 不老不死_洛天依.flac | " << spectrumData.size() << std::endl;
        std::cout << "成功生成 " << spectrumData.size() << " 帧频谱数据" << std::endl;

        int     maxIndex = 0;
        uint8_t maxPoint = 0;

        {
            int i = 0;
            for (auto& item : waveformData) {
                std::cout << int(item) << " ";
                if (item > maxPoint) {
                    maxPoint = item;
                    maxIndex = i;
                }
                ++i;
            }
        }

        for (auto& spectrum : spectrumData) {
            assert(spectrum.size() == 256);
        }

        std::cout << std::endl << "[";
        for (auto& item : spectrumData[maxIndex]) {
            std::cout << (unsigned int)(item) << " ";
        }
        std::cout << "]" << std::endl << std::endl;

        assert(spectrumData.size() == waveformData.size());
        std::cout << std::endl;
    }
    {
        std::cout
            << "AudioSpectrumAnalyzer/mediaxx_get_audio_visualization ....... ===================="
            << std::endl;
        const char* result         = nullptr;
        const char* resultWaveform = nullptr;
        const char* log            = nullptr;
        auto        ret            = mediaxx_get_audio_visualization(
            "./temp/不老不死_洛天依.flac",
            &result,
            &resultWaveform,
            &log
        );
        assert(nullptr != result && ret > 0);
        std::cout << "result: 不老不死_洛天依.flac | " << ret / 256 << std::endl;
        mediaxx_free(result);
        mediaxx_free(resultWaveform);
        mediaxx_free(log);
    }
    {
        std::cout
            << "AudioSpectrumAnalyzer/mediaxx_get_audio_visualization ....... ===================="
            << std::endl;
        const char* result         = nullptr;
        const char* resultWaveform = nullptr;
        const char* log            = nullptr;
        auto        ret            = mediaxx_get_audio_visualization(
            "./temp/A Little Story_Valentine.ape",
            &result,
            &resultWaveform,
            &log
        );
        assert(nullptr != result && ret > 0);
        std::cout << "result: A Little Story_Valentine.ape | " << ret / 256 << std::endl;
        mediaxx_free(result);
        mediaxx_free(resultWaveform);
        mediaxx_free(log);
    }
    {
        std::cout
            << "AudioSpectrumAnalyzer/mediaxx_get_audio_visualization ....... ===================="
            << std::endl;
        const char* result         = nullptr;
        const char* resultWaveform = nullptr;
        const char* log            = nullptr;
        auto        ret            = mediaxx_get_audio_visualization(
            "./temp/李艺皓+-+嚣张.wav",
            &result,
            &resultWaveform,
            &log
        );
        assert(nullptr != result && ret > 0);
        std::cout << "result: 李艺皓+-+嚣张.wav | " << ret / 256 << std::endl;
        mediaxx_free(result);
        mediaxx_free(resultWaveform);
        mediaxx_free(log);
    }
    {
        std::cout
            << "AudioSpectrumAnalyzer/mediaxx_get_audio_visualization ....... ===================="
            << std::endl;
        const char* result         = nullptr;
        const char* resultWaveform = nullptr;
        const char* log            = nullptr;
        auto        ret            = mediaxx_get_audio_visualization(
            "./temp/Gold Town_M2U.mp3",
            &result,
            &resultWaveform,
            &log
        );
        assert(nullptr != result && ret > 0);
        std::cout << "result: Gold Town_M2U.mp3 | " << ret / 256 << std::endl;
        mediaxx_free(result);
        mediaxx_free(resultWaveform);
        mediaxx_free(log);
    }

    {
        const char* log     = nullptr;
        auto        logItem = mediaxx::AnalyseLogItem_c{&log};
        auto        result  = mediaxx::analysePictureColorFromPath("./temp/output.jpg", logItem);
        if (nullptr != *logItem.log) {
            std::cout << "log: " << *logItem.log << std::endl;
        }
        if (nullptr != result) {
            std::cout << std::endl
                      << "## analysePictureColorFromPath: "
                      << result->toJson().view().value_unsafe() << std::endl;
        }
    }

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
            const char* log     = nullptr;
            auto        logItem = mediaxx::AnalyseLogItem_c{&log};
            auto        result
                = mediaxx::analyzePictureColorFromData(buffer.data(), buffer.size(), logItem);
            if (nullptr != *logItem.log) {
                std::cout << "log: " << *logItem.log << std::endl;
            }
            if (nullptr != result) {
                std::cout << std::endl
                          << "## analyzePictureColorFromData: "
                          << result->toJson().view().value_unsafe() << std::endl;
            }
        } else {
            std::cout << "无法打开文件进行二进制读取" << std::endl;
        }
    }

    {
        auto file = std::ifstream{"./temp/decodedImg-2", std::ios::binary};
        if (file.is_open()) {
            file.seekg(0, ios::end);
            size_t fileSize = file.tellg();
            file.seekg(0, ios::beg);
            vector<char> buffer{};
            buffer.resize(fileSize);
            file.read(buffer.data(), fileSize);
            std::cout << std::endl
                      << "## mediaxx_analyse_picture_color_from_decoded_data: " << file.good()
                      << " size:" << buffer.size() << std::endl;
            file.close();
            const char* result = nullptr;
            const char* log    = nullptr;
            auto        ret    = mediaxx_analyse_picture_color_from_decoded_data(
                buffer.data(),
                buffer.size(),
                &result,
                &log
            );
            if (nullptr != log) {
                std::cout << "log: " << log << std::endl;
            }
            std::cout << std::endl
                      << "## mediaxx_analyse_picture_color_from_decoded_data: ret: " << ret
                      << "  result: " << ((nullptr != result) ? result : "nullptr") << std::endl;
        } else {
            std::cout << "无法打开文件进行二进制读取" << std::endl;
        }
    }
}

int main(int argn, char** argv) {
#if IS_LINUX_D
    mediaxx::logxx::signalError(argv[0]);
#endif
    std::cout << "======= Test Start =======" << std::endl;
    test();
    std::cout << "======= Test Done =======" << std::endl;
    std::cout << ">>>";
    int num = 0;
    cin >> num;
    return 0;
}