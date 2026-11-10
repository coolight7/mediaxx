#pragma once

#include "analyse/media_name_symbol.h"
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace mediaxx {

    /// 多模式匹配（对应 Dart 的 AhoCorasick，行为逐条对齐）
    ///
    /// * 在 UTF-16 码元上匹配（与 Dart 的 `String.codeUnits` 一致，非 BMP 字符按代理对处理）；
    /// * 折叠规则：先查码元折叠表（繁体→简体、各种括号/连字符/引号归一），再把 A-Z 转小写；
    ///   与 Dart 的 `AhoCorasick.onCharCode` 相同，**不做**完整 Unicode 小写；
    /// * 模式串按建表时的原样保存，[pattern] 返回的也是原样（大小写保留）：命中歌手名后
    ///   调用处要把它当结果输出，小写化会丢掉 `Taylor Swift` 这样的大小写信息；
    /// * `search(..., onlyContains) = true` 返回"最早结束"的一个匹配（Dart 里也是直接返回，
    ///   没有再排序）；为假时返回按（起点升序、终点降序）排序后互不重叠的匹配。
    class MediaNameAhoCorasick_c {
    public:

        struct Match_c {
            size_t start        = 0;
            size_t end          = 0;
            size_t patternIndex = 0;

            /// 起点升序、终点降序（与 Dart 的 ACMatch.cDefCompare 相同）
            static bool less(const Match_c& left, const Match_c& right) {
                if (left.start != right.start) {
                    return left.start < right.start;
                }
                return right.end < left.end;
            }
        };

        MediaNameAhoCorasick_c() = default;

        explicit MediaNameAhoCorasick_c(const std::vector<std::u16string>& patterns);

        [[nodiscard]] std::vector<Match_c>
            search(const std::u16string& text, size_t start = 0, bool onlyContains = false) const;

        [[nodiscard]] bool contains(const std::u16string& text, size_t start = 0) const;

        /// 删掉全部匹配（[start] 之前的保留）
        [[nodiscard]] std::u16string removeAll(const std::u16string& text, size_t start = 0) const;

        [[nodiscard]] const std::u16string& pattern(size_t index) const {
            return patterns[index];
        }

        [[nodiscard]] bool empty() const {
            return patterns.empty();
        }

    private:

        struct Node_c {
            std::unordered_map<char16_t, int> children{};
            std::vector<int>                  outputs{};
            int                               fail = 0;
        };

        void addPattern(const std::u16string& pattern);
        void build();

        std::vector<Node_c>         nodes{};
        std::vector<std::u16string> patterns{};
        std::vector<size_t>         patternLen{};
    };

    /// 码元折叠与"忽略大小写"的规范化
    class MediaNameFold_c {
    public:

        /// 匹配用的码元折叠（对应 AhoCorasick.onCharCode：先查折叠表，再转 ASCII 小写）
        [[nodiscard]] static char16_t fold(char16_t codeUnit);

        /// 整串按码点做完整小写（对应 Dart 的 String.toLowerCase）
        [[nodiscard]] static std::u16string toLower(std::u16string_view str);

        /// 忽略大小写比较用的键（等价于 `StringUtilxx_c.isIgnoreCaseEqual`：
        /// 先比长度再比小写，所以小写后长度会变的字符保持原样）
        [[nodiscard]] static std::u16string canonical(std::u16string_view str);
    };

    /// 歌名 / 歌手智能识别（对应 Dart 的 `MediaInfoAnalyseBase_c` 与它的两个子类）
    ///
    /// 两个子类只有两处不同：无效标签表 / 弱标签表的内容，以及"临时串怎么拼回歌名"
    /// （`onAddTempStr`）。数据表全部来自 media_name_tables.h（从 Dart 运行期导出）。
    class MediaNameAnalyseBase_c {
    public:

        /// 一次解析的结果（对应 Dart 的 `(reName, reArtists, addRangeTag, tempStr)`）
        struct Result_c {
            std::u16string              reName{};
            std::vector<std::u16string> reArtists{};
            /// 空 = Dart 里的 `null`
            std::optional<bool> addRangeTag{};
            std::u16string      tempStr{};
        };

        MediaNameAnalyseBase_c()          = default;
        virtual ~MediaNameAnalyseBase_c() = default;

        /// 解析一个名字（歌名或歌词搜索串的来源名）
        ///
        /// * [crudeArtistlist]：已知歌手（歌名分析传 artistlist，歌词搜索串传切分出来的歌手）
        Result_c
            analyse(const std::u16string& name, const std::vector<std::u16string>& crudeArtistlist);

        /// 歌手切分（对应 `sliceSongArtist`）
        /// * [append]：1 = 保留原值并追加映射值，0 = 只保留原值，-1 = 用映射值替换原值
        /// * [weakArtist]：只保留别名表里登记过的名字
        /// * [outCrude]：去掉两端空白、其它保持原样的输入（Dart 返回的第二项）
        static std::vector<std::u16string> sliceSongArtist(
            const std::u16string& artist,
            int                   append,
            bool                  weakArtist,
            std::u16string&       outCrude
        );

        /// 可用歌曲信息（对应 `getAvailSongInfo`，返回空 = Dart 里的 null）
        static std::optional<std::u16string> getAvailSongInfo(
            const std::optional<std::u16string>& name,
            const std::optional<std::u16string>& notAvailResult
        );

        /// 这一行歌词是否可用（对应 `checkAvailLyricLine`）
        [[nodiscard]] static bool checkAvailLyricLine(const std::u16string& line);

        /// 别名反查（对应 `artistTagWordVK[artist]`，结果按字符串排序）
        [[nodiscard]] static std::vector<std::u16string> artistAliases(const std::u16string& artist
        );

        // 两个子类各自覆盖的表（对应 Dart 里每个实例自己的一份 addAll 结果）
        std::unordered_map<std::u16string, std::u16string> weakTagWord{};
        std::unordered_set<std::u16string>                 unavailTagWord{};

    protected:

        /// 把「挪到末尾的临时串」拼回结果（基类不拼；歌名分析会拼）
        [[nodiscard]] virtual std::u16string onAddTempStr(
            const std::u16string&      result,
            const std::optional<bool>& addRangeTag,
            const std::u16string&      tempStr
        ) const;
    };

    /// 歌名分析（对应 `MediaInfoAnalyse_c.songNameAnalyse`）
    class MediaInfoNameAnalyse_c : public MediaNameAnalyseBase_c {
    public:

        static MediaInfoNameAnalyse_c& instance();

        /// 歌名 + 已知歌手 →（歌名, 歌手集合）
        std::pair<std::u16string, std::vector<std::u16string>> getNameArtist(
            const std::u16string&              name,
            const std::vector<std::u16string>& artistlist
        );

    protected:

        [[nodiscard]] std::u16string onAddTempStr(
            const std::u16string&      result,
            const std::optional<bool>& addRangeTag,
            const std::u16string&      tempStr
        ) const override;

        MediaInfoNameAnalyse_c();
    };

    /// 歌词搜索串（对应 `LyricSearchStrAnalyse.lyricSearchStrAnalyse`）
    class LyricSearchStrAnalyse_c : public MediaNameAnalyseBase_c {
    public:

        static LyricSearchStrAnalyse_c& instance();

        /// 歌名 + 歌手 → 搜索用的字符串（截断到 50 个码元以内，下划线换成连字符）
        std::u16string createLyricSearchStr(
            const std::u16string& name,
            const std::u16string& artist,
            bool                  weakArtist
        );

    protected:

        LyricSearchStrAnalyse_c();
    };

} // namespace mediaxx
