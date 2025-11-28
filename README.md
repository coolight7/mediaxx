# mediaxx

- 结合`ffmpeg`实现：
  - 读取音视频文件的基本信息（标题、艺术家、内嵌LRC歌词、时长、专辑、年份等）
  - 提取音视频的封面保存到文件
  - 分析图片颜色，归类出 主色调、亮色调4种、暗色调4种、综合颜色占比排序最高的4种

## 适配支持
- 已验证：
  - Android: arm64、arm32、x64
  - windows: x64
  - linux: x64
  - macos: arm64
  - ios: arm64

## Getting Started
- 该项目需要先编译出动态库 libmediaxx ，然后在主程序编译时复制动态库到 install安装根目录，运行时即可自动动态加载动态库
- 编译见：
  - android:   https://github.com/coolight7/libmpv-android-video-build
  - windows:   https://github.com/coolight7/mpv-winbuild-cmake
  - linux:     https://github.com/coolight7/libmpv-linux-build
  - macos/ios: https://github.com/coolight7/libmpv-apple-build
- 调整编译脚本即可控制动态、静态链接 ffmpeg、libmpv
- 静态链接时，需要考虑清楚c++标准库的链接方式

## 问题
  - `安卓`
  - 如果主程序启用了R8裁剪代码，则需要添加规则避免删除了`mediaxx`的代码
  - 已处理修复，如果仍有问题尝试以下做法：
  - 一般是在`主程序代码目录/android/app/proguard-rules.pro`内添加：
```pro
-keep class run.bool.** { *;}
```
  - 已知如果不添加，插件附带的java代码会被删除，导致不能在程序启动时自动将App的`Context`传递到c/c++层，因此当直接读取安卓的Content-URl时会失败，ffmpeg 提示 `av_jni_set_android_app_ctx` 未设置

## LICENSE
- MIT
- 如果静态、动态链接 ffmpeg、libmpv 等库时，可能会附带他们对应启用的 LGPL、GPLv2、GPLv3 
