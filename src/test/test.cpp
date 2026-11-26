#include <thread>
#include <vector>
#include <cassert>
#include <filesystem>
extern "C" {
#include "libavcodec/codec.h"
#include "libavutil/log.h"
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

/// 一致性用例：C++ 实现 vs musicxx 的 Dart 实现（定义在 test_analyse.cpp）
void runAnalyseTests();

void test() {
    // 纯计算、不依赖音频文件的一致性用例先跑
    runAnalyseTests();

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
            auto [isSucc, result] = mediaxx::util::autoConvertToUtf8(buf, encoding);
            if (isSucc) {
                std::cout << item.first << " - " << encoding << std::endl
                          << result.value_or("[crude]") << std::endl
                          << std::endl;
            } else {
                std::cout << "转换失败: " << encoding << std::endl
                          << item.first << std::endl
                          << std::endl;
            }
            assert(item.second == encoding);
        }

        {
            auto [isSucc, result]
                = mediaxx::util::autoConvertToUtf8("Hello \xB0\xA1\xC4\xE3", encoding);
            if (isSucc) {
                std::cout << "GBK: - " << encoding << std::endl
                          << result.value_or("[crude]") << std::endl;
                assert(encoding == "GB18030");
            } else {
                std::cout << "GBK: - 失败" << std::endl;
            }
        }
        {
            auto [isSucc, result] = mediaxx::util::autoConvertToUtf8("Hello 世界", encoding);
            if (isSucc) {
                std::cout << "UTF8: - " << encoding << std::endl
                          << result.value_or("[crude]") << std::endl;
                assert(encoding == "UTF-8");
            } else {
                std::cout << "UTF8: - 失败" << std::endl;
            }
        }
        {
            std::string str = "Hello 世界";
            const char* out = nullptr;
            assert(mediaxx_convert_char_encoding(str.data(), str.size(), &out) == 12);
            mediaxx_free(out);
        }
    }

    {
        assert(mediaxx::util::utf8IsAvail("\0") == false);
        assert(mediaxx::util::utf8IsAvail("1"));
        assert(mediaxx::util::utf8IsAvail("ww 测试 cc"));
        assert(mediaxx::util::utf8IsAvail("ww 测试 cc �") == false);
        assert(
            mediaxx::util::utf8IsAvail(std::string_view{
                std::vector<char>{-49, -7, -43, -59}
                .data(),
                4
        })
            == false
        );
        // 多字节后续字节格式错误，后续字节固定为 10xxxxxx
        assert(mediaxx::util::utf8IsAvail("12 \xE4\x28\x01 fd") == false);
        // 非最短编码检查
        assert(mediaxx::util::utf8IsAvail("12 \xE0\x80\xAF fd") == false);
        assert(mediaxx::util::utf8IsAvail("12 \xC0\x80 fd") == false);
        // 5、6字节编码无效
        assert(mediaxx::util::utf8IsAvail("12 \xF8\xF7 fd") == false);
        assert(mediaxx::util::utf8IsAvail("12 \xFC\xFD fd") == false);
    }

    mediaxx_set_log_level(AV_LOG_TRACE);
    auto result = mediaxx_get_available_hwcodec_list();
    if (nullptr != result) {
        std::cout << "硬件加速编解码器：" << result << std::endl;
        mediaxx_free(result);
    }

    {
        // 音频、视频过滤器列表：都要是能解析的 json 数组，且每一项都带名字
        const char* audioFilterList = mediaxx_get_available_audio_filter_list();
        const char* videoFilterList = mediaxx_get_available_video_filter_list();
        assert(nullptr != audioFilterList);
        assert(nullptr != videoFilterList);

        // 一个 parser 只能保留最后解析出的一份文档，两份列表要各用一个 parser
        simdjson::dom::parser  audioParser;
        simdjson::dom::parser  videoParser;
        simdjson::dom::element audioDoc;
        simdjson::dom::element videoDoc;
        assert(
            audioParser.parse(audioFilterList, std::strlen(audioFilterList)).get(audioDoc)
            == simdjson::SUCCESS
        );
        assert(
            videoParser.parse(videoFilterList, std::strlen(videoFilterList)).get(videoDoc)
            == simdjson::SUCCESS
        );

        simdjson::dom::array audioArray;
        simdjson::dom::array videoArray;
        assert(audioDoc.get_array().get(audioArray) == simdjson::SUCCESS);
        assert(videoDoc.get_array().get(videoArray) == simdjson::SUCCESS);
        assert(audioArray.size() > 0 && videoArray.size() > 0);
        std::cout << "音频过滤器：" << audioArray.size() << " 个，视频过滤器：" << videoArray.size()
                  << " 个" << std::endl;

        auto printFilterNames = [](const char* label, const simdjson::dom::array& array) {
            std::string names;
            size_t      count = 0;
            for (const auto& item : array) {
                std::string_view name;
                assert(item["name"].get_string().get(name) == simdjson::SUCCESS);
                assert(!name.empty());
                if (count < 5) {
                    names += (count > 0 ? ", " : "") + std::string{name};
                }
                count++;
            }
            assert(count == array.size());
            std::cout << label << " 前几个：" << names << std::endl;
        };
        printFilterNames("音频过滤器", audioArray);
        printFilterNames("视频过滤器", videoArray);

        mediaxx_free(audioFilterList);
        mediaxx_free(videoFilterList);
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
            << "AudioSpectrumAnalyzer/mediaxx_get_audio_visualization (limit 10s) ....... ===================="
            << std::endl;
        const char* result         = nullptr;
        const char* resultWaveform = nullptr;
        const char* log            = nullptr;
        auto        ret            = mediaxx_get_audio_visualization(
            "./temp/不老不死_洛天依.flac",
            &result,
            &resultWaveform,
            &log,
            10
        );
        assert(nullptr != result && ret > 0);
        std::cout << "result: 不老不死_洛天依.flac (limit 10s) | " << ret / 256 << std::endl;
        assert(ret / 256 == 100);
        mediaxx_free(result);
        mediaxx_free(resultWaveform);
        mediaxx_free(log);
    }
    {
        std::cout << "mediaxx_get_media_picture with 512 display target ....... ===================="
                  << std::endl;
        const char* log = nullptr;
        auto        ret = mediaxx_get_media_picture(
            "./temp/林力尧 - 初恋旧爱新欢.flac",
            "",
            "./temp/中文输出/输出out-cover.jpg",
            "./temp/中文输出/输出out-cover96.jpg",
            "./temp/中文输出/输出out-cover512.jpg",
            &log
        );
        std::cout << "mediaxx_get_media_picture ret: " << ret << std::endl;
        assert((ret & 1) != 0); // 原图成功
        assert((ret & 2) != 0); // 96 缩略图成功
        assert((ret & 4) != 0); // 512 显示图成功
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
            mediaxx_free(result);
            mediaxx_free(log);
        } else {
            std::cout << "无法打开文件进行二进制读取" << std::endl;
        }
    }
}


void test_fftools() {
    std::cout << "\n======= Testing FFTools (ffmpeg & ffprobe) =======" << std::endl;

    // 1. ffmpeg -version 测试标准输出捕获
    {
        const std::string args = "[\"ffmpeg\", \"-version\"]";
        const char* out_stdout = nullptr;
        const char* out_stderr = nullptr;
        int ret = mediaxx_execute_ffmpeg(args.data(), args.size(), &out_stdout, &out_stderr, nullptr);
        std::cout << "mediaxx_execute_ffmpeg -version ret: " << ret << std::endl;
        assert(ret == 0);
        assert(out_stdout != nullptr);
        assert(std::string(out_stdout).find("ffmpeg version") != std::string::npos);
        std::cout << "ffmpeg stdout preview: " << std::string(out_stdout).substr(0, 50) << "..." << std::endl;
        mediaxx_free(out_stdout);
        mediaxx_free(out_stderr);
    }

    // 2. ffprobe -version 测试标准输出捕获
    {
        const std::string args = "[\"ffprobe\", \"-version\"]";
        const char* out_stdout = nullptr;
        const char* out_stderr = nullptr;
        int ret = mediaxx_execute_ffprobe(args.data(), args.size(), &out_stdout, &out_stderr, nullptr);
        std::cout << "mediaxx_execute_ffprobe -version ret: " << ret << std::endl;
        assert(ret == 0);
        assert(out_stdout != nullptr);
        assert(std::string(out_stdout).find("ffprobe version") != std::string::npos);
        std::cout << "ffprobe stdout preview: " << std::string(out_stdout).substr(0, 50) << "..." << std::endl;
        mediaxx_free(out_stdout);
        mediaxx_free(out_stderr);
    }

    // 3. 测试自动补全程序名（首项直接以 - 开头）
    {
        const std::string args = "[\"-version\"]";
        const char* out_stdout = nullptr;
        const char* out_stderr = nullptr;
        int ret = mediaxx_execute_ffmpeg(args.data(), args.size(), &out_stdout, &out_stderr, nullptr);
        std::cout << "mediaxx_execute_ffmpeg without progname ret: " << ret << std::endl;
        assert(ret == 0);
        assert(out_stdout != nullptr && std::string(out_stdout).find("ffmpeg version") != std::string::npos);
        mediaxx_free(out_stdout);
        mediaxx_free(out_stderr);

        out_stdout = nullptr;
        out_stderr = nullptr;
        ret = mediaxx_execute_ffprobe(args.data(), args.size(), &out_stdout, &out_stderr, nullptr);
        std::cout << "mediaxx_execute_ffprobe without progname ret: " << ret << std::endl;
        assert(ret == 0);
        assert(out_stdout != nullptr && std::string(out_stdout).find("ffprobe version") != std::string::npos);
        mediaxx_free(out_stdout);
        mediaxx_free(out_stderr);
    }

    // 4. 重复调用测试：多次使用 ffmpeg 进行图片提取和处理
    const std::string inputFile = "./temp/Gold Town_M2U.mp3";
    for (int i = 0; i < 3; ++i) {
        std::string outFile = "/tmp/mediaxx_test_out_" + std::to_string(i) + ".jpg";
        std::string args = "[\"ffmpeg\", \"-y\", \"-i\", \"" + inputFile + "\", \"-map\", \"0:v\", \"-c:v\", \"copy\", \"" + outFile + "\"]";
        const char* out_stdout = nullptr;
        const char* out_stderr = nullptr;
        int ret = mediaxx_execute_ffmpeg(args.data(), args.size(), &out_stdout, &out_stderr, nullptr);
        std::cout << "Repeated ffmpeg extract cover #" << i << " ret: " << ret << std::endl;
        assert(ret == 0);
        assert(std::filesystem::exists(outFile));
        assert(std::filesystem::file_size(outFile) > 0);
        std::filesystem::remove(outFile);
        assert(out_stderr != nullptr);
        mediaxx_free(out_stdout);
        mediaxx_free(out_stderr);
    }

    // 5. 重复调用测试：使用 ffprobe 输出 JSON 到 stdout，验证捕获的内容为合法 JSON
    for (int i = 0; i < 3; ++i) {
        std::string args = "[\"ffprobe\", \"-v\", \"quiet\", \"-print_format\", \"json\", \"-show_format\", \"-show_streams\", \"-i\", \"" + inputFile + "\"]";
        const char* out_stdout = nullptr;
        const char* out_stderr = nullptr;
        int ret = mediaxx_execute_ffprobe(args.data(), args.size(), &out_stdout, &out_stderr, nullptr);
        std::cout << "Repeated ffprobe #" << i << " ret: " << ret << std::endl;
        assert(ret == 0);
        assert(out_stdout != nullptr);
        simdjson::dom::parser p;
        simdjson::dom::element doc;
        simdjson::padded_string padded(out_stdout, strlen(out_stdout));
        assert(p.parse(padded).get(doc) == simdjson::SUCCESS);
        mediaxx_free(out_stdout);
        mediaxx_free(out_stderr);
    }

    // 6. 异常参数与不存在文件测试：确保返回非0退出码并捕获错误日志
    {
        // 非法 JSON
        const char* badJson = "{invalid json}";
        const char* out_stdout = nullptr;
        const char* out_stderr = nullptr;
        int ret = mediaxx_execute_ffmpeg(badJson, strlen(badJson), &out_stdout, &out_stderr, nullptr);
        std::cout << "ffmpeg invalid json ret: " << ret << std::endl;
        assert(ret != 0);
        assert(out_stderr != nullptr);
        mediaxx_free(out_stdout);
        mediaxx_free(out_stderr);

        // 不存在文件
        std::string nonExistArgs = "[\"ffmpeg\", \"-i\", \"/tmp/non_existent_file_xyz_12345.mp4\", \"/tmp/should_fail.wav\"]";
        out_stdout = nullptr;
        out_stderr = nullptr;
        ret = mediaxx_execute_ffmpeg(nonExistArgs.data(), nonExistArgs.size(), &out_stdout, &out_stderr, nullptr);
        std::cout << "ffmpeg non-existent file ret (should be non-zero): " << ret << std::endl;
        assert(ret != 0);
        assert(out_stderr != nullptr);
        mediaxx_free(out_stdout);
        mediaxx_free(out_stderr);

        std::string nonExistProbe = "[\"ffprobe\", \"-i\", \"/tmp/non_existent_file_xyz_12345.mp4\"]";
        out_stdout = nullptr;
        out_stderr = nullptr;
        ret = mediaxx_execute_ffprobe(nonExistProbe.data(), nonExistProbe.size(), &out_stdout, &out_stderr, nullptr);
        std::cout << "ffprobe non-existent file ret (should be non-zero): " << ret << std::endl;
        assert(ret != 0);
        assert(out_stderr != nullptr);
        mediaxx_free(out_stdout);
        mediaxx_free(out_stderr);
    }

    // 7. 多线程并发调用测试
    {
        std::cout << "Starting multi-threaded concurrency test..." << std::endl;
        const int numThreads = 6;
        std::vector<std::thread> threads;
        std::vector<int> results(numThreads, -1);

        for (int i = 0; i < numThreads; ++i) {
            threads.emplace_back([i, &results, &inputFile]() {
                if (i % 2 == 0) {
                    std::string outPath = "/tmp/mediaxx_thread_" + std::to_string(i) + ".jpg";
                    std::string args = "[\"ffmpeg\", \"-y\", \"-i\", \"" + inputFile + "\", \"-map\", \"0:v\", \"-c:v\", \"copy\", \"" + outPath + "\"]";
                    const char* out_stdout = nullptr;
                    const char* out_stderr = nullptr;
                    results[i] = mediaxx_execute_ffmpeg(args.data(), args.size(), &out_stdout, &out_stderr, nullptr);
                    if (std::filesystem::exists(outPath)) {
                        assert(std::filesystem::file_size(outPath) > 0);
                        std::filesystem::remove(outPath);
                    }
                    mediaxx_free(out_stdout);
                    mediaxx_free(out_stderr);
                } else {
                    std::string args = "[\"ffprobe\", \"-v\", \"quiet\", \"-print_format\", \"json\", \"-show_format\", \"-i\", \"" + inputFile + "\"]";
                    const char* out_stdout = nullptr;
                    const char* out_stderr = nullptr;
                    results[i] = mediaxx_execute_ffprobe(args.data(), args.size(), &out_stdout, &out_stderr, nullptr);
                    assert(out_stdout != nullptr);
                    simdjson::dom::parser p;
                    simdjson::dom::element doc;
                    simdjson::padded_string padded(out_stdout, strlen(out_stdout));
                    assert(p.parse(padded).get(doc) == simdjson::SUCCESS);
                    mediaxx_free(out_stdout);
                    mediaxx_free(out_stderr);
                }
            });
        }

        for (auto& t : threads) {
            t.join();
        }
        for (int i = 0; i < numThreads; ++i) {
            std::cout << "Thread #" << i << " finished with ret: " << results[i] << std::endl;
            assert(results[i] == 0);
        }
        std::cout << "Multi-threaded concurrency test PASSED!" << std::endl;
    }

    // 8. 日志级别只在当次调用里生效：用了 -v 之后，下一次调用仍然回到默认级别
    {
        // 库的日志级别先设成默认的 INFO，后面的断言以此为准
        mediaxx_set_log_level(AV_LOG_INFO);

        // 只给 -i 不给输出文件：ffmpeg 会打印 Input #0（INFO）后再报错返回，
        // 所以这里既能验证日志级别，又不需要编码器
        std::string args = "[\"ffmpeg\", \"-hide_banner\", \"-v\", \"error\", \"-i\", \"" + inputFile + "\"]";
        const char* out_stdout = nullptr;
        const char* out_stderr = nullptr;
        int ret = mediaxx_execute_ffmpeg(args.data(), args.size(), &out_stdout, &out_stderr, nullptr);
        std::cout << "quiet input-only ret: " << ret << ", stderr bytes: " << (out_stderr ? strlen(out_stderr) : 0) << std::endl;
        assert(ret != 0); // 没有输出文件，ffmpeg 必然报错返回
        assert(out_stderr != nullptr);
        assert(std::string(out_stderr).find("Input #0") == std::string::npos);
        mediaxx_free(out_stdout);
        mediaxx_free(out_stderr);

        // 紧接着不带 -v 再跑一次：级别应已还原成进入前的值，stderr 里有 Input #0
        args = "[\"ffmpeg\", \"-hide_banner\", \"-i\", \"" + inputFile + "\"]";
        out_stdout = nullptr;
        out_stderr = nullptr;
        ret = mediaxx_execute_ffmpeg(args.data(), args.size(), &out_stdout, &out_stderr, nullptr);
        std::cout << "default level input-only ret: " << ret << ", stderr bytes: " << (out_stderr ? strlen(out_stderr) : 0) << std::endl;
        assert(ret != 0);
        assert(out_stderr != nullptr);
        assert(std::string(out_stderr).find("Input #0") != std::string::npos);
        mediaxx_free(out_stdout);
        mediaxx_free(out_stderr);
    }

    // 9. 调过 -version 之后，后面调用的日志不会混进 stdout
    {
        std::string args = "[\"ffmpeg\", \"-version\"]";
        const char* out_stdout = nullptr;
        const char* out_stderr = nullptr;
        int ret = mediaxx_execute_ffmpeg(args.data(), args.size(), &out_stdout, &out_stderr, nullptr);
        std::cout << "ffmpeg -version ret: " << ret << std::endl;
        assert(ret == 0);
        assert(out_stdout != nullptr && std::string(out_stdout).find("ffmpeg version") != std::string::npos);
        mediaxx_free(out_stdout);
        mediaxx_free(out_stderr);

        // -version 这类选项会把日志回调换成写 stdout 的版本，这里用 ffprobe 输出 JSON，
        // 验证它没有再留到后面的调用里（留下的话 stdout 里会先出现 banner，JSON 就解析不了）
        args = "[\"ffprobe\", \"-v\", \"error\", \"-print_format\", \"json\", \"-show_format\", \"-i\", \"" + inputFile + "\"]";
        out_stdout = nullptr;
        out_stderr = nullptr;
        ret = mediaxx_execute_ffprobe(args.data(), args.size(), &out_stdout, &out_stderr, nullptr);
        std::cout << "after -version ffprobe ret: " << ret << ", stdout bytes: " << (out_stdout ? strlen(out_stdout) : 0) << std::endl;
        assert(ret == 0);
        assert(out_stdout != nullptr);
        simdjson::dom::parser p;
        simdjson::dom::element doc;
        simdjson::padded_string padded(out_stdout, strlen(out_stdout));
        assert(p.parse(padded).get(doc) == simdjson::SUCCESS);
        mediaxx_free(out_stdout);
        mediaxx_free(out_stderr);
    }

    std::cout << "======= FFTools Tests PASSED! =======\n" << std::endl;
}

int main(int argn, char** argv) {
#if IS_LINUX_D
    mediaxx::util::signalError(argv[0]);
#endif
    std::cout << "======= Test Start =======" << std::endl;
    test();
    test_fftools();
    std::cout << "======= Test Done =======" << std::endl;
    std::cout << ">>>";
    // int num = 0;
    // cin >> num;
    return 0;
}