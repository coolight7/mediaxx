# 脚本与数据

这里放的是本仓库自己维护的生成脚本与原始数据，跑它们不需要 musicxx。

## `gen_media_name_tables.dart`

把 `data/media_name/*.json` 生成两个文件（智能识别要的大小写折叠表、符号区间、码元折叠表、
别名表与无效词表）：

| 文件 | 内容 |
|---|---|
| `src/lib/analyse/media_name_tables.h` | 结构体与 `extern` 声明，带每张表的元素个数 |
| `src/lib/analyse/media_name_tables.cpp` | 数据本身 |

头文件里只放声明：数据有 3000 多行，写在头文件里会让每个包含它的编译单元都重新编一遍。
生成物已经提交，平时不用跑。

```sh
dart run script/gen_media_name_tables.dart           # 重新生成
dart run script/gen_media_name_tables.dart --check    # 只校验：两个文件中有一个与数据不一致时退出码 1
```

生成规则：

- 字符照原样写：`{u"Au / Ra", u"Au/Ra"}`、`u" - ", u"-"`，空格、零宽字符也不转义
  （这个头文件是生成物，只给程序读，人不改）；
- 只有三种情况写成转义：双引号与反斜杠、控制字符（换行、制表符等，写了会破坏源码）、
  非 BMP 字符（写成 `\UXXXXXXXX`，不同编译器对 `u"..."` 里直接写这类字符的接受程度不一致）；
  被转义的码点会在生成时打印出来；
- 数值表只能是数字，每行行尾给出这一行数据对应的字符，方便核对（`{0x000232, 0x000233},  // Ò→Ó`）；
  注释里看不见的字符（空白、零宽、控制字符、非 BMP）仍写成 `U+XXXX`，否则那一行读不出来；
- 生成的文件整段 `// clang-format off`，排版由生成器决定。

## `data/media_name/`

| 文件 | 内容 |
|---|---|
| `code_points.json` | `caseFold`（`String.toLowerCase()` 结果仍是单个码点的映射）、`punctuationSymbolRanges`（`\p{P}\p{S}`）、`spaceRanges`（`\s`）、`codeUnitShift`（繁→简与符号归一）、`groupTagCode`（歌名首尾要去掉的符号）、`artistSplitCode`（歌手分隔符） |
| `words.json` | `artistTagWord` / `artistCombinaWord` / `weakTagWord*`（歌手别名、组合歌手词、弱标签）、`unavailWord` / `unavailTagWord*`（无效词与无效标签）、`notAvail*`（不可用歌曲信息与无效歌词行） |

这两份 JSON 是最初从 musicxx 的 Dart 实现（`lib/share/components/media/MediaInfoAnalyse.dart` +
`MediaInfoSymbol.dart`）导出的一次性快照，导出脚本是 musicxx 的
`test/tool/mediaxx_analyse_fixture_dump.dart`。

**导出之后这里就是数据源**：要增删歌手别名、无效词这些表，直接改 JSON（里面就是可读的中文与原名写法），
再跑一次 `gen_media_name_tables.dart`，不要手改生成的头文件。

## 一致性用例

`src/test/data/analyse/*.json` 是 C++ 实现与 Dart 实现比对用的用例与期望值，同样由那个 musicxx 脚本导出，
说明见 [src/test/data/analyse/README.md](../../src/test/data/analyse/README.md)。
