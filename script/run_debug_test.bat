@echo off
rem set utf8
chcp 65001 > NUL

set build_dir=D:\0Acoolight\Program\flutter\mymusic\package\mediaxx\build_bin
set src_dir=D:\0Acoolight\Program\flutter\mymusic\package\mediaxx\src

cmake --build "%build_dir%" --config debug
cmake --install "%build_dir%" --config Debug

%build_dir%\\exec\\test.exe