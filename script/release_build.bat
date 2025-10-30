@echo off
rem set utf8
chcp 65001 > NUL

set build_dir=D:\0Acoolight\Program\flutter\mymusic\package\mediaxx\build_rel
set src_dir=D:\0Acoolight\Program\flutter\mymusic\package\mediaxx\src

cmake -B "%build_dir%" -S "%src_dir%" -DLUMENXX_BUILD_TYPE=LUMENXX_BUILD_RELEASE -DCMAKE_BUILD_TYPE=Release
cmake --build "%build_dir%" --config release
cmake --install "%build_dir%" --config release

%build_dir%\\exec\\test.exe
