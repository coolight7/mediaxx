import 'dart:ffi';

import 'package:mediaxx/ffi/utf8.dart';

class NativeStringxx {
  static const int sizeOfCInt = 4;

  late final int strLen;
  late final Pointer<Char>? basePtr;

  Pointer<Char>? get strPtr {
    if (null == basePtr || nullptr == basePtr) {
      return null;
    }
    return Pointer<Char>.fromAddress(basePtr!.address + sizeOfCInt);
  }

  NativeStringxx({required this.basePtr, int? len}) {
    if (null == len) {
      // 计算len
      if (null != basePtr && nullptr != basePtr) {
        len = strPtr!.cast<Utf8>().length;
      } else {
        len = 0;
      }
    }
    strLen = len;
  }
}
