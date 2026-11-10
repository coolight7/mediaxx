// ignore_for_file: non_constant_identifier_names, constant_identifier_names

import 'dart:async';
import 'dart:convert';
import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';
import 'dart:typed_data';

import 'package:mediaxx/ffi/allocation.dart';
import 'package:mediaxx/ffi/system_library_preload.dart';
import 'package:mediaxx/ffi/utf8.dart';

import 'mediaxx_bindings_generated.dart';

const mediaxx_label = "libmediaxx by coolight";

/// 常见错误
/// - windows debug 运行时固定返回指针值 123 / 0x0000007B
///     - 调用动态库失败，很可能缺失依赖的其他动态库

Pointer<Void> mediaxx_malloc(int size) {
  return _bindings.mediaxx_malloc(size);
}

void mediaxx_free(Pointer ptr) {
  _bindings.mediaxx_free(ptr.cast<Void>());
}

/// 读取 C 接口通过出参返回的字符串并释放它
///
/// C 侧用 `mediaxx_malloc` 分配，所以统一用 [mediaxx_free] 释放。
String? _takeOwnedString(Pointer<Char>? ptr) {
  if (null == ptr || nullptr == ptr) {
    return null;
  }
  final text = ptr.cast<Utf8>().tryToDartString();
  mediaxx_free(ptr);
  return text;
}

int mediaxx_get_log_level() {
  return _bindings.mediaxx_get_log_level();
}

void mediaxx_set_log_level(int level) {
  _bindings.mediaxx_set_log_level(level);
}

String mediaxx_get_label_malloc() {
  final ptr = _bindings.mediaxx_get_label_malloc().cast<Utf8>();
  final str = ptr.toDartString();
  mediaxx_free(ptr);
  return str;
}

Future<(int? ret, String? result, String? log)> mediaxx_get_media_info_malloc(
  String filepath,
  String headers,
  String pictureOutputPath,
  String picture96OutputPath,
) async {
  final SendPort helperIsolateSendPort = await _helperIsolateSendPort;
  final int requestId = _nextAsyncxxRequestId++;
  final request = _AsyncxxRequestMediaInfo(
    requestId,
    filepath: filepath,
    headers: headers,
    pictureOutputPath: pictureOutputPath,
    picture96OutputPath: picture96OutputPath,
  );
  final completer = Completer<_AsyncxxResponseMediaInfo>();
  _asyncxxRequests[requestId] = completer;
  helperIsolateSendPort.send(request);
  final result = await completer.future;
  return (result.ret, result.result, result.log);
}

Future<(int ret, String? log)> mediaxx_get_media_picture(
  String filepath,
  String headers,
  String pictureOutputPath,
  String picture96OutputPath, [
  String pictureDisplayOutputPath = "",
]) async {
  final SendPort helperIsolateSendPort = await _helperIsolateSendPort;
  final int requestId = _nextAsyncxxRequestId++;
  final request = _AsyncxxRequestMediaPicture(
    requestId,
    filepath: filepath,
    headers: headers,
    pictureOutputPath: pictureOutputPath,
    picture96OutputPath: picture96OutputPath,
    pictureDisplayOutputPath: pictureDisplayOutputPath,
  );
  final completer = Completer<_AsyncxxResponseDefault>();
  _asyncxxRequests[requestId] = completer;
  helperIsolateSendPort.send(request);
  final result = await completer.future;
  return (result.result, result.log);
}

Future<(int ret, String? result, String? log)> mediaxx_analyse_picture_color(
  final String? filepath,
  final List<int>? data,
) async {
  assert(null != filepath || null != data);
  final SendPort helperIsolateSendPort = await _helperIsolateSendPort;
  final int requestId = _nextAsyncxxRequestId++;
  final request = _AsyncxxRequestAnalysePictureColor(
    requestId,
    filepath: filepath,
    data: data,
    decodedData: null,
  );
  final completer = Completer<_AsyncxxResponseMediaInfo>();
  _asyncxxRequests[requestId] = completer;
  helperIsolateSendPort.send(request);
  final result = await completer.future;
  return (result.ret, result.result, result.log);
}

Future<(int ret, String? result, String? log)>
mediaxx_analyse_picture_color_from_decoded_data(Uint8List data) async {
  final SendPort helperIsolateSendPort = await _helperIsolateSendPort;
  final int requestId = _nextAsyncxxRequestId++;
  final request = _AsyncxxRequestAnalysePictureColor(
    requestId,
    filepath: null,
    data: null,
    decodedData: data,
  );
  final completer = Completer<_AsyncxxResponseMediaInfo>();
  _asyncxxRequests[requestId] = completer;
  helperIsolateSendPort.send(request);
  final result = await completer.future;
  return (result.ret, result.result, result.log);
}

Future<
  (int ret, Uint8List? resultSpectrums, Uint8List? resultWaveform, String? log)
>
mediaxx_get_audio_visualization(
  final String? filepath, {
  int maxDurationSeconds = 0,
}) async {
  assert(null != filepath);
  final SendPort helperIsolateSendPort = await _helperIsolateSendPort;
  final int requestId = _nextAsyncxxRequestId++;
  final request = _AsyncxxRequestAnalyseAudioVisualization(
    requestId,
    filepath: filepath,
    maxDurationSeconds: maxDurationSeconds,
  );
  final completer = Completer<_AsyncxxResponseUInt8List>();
  _asyncxxRequests[requestId] = completer;
  helperIsolateSendPort.send(request);
  final result = await completer.future;
  return (
    result.ret,
    result.resultSpectrums,
    result.resultWaveform,
    result.log,
  );
}

/// # 副歌分析
///
/// 输入是 [mediaxx_get_audio_visualization] 的原样输出，分析在 helper isolate 里做：
/// - [spectrums]：每帧 `spectrumSize` 字节（256 个频点，0 = -80dB，255 = 0dB）
/// - [waveform]：每帧 1 字节（RMS 响度，-60~+20dB）；传空表示只要频谱
/// - [durationMs]：音频总时长（毫秒）
/// - [configJson]：可选配置覆盖，如 `{"bandCount":32,"hopFrames":10}`
///
/// ## Return:
/// - ret：1 成功（没有副歌时 `sections` 为空数组）/ 0 输入不合法（原因在 log）
/// - result：JSON `{"sections":[{"start","end","confidence","energy","repeat","similarity"}],"totalDuration"}`
Future<(int ret, String? result, String? log)> mediaxx_analyse_audio_chorus(
  Uint8List spectrums,
  Uint8List waveform,
  int durationMs, {
  String? configJson,
}) {
  return _requestAudioChorus(
    spectrums,
    waveform,
    durationMs,
    configJson,
    false,
  );
}

/// # 副歌分析的结构中间结果
///
/// 参数与 [mediaxx_analyse_audio_chorus] 相同，结果里多出每帧曲线与种子区段，
/// 用于离线分析、调试与一致性核对。
///
/// ## Return:
/// - ret：1 成功 / 0 音频过短、参数非法或整首静音（这时没有结果）
Future<(int ret, String? result, String? log)>
mediaxx_analyse_audio_chorus_structure(
  Uint8List spectrums,
  Uint8List waveform,
  int durationMs, {
  String? configJson,
}) {
  return _requestAudioChorus(spectrums, waveform, durationMs, configJson, true);
}

Future<(int ret, String? result, String? log)> _requestAudioChorus(
  Uint8List spectrums,
  Uint8List waveform,
  int durationMs,
  String? configJson,
  bool structure,
) async {
  final SendPort helperIsolateSendPort = await _helperIsolateSendPort;
  final int requestId = _nextAsyncxxRequestId++;
  final request = _AsyncxxRequestAnalyseAudioChorus(
    requestId,
    spectrums: spectrums,
    waveform: waveform,
    durationMs: durationMs,
    configJson: configJson,
    structure: structure,
  );
  final completer = Completer<_AsyncxxResponseMediaInfo>();
  _asyncxxRequests[requestId] = completer;
  helperIsolateSendPort.send(request);
  final result = await completer.future;
  return (result.ret, result.result, result.log);
}

Future<String?> mediaxx_convert_char_encoding(List<int> data) async {
  if (data.isEmpty) {
    return null;
  }
  final SendPort helperIsolateSendPort = await _helperIsolateSendPort;
  final int requestId = _nextAsyncxxRequestId++;
  final request = _AsyncxxRequestConvertCharEncoding(requestId, data: data);
  final completer = Completer<_AsyncxxResponseStringDefault>();
  _asyncxxRequests[requestId] = completer;
  helperIsolateSendPort.send(request);
  final result = await completer.future;
  return result.str;
}

String mediaxx_get_available_hwcodec_list() {
  final result = _bindings.mediaxx_get_available_hwcodec_list();
  final str = result.cast<Utf8>().tryToDartString();
  mediaxx_free(result);
  return str ?? "";
}

/// 获取音频过滤器列表，返回 json 数组字符串（失败时返回空串）。
///
/// 每一项为 `{"name":名字,"desc":说明,"flags":AVFILTER_FLAG_* 位掩码}`，
/// 名字与 mpv 自带过滤器重名时还会有一项 `mpv_name`，内容为加 `lavfi-` 前缀后的名字。
String mediaxx_get_available_audio_filter_list() {
  final result = _bindings.mediaxx_get_available_audio_filter_list();
  final str = result.cast<Utf8>().tryToDartString();
  mediaxx_free(result);
  return str ?? "";
}

/// 获取视频过滤器列表，返回 json 数组字符串（失败时返回空串），字段含义同
/// [mediaxx_get_available_audio_filter_list]。
String mediaxx_get_available_video_filter_list() {
  final result = _bindings.mediaxx_get_available_video_filter_list();
  final str = result.cast<Utf8>().tryToDartString();
  mediaxx_free(result);
  return str ?? "";
}

/// 读取并释放出参里的结果与日志（同步接口共用）
(int ret, String? result, String? log) _readOutStrings(
  int ret,
  Pointer<Pointer<Char>> resultSlot,
  Pointer<Pointer<Char>> logSlot,
) {
  return (
    ret,
    _takeOwnedString(resultSlot.value),
    _takeOwnedString(logSlot.value),
  );
}

/// # 歌名 / 歌手智能识别（同步）
///
/// 不联网、不做 IO，纯字符串计算：把「【歌手】歌名【Hi-res】」这类标题拆成规范歌名与歌手。
///
/// ## Args:
/// - [name]：待识别的歌名
/// - [artists]：已知歌手（为空表示没有）
///
/// ## Return:
/// - result：JSON `{"name":"歌名","artists":["歌手"]}`
(int ret, String? result, String? log) mediaxx_analyse_media_name(
  String name, [
  List<String>? artists,
]) {
  final Pointer<Char> namePtr = name.toNativeUtf8().cast<Char>();
  final String? artistJson = (null == artists || artists.isEmpty)
      ? null
      : jsonEncode(artists);
  final Pointer<Char> artistPtr = null == artistJson
      ? nullptr
      : artistJson.toNativeUtf8().cast<Char>();
  final Pointer<Pointer<Char>> result = malloc<Pointer<Char>>();
  result.value = nullptr;
  final Pointer<Pointer<Char>> log = malloc<Pointer<Char>>();
  log.value = nullptr;
  try {
    final ret = _bindings.mediaxx_analyse_media_name(
      namePtr,
      artistPtr,
      result,
      log,
    );
    return _readOutStrings(ret, result, log);
  } finally {
    malloc.free(namePtr);
    if (nullptr != artistPtr) {
      malloc.free(artistPtr);
    }
    malloc.free(result);
    malloc.free(log);
  }
}

/// # 歌手切分（同步）
///
/// 按分隔符切开歌手串并统一别名。
///
/// ## Args:
/// - [artist]：歌手串
/// - [append]：`null` 用映射值替换原值（`周杰倫` → `周杰伦`）、`false` 只保留原值、
///   `true` 原值和映射值都保留
/// - [weakArtist]：只要别名表里登记过的名字
///
/// ## Return:
/// - result：JSON `{"artists":["歌手"],"crude":"去掉两端空白的原串"}`
(int ret, String? result, String? log) mediaxx_slice_song_artist(
  String artist, {
  bool? append,
  bool weakArtist = false,
}) {
  // C 接口用整数表示 Dart 的可空布尔：-1 = 替换，0 = 只保留原值，1 = 原值 + 映射值
  final int appendMode = null == append ? -1 : (append ? 1 : 0);
  final Pointer<Char> artistPtr = artist.toNativeUtf8().cast<Char>();
  final Pointer<Pointer<Char>> result = malloc<Pointer<Char>>();
  result.value = nullptr;
  final Pointer<Pointer<Char>> log = malloc<Pointer<Char>>();
  log.value = nullptr;
  try {
    final ret = _bindings.mediaxx_slice_song_artist(
      artistPtr,
      appendMode,
      weakArtist ? 1 : 0,
      result,
      log,
    );
    return _readOutStrings(ret, result, log);
  } finally {
    malloc.free(artistPtr);
    malloc.free(result);
    malloc.free(log);
  }
}

/// # 生成歌词搜索串（同步）
///
/// 歌名 + 歌手 → 用于歌词搜索的字符串（去标签、补歌手、超过 50 个码元就截断）。
///
/// ## Return:
/// - result：JSON `{"str":"搜索串"}`
(int ret, String? result, String? log) mediaxx_create_lyric_search_str(
  String name,
  String artist, {
  bool weakArtist = false,
}) {
  final Pointer<Char> namePtr = name.toNativeUtf8().cast<Char>();
  final Pointer<Char> artistPtr = artist.toNativeUtf8().cast<Char>();
  final Pointer<Pointer<Char>> result = malloc<Pointer<Char>>();
  result.value = nullptr;
  final Pointer<Pointer<Char>> log = malloc<Pointer<Char>>();
  log.value = nullptr;
  try {
    final ret = _bindings.mediaxx_create_lyric_search_str(
      namePtr,
      artistPtr,
      weakArtist ? 1 : 0,
      result,
      log,
    );
    return _readOutStrings(ret, result, log);
  } finally {
    malloc.free(namePtr);
    malloc.free(artistPtr);
    malloc.free(result);
    malloc.free(log);
  }
}

/// # 判断歌曲信息是否可用（同步）
///
/// 过滤推广词、网址、乱码与 `kuwo` / `kugou` 这类占位值。
///
/// ## Args:
/// - [name]：待检查的名字（null 表示没有）
/// - [notAvailResult]：不可用时返回的替代值（null 表示没有）
///
/// ## Return:
/// - result：JSON `{"name":"可用的名字"}`，不可用时 `name` 为 `null`
(int ret, String? result, String? log) mediaxx_get_avail_song_info({
  String? name,
  String? notAvailResult,
}) {
  final Pointer<Char> namePtr = null == name
      ? nullptr
      : name.toNativeUtf8().cast<Char>();
  final Pointer<Char> notAvailPtr = null == notAvailResult
      ? nullptr
      : notAvailResult.toNativeUtf8().cast<Char>();
  final Pointer<Pointer<Char>> result = malloc<Pointer<Char>>();
  result.value = nullptr;
  final Pointer<Pointer<Char>> log = malloc<Pointer<Char>>();
  log.value = nullptr;
  try {
    final ret = _bindings.mediaxx_get_avail_song_info(
      namePtr,
      notAvailPtr,
      result,
      log,
    );
    return _readOutStrings(ret, result, log);
  } finally {
    if (nullptr != namePtr) {
      malloc.free(namePtr);
    }
    if (nullptr != notAvailPtr) {
      malloc.free(notAvailPtr);
    }
    malloc.free(result);
    malloc.free(log);
  }
}

/// # 判断歌词行是否可用（同步）
///
/// 过滤 `//`、`翻译`、版权声明、音乐平台推广语这类行。
bool mediaxx_check_avail_lyric_line(String line) {
  final Pointer<Char> linePtr = line.toNativeUtf8().cast<Char>();
  try {
    return 1 == _bindings.mediaxx_check_avail_lyric_line(linePtr);
  } finally {
    malloc.free(linePtr);
  }
}

/// # 歌手别名反查（同步）
///
/// 给一个规范歌手名，返回别名表里映射到它的所有写法（按字符串排序）。
///
/// ## Return:
/// - result：JSON `{"aliases":["别名"]}`（没有别名时是空数组）
(int ret, String? result, String? log) mediaxx_get_artist_aliases(
  String artist,
) {
  final Pointer<Char> artistPtr = artist.toNativeUtf8().cast<Char>();
  final Pointer<Pointer<Char>> result = malloc<Pointer<Char>>();
  result.value = nullptr;
  final Pointer<Pointer<Char>> log = malloc<Pointer<Char>>();
  log.value = nullptr;
  try {
    final ret = _bindings.mediaxx_get_artist_aliases(artistPtr, result, log);
    return _readOutStrings(ret, result, log);
  } finally {
    malloc.free(artistPtr);
    malloc.free(result);
    malloc.free(log);
  }
}

const String _libName = 'mediaxx';

/// The dynamic library in which the symbols for [MediaxxBindings] can be found.
final DynamicLibrary _dylib = _openMediaxxLibrary();

/// 预加载的系统库，加载 libmediaxx.so 失败时会卸载它们再重试。
List<DynamicLibrary> _preloadedSystemLibraries = const <DynamicLibrary>[];

/// 加载 libmediaxx.so。
///
/// Linux 上先加载系统里的 libva、libpipewire 等库（见 ffi/system_library_preload.dart），
/// 让 libmediaxx.so 使用系统版本，随它分发的副本只在系统缺库或系统库太旧时生效；
/// 万一预加载的系统库让 libmediaxx.so 直接加载失败（缺少依赖库等），
/// 卸载它们后按原样再试一次，这时会使用分发时带的副本。
DynamicLibrary _openMediaxxLibrary() {
  try {
    _preloadedSystemLibraries = Platform.isLinux
        ? preloadSystemLibraries()
        : const <DynamicLibrary>[];
  } catch (_) {
    _preloadedSystemLibraries = const <DynamicLibrary>[];
  }
  try {
    return _openMediaxxDynamicLibrary();
  } catch (_) {
    if (_preloadedSystemLibraries.isEmpty) {
      rethrow;
    }
    // 系统库和 libmediaxx.so 不兼容，改用分发时带的副本
    closeSystemLibraries(_preloadedSystemLibraries);
    _preloadedSystemLibraries = const <DynamicLibrary>[];
    return _openMediaxxDynamicLibrary();
  }
}

/// 按平台打开 libmediaxx 动态库。
DynamicLibrary _openMediaxxDynamicLibrary() {
  if (Platform.isMacOS || Platform.isIOS) {
    return DynamicLibrary.open('lib$_libName.dylib');
  }
  if (Platform.isAndroid || Platform.isLinux) {
    return DynamicLibrary.open('lib$_libName.so');
  }
  if (Platform.isWindows) {
    return DynamicLibrary.open('lib$_libName.dll');
  }
  throw UnsupportedError('Unknown platform: ${Platform.operatingSystem}');
}

/// The bindings to the native functions in [_dylib].
final MediaxxBindings _bindings = MediaxxBindings(_dylib);

class _AsyncxxRequestMediaInfo {
  final int id;

  late Pointer<Char> filepathPtr;
  late Pointer<Char> headersPtr;
  late Pointer<Char> pictureOutputPathPtr;
  late Pointer<Char> picture96OutputPathPtr;

  bool isDispose = false;

  _AsyncxxRequestMediaInfo(
    this.id, {
    required String filepath,
    required String headers,
    required String pictureOutputPath,
    required String picture96OutputPath,
  }) {
    filepathPtr = filepath.toNativeUtf8().cast<Char>();
    headersPtr = headers.toNativeUtf8().cast<Char>();
    pictureOutputPathPtr = pictureOutputPath.toNativeUtf8().cast<Char>();
    picture96OutputPathPtr = picture96OutputPath.toNativeUtf8().cast<Char>();
  }
}

class _AsyncxxResponseUInt8List {
  final int id;
  final int ret;
  final TransferableTypedData? transferableSpectrums;
  final TransferableTypedData? transferableWaveform;
  final String? log;

  Uint8List? resultSpectrums;
  Uint8List? resultWaveform;

  _AsyncxxResponseUInt8List(
    this.id, {
    required this.ret,
    this.transferableSpectrums,
    this.transferableWaveform,
    this.log,
  });
}

class _AsyncxxResponseMediaInfo {
  final int id;
  final int ret;
  final Pointer<Char>? resultPtr;
  final Pointer<Char>? logPtr;

  String? result;
  String? log;

  _AsyncxxResponseMediaInfo(
    this.id, {
    required this.ret,
    this.resultPtr,
    this.logPtr,
  });
}

class _AsyncxxRequestMediaPicture {
  final int id;

  late Pointer<Char> filepathPtr;
  late Pointer<Char> headersPtr;
  late Pointer<Char> pictureOutputPathPtr;
  late Pointer<Char> picture96OutputPathPtr;
  late Pointer<Char> pictureDisplayOutputPathPtr;

  bool isDispose = false;

  _AsyncxxRequestMediaPicture(
    this.id, {
    required String filepath,
    required String headers,
    required String pictureOutputPath,
    required String picture96OutputPath,
    String pictureDisplayOutputPath = "",
  }) {
    filepathPtr = filepath.toNativeUtf8().cast<Char>();
    headersPtr = headers.toNativeUtf8().cast<Char>();
    pictureOutputPathPtr = pictureOutputPath.toNativeUtf8().cast<Char>();
    picture96OutputPathPtr = picture96OutputPath.toNativeUtf8().cast<Char>();
    pictureDisplayOutputPathPtr = pictureDisplayOutputPath
        .toNativeUtf8()
        .cast<Char>();
  }
}

class _AsyncxxRequestAnalysePictureColor {
  final int id;

  late Pointer<Char>? filepathPtr;
  Pointer<Uint8>? dataPtr;
  Pointer<Uint8>? decodedDataPtr;
  late int dataSize;

  bool isDispose = false;

  _AsyncxxRequestAnalysePictureColor(
    this.id, {
    required String? filepath,
    required final List<int>? data,
    required final List<int>? decodedData,
  }) {
    filepathPtr = filepath?.toNativeUtf8().cast<Char>();
    dataSize = 0;
    if (null != data) {
      dataSize = data.length;
      dataPtr = malloc<Uint8>(data.length);
      final Uint8List nativeString = dataPtr!.asTypedList(data.length);
      nativeString.setAll(0, data);
    } else if (null != decodedData) {
      dataSize = decodedData.length;
      decodedDataPtr = malloc<Uint8>(decodedData.length);
      final Uint8List nativeString = decodedDataPtr!.asTypedList(
        decodedData.length,
      );
      nativeString.setAll(0, decodedData);
    }
  }
}

class _AsyncxxRequestAnalyseAudioVisualization {
  final int id;

  late Pointer<Char>? filepathPtr;
  final int maxDurationSeconds;

  bool isDispose = false;

  _AsyncxxRequestAnalyseAudioVisualization(
    this.id, {
    required String? filepath,
    this.maxDurationSeconds = 0,
  }) {
    filepathPtr = filepath?.toNativeUtf8().cast<Char>();
  }
}

class _AsyncxxRequestAnalyseAudioChorus {
  final int id;
  final TransferableTypedData transferableSpectrums;
  final TransferableTypedData transferableWaveform;
  final int durationMs;
  final String? configJson;
  final bool structure;

  bool isDispose = false;

  _AsyncxxRequestAnalyseAudioChorus(
    this.id, {
    required Uint8List spectrums,
    required Uint8List waveform,
    required this.durationMs,
    this.configJson,
    this.structure = false,
  }) : transferableSpectrums = TransferableTypedData.fromList([spectrums]),
       transferableWaveform = TransferableTypedData.fromList([waveform]);
}

class _AsyncxxRequestConvertCharEncoding {
  final int id;

  late Pointer<Uint8> dataPtr;
  late int dataSize;

  bool isDispose = false;

  _AsyncxxRequestConvertCharEncoding(this.id, {required final List<int> data}) {
    dataSize = data.length;
    dataPtr = malloc<Uint8>(data.length);
    final Uint8List nativeString = dataPtr.asTypedList(data.length);
    nativeString.setAll(0, data);
  }
}

class _AsyncxxResponseStringDefault {
  final int id;
  final Pointer<Char>? strPtr;
  final int strSize;

  String? str;

  _AsyncxxResponseStringDefault(this.id, {this.strPtr, required this.strSize});
}

class _AsyncxxResponseDefault {
  final int id;
  final int result;
  final Pointer<Char>? logPtr;

  String? log;

  _AsyncxxResponseDefault(this.id, {required this.result, this.logPtr});
}

/// Counter to identify [_SumRequest]s and [_SumResponse]s.
int _nextAsyncxxRequestId = 0;

/// Mapping from [_AsyncxxRequest] `id`s to the completers corresponding to the correct future of the pending request.
final Map<int, Completer<dynamic>> _asyncxxRequests =
    <int, Completer<dynamic>>{};

/// The SendPort belonging to the helper isolate.
Future<SendPort> _helperIsolateSendPort = () async {
  final Completer<SendPort> sendCompleter = Completer<SendPort>();

  /// App
  final ReceivePort receivePort = ReceivePort()
    ..listen((dynamic data) {
      if (data is SendPort) {
        sendCompleter.complete(data);
        return;
      }

      final completer = _asyncxxRequests[data.id]!;
      _asyncxxRequests.remove(data.id);
      // App接收数据，在这里才转 dartStr，减少拷贝
      if (data is _AsyncxxResponseUInt8List) {
        if (null != data.transferableSpectrums) {
          data.resultSpectrums = data.transferableSpectrums!
              .materialize()
              .asUint8List();
        }
        if (null != data.transferableWaveform) {
          data.resultWaveform = data.transferableWaveform!
              .materialize()
              .asUint8List();
        }
        completer.complete(data);
        return;
      } else if (data is _AsyncxxResponseMediaInfo) {
        data.result = data.resultPtr?.cast<Utf8>().tryToDartString();
        data.log = data.logPtr?.cast<Utf8>().tryToDartString();
        completer.complete(data);

        if (null != data.resultPtr) {
          malloc.free(data.resultPtr!);
        }
        if (null != data.logPtr && nullptr != data.logPtr) {
          malloc.free(data.logPtr!);
        }
        return;
      } else if (data is _AsyncxxResponseDefault) {
        data.log = data.logPtr?.cast<Utf8>().tryToDartString();
        completer.complete(data);

        if (null != data.logPtr) {
          malloc.free(data.logPtr!);
        }
        return;
      } else if (data is _AsyncxxResponseStringDefault) {
        data.str = data.strPtr?.cast<Utf8>().tryToDartString(
          length: data.strSize,
        );
        completer.complete(data);

        if (null != data.strPtr) {
          malloc.free(data.strPtr!);
        }
        return;
      }
      throw UnsupportedError('Unsupported message type: ${data.runtimeType}');
    });

  /// Isolate
  /// 在 App主线程 将数据转 Ptr，然后发送到这个线程，直接将指针转给c++，减少拷贝
  /// 返回时保留指针到 App主线程 才转回 string
  await Isolate.spawn((SendPort sendPort) async {
    final ReceivePort helperReceivePort = ReceivePort()
      ..listen((dynamic data) {
        if (data is _AsyncxxRequestMediaInfo) {
          assert(false == data.isDispose);
          // MediaInfo
          final filepathPtr = data.filepathPtr;
          final headersPtr = data.headersPtr;
          final pictureOutputPathPtr = data.pictureOutputPathPtr;
          final picture96OutputPathPtr = data.picture96OutputPathPtr;
          final Pointer<Pointer<Char>> result = malloc<Pointer<Char>>();
          result.value = nullptr;
          final Pointer<Pointer<Char>> log = malloc<Pointer<Char>>();
          log.value = nullptr;
          final ret = _bindings.mediaxx_get_media_info_malloc(
            filepathPtr,
            headersPtr,
            pictureOutputPathPtr,
            picture96OutputPathPtr,
            result,
            log,
          );
          final resultPtr = result.value;
          final logPtr = log.value;

          malloc.free(filepathPtr);
          malloc.free(headersPtr);
          malloc.free(pictureOutputPathPtr);
          malloc.free(picture96OutputPathPtr);
          malloc.free(result);
          malloc.free(log);
          data.isDispose = true;
          final response = _AsyncxxResponseMediaInfo(
            data.id,
            ret: ret,
            resultPtr: (nullptr != resultPtr) ? resultPtr : null,
            logPtr: (nullptr != logPtr) ? logPtr : null,
          );
          sendPort.send(response);
          return;
        } else if (data is _AsyncxxRequestMediaPicture) {
          // MediaPicture
          final filepathPtr = data.filepathPtr;
          final headersPtr = data.headersPtr;
          final pictureOutputPathPtr = data.pictureOutputPathPtr;
          final picture96OutputPathPtr = data.picture96OutputPathPtr;
          final pictureDisplayOutputPathPtr = data.pictureDisplayOutputPathPtr;
          final Pointer<Pointer<Char>> log = malloc<Pointer<Char>>();
          log.value = nullptr;

          final result = _bindings.mediaxx_get_media_picture(
            filepathPtr,
            headersPtr,
            pictureOutputPathPtr,
            picture96OutputPathPtr,
            pictureDisplayOutputPathPtr,
            log,
          );
          final logPtr = log.value;

          malloc.free(filepathPtr);
          malloc.free(headersPtr);
          malloc.free(pictureOutputPathPtr);
          malloc.free(picture96OutputPathPtr);
          malloc.free(pictureDisplayOutputPathPtr);
          malloc.free(log);
          final response = _AsyncxxResponseDefault(
            data.id,
            result: result,
            logPtr: (nullptr != logPtr) ? logPtr : null,
          );
          sendPort.send(response);
          return;
        } else if (data is _AsyncxxRequestAnalysePictureColor) {
          // AnalysePictureColor
          final filepathPtr = data.filepathPtr;
          final Pointer<Pointer<Char>> result = malloc<Pointer<Char>>();
          result.value = nullptr;
          final Pointer<Pointer<Char>> log = malloc<Pointer<Char>>();
          log.value = nullptr;
          assert(
            null != filepathPtr ||
                (null != data.dataPtr && data.dataSize > 0) ||
                (null != data.decodedDataPtr && data.dataSize > 0),
          );
          int ret = 0;
          if (null != data.decodedDataPtr) {
            ret = _bindings.mediaxx_analyse_picture_color_from_decoded_data(
              data.decodedDataPtr?.cast<Char>() ?? nullptr,
              data.dataSize,
              result,
              log,
            );
          } else {
            ret = _bindings.mediaxx_analyse_picture_color(
              filepathPtr ?? nullptr,
              data.dataPtr?.cast<Char>() ?? nullptr,
              data.dataSize,
              result,
              log,
            );
          }
          final resultPtr = result.value;
          final logPtr = log.value;

          if (null != filepathPtr) {
            malloc.free(filepathPtr);
          }
          if (null != data.dataPtr) {
            malloc.free(data.dataPtr!);
          }
          if (null != data.decodedDataPtr) {
            malloc.free(data.decodedDataPtr!);
          }
          malloc.free(result);
          malloc.free(log);
          final response = _AsyncxxResponseMediaInfo(
            data.id,
            ret: ret,
            resultPtr: (nullptr != resultPtr) ? resultPtr : null,
            logPtr: (nullptr != logPtr) ? logPtr : null,
          );
          sendPort.send(response);
          return;
        } else if (data is _AsyncxxRequestAnalyseAudioVisualization) {
          // AnalyseAudioVisualization
          final filepathPtr = data.filepathPtr;
          final Pointer<Pointer<Char>> resultSpectrums =
              malloc<Pointer<Char>>();
          resultSpectrums.value = nullptr;
          final Pointer<Pointer<Char>> resultWaveform = malloc<Pointer<Char>>();
          resultWaveform.value = nullptr;
          final Pointer<Pointer<Char>> log = malloc<Pointer<Char>>();
          log.value = nullptr;
          assert(null != filepathPtr);
          int ret = 0;
          ret = _bindings.mediaxx_get_audio_visualization(
            filepathPtr ?? nullptr,
            resultSpectrums,
            resultWaveform,
            log,
            data.maxDurationSeconds,
          );
          final resultSpectrumsPtr = resultSpectrums.value;
          final resultWaveformPtr = resultWaveform.value;
          final logPtr = log.value;

          if (null != filepathPtr) {
            malloc.free(filepathPtr);
          }
          malloc.free(resultSpectrums);
          malloc.free(resultWaveform);
          malloc.free(log);

          TransferableTypedData? transferableSpectrums;
          TransferableTypedData? transferableWaveform;
          if (ret > 0 &&
              nullptr != resultSpectrumsPtr &&
              nullptr != resultWaveformPtr) {
            final spectrumsTypedList = resultSpectrumsPtr
                .cast<Uint8>()
                .asTypedList(ret);
            final waveformLen = ret ~/ 256;
            final waveformTypedList = resultWaveformPtr
                .cast<Uint8>()
                .asTypedList(waveformLen);

            // 拷贝至可高效跨 Isolate 零拷贝传递的 TransferableTypedData，随即释放底层 C++ 原生堆内存
            transferableSpectrums = TransferableTypedData.fromList([
              Uint8List.fromList(spectrumsTypedList),
            ]);
            transferableWaveform = TransferableTypedData.fromList([
              Uint8List.fromList(waveformTypedList),
            ]);
            mediaxx_free(resultSpectrumsPtr);
            mediaxx_free(resultWaveformPtr);
          } else {
            if (nullptr != resultSpectrumsPtr) {
              mediaxx_free(resultSpectrumsPtr);
            }
            if (nullptr != resultWaveformPtr) {
              mediaxx_free(resultWaveformPtr);
            }
          }

          final logStr = (nullptr != logPtr)
              ? logPtr.cast<Utf8>().tryToDartString()
              : null;
          if (nullptr != logPtr) {
            mediaxx_free(logPtr);
          }

          final response = _AsyncxxResponseUInt8List(
            data.id,
            ret: ret,
            transferableSpectrums: transferableSpectrums,
            transferableWaveform: transferableWaveform,
            log: logStr,
          );
          sendPort.send(response);
          return;
        } else if (data is _AsyncxxRequestAnalyseAudioChorus) {
          // 副歌分析：频谱与响度在这一侧分配和释放，只回传结果 JSON
          final spectrums = data.transferableSpectrums
              .materialize()
              .asUint8List();
          final waveform = data.transferableWaveform
              .materialize()
              .asUint8List();
          final Pointer<Uint8> spectrumsPtr = spectrums.isEmpty
              ? nullptr
              : malloc<Uint8>(spectrums.length);
          if (nullptr != spectrumsPtr) {
            spectrumsPtr.asTypedList(spectrums.length).setAll(0, spectrums);
          }
          final Pointer<Uint8> waveformPtr = waveform.isEmpty
              ? nullptr
              : malloc<Uint8>(waveform.length);
          if (nullptr != waveformPtr) {
            waveformPtr.asTypedList(waveform.length).setAll(0, waveform);
          }
          final Pointer<Char> configPtr = null == data.configJson
              ? nullptr
              : data.configJson!.toNativeUtf8().cast<Char>();
          final Pointer<Pointer<Char>> result = malloc<Pointer<Char>>();
          result.value = nullptr;
          final Pointer<Pointer<Char>> log = malloc<Pointer<Char>>();
          log.value = nullptr;
          final int ret = data.structure
              ? _bindings.mediaxx_analyse_audio_chorus_structure(
                  spectrumsPtr.cast<Char>(),
                  spectrums.length,
                  waveformPtr.cast<Char>(),
                  waveform.length,
                  data.durationMs,
                  configPtr,
                  result,
                  log,
                )
              : _bindings.mediaxx_analyse_audio_chorus(
                  spectrumsPtr.cast<Char>(),
                  spectrums.length,
                  waveformPtr.cast<Char>(),
                  waveform.length,
                  data.durationMs,
                  configPtr,
                  result,
                  log,
                );
          final resultPtr = result.value;
          final logPtr = log.value;

          if (nullptr != spectrumsPtr) {
            malloc.free(spectrumsPtr);
          }
          if (nullptr != waveformPtr) {
            malloc.free(waveformPtr);
          }
          if (nullptr != configPtr) {
            malloc.free(configPtr);
          }
          malloc.free(result);
          malloc.free(log);
          data.isDispose = true;
          sendPort.send(
            _AsyncxxResponseMediaInfo(
              data.id,
              ret: ret,
              resultPtr: (nullptr != resultPtr) ? resultPtr : null,
              logPtr: (nullptr != logPtr) ? logPtr : null,
            ),
          );
          return;
        } else if (data is _AsyncxxRequestConvertCharEncoding) {
          final Pointer<Pointer<Char>> result = malloc<Pointer<Char>>();
          result.value = nullptr;
          int ret = 0;
          ret = _bindings.mediaxx_convert_char_encoding(
            data.dataPtr.cast<Char>(),
            data.dataSize,
            result,
          );
          final resultPtr = result.value;

          if (null != data.dataPtr) {
            malloc.free(data.dataPtr);
          }
          malloc.free(result);
          final response = _AsyncxxResponseStringDefault(
            data.id,
            strSize: ret,
            strPtr: (nullptr != resultPtr) ? resultPtr : null,
          );
          sendPort.send(response);
          return;
        }

        throw UnsupportedError('Unsupported message type: ${data.runtimeType}');
      });

    // Send the port to the main isolate on which we can receive requests.
    sendPort.send(helperReceivePort.sendPort);
  }, receivePort.sendPort);

  // Wait until the helper isolate has sent us back the SendPort on which we
  // can start sending requests.
  return sendCompleter.future;
}();
