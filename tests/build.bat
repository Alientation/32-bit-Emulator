@echo off

REM This script configures and builds the core libraries
REM Configure and install the assembler library
cmake -S ../core/assembler -B ../core/assembler/build -G Ninja -DCMAKE_INSTALL_PREFIX=../core/assembler/install
cmake --build ../core/assembler/build --target install

REM Configure and install the emulator6502 library
cmake -S ../core/emulator6502 -B ../core/emulator6502/build -G Ninja -DCMAKE_INSTALL_PREFIX=../core/emulator6502/install
cmake --build ../core/emulator6502/build --target install

REM Configure and install the util library
cmake -S ../core/util -B ../core/util/build -G Ninja -DCMAKE_INSTALL_PREFIX=../core/util/install
cmake --build ../core/util/build --target install

REM Configure and build the test application
cmake -S . -B build/test -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=../core/assembler/install;../core/emulator6502/install

cmake --build build/test

cd build/test && ctest