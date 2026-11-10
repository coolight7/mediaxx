// 生成 src/lib/analyse/media_name_tables.h（智能识别要用的 C++ 数据表）
//
// 数据来自 script/data/media_name/：
//   * code_points.json：大小写折叠、\p{P}\p{S} 与 \s 的码点区间、码元折叠表、首尾符号与歌手分隔符；
//   * words.json：歌手别名、组合歌手词、弱标签、无效词、不可用歌曲信息与歌词行词表。
//
// 这两份 JSON 最初由 musicxx 的 `test/tool/mediaxx_analyse_fixture_dump.dart` 从 Dart 实现的
// 运行期数据导出。导出之后数据由本仓库维护：改词表就是改 JSON，再跑这个脚本重新生成头文件。
//
// 用法（在仓库根目录）：
// ```sh
// dart run script/gen_media_name_tables.dart          # 生成
// dart run script/gen_media_name_tables.dart --check   # 只校验，与磁盘上的文件不一致时退出码 1
// ```
import 'dart:convert';
import 'dart:io';

/// 数据目录（相对仓库根目录）
const String _dataDir = 'script/data/media_name';

/// 生成的头文件（相对仓库根目录）
const String _outputFile = 'src/lib/analyse/media_name_tables.h';

void main(List<String> args) {
  final bool checkOnly = args.contains('--check');
  final Directory root = _repoRoot();
  final Map<String, Object?> codePoints = _readJson(
    File('${root.path}/$_dataDir/code_points.json'),
  );
  final Map<String, Object?> words = _readJson(
    File('${root.path}/$_dataDir/words.json'),
  );

  final String content = _buildHeader(codePoints, words);
  final File output = File('${root.path}/$_outputFile');
  if (checkOnly) {
    final String current = output.existsSync() ? output.readAsStringSync() : '';
    if (current == content) {
      stdout.writeln('${output.path} 与数据一致');
      return;
    }
    stdout.writeln('${output.path} 与数据不一致，请重新运行本脚本生成');
    exitCode = 1;
    return;
  }
  output.writeAsStringSync(content);
  stdout.writeln(
    '已生成 ${output.path}（${content.length} 字符，'
    '${'\n'.allMatches(content).length + 1} 行）',
  );
}

/// 仓库根目录：脚本在 `<根>/script/` 下，被复制到别处运行时退回当前目录
Directory _repoRoot() {
  final Directory scriptDir = File.fromUri(Platform.script).parent;
  final Directory parent = scriptDir.parent;
  if (File('${parent.path}/pubspec.yaml').existsSync() &&
      Directory('${parent.path}/src/lib/analyse').existsSync()) {
    return parent;
  }
  return Directory.current;
}

Map<String, Object?> _readJson(File file) {
  if (false == file.existsSync()) {
    stderr.writeln('找不到数据文件：${file.path}');
    exit(1);
  }
  final Object? data = jsonDecode(file.readAsStringSync());
  if (data is! Map<String, Object?>) {
    stderr.writeln('数据文件不是 JSON 对象：${file.path}');
    exit(1);
  }
  return data;
}

// ===================================================================
// 取值（顺便当格式校验）
// ===================================================================

List<List<int>> _intPairs(Map<String, Object?> data, String key) {
  final Object? value = data[key];
  if (value is! List) {
    throw StateError('$key 不是数组');
  }
  return <List<int>>[
    for (final Object? item in value)
      if (item is List && item.length == 2)
        <int>[item[0] as int, item[1] as int]
      else
        throw StateError('$key 里有不是两元组的项：$item'),
  ];
}

List<int> _ints(Map<String, Object?> data, String key) {
  final Object? value = data[key];
  if (value is! List) {
    throw StateError('$key 不是数组');
  }
  return <int>[for (final Object? item in value) item as int];
}

List<List<String>> _stringPairs(Map<String, Object?> data, String key) {
  final Object? value = data[key];
  if (value is! List) {
    throw StateError('$key 不是数组');
  }
  return <List<String>>[
    for (final Object? item in value)
      if (item is List && item.length == 2)
        <String>[item[0] as String, item[1] as String]
      else
        throw StateError('$key 里有不是两元组的项：$item'),
  ];
}

List<String> _strings(Map<String, Object?> data, String key) {
  final Object? value = data[key];
  if (value is! List) {
    throw StateError('$key 不是数组');
  }
  return <String>[for (final Object? item in value) item as String];
}

// ===================================================================
// 字面量：能看清的字符直接写，看不见的写成转义
// ===================================================================

/// 可以直接写进 C++ 源码的码点区间（能看清的字母、文字与标点）
///
/// 不在这些区间里的（空白、格式字符、组合附加符号、表情符号、非 BMP 字符……）
/// 一律写成 `\uXXXX` / `\UXXXXXXXX`，免得源文件里出现看不见、宽度不一致的字符。
const List<List<int>> _readableRanges = <List<int>>[
  <int>[0x0021, 0x007E], // ASCII 可见字符（空格另外转义）
  <int>[0x00A1, 0x00AC], // Latin-1 标点与字母（跳过软连字符 U+00AD）
  <int>[0x00AE, 0x00FF], // Latin-1 字母与符号
  <int>[0x0100, 0x02FF], // 拉丁扩展、国际音标、修饰字母
  <int>[0x0370, 0x03FF], // 希腊字母
  <int>[0x0400, 0x04FF], // 西里尔字母
  <int>[0x0531, 0x058F], // 亚美尼亚字母
  <int>[0x05D0, 0x05EA], // 希伯来字母
  <int>[0x0620, 0x064A], // 阿拉伯字母（跳过读音符号）
  <int>[0x0660, 0x06FF], // 阿拉伯数字与补充字母
  <int>[0x0900, 0x0DFF], // 印度诸文字
  <int>[0x0E00, 0x0E7F], // 泰文
  <int>[0x1100, 0x11FF], // 谚文字母
  <int>[0x1E00, 0x1EFF], // 拉丁扩展附加
  <int>[0x2010, 0x2027], // 破折号、引号、省略号
  <int>[0x2030, 0x205E], // 千分号、角分号等可见标点
  <int>[0x20A0, 0x20BF], // 货币符号
  <int>[0x2100, 0x214F], // 字母式符号（™、№）
  <int>[0x2190, 0x21FF], // 箭头
  <int>[0x2460, 0x24FF], // 带圈字符
  <int>[0x2500, 0x257F], // 制表符号
  <int>[0x25A0, 0x27BF], // 几何图形、杂项符号、装饰符号
  <int>[0x2E80, 0x2EFF], // CJK 部首
  <int>[0x3001, 0x303F], // CJK 标点（U+3000 全角空格另外转义）
  <int>[0x3040, 0x30FF], // 平假名、片假名
  <int>[0x3100, 0x318F], // 注音字母、谚文兼容字母
  <int>[0x31F0, 0x31FF], // 片假名扩展
  <int>[0x3400, 0x4DBF], // CJK 扩展 A
  <int>[0x4E00, 0x9FFF], // CJK 基本区
  <int>[0xAC00, 0xD7AF], // 谚文音节
  <int>[0xF900, 0xFAFF], // CJK 兼容表意文字
  <int>[0xFE50, 0xFE6F], // 小写变体
  <int>[0xFF01, 0xFFEF], // 全角与半角形式
];

bool _isReadable(int codePoint) {
  int low = 0;
  int high = _readableRanges.length - 1;
  while (low <= high) {
    final int middle = (low + high) >> 1;
    final List<int> range = _readableRanges[middle];
    if (codePoint < range[0]) {
      high = middle - 1;
    } else if (codePoint > range[1]) {
      low = middle + 1;
    } else {
      return true;
    }
  }
  return false;
}

/// 被转义的码点及次数，生成结束时打印出来供检查
final Map<int, int> _escaped = <int, int>{};

/// C++ 的 `u"..."` 字面量
String _cppString(String text) {
  final StringBuffer buffer = StringBuffer('u"');
  final List<int> units = text.codeUnits;
  int index = 0;
  while (index < units.length) {
    final int unit = units[index];
    int codePoint = unit;
    int width = 1;
    if (unit >= 0xD800 && unit <= 0xDBFF && index + 1 < units.length) {
      final int next = units[index + 1];
      if (next >= 0xDC00 && next <= 0xDFFF) {
        codePoint = 0x10000 + ((unit - 0xD800) << 10) + (next - 0xDC00);
        width = 2;
      }
    }
    if (1 == width && unit >= 0xD800 && unit <= 0xDFFF) {
      throw StateError('数据里出现孤立代理码元，写不成 C++ 字面量：$text');
    }
    buffer.write(_literalUnit(codePoint));
    index += width;
  }
  buffer.write('"');
  return buffer.toString();
}

/// 单个码点的写法
String _literalUnit(int codePoint) {
  if (0x22 == codePoint) {
    return r'\"';
  }
  if (0x5C == codePoint) {
    return r'\\';
  }
  if (_isReadable(codePoint)) {
    return String.fromCharCode(codePoint);
  }
  _escaped[codePoint] = (_escaped[codePoint] ?? 0) + 1;
  if (codePoint > 0xFFFF) {
    return '\\U${codePoint.toRadixString(16).toUpperCase().padLeft(8, '0')}';
  }
  return '\\u${codePoint.toRadixString(16).toUpperCase().padLeft(4, '0')}';
}

/// 注释里显示一个码点（看不见的照旧写成转义）
String _depictUnit(int codePoint) {
  if (0x20 == codePoint) {
    return r'\u0020';
  }
  if (codePoint > 0xFFFF || false == _isReadable(codePoint)) {
    return 'U+${codePoint.toRadixString(16).toUpperCase().padLeft(4, '0')}';
  }
  return String.fromCharCode(codePoint);
}

String _hex(int value, int width) =>
    '0x${value.toRadixString(16).toUpperCase().padLeft(width, '0')}';

// ===================================================================
// 生成头文件
// ===================================================================

String _buildHeader(
  Map<String, Object?> codePoints,
  Map<String, Object?> words,
) {
  final StringBuffer buffer = StringBuffer();
  buffer.writeln('''
// clang-format off
// 由 script/gen_media_name_tables.dart 从 script/data/media_name/*.json 生成，不要手改。
//
// 内容是「智能识别」（mediaxx 的 MediaNameAnalyse_c）要用到的全部数据表：
//   * kCaseFold / kPunctuationSymbolRanges / kSpaceRanges：Dart 的 String.toLowerCase()
//     与正则引擎（\\p{P}\\p{S}、\\s）逐码点跑出来的结果；
//   * kCodeUnitShift：等价于 MediaInfoAnalyseBase_c.shiftCodeDict（繁→简与符号归一，键值都只取第一个码元）；
//   * kArtistTagWord / kArtistCombinaWord / kWeakTagWord*：别名与弱标签表，键保留原始大小写
//     （AhoCorasick 命中后会把模式串当结果输出），C++ 加载时用 canonical() 生成查询键；
//   * kUnavailWord / kUnavailTagWord* / kNotAvail*：无效词与可用性判断用到的词表。
//
// 数据来源与重新生成的步骤见 script/README.md。
//
// 排版由生成器决定，整个文件不参与 clang-format（结尾不写 `on`，否则 clang-format 会把
// 最后一行误报成未格式化）。
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
''');

  _writeCodePointPairs(
    buffer,
    'kCaseFold',
    _intPairs(codePoints, 'caseFold'),
    '码点的小写映射（String.toLowerCase 结果仍是单个码点的那些）',
  );
  _writeCodePointRanges(
    buffer,
    'kPunctuationSymbolRanges',
    _intPairs(codePoints, 'punctuationSymbolRanges'),
    r'\p{P}\p{S} 的码点区间',
  );
  _writeCodePointRanges(
    buffer,
    'kSpaceRanges',
    _intPairs(codePoints, 'spaceRanges'),
    r'\s 的码点区间',
  );
  _writeCodeUnitPairs(
    buffer,
    'kCodeUnitShift',
    _intPairs(codePoints, 'codeUnitShift'),
  );
  _writeStringPairs(
    buffer,
    'kArtistCombinaWord',
    _stringPairs(words, 'artistCombinaWord'),
    '组合歌手词（整体保留，不按 & 切分）',
  );
  _writeStringPairs(
    buffer,
    'kArtistTagWord',
    _stringPairs(words, 'artistTagWord'),
    '歌手别名表（键是原始写法，查询时用 canonical() 规范化）',
  );
  _writeStringPairs(
    buffer,
    'kWeakTagWordSong',
    _stringPairs(words, 'weakTagWordSong'),
    '弱标签（歌名分析）',
  );
  _writeStringPairs(
    buffer,
    'kWeakTagWordLyric',
    _stringPairs(words, 'weakTagWordLyric'),
    '弱标签（歌词搜索串）',
  );
  _writeStrings(
    buffer,
    'kUnavailWord',
    _strings(words, 'unavailWord'),
    '无效词（歌名里的广告与推广语）',
  );
  _writeStrings(
    buffer,
    'kUnavailTagWordSong',
    _strings(words, 'unavailTagWordSong'),
    '无效标签（歌名分析）',
  );
  _writeStrings(
    buffer,
    'kUnavailTagWordLyricExtra',
    _strings(words, 'unavailTagWordLyricExtra'),
    '无效标签在歌词搜索串里额外多出来的那些',
  );
  _writeStrings(
    buffer,
    'kNotAvailInfoWord',
    _strings(words, 'notAvailInfoWord'),
    '不可用歌曲信息词',
  );
  _writeStrings(
    buffer,
    'kNotAvailInfoStr',
    _strings(words, 'notAvailInfoStr'),
    '不可用歌曲信息（整串相等）',
  );
  _writeStrings(
    buffer,
    'kNotAvailLyricLine',
    _strings(words, 'notAvailLyricLine'),
    '无效歌词行（整行相等）',
  );
  _writeStrings(
    buffer,
    'kNotAvailLyricWord',
    _strings(words, 'notAvailLyricWord'),
    '无效歌词关键词',
  );
  _writeChars(
    buffer,
    'kGroupTagCode',
    _ints(codePoints, 'groupTagCode'),
    '歌名首尾要去掉的符号码元（MediaInfoAnalyseBase_c.groupTagCode）',
  );
  _writeChars(
    buffer,
    'kArtistSplitCode',
    _ints(codePoints, 'artistSplitCode'),
    '歌手分隔符码元（MediaInfoAnalyseBase_c.songArtistSplitCode）',
  );

  buffer.writeln('    } // namespace media_name_tables');
  buffer.writeln('} // namespace mediaxx');

  if (_escaped.isNotEmpty) {
    final List<MapEntry<int, int>> list = _escaped.entries.toList()
      ..sort((a, b) => a.key.compareTo(b.key));
    stdout.writeln(
      '写成转义的字符 ${list.length} 种：'
      '${list.take(20).map((e) => 'U+${e.key.toRadixString(16).toUpperCase().padLeft(4, '0')} ×${e.value}').join('，')}'
      '${list.length > 20 ? ' ……' : ''}',
    );
  }
  return buffer.toString();
}

void _writeCodePointPairs(
  StringBuffer buffer,
  String name,
  List<List<int>> list,
  String depict,
) {
  buffer.writeln('        /// $depict');
  buffer.writeln('        inline constexpr CodePointPair_c $name[] = {');
  final StringBuffer line = StringBuffer();
  final StringBuffer note = StringBuffer();
  for (int i = 0; i < list.length; ++i) {
    final List<int> pair = list[i];
    line.write('{${_hex(pair[0], 6)}, ${_hex(pair[1], 6)}}, ');
    note.write('${_depictUnit(pair[0])}→${_depictUnit(pair[1])}  ');
    if (0 == (i + 1) % 4) {
      _writeTableLine(buffer, line, note);
    }
  }
  _writeTableLine(buffer, line, note);
  buffer.writeln('        };');
  buffer.writeln();
}

void _writeCodePointRanges(
  StringBuffer buffer,
  String name,
  List<List<int>> list,
  String depict,
) {
  buffer.writeln('        /// $depict');
  buffer.writeln('        inline constexpr CodePointRange_c $name[] = {');
  final StringBuffer line = StringBuffer();
  final StringBuffer note = StringBuffer();
  for (int i = 0; i < list.length; ++i) {
    final List<int> range = list[i];
    line.write('{${_hex(range[0], 6)}, ${_hex(range[1], 6)}}, ');
    note.write('${_depictUnit(range[0])}~${_depictUnit(range[1])}  ');
    if (0 == (i + 1) % 3) {
      _writeTableLine(buffer, line, note);
    }
  }
  _writeTableLine(buffer, line, note);
  buffer.writeln('        };');
  buffer.writeln();
}

void _writeCodeUnitPairs(
  StringBuffer buffer,
  String name,
  List<List<int>> list,
) {
  buffer.writeln('        /// 码元折叠表（繁体→简体、各种括号/连字符/引号归一），键值都只有第一个码元');
  buffer.writeln('        inline constexpr CodeUnitPair_c $name[] = {');
  final StringBuffer line = StringBuffer();
  final StringBuffer note = StringBuffer();
  for (int i = 0; i < list.length; ++i) {
    final List<int> pair = list[i];
    line.write('{${_hex(pair[0], 4)}, ${_hex(pair[1], 4)}}, ');
    note.write('${_depictUnit(pair[0])}→${_depictUnit(pair[1])}  ');
    if (0 == (i + 1) % 4) {
      _writeTableLine(buffer, line, note);
    }
  }
  _writeTableLine(buffer, line, note);
  buffer.writeln('        };');
  buffer.writeln();
}

void _writeStringPairs(
  StringBuffer buffer,
  String name,
  List<List<String>> list,
  String depict,
) {
  buffer.writeln('        /// $depict');
  buffer.writeln('        inline constexpr StringPair_c $name[] = {');
  for (final List<String> pair in list) {
    buffer.writeln(
      '            {${_cppString(pair[0])}, ${_cppString(pair[1])}},',
    );
  }
  buffer.writeln('        };');
  buffer.writeln();
}

void _writeStrings(
  StringBuffer buffer,
  String name,
  List<String> list,
  String depict,
) {
  buffer.writeln('        /// $depict');
  buffer.writeln('        inline constexpr std::u16string_view $name[] = {');
  final StringBuffer line = StringBuffer();
  for (int i = 0; i < list.length; ++i) {
    line.write('${_cppString(list[i])}, ');
    if (0 == (i + 1) % 3) {
      buffer.writeln('            ${line.toString().trimRight()}');
      line.clear();
    }
  }
  if (line.isNotEmpty) {
    buffer.writeln('            ${line.toString().trimRight()}');
  }
  buffer.writeln('        };');
  buffer.writeln();
}

void _writeChars(
  StringBuffer buffer,
  String name,
  List<int> list,
  String depict,
) {
  buffer.writeln('        /// $depict');
  buffer.writeln('        inline constexpr char16_t $name[] = {');
  buffer.writeln(
    '            ${list.map((item) => _hex(item, 4)).join(', ')},',
  );
  buffer.writeln('        };  // ${list.map(_depictUnit).join(' ')}');
  buffer.writeln();
}

/// 一行数据，行尾跟上这一行的字符对照
void _writeTableLine(
  StringBuffer buffer,
  StringBuffer line,
  StringBuffer note,
) {
  if (line.isEmpty) {
    return;
  }
  buffer.writeln(
    '            ${line.toString().trimRight()}  // ${note.toString().trimRight()}',
  );
  line.clear();
  note.clear();
}
