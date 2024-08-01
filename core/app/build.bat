@echo off

REM This script configures and builds the core application and its dependencies
REM Configure and install the util library
cmake -S ../util -B ../util/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX="../util/install"
cmake --build ../util/build --target install

REM Configure and install the emulator32bit library
cmake -S ../emulator32bit -B ../emulator32bit/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX="../emulator32bit/install"
cmake --build ../emulator32bit/build --target install

REM Configure and install the assembler library
cmake -S ../assembler -B ../assembler/build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_INSTALL_PREFIX="../assembler/install"
cmake --build ../assembler/build --target install

REM Configure and build the core application
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH="../assembler/install;../emulator32bit/install;../util/install"
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=Release -DCMAKE_PREFIX_PATH="../assembler/install;../emulator32bit/install;../util/install"

cmake --build build/debug
cmake --build build/release
