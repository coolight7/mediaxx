// ignore_for_file: non_constant_identifier_names, constant_identifier_names

import 'dart:async';
import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';
import 'dart:typed_data';

import 'package:mediaxx/ffi/allocation.dart';
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
  String picture96OutputPath,
) async {
  final SendPort helperIsolateSendPort = await _helperIsolateSendPort;
  final int requestId = _nextAsyncxxRequestId++;
  final request = _AsyncxxRequestMediaPicture(
    requestId,
    filepath: filepath,
    headers: headers,
    pictureOutputPath: pictureOutputPath,
    picture96OutputPath: picture96OutputPath,
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
mediaxx_get_audio_visualization(final String? filepath) async {
  assert(null != filepath);
  final SendPort helperIsolateSendPort = await _helperIsolateSendPort;
  final int requestId = _nextAsyncxxRequestId++;
  final request = _AsyncxxRequestAnalyseAudioVisualization(
    requestId,
    filepath: filepath,
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

const String _libName = 'mediaxx';

/// The dynamic library in which the symbols for [MediaxxBindings] can be found.
final DynamicLibrary _dylib = () {
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
}();

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
  final int resultSpectrumsLen;
  final Pointer<Char>? resultSpectrumsPtr;
  final int resultWaveformLen;
  final Pointer<Char>? resultWaveformPtr;
  final Pointer<Char>? logPtr;

  Uint8List? resultSpectrums;
  Uint8List? resultWaveform;
  String? log;

  _AsyncxxResponseUInt8List(
    this.id, {
    required this.ret,
    required this.resultSpectrumsLen,
    this.resultSpectrumsPtr,
    required this.resultWaveformLen,
    this.resultWaveformPtr,
    this.logPtr,
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

  bool isDispose = false;

  _AsyncxxRequestMediaPicture(
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

  bool isDispose = false;

  _AsyncxxRequestAnalyseAudioVisualization(
    this.id, {
    required String? filepath,
  }) {
    filepathPtr = filepath?.toNativeUtf8().cast<Char>();
  }
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
        if (null != data.resultSpectrumsPtr &&
            nullptr != data.resultSpectrumsPtr) {
          data.resultSpectrums = Uint8List.fromList(
            data.resultSpectrumsPtr!.cast<Uint8>().asTypedList(
              data.resultSpectrumsLen,
            ),
          );
        }
        if (null != data.resultWaveformPtr &&
            nullptr != data.resultWaveformPtr) {
          data.resultWaveform = Uint8List.fromList(
            data.resultWaveformPtr!.cast<Uint8>().asTypedList(
              data.resultWaveformLen,
            ),
          );
        }
        data.log = data.logPtr?.cast<Utf8>().tryToDartString();
        completer.complete(data);

        if (null != data.resultSpectrumsPtr) {
          malloc.free(data.resultSpectrumsPtr!);
        }
        if (null != data.resultWaveformPtr) {
          malloc.free(data.resultWaveformPtr!);
        }
        if (null != data.logPtr && nullptr != data.logPtr) {
          malloc.free(data.logPtr!);
        }
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
          final Pointer<Pointer<Char>> log = malloc<Pointer<Char>>();
          log.value = nullptr;

          final result = _bindings.mediaxx_get_media_picture(
            filepathPtr,
            headersPtr,
            pictureOutputPathPtr,
            picture96OutputPathPtr,
            log,
          );
          final logPtr = log.value;

          malloc.free(filepathPtr);
          malloc.free(headersPtr);
          malloc.free(pictureOutputPathPtr);
          malloc.free(picture96OutputPathPtr);
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
          final response = _AsyncxxResponseUInt8List(
            data.id,
            ret: ret,
            resultSpectrumsLen: ret, // 返回值为长度
            resultSpectrumsPtr: (nullptr != resultSpectrumsPtr)
                ? resultSpectrumsPtr
                : null,
            resultWaveformLen: ret ~/ 256,
            resultWaveformPtr: (nullptr != resultWaveformPtr)
                ? resultWaveformPtr
                : null,
            logPtr: (nullptr != logPtr) ? logPtr : null,
          );
          sendPort.send(response);
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
