# 脚本与数据

这里放的是本仓库自己维护的生成脚本与原始数据，跑它们不需要 musicxx。

## `gen_media_name_tables.dart`

把 `data/media_name/*.json` 生成 C++ 数据表 `src/lib/analyse/media_name_tables.h`
（智能识别要用的大小写折叠表、符号区间、码元折叠表、别名表与无效词表）。生成物已经提交，
平时不用跑。

```sh
dart run script/gen_media_name_tables.dart           # 重新生成
dart run script/gen_media_name_tables.dart --check    # 只校验：与磁盘上的文件不一致时退出码 1
```

生成规则：

- 能看清的字符直接写成字符：`{u"洛天依", u""}`、`{0x005B, 0x0028},  // [→(`
  （数值表只能是数字，所以每行行尾给出了这一行数据对应的字符，方便核对）；
- 空白、格式字符、组合附加符号、表情符号与非 BMP 字符写成 `\uXXXX` / `\UXXXXXXXX`，
  免得源文件里出现看不见或宽度不一致的字符；具体哪些字符被转义会在运行时打印出来；
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
