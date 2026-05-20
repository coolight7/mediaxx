#!/bin/bash

script_dir=$(dirname "$0")
src_dir=$script_dir/../
build_dir=$src_dir/build/debug

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

if [[ $1 != "--not-run-test" ]]; then
    LD_LIBRARY_PATH=$build_dir/exec $build_dir/exec/test
fi
