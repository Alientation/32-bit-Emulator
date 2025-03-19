@echo off

REM This script configures and builds the core application and its dependencies
REM Configure and build the core application
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo

cmake --build build/debug
cmake --build build/release
