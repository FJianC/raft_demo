#!/bin/bash

VERSION="Debug"
if [ "$1" = "release" ]; then
    $VERSION="Release"
fi

rm -rf build

mkdir -p build
cd build
cmake -DCMAKE_BUILD_TYPE=$VERSION ..
make all -j8
