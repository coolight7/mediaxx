# mediaxx

- 该项目来自 [拟声](https://github.com/coolight7/musicxx) 和 [流明](https://github.com/coolight7/lumenxx-docx) 的开发
- 功能:
  - 结合`ffmpeg`实现：
    1. 读取音视频文件的基本信息（标题、艺术家、内嵌LRC歌词、时长、专辑、年份等）
    2. 提取音视频的封面保存到文件
    3. 读取链接的 ffmpeg 支持的编解码器、硬件加速列表信息
  - 结合`uchardet`、`iconv`尝试识别字符串的字符编码格式，以及转换到 utf8
  - 分析图片颜色，聚类出 主色调、亮色调4种、暗色调4种、综合颜色占比排序最高的4种

## 开始使用
- 本项目分为 `c++ 代码` 和 `dart 代码` 两部分，c++ 部分主要是链接依赖库、实现功能，dart 代码为 ffi 调用 c++ 动态库的符号接口，方便 flutter/dart 程序使用
- 流程为 先将 `c++ 插件代码` 编译为 动态库，`主程序`打包时需要添加这个动态库文件， `dart 插件代码`在运行时即可加载动态库使用
- 可见本插件的 dart 代码并无实际功能实现，仅方便 flutter/dart 主程序调用 c++ 代码而已。另外也就是说本插件编译出的动态库 `libmediaxx` 可以给其他编程语言/项目使用
- 在编译本插件的动态库 `libmediaxx` 时，还可以选择静态链接 `ffmpeg/libav`、`libmpv` 等库，提高代码段复用，大幅缩减体积

## 开发、测试
- c++代码开发初始化：
  - 建议在 linux 开发测试 c++ 代码，需要下载或编译 ffmpeg 的动态库 `libavcodec.so/libavdevice.so/libavfilter.so/libavutil.so/libswresample.so/libswscale.so` 复制到 `src/third_party/ffmpeg/libs-linux`中，并复制系统库 `libva-drm.so.2/libva-x11.so.2/libva.so.2/libvdpau.so.1` 到`src/third_party/ffmpeg/libs-linux`，编译 libmediaxx 时需要用到，详见 [CMakeLists.txt](src/CMakeLists.txt)
  - 拉取依赖库源码：`git submodule update --init` 可以拉取依赖库到 `src/third_party/` 中
  - 另外还需要手动下载 iconv，同样解压到 `src/third_party/`:
```sh
cd src/third_party
wget https://ftp.gnu.org/pub/gnu/libiconv/libiconv-1.18.tar.gz && tar -zxvf libiconv-1.18.tar.gz && mv libiconv-1.18 libiconv && rm libiconv-1.18.tar.gz
```
  - 接着在项目根目录执行 `./script/debug_build.sh` 或 `./script/release_build.sh` 即可编译 c++ 代码，debug 编译脚本在编译完成后会运行 test
- 当修改导出的头文件(src/mediaxx.h)[src/mediaxx.h]内的函数声明后，需要执行 `dart run ffigen --config ffigen.yaml` 更新生成的dart代码

## 动态库编译简要说明
- 该项目需要先编译出动态库 libmediaxx ，然后在主程序编译时复制动态库到 install安装根目录，一般即为主程序编译输出所在目录，dart插件代码运行时即可自动动态加载动态库
- 编译见：
  - android:   https://github.com/coolight7/libmpv-android-video-build
  - windows:   https://github.com/coolight7/mpv-winbuild-cmake
  - linux:     https://github.com/coolight7/libmpv-linux-build
  - macos/ios: https://github.com/coolight7/libmpv-apple-build
- 已验证系统/架构支持：
  - Android: arm64、arm32、x64
  - windows: x64
  - linux: x64
  - macos: arm64
  - ios: arm64
- 调整编译脚本即可控制动态、静态链接 ffmpeg、libmpv
- 静态链接时，需要考虑清楚c++标准库的链接方式

### 相通点
- 五大系统平台上的动态库尽管差别不小，但相同点也不少，其中 android和linux 基本互通，ios和macos基本相同，windows的编译是在 linux 上使用 clang 交叉编译出来的，所以在 CMakeLists.txt 中不少参数指定也跟 linux 差不多
- 关于控制动态库的符号导出：
  - 只导出需要的符号，可以帮助链接器了解哪些符号是没用的，以便最终生成动态库时可以删除无用的代码和数据段
  - 控制符号导出主要是两步：
    - 一是强制声明需要的符号是未定义的 `--undefined=symbol_hello`，因为要导出的符号有些并没有在代码中被使用，如果不声明，可能在编译期就被裁剪删除了，声明后可以留到链接期，由链接器将需要的符号定义找出来
    - 二是待导出符号表的声明，不同系统平台有各自的方法。前面编译后，整体有很多 .o 文件，内有很多符号，有的符号其实代码中已经声明了需要导出，有的可能没有，默认情况下，未声明要不要导出的符号可以由编译器参数决定，`-fvisibility=default`控制导出所有`非 static`声明的符号，`-Wl,--version-script=符号表文件`可以控制精确哪些符号需要导出

### 安卓 Android
- 如果主程序启用了R8裁剪代码，则需要添加规则避免删除了`mediaxx`的代码
  - 已处理修复，如果仍有问题尝试以下做法：
  - 一般是在`主程序代码目录/android/app/proguard-rules.pro`内添加：
  ```pro
  -keep class run.bool.** { *;}
  ```
  - 已知如果不添加，插件附带的java代码会被删除，导致不能在程序启动时自动将App的`Context`传递到c/c++层，因此当直接读取安卓的Content-URl时会失败，ffmpeg 提示 `av_jni_set_android_app_ctx` 未设置
- 动态库符号导出控制：
  - 安卓端分了 arm32、arm64 和 x86_64，有时可能需要导出不同的符号，但我们只使用了一个符号表文件 libmpv-android-symbols.txt，当然区分开搞多个文件是可以的，也可以在 [libmpv-android-symbols.txt](src/ffmpeg-help/libmpv-android-symbols.txt) 内添加一行，允许链接时找不到符号也不报错：
```sh
--undefined-version
```

### Windows
- win端的动态库名称为 libmediaxx.dll
- 导出符号控制方式与其他系统不同，使用 .def 文件声明要导出的符号，并在 CMakeLists.txt 中编译时直接指定即可，见 [CMakeLists.txt](src/CMakeLists.txt):
```cmake
add_library(mediaxx SHARED
  ${DIR_IMPL}
  ${DIR_IMPL_ANDROID}
  ${DIR_UTIL}
  ${DIR_ANALYSE}

  ${mediaxx_EXPORT_DEF} # 这里直接在编译时指定导出符号
)
```
- 另外 windows 生成动态库时，一般还会生成导入库 libmediaxx.lib 这里虽然后缀是 .lib 但不是静态库，它只是声明了动态库的符号等信息，用于其他程序编译时可以链接，因此 win 端链接动态库时，可以只需要对应的导入库，而不需要动态库文件。如果还需要 .def 文件，可以从动态库生成，方法见 [win_create_dll_def.bat](script/win_create_dll_def.bat)

### Linux
- 整体类似于 Android

### IOS/Macos
- apple 端的动态库名称为 libmediaxx.dylib ，后缀不同于其他系统
- 另外动态库的导出符号需要前缀下划线，见:
  - [libmpv-apple.symbols.txt](src/ffmpeg-help/libmpv-apple.symbols.txt) 和 [libmpv-apple.exp](src/ffmpeg-help/libmpv-apple.exp)

## LICENSE
- MIT
- 如果静态、动态链接 ffmpeg、libmpv 等库时，可能会附带他们对应启用的 LGPL、GPLv2、GPLv3 
