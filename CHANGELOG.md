## 0.0.2

* 数据表改由本仓库维护：原始数据在 `script/data/media_name/*.json`，`script/gen_media_name_tables.dart` 生成 C++ 表
* 新增副歌分析：`mediaxx_analyse_audio_chorus`（区段结果）与 `mediaxx_analyse_audio_chorus_structure`
  （每帧曲线与种子区段，调试与一致性核对用）
* 新增歌名/歌手智能识别：`mediaxx_analyse_media_name`、`mediaxx_slice_song_artist`、
  `mediaxx_create_lyric_search_str`、`mediaxx_get_avail_song_info`、`mediaxx_check_avail_lyric_line`、
  `mediaxx_get_artist_aliases`
* 智能识别的查表与切分改用 `string_view`：别名表 / 无效词表能直接用视图查找，不需要为每次查找构造临时串；
  小写转换改为查 BMP 表
* 数据表生成：字符照原样写（空格、零宽字符不再转成 `\uXXXX`），只保留双引号、反斜杠、
  控制字符与非 BMP 字符的转义
* 数据表拆成 `media_name_tables.h`（声明）与 `media_name_tables.cpp`（数据）：
  数据有 3000 多行，不再放在头文件里
* 新增与 Dart 实现的一致性用例（`src/test/data/analyse/`，`test()` 开头运行）

## 0.0.1

* TODO: Describe initial release.
