@echo off
rem set utf8
chcp 65001 > NUL

set build_dir=D:\0Acoolight\Program\flutter\mymusic\package\mediaxx\build
set src_dir=D:\0Acoolight\Program\flutter\mymusic\package\mediaxx\src

cmake -B "%build_dir%" -S "%src_dir%" -DLUMENXX_BUILD_TYPE=LUMENXX_BUILD_DEBUG -DCMAKE_BUILD_TYPE=Debug
cmake --build "%build_dir%" --config debug -v
cmake --install "%build_dir%" --config Debug --component Runtime

%build_dir%\\exec\\test.exe
