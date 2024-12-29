@echo off
REM This script configures and builds the core application and its dependencies

REM Configure and install the util library
cmake -S ../../util -B ../../util/build -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX="../../util/install"
cmake --build ../../util/build --target install

REM Configure and install the assembler library
cmake -S ../ ../build -G Ninja =DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_INSTALL_PREFIX="../install"
cmake --build ../build --target install

REM Configure and build the core application
cmake -S . -B build/test -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH="../../util/install;../install"

cmake --build build/test

cd build/test && ctest --output-on-failure