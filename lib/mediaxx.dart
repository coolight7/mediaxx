// ignore_for_file: non_constant_identifier_names, constant_identifier_names

import 'dart:async';
import 'dart:ffi';
import 'dart:io';
import 'dart:isolate';

import 'package:mediaxx/ffi/allocation.dart';
import 'package:mediaxx/ffi/utf8.dart';

import 'mediaxx_bindings_generated.dart';

const mediaxx_label = "libmediaxx by coolight";

Pointer<Void> mediaxx_malloc(int size) {
  return _bindings.mediaxx_malloc(size);
}

void mediaxx_free(Pointer ptr) {
  _bindings.mediaxx_free(ptr.cast<Void>());
}

int mediaxx_get_libav_version() {
  return _bindings.mediaxx_get_libav_version();
}

(String, Pointer<Utf8>) mediaxx_get_label_malloc() {
  final ptr = _bindings.mediaxx_get_label_malloc().cast<Utf8>();
  return (ptr.toDartString(), ptr);
}

Future<(String? result, String? log)> mediaxx_get_media_info_malloc(
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
  return (result.result, result.log);
}

Future<(int result, String? log)> mediaxx_get_media_picture(
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
  final completer = Completer<_AsyncxxResponseMediaPicture>();
  _asyncxxRequests[requestId] = completer;
  helperIsolateSendPort.send(request);
  final result = await completer.future;
  return (result.result, result.log);
}

const String _libName = 'mediaxx';

/// The dynamic library in which the symbols for [MediaxxBindings] can be found.
final DynamicLibrary _dylib = () {
  if (Platform.isMacOS || Platform.isIOS) {
    return DynamicLibrary.open('$_libName.framework/$_libName');
  }
  if (Platform.isAndroid || Platform.isLinux) {
    return DynamicLibrary.open('lib$_libName.so');
  }
  if (Platform.isWindows) {
    return DynamicLibrary.open('$_libName.dll');
  }
  throw UnsupportedError('Unknown platform: ${Platform.operatingSystem}');
}();

/// The bindings to the native functions in [_dylib].
final MediaxxBindings _bindings = MediaxxBindings(_dylib);

class _AsyncxxRequestMediaInfo {
  final int id;
  final String filepath;
  final String headers;
  final String pictureOutputPath;
  final String picture96OutputPath;

  late Pointer<Char> filepathPtr;
  late Pointer<Char> headersPtr;
  late Pointer<Char> pictureOutputPathPtr;
  late Pointer<Char> picture96OutputPathPtr;

  bool isDispose = false;

  _AsyncxxRequestMediaInfo(
    this.id, {
    required this.filepath,
    required this.headers,
    required this.pictureOutputPath,
    required this.picture96OutputPath,
  }) {
    filepathPtr = filepath.toNativeUtf8().cast<Char>();
    headersPtr = headers.toNativeUtf8().cast<Char>();
    pictureOutputPathPtr = pictureOutputPath.toNativeUtf8().cast<Char>();
    picture96OutputPathPtr = picture96OutputPath.toNativeUtf8().cast<Char>();
  }
}

class _AsyncxxResponseMediaInfo {
  final int id;
  final Pointer<Char>? resultPtr;
  final Pointer<Char>? logPtr;

  String? result;
  String? log;

  _AsyncxxResponseMediaInfo(this.id, {this.resultPtr, this.logPtr});
}

class _AsyncxxRequestMediaPicture {
  final int id;
  final String filepath;
  final String headers;
  final String pictureOutputPath;
  final String picture96OutputPath;

  late Pointer<Char> filepathPtr;
  late Pointer<Char> headersPtr;
  late Pointer<Char> pictureOutputPathPtr;
  late Pointer<Char> picture96OutputPathPtr;

  bool isDispose = false;

  _AsyncxxRequestMediaPicture(
    this.id, {
    required this.filepath,
    required this.headers,
    required this.pictureOutputPath,
    required this.picture96OutputPath,
  }) {
    filepathPtr = filepath.toNativeUtf8().cast<Char>();
    headersPtr = headers.toNativeUtf8().cast<Char>();
    pictureOutputPathPtr = pictureOutputPath.toNativeUtf8().cast<Char>();
    picture96OutputPathPtr = picture96OutputPath.toNativeUtf8().cast<Char>();
  }
}

class _AsyncxxResponseMediaPicture {
  final int id;
  final int result;
  final Pointer<Char>? logPtr;

  String? log;

  _AsyncxxResponseMediaPicture(this.id, {required this.result, this.logPtr});
}

/// Counter to identify [_SumRequest]s and [_SumResponse]s.
int _nextAsyncxxRequestId = 0;

/// Mapping from [_AsyncxxRequest] `id`s to the completers corresponding to the correct future of the pending request.
final Map<int, Completer<dynamic>> _asyncxxRequests =
    <int, Completer<dynamic>>{};

/// The SendPort belonging to the helper isolate.
Future<SendPort> _helperIsolateSendPort = () async {
  final Completer<SendPort> completer = Completer<SendPort>();

  /// App
  final ReceivePort receivePort = ReceivePort()
    ..listen((dynamic data) {
      if (data is SendPort) {
        completer.complete(data);
        return;
      }

      // App接收数据，在这里才转 dartStr，减少拷贝
      if (data is _AsyncxxResponseMediaInfo) {
        final Completer<dynamic> completer = _asyncxxRequests[data.id]!;
        _asyncxxRequests.remove(data.id);

        data.result = data.resultPtr?.cast<Utf8>().tryToDartString();
        data.log = data.logPtr?.cast<Utf8>().tryToDartString();
        completer.complete(data);

        if (null != data.resultPtr) {
          malloc.free(data.resultPtr!);
        }
        if (null != data.logPtr) {
          malloc.free(data.logPtr!);
        }
        return;
      } else if (data is _AsyncxxResponseMediaPicture) {
        final Completer<dynamic> completer = _asyncxxRequests[data.id]!;
        _asyncxxRequests.remove(data.id);

        data.log = data.logPtr?.cast<Utf8>().tryToDartString();
        completer.complete(data);

        if (null != data.logPtr) {
          malloc.free(data.logPtr!);
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
          final Pointer<Pointer<Char>> log = malloc<Pointer<Char>>();
          log.value = nullptr;

          final resultPtr = _bindings.mediaxx_get_media_info_malloc(
            filepathPtr,
            headersPtr,
            pictureOutputPathPtr,
            picture96OutputPathPtr,
            log,
          );

          malloc.free(filepathPtr);
          malloc.free(headersPtr);
          malloc.free(pictureOutputPathPtr);
          malloc.free(picture96OutputPathPtr);
          malloc.free(log);
          data.isDispose = true;

          final response = _AsyncxxResponseMediaInfo(
            data.id,
            resultPtr: resultPtr,
            logPtr: (nullptr != log.value) ? log.value : null,
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

          malloc.free(filepathPtr);
          malloc.free(headersPtr);
          malloc.free(pictureOutputPathPtr);
          malloc.free(picture96OutputPathPtr);
          malloc.free(log);
          final response = _AsyncxxResponseMediaPicture(
            data.id,
            result: result,
            logPtr: (nullptr != log.value) ? log.value : null,
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
  return completer.future;
}();
