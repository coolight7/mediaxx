// ignore_for_file: non_constant_identifier_names, constant_identifier_names

import 'dart:ffi';
import 'dart:io';

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
