@echo off
REM This script configures and builds the core application and its dependencies

REM Configure and install the util library
cmake -S ../../util -B ../../util/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX="../../util/install"
cmake --build ../../util/build --target install

REM Configure and install the emulator32bit library
cmake -S ../../emulator32bit -B ../../emulator32bit/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX="../../emulator32bit/install"
cmake --build ../../emulator32bit/build --target install

REM Configure and install the assembler library
cmake -S ../ ../build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX="../install"
cmake --build ../build --target install

REM Configure and build the core application
cmake -S . -B build/test -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="../../util/install;../../emulator32bit/install;../install"

cmake --build build/test

if errorlevel 1 exit /B

cd build/test && ctest --output-on-failure