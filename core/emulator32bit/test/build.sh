#!/bin/bash
cmake -S . -B build/test -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo

cmake --build build/test || exit 1

cd build/test && ctest --output-on-failure