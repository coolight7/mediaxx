## 0.0.4

* ffmpeg / ffprobe 调用的返回值固定成几种：0 成功、1 失败、2 没做事（目标已存在又没给 `-y`、
  `-y` 和 `-n` 同时给）、255 被取消、-1 参数不合法；`-h` / `-version` 这类打印完就退出的
  选项仍然是 0，内容在 stdout 里
* 新增 `mediaxx_execute_fftools_cancel`：从另一个线程取消正在跑的调用，约 1 秒内生效；
  ffprobe 通过输入上下文的 avformat 中断回调一起支持
* 日志捕获不再按线程过滤：ffmpeg 自己开的复用器 / 编码器线程打的报错也能拿到
* 直接写进程标准输出的地方全部改走捕获流（`printf` / `fprintf(stderr)`）：`-h` 的选项表、
  `-y -n` 的冲突提示、`-stats` 的进度行不会再漏到宿主的 stdout / stderr
* stdout / stderr 单次最多收 4MB，超过部分丢弃并在末尾加一句说明，截断不会切开 UTF-8 字符
* 参数里出现单独的 `-`（管道）直接按参数错误返回，不再读写宿主的标准输入输出
* Dart 侧：`mediaxx_execute_ffmpeg` / `mediaxx_execute_ffprobe` 增加可选进度回调（内部用
  `-progress` 写临时文件再读回，不改 C 接口），新增 `MediaxxFftoolsExit` 返回码常量与
  `mediaxx_execute_fftools_cancel`

## 0.0.3

* 工具函数改为使用公共基础库：日志、字符串 / UTF-8 / 编码转换、UTF-16 互转、base64 来自
  `cxx_utilxx_base`（`utilxx_base`），不再在本仓库各存一份；`string_util.cpp` 删除
* `util` 目录里的命名空间统一成 `mediaxx::util`（原先是 `mediaxx::utilxx` / `mediaxx::stringxx` /
  `mediaxx::logxx` 三个），调用点跟着改名，函数名不变
* `util/utilxx.h` 改名 `util/av_util.h`（ffmpeg 错误码 / 时间戳转字符串），只包了头文件、
  没有任何实现的 `util/utilxx.cpp` 删除
* 构建接入 `cxx_utilxx_base` / `cxx_utilxx`（`src/third_party/` 子模块）：
  fmt / simdjson / uchardet / iconv / html2md 改由本工程一起构建（iconv 换成 `libiconv-native`，
  不再下载 autotools 源码），Boost 用 `MEDIAXX_BOOST_INCLUDE` 指定的头文件，OpenSSL 用系统或
  `MEDIAXX_OPENSSL_ROOT`
* 调试构建的日志接到 `cxx_utilxx_base` 的日志分发器（仍输出到 stderr）；发布构建的日志宏仍是空语句

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
