# 一致性用例（C++ 实现 vs Dart 实现）

这些文件不是手写的，全部由 musicxx 仓库的 `test/tool/mediaxx_analyse_fixture_dump.dart` 从 **Dart 运行期**
导出。C++ 实现（`src/lib/analyse/media_name.*`、`chorus_analyse.*`）必须与 Dart 实现给出相同结果，改任一边
之后都要重新导出一遍并重跑用例。

`test()` 一开头就调用 `runAnalyseTests()`（`src/test/test_analyse.cpp`），失败会打印前 40 条差异并 `abort`，
所以先跑一致性用例，再跑需要 `temp/` 音频文件的老用例。

## 文件

| 文件 | 内容 |
|---|---|
| `band_ranges.json` | 副歌分析把 256 个频点压成 N 个频带时的边界表（N = 1~64、100、128、160、200、255、256） |
| `chorus_cases.json` | 副歌用例：输入（11 个假音频，含结构曲线 base64）与期望的区段结果、结构中间结果 |
| `symbol_cases.json` | 符号合并/剥离/空格折叠用例（短串穷举 + 随机长串） |
| `media_name_cases.json` | 歌名解析、歌手切分、歌词搜索串、可用性判断、歌词行、别名反查用例 |

配套的数据表 `src/lib/analyse/media_name_tables.h` 同样由那个脚本导出（大小写折叠表、`\p{P}\p{S}` 与
空白区间、码元折叠表、各词表），也不要手改。

## 用例怎么造出来的

- 副歌用例用的是**合成数据**：按区段填 32 个频带的 dB 与响度，噪声用固定种子的 xorshift32（Dart 与 C++
  各实现一份，只有整数运算，两边逐位一致）。`structureArrays` 为真的用例会把每帧曲线以
  `{"type":"float32"|"int32","base64":"..."}`（小端）带出来逐字节比较，用来确认两边的浮点运算顺序完全一致。
- 智能识别用例覆盖：真实标题样例、别名表全表逐个当歌名跑一遍、别名拼成 `【歌手】歌名` 形状、歌手串切分、
  歌词搜索串、可用性判断、歌词行过滤、别名反查。
- 集合类结果（歌手列表、别名列表）在两边都排序后比较：Dart 的 `HashSet` 迭代顺序不保证一致。
- `analyseTimeMs`（耗时）不参与比较；浮点容差 1e-9（有响度数据时实际逐位相同，报告里会打印最大误差）。

## 重新生成与运行

```sh
# 1. 在 musicxx 仓库导出数据表与用例（写到 MEDIAXX_ANALYSE_OUT_DIR 指定的目录）
$env:MEDIAXX_ANALYSE_OUT_DIR='<输出目录>'
flutter test test/tool/mediaxx_analyse_fixture_dump.dart

# 2. 把生成的 media_name_tables.h 放到 src/lib/analyse/，其余 json 放到这个目录

# 3. 在本仓库编译并跑
cmake -B build/debug -S src -DLUMENXX_BUILD_TYPE=LUMENXX_BUILD_DEBUG -DCMAKE_BUILD_TYPE=Debug
cmake --build build/debug -j 8
LD_LIBRARY_PATH=build/debug:build/debug/exec build/debug/test/test
```

用例文件按相对路径查找（`./src/test/data/analyse/`、`../src/test/data/analyse/`、`./data/analyse/`、
`./test/data/analyse/`），从仓库根或 `build/debug` 下运行都能找到。
