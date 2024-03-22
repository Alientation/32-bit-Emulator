@echo off

cmake -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=%cd%/../assembler/install;%cd%/../emulator6502/install
cmake -B build/release -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=%cd%/../assembler/install;%cd%/../emulator6502/install

cmake --build build/debug
cmake --build build/release