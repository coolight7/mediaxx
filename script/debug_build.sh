#!/bin/bash

script_dir=$(dirname "$0")
root_dir=$script_dir/../
src_dir=$root_dir/src
build_dir=$root_dir/build/debug

# 依赖库 (cxx_utilxx_base / cxx_utilxx) 只用 Boost 头文件, 不链接 Boost 编译库:
# 默认在 src/third_party/boost/include 找, 也可以用 MEDIAXX_BOOST_INCLUDE 指定
boost_arg=""
if [ -n "$MEDIAXX_BOOST_INCLUDE" ]; then
    boost_arg="-DMEDIAXX_BOOST_INCLUDE=$MEDIAXX_BOOST_INCLUDE"
fi

cmake -B "$build_dir" -S "$src_dir" -DLUMENXX_BUILD_TYPE=LUMENXX_BUILD_DEBUG -DCMAKE_BUILD_TYPE=Debug $boost_arg

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
