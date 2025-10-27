dumpbin /exports avcodec.dll > avcodec.def
dumpbin /exports avdevice.dll > avdevice.def
dumpbin /exports avfilter.dll > avfilter.def
dumpbin /exports avformat.dll > avformat.def
dumpbin /exports avutil.dll > avutil.def
dumpbin /exports swresample.dll > swresample.def
dumpbin /exports swscale.dll > swscale.def

rem 修改 xx.def 内容格式

lib /def:avcodec.def /machine:x64 /out:avcodec.lib
lib /def:avdevice.def /machine:x64 /out:avdevice.lib
lib /def:avfilter.def /machine:x64 /out:avfilter.lib
lib /def:avformat.def /machine:x64 /out:avformat.lib
lib /def:avutil.def /machine:x64 /out:avutil.lib
lib /def:swresample.def /machine:x64 /out:swresample.lib
lib /def:swscale.def /machine:x64 /out:swscale.lib