#!/bin/bash


# This script configures and builds the core application and its dependencies
# Configure and install the assembler library
cmake -S ../assembler -B ../assembler/build -G Ninja -DCMAKE_INSTALL_PREFIX=../assembler/install
cmake --build ../assembler/build --target install

# Configure and install the emulator6502 library
cmake -S ../emulator6502 -B ../emulator6502/build -G Ninja -DCMAKE_INSTALL_PREFIX=../emulator6502/install
cmake --build ../emulator6502/build --target install

# Configure and install the util library
cmake -S ../util -B ../util/build -G Ninja -DCMAKE_INSTALL_PREFIX=../util/install
cmake --build ../util/build --target install


# Configure and build the core application
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_PREFIX_PATH=../assembler/install;../emulator6502/install
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_PREFIX_PATH=../assembler/install;../emulator6502/install

cmake --build build/debug
cmake --build build/release