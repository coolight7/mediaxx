import 'dart:ffi';
import 'dart:io';

/// Linux 上加载 libmediaxx.so 之前，先把系统里的同名依赖库加载进来。
///
/// libmediaxx.so 与一批系统库的副本放在同一目录一起分发（libva、libpipewire 等），
/// 这些副本在加载时排在系统同名库前面，会带来两个问题：
/// - 副本里的模块目录、驱动目录是按打包机器的路径写死的，换到别的发行版
///   （Fedora 的 /usr/lib64、Arch 的 /usr/lib）就对不上，pipewire 音频输出会失败
/// - 用户系统里更新的库（新显卡驱动需要的新接口）用不上
///
/// 先加载系统同名库后，libmediaxx.so 的依赖项会复用已经加载的库，副本只在系统
/// 没有该库、或系统库缺少 libmediaxx.so 需要的符号（版本太旧）时才生效。

/// 系统库的查找目录：当前架构的 multiarch 目录优先，然后是常见的 lib64、lib。
///
/// 顺序不能反过来：部分发行版会在 /usr/lib 放 32 位库，虽然加载失败会被忽略，
/// 但会把真正可用的目录挤到后面。
List<String> _systemLibraryDirs() {
  final List<String> dirs = <String>[];
  final Abi abi = Abi.current();
  if (abi == Abi.linuxX64) {
    dirs.add('/usr/lib/x86_64-linux-gnu');
  } else if (abi == Abi.linuxArm64) {
    dirs.add('/usr/lib/aarch64-linux-gnu');
  } else if (abi == Abi.linuxArm) {
    dirs.add('/usr/lib/arm-linux-gnueabihf');
  } else if (abi == Abi.linuxIA32) {
    dirs.add('/usr/lib/i386-linux-gnu');
  } else if (abi == Abi.linuxRiscv64) {
    dirs.add('/usr/lib/riscv64-linux-gnu');
  } else if (abi == Abi.linuxRiscv32) {
    dirs.add('/usr/lib/riscv32-linux-gnu');
  }
  dirs.add('/usr/lib64');
  dirs.add('/usr/lib');
  dirs.add('/usr/local/lib64');
  dirs.add('/usr/local/lib');
  return dirs;
}

/// 需要优先使用系统版本的库，以及 libmediaxx.so 会从该库取用的符号。
///
/// 系统库缺少其中任何一个符号时就放弃预加载，改用随 libmediaxx.so 分发的副本，
/// 避免旧系统库让 libmediaxx.so 加载失败。
///
/// 符号列表由 libmpv-linux-build 的 help/create_preload_syms.sh 生成，
/// 内容是 libmediaxx.so 的未定义符号与各库导出符号的交集；
/// 编译依赖升级（libpipewire、libva 版本变化）后需要重新生成，
/// 重新编译 libmediaxx 时 bundle_default.sh 会调用 help/check_preload_syms.sh 检查是否过期。
const Map<String, List<String>> _requiredSymbols = <String, List<String>>{
  'libpipewire-0.3.so.0': <String>[
    'pw_context_connect',
    'pw_context_destroy',
    'pw_context_new',
    'pw_core_get_context',
    'pw_deinit',
    'pw_get_library_version',
    'pw_init',
    'pw_properties_free',
    'pw_properties_new',
    'pw_properties_set',
    'pw_properties_setf',
    'pw_proxy_destroy',
    'pw_stream_add_listener',
    'pw_stream_connect',
    'pw_stream_dequeue_buffer',
    'pw_stream_destroy',
    'pw_stream_flush',
    'pw_stream_get_nsec',
    'pw_stream_get_time_n',
    'pw_stream_new',
    'pw_stream_queue_buffer',
    'pw_stream_set_active',
    'pw_stream_set_control',
    'pw_stream_state_as_string',
    'pw_stream_update_params',
    'pw_stream_update_properties',
    'pw_thread_loop_destroy',
    'pw_thread_loop_get_loop',
    'pw_thread_loop_get_time',
    'pw_thread_loop_lock',
    'pw_thread_loop_new',
    'pw_thread_loop_signal',
    'pw_thread_loop_start',
    'pw_thread_loop_stop',
    'pw_thread_loop_timed_wait_full',
    'pw_thread_loop_unlock',
    'pw_thread_loop_wait',
  ],
  'libva.so.2': <String>[
    'vaAssociateSubpicture',
    'vaBeginPicture',
    'vaCreateBuffer',
    'vaCreateConfig',
    'vaCreateContext',
    'vaCreateImage',
    'vaCreateSubpicture',
    'vaCreateSurfaces',
    'vaDeassociateSubpicture',
    'vaDeriveImage',
    'vaDestroyBuffer',
    'vaDestroyConfig',
    'vaDestroyContext',
    'vaDestroyImage',
    'vaDestroySubpicture',
    'vaDestroySurfaces',
    'vaEndPicture',
    'vaEntrypointStr',
    'vaErrorStr',
    'vaExportSurfaceHandle',
    'vaGetConfigAttributes',
    'vaGetImage',
    'vaInitialize',
    'vaMapBuffer',
    'vaMaxNumDisplayAttributes',
    'vaMaxNumEntrypoints',
    'vaMaxNumImageFormats',
    'vaMaxNumProfiles',
    'vaMaxNumSubpictureFormats',
    'vaProfileStr',
    'vaPutImage',
    'vaQueryConfigEntrypoints',
    'vaQueryConfigProfiles',
    'vaQueryDisplayAttributes',
    'vaQueryImageFormats',
    'vaQuerySubpictureFormats',
    'vaQuerySurfaceAttributes',
    'vaQueryVendorString',
    'vaQueryVideoProcFilterCaps',
    'vaQueryVideoProcFilters',
    'vaQueryVideoProcPipelineCaps',
    'vaRenderPicture',
    'vaSetDriverName',
    'vaSetErrorCallback',
    'vaSetInfoCallback',
    'vaSyncSurface',
    'vaTerminate',
    'vaUnmapBuffer',
  ],
  'libva-drm.so.2': <String>['vaGetDisplayDRM'],
  'libva-x11.so.2': <String>['vaGetDisplay', 'vaPutSurface'],
  'libva-wayland.so.2': <String>['vaGetDisplayWl'],
  'libvdpau.so.1': <String>['vdp_device_create_x11'],
};

/// 加载系统里的目标库，返回本次加载成功的库。
///
/// 返回的库会一直保持加载，libmediaxx.so 的依赖项会复用它；如果之后 libmediaxx.so
/// 加载失败，可以调用 [closeSystemLibraries] 卸载它们再重试。
List<DynamicLibrary> preloadSystemLibraries() {
  final List<DynamicLibrary> loaded = <DynamicLibrary>[];
  for (final MapEntry<String, List<String>> entry in _requiredSymbols.entries) {
    final String? path = _findSystemLibrary(entry.key);
    if (path == null) {
      // 系统没装这个库，libmediaxx.so 会用同目录下的副本
      continue;
    }
    final DynamicLibrary library;
    try {
      library = DynamicLibrary.open(path);
    } catch (_) {
      // 架构不符、依赖缺失等，交给副本
      continue;
    }
    if (entry.value.every(library.providesSymbol)) {
      loaded.add(library);
      continue;
    }
    // 系统库太旧，缺少 libmediaxx.so 需要的符号，卸载后交给副本
    _closeQuietly(library);
  }
  return loaded;
}

/// 卸载 [preloadSystemLibraries] 加载的库，让 libmediaxx.so 的依赖项回落到同目录的副本。
void closeSystemLibraries(List<DynamicLibrary> libraries) {
  for (final DynamicLibrary library in libraries) {
    _closeQuietly(library);
  }
}

void _closeQuietly(DynamicLibrary library) {
  try {
    library.close();
  } catch (_) {
    // 还有别的代码在用这个库时关闭会失败，忽略即可
  }
}

/// 在系统目录里查找同名库，找不到返回 null。
String? _findSystemLibrary(String name) {
  for (final String dir in _systemLibraryDirs()) {
    final File file = File('$dir/$name');
    if (file.existsSync()) {
      return file.path;
    }
  }
  return null;
}
