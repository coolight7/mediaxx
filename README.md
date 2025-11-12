# mediaxx

- 结合`ffmpeg`实现：
  - 读取音视频文件的基本信息（标题、艺术家、内嵌LRC歌词、时长、专辑、年份等）
  - 提取音视频的封面保存到文件

## Getting Started
- `安卓`
  - 如果主程序启用了R8裁剪代码，则需要添加规则避免删除了`mediaxx`的代码
  - 一般是在`主程序代码目录/android/app/proguard-rules.pro`内添加：
```pro
-keep class run.bool.** { *;}
```
  - 已知如果不添加，插件附带的java代码会被删除，导致不能在程序启动时自动将App的`Context`传递到c/c++层，因此当直接读取安卓的Content-URl时会失败，ffmpeg 提示 `av_jni_set_android_app_ctx` 未设置
