// 由 script/gen_media_name_tables.dart 从 script/data/media_name/*.json 生成，不要手改。
//
// 这里是「智能识别」（mediaxx 的 MediaNameAnalyse_c）用到的数据表的声明，数据本身在
// 同目录的 media_name_tables.cpp。
//   * kCaseFold / kPunctuationSymbolRanges / kSpaceRanges：Dart 的 String.toLowerCase()
//     与正则引擎（\p{P}\p{S}、\s）逐码点跑出来的结果；
//   * kCodeUnitShift：等价于 MediaInfoAnalyseBase_c.shiftCodeDict
//     （繁→简与符号归一，键值都只取第一个码元）；
//   * kArtistTagWord / kArtistCombinaWord / kWeakTagWord*：别名与弱标签表，键保留原始大小写
//     （AhoCorasick 命中后会把模式串当结果输出），C++ 加载时用 canonical() 生成查询键；
//   * kUnavailWord / kUnavailTagWord* / kNotAvail*：无效词与可用性判断用到的词表。
//
// 数组维度写在声明里，包含这个头文件的编译单元才能用 std::size() 与范围 for。
//
// 数据来源与重新生成的步骤见 script/README.md。
#pragma once

#include <cstdint>
#include <string_view>

namespace mediaxx {
    namespace media_name_tables {

        struct CodePointPair_c {
            int32_t from;
            int32_t to;
        };

        struct CodePointRange_c {
            int32_t start;
            int32_t end;
        };

        struct CodeUnitPair_c {
            char16_t from;
            char16_t to;
        };

        struct StringPair_c {
            std::u16string_view key;
            std::u16string_view value;
        };

        /// 码点的小写映射（String.toLowerCase 结果仍是单个码点的那些）
        extern const CodePointPair_c kCaseFold[1023];

        /// \p{P}\p{S} 的码点区间
        extern const CodePointRange_c kPunctuationSymbolRanges[355];

        /// \s 的码点区间
        extern const CodePointRange_c kSpaceRanges[10];

        /// 码元折叠表（繁体→简体、各种括号/连字符/引号归一），键值都只有第一个码元
        extern const CodeUnitPair_c kCodeUnitShift[2729];

        /// 组合歌手词（整体保留，不按 & 切分）
        extern const StringPair_c kArtistCombinaWord[48];

        /// 歌手别名表（键是原始写法，查询时用 canonical() 规范化）
        extern const StringPair_c kArtistTagWord[1647];

        /// 弱标签（歌名分析）
        extern const StringPair_c kWeakTagWordSong[13];

        /// 弱标签（歌词搜索串）
        extern const StringPair_c kWeakTagWordLyric[23];

        /// 无效词（歌名里的广告与推广语）
        extern const std::u16string_view kUnavailWord[372];

        /// 无效标签（歌名分析）
        extern const std::u16string_view kUnavailTagWordSong[377];

        /// 无效标签在歌词搜索串里额外多出来的那些
        extern const std::u16string_view kUnavailTagWordLyricExtra[5];

        /// 不可用歌曲信息词
        extern const std::u16string_view kNotAvailInfoWord[44];

        /// 不可用歌曲信息（整串相等）
        extern const std::u16string_view kNotAvailInfoStr[2];

        /// 无效歌词行（整行相等）
        extern const std::u16string_view kNotAvailLyricLine[7];

        /// 无效歌词关键词
        extern const std::u16string_view kNotAvailLyricWord[65];

        /// 歌名首尾要去掉的符号码元（MediaInfoAnalyseBase_c.groupTagCode）
        extern const char16_t kGroupTagCode[24];

        /// 歌手分隔符码元（MediaInfoAnalyseBase_c.songArtistSplitCode）
        extern const char16_t kArtistSplitCode[13];

    } // namespace media_name_tables
} // namespace mediaxx
