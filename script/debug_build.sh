#!/bin/bash

build_dir=$PWD/build/debug
src_dir=$PWD/src

cmake -B "$build_dir" -S "$src_dir" -DLUMENXX_BUILD_TYPE=LUMENXX_BUILD_DEBUG -DCMAKE_BUILD_TYPE=Debug

if [[ $? -ne 0 ]]; then
    echo "cmake config failed!"
    exit 1
fi

cmake --build "$build_dir" --config debug

if [[ $? -ne 0 ]]; then
    echo "cmake build failed!"
    exit 1
fi

cmake --install "$build_dir" --config Debug

if [[ $? -ne 0 ]]; then
    echo "cmake install failed!"
    exit 1
fi

LD_LIBRARY_PATH=$build_dir/exec $build_dir/exec/test
