@echo off

REM This script configures and builds the core application and its dependencies
REM Configure and install the assembler library
cmake -S %cd%/../assembler -B %cd%/../assembler/build -G Ninja -DCMAKE_INSTALL_PREFIX=%cd%/../assembler/install
cmake --build %cd%/../assembler/build --target install

REM Configure and install the emulator6502 library
cmake -S %cd%/../emulator6502 -B %cd%/../emulator6502/build -G Ninja -DCMAKE_INSTALL_PREFIX=%cd%/../emulator6502/install
cmake --build %cd%/../emulator6502/build --target install

REM Configure and install the util library
cmake -S %cd%/../util -B %cd%/../util/build -G Ninja -DCMAKE_INSTALL_PREFIX=%cd%/../util/install
cmake --build %cd%/../util/build --target install


REM Configure and build the core application
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=%cd%/../assembler/install;%cd%/../emulator6502/install
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=%cd%/../assembler/install;%cd%/../emulator6502/install

cmake --build build/debug
cmake --build build/release