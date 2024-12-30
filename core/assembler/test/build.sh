#!/bin/bash
cmake -S ../../util -B ../../util/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX="../../util/install"
cmake --build ../../util/build --target install

cmake -S ../../emulator32bit -B ../../emulator32bit/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX="../../emulator32bit/install"
cmake --build ../../emulator32bit/build --target install

cmake -S ../ ../build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX="../install"
cmake --build ../build --target install

cmake -S . -B build/test -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="../../util/install;../../emulator32bit/install;../install"

cmake --build build/test || exit 1

cd build/test && ctest --output-on-failure