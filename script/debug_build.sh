#!/bin/bash

build_dir=$PWD/build/debug
src_dir=$PWD/src

cmake -B "$build_dir" -S "$src_dir" -DLUMENXX_BUILD_TYPE=LUMENXX_BUILD_DEBUG -DCMAKE_BUILD_TYPE=Debug

if errorlevel 1 (
    echo "cmake configure exec faild!"
    exit /b 1
)

cmake --build "$build_dir" --config debug

if errorlevel 1 (
    echo "cmake build exec faild!"
    exit /b 1
)

cmake --install "$build_dir" --config Debug

if errorlevel 1 (
    echo "cmake install exec faild!"
    exit /b 1
)

. $build_dir/exec/test.exe
