#include "analyse/media_name.h"

#include <algorithm>
#include <array>
#include <iterator>

namespace mediaxx {

    namespace {

        using namespace media_name_tables;

        // ===================================================================
        // 小工具
        // ===================================================================

        /// 去掉两端的 空格/\t/\r/\n（对应 `StringUtilxx_c.removeBetweenSpace`，只去这几个字符）
        /// * 返回视图，不拷贝；[str] 必须在返回值使用期间有效
        [[nodiscard]] std::u16string_view trimSpacesView(std::u16string_view str) {
            auto isTrim = [](char16_t code) {
                return code == u' ' || code == u'\t' || code == u'\r' || code == u'\n';
            };
            size_t left  = 0;
            size_t right = str.size();
            while (left < right && isTrim(str[left])) {
                ++left;
            }
            while (right > left && isTrim(str[right - 1])) {
                --right;
            }
            return str.substr(left, right - left);
        }

        /// 同 [trimSpacesView]，需要独立字符串时用它
        [[nodiscard]] std::u16string trimSpaces(std::u16string_view str) {
            return std::u16string{trimSpacesView(str)};
        }

        /// 同上，结果为空时返回空（对应 `removeBetweenSpaceMayNull`）
        [[nodiscard]] std::optional<std::u16string> trimSpacesMayNull(std::u16string_view str) {
            const std::u16string result = trimSpaces(str);
            if (result.empty()) {
                return std::nullopt;
            }
            return result;
        }

        /// `StringUtilxx_c.subString` 的安全版本（原版越界时返回 null，调用处都保证不越界）
        [[nodiscard]] std::u16string_view
            subStringView(std::u16string_view str, size_t start, size_t end) {
            if (start > str.size()) {
                return {};
            }
            if (end > str.size() || end < start) {
                return str.substr(start);
            }
            return str.substr(start, end - start);
        }

        /// 按给定码元集合切分（对应 Dart 的 `String.split(RegExp)`，保留空串分片）
        /// * 返回的是 [str] 的分片视图
        [[nodiscard]] std::vector<std::u16string_view>
            splitByCodeSet(std::u16string_view str, const std::unordered_set<char16_t>& codes) {
            std::vector<std::u16string_view> result{};
            size_t                           start = 0;
            for (size_t i = 0; i < str.size(); ++i) {
                if (codes.count(str[i]) > 0) {
                    result.push_back(str.substr(start, i - start));
                    start = i + 1;
                }
            }
            result.push_back(str.substr(start));
            return result;
        }

        /// 码点写回 UTF-16
        void appendCodePoint(std::u16string& str, int32_t codePoint) {
            if (codePoint <= 0xFFFF) {
                str.push_back(static_cast<char16_t>(codePoint));
                return;
            }
            const int32_t value = codePoint - 0x10000;
            str.push_back(static_cast<char16_t>(0xD800 + (value >> 10)));
            str.push_back(static_cast<char16_t>(0xDC00 + (value & 0x3FF)));
        }

        /// 码点的小写映射（表里没有就返回原值）
        [[nodiscard]] int32_t foldCodePoint(int32_t codePoint) {
            const auto* list  = kCaseFold;
            size_t      begin = 0;
            size_t      end   = std::size(kCaseFold);
            while (begin < end) {
                const size_t middle = begin + (end - begin) / 2;
                if (codePoint < list[middle].from) {
                    end = middle;
                } else if (codePoint > list[middle].from) {
                    begin = middle + 1;
                } else {
                    return list[middle].to;
                }
            }
            return codePoint;
        }

        /// BMP 码元的小写表（没有映射就是原值）
        ///
        /// 查表比 [foldCodePoint] 的二分快，热路径（判断一个串要不要小写、或者把它小写）都用它。
        [[nodiscard]] const std::array<int32_t, 65536>& lowerTable() {
            static const std::array<int32_t, 65536> table = [] {
                std::array<int32_t, 65536> result{};
                for (int32_t code = 0; code <= 0xFFFF; ++code) {
                    result[static_cast<size_t>(code)] = code;
                }
                for (const auto& item : kCaseFold) {
                    if (item.from >= 0 && item.from <= 0xFFFF) {
                        result[static_cast<size_t>(item.from)] = item.to;
                    }
                }
                return result;
            }();
            return table;
        }

        /// 单个码点的小写：BMP 查表，非 BMP 才二分
        [[nodiscard]] int32_t lowerCodePoint(int32_t codePoint) {
            if (codePoint >= 0 && codePoint <= 0xFFFF) {
                return lowerTable()[static_cast<size_t>(codePoint)];
            }
            return foldCodePoint(codePoint);
        }

        /// 规范化用的复用缓冲（每个线程一份；只有真的需要小写时才会被写入）
        [[nodiscard]] std::u16string& canonicalScratch() {
            static thread_local std::u16string buffer{};
            return buffer;
        }

        /// 匹配用的码元折叠表（先查码元折叠，再转 ASCII 小写）
        [[nodiscard]] const std::array<char16_t, 65536>& foldTable() {
            static const std::array<char16_t, 65536> table = [] {
                std::array<char16_t, 65536> result{};
                for (int32_t code = 0; code <= 0xFFFF; ++code) {
                    result[static_cast<size_t>(code)] = static_cast<char16_t>(code);
                }
                for (const auto& item : kCodeUnitShift) {
                    result[static_cast<size_t>(item.from)] = item.to;
                }
                for (int32_t code = 0; code <= 0xFFFF; ++code) {
                    const char16_t value = result[static_cast<size_t>(code)];
                    if (value >= u'A' && value <= u'Z') {
                        result[static_cast<size_t>(code)]
                            = static_cast<char16_t>(value + (u'a' - u'A'));
                    }
                }
                return result;
            }();
            return table;
        }

        /// 表里的字符串对 → 忽略大小写查询用的映射
        ///
        /// 表里的键保留原始大小写（AhoCorasick 命中后会把模式串原样当结果输出，
        /// 例如 `artistSearcher.pattern()` 要返回 `Taylor Swift`），查询键由
        /// [MediaNameFold_c::canonical] 生成，所以加载时就统一规范化。
        template<size_t N>
        [[nodiscard]] MediaNameMap_c<std::u16string> toMap(const StringPair_c (&list)[N]) {
            MediaNameMap_c<std::u16string> result{};
            result.reserve(N);
            for (const auto& item : list) {
                result[MediaNameFold_c::canonical(item.key)] = std::u16string{item.value};
            }
            return result;
        }

        /// 表里的字符串 → 忽略大小写查询用的集合（键规范化）
        template<size_t N>
        [[nodiscard]] MediaNameSet_c toSet(const std::u16string_view (&list)[N]) {
            MediaNameSet_c result{};
            result.reserve(N);
            for (const auto& item : list) {
                result.insert(MediaNameFold_c::canonical(item));
            }
            return result;
        }

        template<size_t N>
        [[nodiscard]] std::vector<std::u16string> toPatterns(const std::u16string_view (&list)[N]) {
            std::vector<std::u16string> result{};
            result.reserve(N);
            for (const auto& item : list) {
                result.emplace_back(item);
            }
            return result;
        }

        template<size_t N>
        [[nodiscard]] std::vector<std::u16string> toPatterns(const StringPair_c (&list)[N]) {
            std::vector<std::u16string> result{};
            result.reserve(N);
            for (const auto& item : list) {
                result.emplace_back(item.key);
            }
            return result;
        }

        template<size_t N>
        [[nodiscard]] std::unordered_set<char16_t> toCodeSet(const char16_t (&list)[N]) {
            return std::unordered_set<char16_t>{std::begin(list), std::end(list)};
        }

        /// `XXGlobalUtil_c.isUndefineString`：含代理码元 / FFFC~FFFD / 私有使用区的串当作乱码
        /// * Dart 那边是**非 unicode** 正则，按码元匹配，所以正常 emoji 也会命中
        [[nodiscard]] bool isUndefineString(const std::u16string& str) {
            for (const char16_t code : str) {
                if ((code >= 0xD800 && code <= 0xDFFF) || (code >= 0xFFFC && code <= 0xFFFD)
                    || (code >= 0xE000 && code <= 0xF8FF)) {
                    return true;
                }
            }
            return false;
        }

        /// `XXGlobalUtil_c.getAvailString`
        [[nodiscard]] std::optional<std::u16string>
            getAvailString(const std::optional<std::u16string>& str) {
            if (false == str.has_value() || str->empty() || isUndefineString(*str)) {
                return std::nullopt;
            }
            return str;
        }

        /// `createLyricSearchStr` 开头的「数字/空白/分隔符」前缀剥离
        /// （对应 `RegExp(r"^[\d\s\.\|\/\\:：、]+")`）
        [[nodiscard]] std::u16string stripLeadingSearchPrefix(const std::u16string& str) {
            size_t index = 0;
            while (index < str.size()) {
                const int32_t codePoint = MediaNameSymbol_c::codePointAt(str, index);
                const bool    isPrefix  = (codePoint >= 0x30 && codePoint <= 0x39)
                                      || MediaNameSymbol_c::isSpace(codePoint) || codePoint == u'.'
                                      || codePoint == u'|' || codePoint == u'/'
                                      || codePoint == u'\\' || codePoint == u':'
                                      || codePoint == 0xFF1A || codePoint == 0x3001;
                if (false == isPrefix) {
                    break;
                }
                index += MediaNameSymbol_c::codePointWidth(codePoint);
            }
            return (0 == index) ? str : str.substr(index);
        }

        /// 忽略大小写去重后加入（Dart 那边是 `HashSet<String>`，先加入的保留）
        void addUnique(std::vector<std::u16string>& list, std::u16string_view item) {
            if (item.empty()) {
                return;
            }
            const std::u16string key = MediaNameFold_c::canonical(item);
            for (const auto& exist : list) {
                if (MediaNameFold_c::canonicalView(exist) == key) {
                    return;
                }
            }
            list.emplace_back(item);
        }

        /// 单个模式的匹配器
        [[nodiscard]] MediaNameAhoCorasick_c makeSingleSearcher(std::u16string_view pattern) {
            const std::vector<std::u16string> list{std::u16string{pattern}};
            return MediaNameAhoCorasick_c{list};
        }

        // ===================================================================
        // 共享数据（各实例共用的表与匹配器：内容不可变，构建一次）
        // ===================================================================

        class MediaNameData_c {
        public:

            static const MediaNameData_c& instance() {
                static const MediaNameData_c data{};
                return data;
            }

            /// 歌手别名（键已规范化）
            MediaNameMap_c<std::u16string> artistTagWord{};
            /// 组合歌手词（整体保留，不按 & 切分）
            MediaNameMap_c<std::u16string> artistCombinaWord{};
            /// 歌手别名反查：规范名 → 表里出现的别名（已排序）
            MediaNameMap_c<std::vector<std::u16string>> artistTagWordVK{};
            /// 不可用歌曲信息（整串相等）
            MediaNameSet_c notAvailInfoStr{};
            /// 无效歌词行（整行相等）
            MediaNameSet_c notAvailLyricLine{};
            /// 歌名首尾要剥掉的符号码元
            std::unordered_set<char16_t> groupTagCode{};
            /// 歌手分隔符码元
            std::unordered_set<char16_t> artistSplitCode{};

            MediaNameAhoCorasick_c artistSearcher{};
            MediaNameAhoCorasick_c artistCombinaSearcher{};
            MediaNameAhoCorasick_c unavailWordSearcher{};
            MediaNameAhoCorasick_c notAvailInfoWordSearcher{};
            MediaNameAhoCorasick_c notAvailLyricWordSearcher{};
            MediaNameAhoCorasick_c groupTagQuoteLeftSearcher{};
            MediaNameAhoCorasick_c groupTagQuoteRightSearcher{};
            MediaNameAhoCorasick_c groupTagBracketLeftSearcher{};
            MediaNameAhoCorasick_c groupTagBracketRightSearcher{};
            MediaNameAhoCorasick_c groupTagGuillemetLeftSearcher{};
            MediaNameAhoCorasick_c groupTagGuillemetRightSearcher{};

            /// 别名反查结果
            [[nodiscard]] std::vector<std::u16string> aliasesOf(std::u16string_view artist) const {
                const auto it = artistTagWordVK.find(MediaNameFold_c::canonicalView(artist));
                if (it == artistTagWordVK.end()) {
                    return {};
                }
                return it->second;
            }

        private:

            MediaNameData_c() {
                artistTagWord     = toMap(kArtistTagWord);
                artistCombinaWord = toMap(kArtistCombinaWord);
                notAvailInfoStr   = toSet(kNotAvailInfoStr);
                notAvailLyricLine = toSet(kNotAvailLyricLine);
                groupTagCode      = toCodeSet(kGroupTagCode);
                artistSplitCode   = toCodeSet(kArtistSplitCode);

                // 别名反查：只登记有映射值的项（对应 createIgnoreCaseMapValueKey）
                for (const auto& item : kArtistTagWord) {
                    if (item.value.empty()) {
                        continue;
                    }
                    auto& list = artistTagWordVK[MediaNameFold_c::canonical(item.value)];
                    list.emplace_back(item.key);
                }
                for (auto& item : artistTagWordVK) {
                    std::sort(item.second.begin(), item.second.end());
                }

                artistSearcher            = MediaNameAhoCorasick_c{toPatterns(kArtistTagWord)};
                artistCombinaSearcher     = MediaNameAhoCorasick_c{toPatterns(kArtistCombinaWord)};
                unavailWordSearcher       = MediaNameAhoCorasick_c{toPatterns(kUnavailWord)};
                notAvailInfoWordSearcher  = MediaNameAhoCorasick_c{toPatterns(kNotAvailInfoWord)};
                notAvailLyricWordSearcher = MediaNameAhoCorasick_c{toPatterns(kNotAvailLyricWord)};

                groupTagQuoteLeftSearcher      = makeSingleSearcher(u"\u201C");
                groupTagQuoteRightSearcher     = makeSingleSearcher(u"\u201D");
                groupTagBracketLeftSearcher    = makeSingleSearcher(u"\u3010");
                groupTagBracketRightSearcher   = makeSingleSearcher(u"\u3011");
                groupTagGuillemetLeftSearcher  = makeSingleSearcher(u"\u300A");
                groupTagGuillemetRightSearcher = makeSingleSearcher(u"\u300B");
            }
        };

    } // namespace

    // ===================================================================
    // MediaNameFold_c
    // ===================================================================

    char16_t MediaNameFold_c::fold(char16_t codeUnit) {
        return foldTable()[static_cast<size_t>(codeUnit)];
    }

    std::u16string MediaNameFold_c::toLower(std::u16string_view str) {
        std::u16string result{};
        toLowerInto(result, str);
        return result;
    }

    void MediaNameFold_c::toLowerInto(std::u16string& into, std::u16string_view str) {
        into.clear();
        into.reserve(str.size());
        for (size_t index = 0; index < str.size();) {
            const char16_t code      = str[index];
            int32_t        codePoint = code;
            size_t         width     = 1;
            if (code >= 0xD800 && code <= 0xDBFF && index + 1 < str.size()) {
                const char16_t next = str[index + 1];
                if (next >= 0xDC00 && next <= 0xDFFF) {
                    codePoint = 0x10000 + ((code - 0xD800) << 10) + (next - 0xDC00);
                    width     = 2;
                }
            }
            appendCodePoint(into, lowerCodePoint(codePoint));
            index += width;
        }
    }

    std::u16string_view MediaNameFold_c::canonicalView(std::u16string_view str) {
        // 快路径：没有任何码点需要小写时直接用原视图（中文、已经小写的英文都走这里）
        bool needLower = false;
        for (size_t index = 0; index < str.size();) {
            const int32_t codePoint = MediaNameSymbol_c::codePointAt(str, index);
            if (lowerCodePoint(codePoint) != codePoint) {
                needLower = true;
                break;
            }
            index += MediaNameSymbol_c::codePointWidth(codePoint);
        }
        if (false == needLower) {
            return str;
        }
        std::u16string& buffer = canonicalScratch();
        toLowerInto(buffer, str);
        // 小写后长度变了（某些字母会展开成多个码点）就不算"同一个字符串"，
        // 与 Dart 的 isIgnoreCaseEqual（先比 length）一致
        if (buffer.size() != str.size()) {
            return str;
        }
        return buffer;
    }

    std::u16string MediaNameFold_c::canonical(std::u16string_view str) {
        return std::u16string{canonicalView(str)};
    }

    // ===================================================================
    // MediaNameAhoCorasick_c
    // ===================================================================

    MediaNameAhoCorasick_c::MediaNameAhoCorasick_c(const std::vector<std::u16string>& in_patterns) {
        nodes.emplace_back(); // 根节点
        for (const auto& pattern : in_patterns) {
            addPattern(pattern);
        }
        build();
    }

    void MediaNameAhoCorasick_c::addPattern(const std::u16string& pattern) {
        patterns.push_back(pattern);
        patternLen.push_back(pattern.size());
        int current = 0;
        for (const char16_t code : pattern) {
            const char16_t useChar = MediaNameFold_c::fold(code);
            const auto     found   = nodes[static_cast<size_t>(current)].children.find(useChar);
            if (found == nodes[static_cast<size_t>(current)].children.end()) {
                const int next = static_cast<int>(nodes.size());
                nodes[static_cast<size_t>(current)].children.emplace(useChar, next);
                nodes.emplace_back();
                current = next;
            } else {
                current = found->second;
            }
        }
        nodes[static_cast<size_t>(current)].outputs.push_back(
            static_cast<int>(patterns.size()) - 1
        );
    }

    void MediaNameAhoCorasick_c::build() {
        if (nodes.empty()) {
            return;
        }
        std::vector<int> queue{};
        nodes[0].fail = 0;
        for (const auto& entry : nodes[0].children) {
            nodes[static_cast<size_t>(entry.second)].fail = 0;
            queue.push_back(entry.second);
        }
        size_t head = 0;
        while (head < queue.size()) {
            const size_t current = static_cast<size_t>(queue[head]);
            ++head;
            for (const auto& entry : nodes[current].children) {
                const char16_t code  = entry.first;
                const size_t   child = static_cast<size_t>(entry.second);
                int            fail  = nodes[current].fail;
                while (fail != 0
                       && nodes[static_cast<size_t>(fail)].children.find(code)
                              == nodes[static_cast<size_t>(fail)].children.end()) {
                    fail = nodes[static_cast<size_t>(fail)].fail;
                }
                const auto found = nodes[static_cast<size_t>(fail)].children.find(code);
                if (fail != 0 && found != nodes[static_cast<size_t>(fail)].children.end()) {
                    nodes[child].fail = found->second;
                } else {
                    nodes[child].fail = 0;
                }
                // 合并输出：自己的 + 失败指针指向节点的
                const auto& failOutputs = nodes[static_cast<size_t>(nodes[child].fail)].outputs;
                nodes[child].outputs.insert(
                    nodes[child].outputs.end(),
                    failOutputs.begin(),
                    failOutputs.end()
                );
                queue.push_back(static_cast<int>(child));
            }
        }
    }

    std::vector<MediaNameAhoCorasick_c::Match_c>
        MediaNameAhoCorasick_c::search(std::u16string_view text, size_t start, bool onlyContains)
            const {
        std::vector<Match_c> matches{};
        if (nodes.empty() || start >= text.size()) {
            return matches;
        }
        size_t current = 0;
        for (size_t index = start; index < text.size(); ++index) {
            const char16_t useChar = MediaNameFold_c::fold(text[index]);
            while (current > 0
                   && nodes[current].children.find(useChar) == nodes[current].children.end()) {
                current = static_cast<size_t>(nodes[current].fail);
            }
            const auto found = nodes[current].children.find(useChar);
            current
                = (found == nodes[current].children.end()) ? 0 : static_cast<size_t>(found->second);
            for (const int patternId : nodes[current].outputs) {
                const size_t end         = index + 1;
                const size_t patternSize = patternLen[static_cast<size_t>(patternId)];
                if (end >= patternSize) {
                    matches.push_back(
                        Match_c{end - patternSize, end, static_cast<size_t>(patternId)}
                    );
                    if (onlyContains) {
                        return matches;
                    }
                }
            }
        }
        if (matches.empty()) {
            return matches;
        }
        // 起点升序、终点降序；再取互不重叠的一批（长的优先）
        std::stable_sort(matches.begin(), matches.end(), Match_c::less);
        std::vector<Match_c> result{};
        result.reserve(matches.size());
        long long lastEnd = -1;
        for (const auto& match : matches) {
            if (static_cast<long long>(match.start) >= lastEnd) {
                result.push_back(match);
                lastEnd = static_cast<long long>(match.end);
            }
        }
        return result;
    }

    bool MediaNameAhoCorasick_c::contains(std::u16string_view text, size_t start) const {
        return false == search(text, start, true).empty();
    }

    std::u16string MediaNameAhoCorasick_c::removeAll(std::u16string_view text, size_t start) const {
        const auto matches = search(text, start, false);
        if (matches.empty()) {
            return std::u16string{text};
        }
        std::u16string result{};
        size_t         index = 0;
        for (const auto& match : matches) {
            if (match.start > index) {
                result.append(text.data() + index, match.start - index);
            }
            if (match.end > index) {
                index = match.end;
            }
        }
        if (index < text.size()) {
            result.append(text.data() + index, text.size() - index);
        }
        return result;
    }

    // ===================================================================
    // MediaNameAnalyseBase_c
    // ===================================================================

    std::u16string MediaNameAnalyseBase_c::onAddTempStr(
        const std::u16string&      result,
        const std::optional<bool>& addRangeTag,
        const std::u16string&      tempStr
    ) const {
        (void)addRangeTag;
        (void)tempStr;
        return result;
    }

    MediaNameAnalyseBase_c::Result_c MediaNameAnalyseBase_c::analyse(
        const std::u16string&              name,
        const std::vector<std::u16string>& crudeArtistlist
    ) {
        const MediaNameData_c& data = MediaNameData_c::instance();

        std::u16string result = trimSpaces(name);
        result                = data.unavailWordSearcher.removeAll(result);
        if (result.empty()) {
            Result_c empty{};
            // Dart 这里返回的是 (`""`, {}, false, "")
            empty.addRangeTag = false;
            return empty;
        }

        MediaNameSet_c crudeArtistSet{};
        crudeArtistSet.reserve(crudeArtistlist.size());
        for (const auto& item : crudeArtistlist) {
            crudeArtistSet.insert(MediaNameFold_c::canonical(item));
        }

        std::optional<bool>         addRangeTag{};
        std::u16string              tempsb{};
        std::vector<std::u16string> reArtists{};

        auto addArtist = [&reArtists](std::u16string_view item) {
            addUnique(reArtists, item);
        };

        struct SubStr_c {
            bool                found = false;
            std::u16string_view str{};
            size_t              li = 0;
            size_t              ri = 0;
        };

        // 找一对括号里的内容（左右都是单字符，`onlyContains` 拿到的是最早的一个）
        // 返回的 [SubStr_c::str] 指向 [result]，在 result 被重新赋值前有效
        auto subStr = [&result](
                          const MediaNameAhoCorasick_c& left,
                          const MediaNameAhoCorasick_c& right,
                          size_t                        start
                      ) -> SubStr_c {
            SubStr_c   out{};
            const auto leftMatches = left.search(result, start, true);
            if (leftMatches.empty()) {
                return out;
            }
            const size_t li = leftMatches.front().start;
            if (false == (li + 1 < result.size())) {
                return out;
            }
            const auto rightMatches = right.search(result, li + 1, true);
            if (rightMatches.empty()) {
                return out;
            }
            const size_t ri = rightMatches.front().start;
            if (false == (ri > li)) {
                return out;
            }
            out.found = true;
            out.li    = li;
            out.ri    = ri;
            out.str   = trimSpacesView(std::u16string_view{result}.substr(li + 1, ri - li - 1));
            return out;
        };

        // 删掉 [li, ri]，后面还有内容时补一个空格
        auto removeStr = [&result](size_t li, std::optional<size_t> ri) -> std::u16string {
            if (ri.has_value() && *ri >= result.size()) {
                ri = std::nullopt;
            }
            if (ri.has_value()) {
                const std::u16string_view right = std::u16string_view{result}.substr(*ri);
                if (false == right.empty()) {
                    std::u16string out = result.substr(0, li);
                    out.push_back(u' ');
                    out.append(right);
                    return out;
                }
            }
            return result.substr(0, li);
        };

        // 处理成对的括号（引号只挪到末尾，其余尝试当歌手/标签处理）
        auto resetStr = [&](const MediaNameAhoCorasick_c& left,
                            const MediaNameAhoCorasick_c& right,
                            bool                          onlyMoveEnd) {
            for (long long i = 0; i < static_cast<long long>(result.size()); ++i) {
                const SubStr_c found = subStr(left, right, static_cast<size_t>(i));
                if (false == found.found) {
                    // 没有匹配就结束（不是继续往后找）
                    return;
                }
                const size_t li = found.li;
                const size_t ri = found.ri;
                i               = static_cast<long long>(ri);
                if (found.str.empty()) {
                    // 空括号：移除
                    result       = removeStr(li, ri + 1);
                    i           -= static_cast<long long>(ri + 1 - li);
                    addRangeTag  = true;
                    continue;
                }
                if (onlyMoveEnd) {
                    // 剥离并挪到后面
                    tempsb.append(std::u16string_view{result}.substr(li, ri + 1 - li));
                    result       = removeStr(li, ri + 1);
                    i           -= static_cast<long long>(ri + 1 - li);
                    addRangeTag  = true;
                    continue;
                }
                const std::u16string_view key = MediaNameFold_c::canonicalView(found.str);
                if (unavailTagWord.count(key) > 0 || crudeArtistSet.count(key) > 0) {
                    // 无效词、与歌手重复：移除
                    result       = removeStr(li, ri + 1);
                    i           -= static_cast<long long>(ri + 1 - li);
                    addRangeTag  = true;
                    continue;
                }
                const auto weakIt = weakTagWord.find(key);
                if (weakIt != weakTagWord.end()) {
                    // 弱标签：剥离并挪到后面
                    // 先把要拼接的内容取出来：下一行会重新赋值 result，found.str 指向它
                    tempsb.append(
                        weakIt->second.empty() ? found.str : std::u16string_view{weakIt->second}
                    );
                    result       = removeStr(li, ri + 1);
                    i           -= static_cast<long long>(ri + 1 - li);
                    addRangeTag  = true;
                    continue;
                }
                const auto artistIt = data.artistTagWord.find(key);
                if (artistIt != data.artistTagWord.end()) {
                    // 尝试切出歌手名（同样先取出来，再重新赋值 result）
                    const std::u16string art{
                        artistIt->second.empty() ? found.str : std::u16string_view{artistIt->second}
                    };
                    result  = removeStr(li, ri + 1);
                    i      -= static_cast<long long>(ri + 1 - li);
                    addArtist(art);
                    continue;
                }
                // 在内容里查找歌手名
                const auto relist = data.artistSearcher.search(found.str);
                if (false == relist.empty()) {
                    for (const auto& item : relist) {
                        const std::u16string_view art
                            = data.artistSearcher.pattern(item.patternIndex);
                        const auto mapped
                            = data.artistTagWord.find(MediaNameFold_c::canonicalView(art));
                        addArtist(
                            mapped != data.artistTagWord.end() && false == mapped->second.empty()
                                ? std::u16string_view{mapped->second}
                                : art
                        );
                    }
                    result  = removeStr(li, ri + 1);
                    i      -= static_cast<long long>(ri + 1 - li);
                }
                // Dart 这里还有一句 `addRangeTag = ((addRangeTag ?? false) || false);`，
                // 结果等于原值，是空操作，不写
            }
        };

        // 1. 把 “” 里的内容挪到末尾
        resetStr(data.groupTagQuoteLeftSearcher, data.groupTagQuoteRightSearcher, true);
        // 2. 【】里的内容尝试当歌手/标签处理
        resetStr(data.groupTagBracketLeftSearcher, data.groupTagBracketRightSearcher, false);
        // 3. 书名号：《》里的当歌名，其余当"歌名 - 前缀"
        {
            const SubStr_c found = subStr(
                data.groupTagGuillemetLeftSearcher,
                data.groupTagGuillemetRightSearcher,
                0
            );
            if (found.found) {
                if (found.str.empty()) {
                    result = removeStr(found.li, found.ri + 1);
                } else {
                    addRangeTag = false;
                    // 先拼好新值：removeStr 之后 result 就变了，found.str 指向它
                    std::u16string next{found.str};
                    const auto     rStr = trimSpacesMayNull(removeStr(found.li, found.ri + 1));
                    if (rStr.has_value()) {
                        next.append(u" - ");
                        next.append(*rStr);
                    }
                    result = std::move(next);
                }
            }
        }

        result = onAddTempStr(result, addRangeTag, tempsb);

        // 4. 合并多余符号（保留左右两个）、去掉开头符号
        result = MediaNameSymbol_c::mergeAndStripSymbols(result);
        // 5. 合并连续空格，去掉两端空白
        result = trimSpaces(MediaNameSymbol_c::collapseSpaces(result));
        // 6. 去掉首尾的成对符号
        if (false == result.empty()) {
            size_t starti = 0;
            size_t endi   = result.size();
            if (data.groupTagCode.count(result[0]) > 0) {
                starti = 1;
            }
            if (data.groupTagCode.count(result[result.size() - 1]) > 0) {
                endi = result.size() - 1;
            }
            if (starti != 0 || endi != result.size()) {
                if (endi < starti) {
                    // Dart 在这种情况下会抛 RangeError（实际数据到不了），这里按空串处理
                    endi = starti;
                }
                result = result.substr(starti, endi - starti);
            }
        }

        // 7. 用已知歌手修正、补充
        if (false == crudeArtistlist.empty()) {
            for (const auto& artist : crudeArtistlist) {
                const auto relist = data.artistSearcher.search(artist);
                if (relist.empty()) {
                    addArtist(artist);
                    continue;
                }
                for (const auto& item : relist) {
                    const std::u16string_view art = data.artistSearcher.pattern(item.patternIndex);
                    const auto                mapped
                        = data.artistTagWord.find(MediaNameFold_c::canonicalView(art));
                    addArtist(
                        mapped != data.artistTagWord.end() && false == mapped->second.empty()
                            ? std::u16string_view{mapped->second}
                            : art
                    );
                }
            }
        }

        Result_c out{};
        out.reName      = trimSpaces(result);
        out.reArtists   = std::move(reArtists);
        out.addRangeTag = addRangeTag;
        out.tempStr     = tempsb;
        return out;
    }

    std::vector<std::u16string> MediaNameAnalyseBase_c::sliceSongArtist(
        const std::u16string& artist,
        int                   append,
        bool                  weakArtist,
        std::u16string&       outCrude
    ) {
        const MediaNameData_c&      data        = MediaNameData_c::instance();
        const std::u16string_view   crudeArtist = trimSpacesView(artist);
        std::u16string              resultArtistStorage{};
        std::u16string_view         resultArtist = artist;
        std::vector<std::u16string> resultlist{};

        // 提前提取：组合歌手词整体保留（`伍佰 & China Blue` 不要按 & 切成两个）
        {
            const auto prefixlist = data.artistCombinaSearcher.search(crudeArtist);
            if (false == prefixlist.empty()) {
                size_t         index = 0;
                std::u16string tempArtist{};
                for (const auto& item : prefixlist) {
                    if (item.start < index) {
                        continue;
                    }
                    const std::u16string& artItem
                        = data.artistCombinaSearcher.pattern(item.patternIndex);
                    const size_t endPos  = item.start + artItem.size();
                    const bool   atStart = (0 == item.start);
                    const bool   leftIsSplit
                        = (false == atStart)
                          && data.artistSplitCode.count(crudeArtist[item.start - 1]) > 0;
                    const bool rightIsSplit
                        = endPos >= crudeArtist.size()
                          || data.artistSplitCode.count(crudeArtist[endPos]) > 0;
                    if (atStart || (leftIsSplit && rightIsSplit)) {
                        // 开头，或者前后是分隔符：可用
                        tempArtist.append(subStringView(crudeArtist, index, item.start));
                        const auto mapped
                            = data.artistCombinaWord.find(MediaNameFold_c::canonicalView(artItem));
                        if (mapped != data.artistCombinaWord.end()
                            && false == mapped->second.empty()) {
                            addUnique(resultlist, mapped->second);
                        } else {
                            addUnique(resultlist, artItem);
                        }
                    } else {
                        tempArtist.append(subStringView(crudeArtist, index, endPos));
                    }
                    index = endPos;
                    tempArtist.append(u"&");
                }
                tempArtist.append(subStringView(crudeArtist, index, crudeArtist.size()));
                resultArtistStorage = std::move(tempArtist);
                resultArtist        = resultArtistStorage;
            }
        }

        const auto parts = splitByCodeSet(resultArtist, data.artistSplitCode);
        for (const auto& part : parts) {
            const std::u16string_view item = trimSpacesView(part);
            if (item.empty()) {
                continue;
            }
            const auto target = data.artistTagWord.find(MediaNameFold_c::canonicalView(item));
            const bool hasTarget
                = target != data.artistTagWord.end() && false == target->second.empty();
            if (append < 0) {
                // 用映射值替换原值
                addUnique(
                    resultlist,
                    hasTarget ? std::u16string_view{target->second} : std::u16string_view{item}
                );
                continue;
            }
            addUnique(resultlist, item);
            if (append > 0 && hasTarget) {
                addUnique(resultlist, target->second);
            }
        }

        if (weakArtist) {
            // 删除未记录的歌手名称
            resultlist.erase(
                std::remove_if(
                    resultlist.begin(),
                    resultlist.end(),
                    [&data](const std::u16string& item) {
                        return data.artistTagWord.find(MediaNameFold_c::canonicalView(item))
                               == data.artistTagWord.end();
                    }
                ),
                resultlist.end()
            );
        }

        outCrude.assign(crudeArtist.data(), crudeArtist.size());
        return resultlist;
    }

    std::optional<std::u16string> MediaNameAnalyseBase_c::getAvailSongInfo(
        const std::optional<std::u16string>& name,
        const std::optional<std::u16string>& notAvailResult
    ) {
        const MediaNameData_c& data    = MediaNameData_c::instance();
        auto                   current = getAvailString(name);
        if (current.has_value()) {
            std::u16string value = data.notAvailInfoWordSearcher.removeAll(*current);
            // 去掉网址（保留网址前后两段）
            const size_t startIndex = value.find(u"www.");
            if (startIndex != std::u16string::npos) {
                const size_t lastIndex = value.rfind(u".com");
                if (lastIndex != std::u16string::npos) {
                    value = value.substr(0, startIndex) + u" " + value.substr(lastIndex + 4);
                }
            }
            current = trimSpacesMayNull(value);
        }
        if (false == current.has_value() || current->empty()) {
            return notAvailResult;
        }
        if (data.notAvailInfoStr.count(MediaNameFold_c::canonicalView(*current)) > 0) {
            return notAvailResult;
        }
        return current;
    }

    bool MediaNameAnalyseBase_c::checkAvailLyricLine(std::u16string_view line) {
        const MediaNameData_c& data = MediaNameData_c::instance();
        if (data.notAvailLyricLine.count(MediaNameFold_c::canonicalView(line)) > 0) {
            return false;
        }
        return false == data.notAvailLyricWordSearcher.contains(line);
    }

    std::vector<std::u16string> MediaNameAnalyseBase_c::artistAliases(std::u16string_view artist) {
        return MediaNameData_c::instance().aliasesOf(artist);
    }

    // ===================================================================
    // 歌名分析
    // ===================================================================

    MediaInfoNameAnalyse_c& MediaInfoNameAnalyse_c::instance() {
        static MediaInfoNameAnalyse_c analyse{};
        return analyse;
    }

    MediaInfoNameAnalyse_c::MediaInfoNameAnalyse_c() {
        weakTagWord    = toMap(kWeakTagWordSong);
        unavailTagWord = toSet(kUnavailTagWordSong);
    }

    std::u16string MediaInfoNameAnalyse_c::onAddTempStr(
        const std::u16string&      result,
        const std::optional<bool>& addRangeTag,
        const std::u16string&      tempStr
    ) const {
        std::u16string out = result;
        if (false == tempStr.empty()) {
            if (false == addRangeTag.has_value() || addRangeTag.value()) {
                out += u" -";
            }
            out += u" ";
            out += tempStr;
        }
        return out;
    }

    std::pair<std::u16string, std::vector<std::u16string>> MediaInfoNameAnalyse_c::getNameArtist(
        const std::u16string&              name,
        const std::vector<std::u16string>& artistlist
    ) {
        Result_c result = analyse(name, artistlist);
        return std::make_pair(std::move(result.reName), std::move(result.reArtists));
    }

    // ===================================================================
    // 歌词搜索串
    // ===================================================================

    LyricSearchStrAnalyse_c& LyricSearchStrAnalyse_c::instance() {
        static LyricSearchStrAnalyse_c analyse{};
        return analyse;
    }

    LyricSearchStrAnalyse_c::LyricSearchStrAnalyse_c() {
        weakTagWord    = toMap(kWeakTagWordLyric);
        unavailTagWord = toSet(kUnavailTagWordSong);
        for (const auto& item : kUnavailTagWordLyricExtra) {
            unavailTagWord.insert(MediaNameFold_c::canonical(item));
        }
    }

    std::u16string LyricSearchStrAnalyse_c::createLyricSearchStr(
        const std::u16string& name,
        const std::u16string& artist,
        bool                  weakArtist
    ) {
        const MediaNameData_c& data = MediaNameData_c::instance();

        std::u16string crude{};
        const auto     artistlist = sliceSongArtist(artist, -1, false, crude);

        std::u16string useName = stripLeadingSearchPrefix(name);
        if (useName.empty()) {
            useName = name;
        }
        const Result_c result = analyse(useName, artistlist);

        std::u16string out = result.reName;
        for (const auto& item : result.reArtists) {
            if (weakArtist) {
                // 弱化：记录里没有的歌手直接丢掉
                if (data.artistTagWord.find(MediaNameFold_c::canonicalView(item))
                    != data.artistTagWord.end()) {
                    out += u" - ";
                    out += item;
                }
                continue;
            }
            out += u" - ";
            out += item;
        }
        if (false == result.tempStr.empty()) {
            out += u" ";
            out += result.tempStr;
        }
        if (out.size() > 50) {
            out = out.substr(0, 48);
        }
        std::replace(out.begin(), out.end(), u'_', u'-');
        return out;
    }

} // namespace mediaxx
