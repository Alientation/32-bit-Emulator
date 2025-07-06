#!/bin/bash
set -e

cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo

cmake --build build/debug
cmake --build build/release

# Run tests for both Debug and Release configurations
echo Running Debug tests...
cd build/debug
ctest --build-config Debug --output-on-failure

cd ../../

echo Running Release tests...
cd build/release
ctest --build-config Release --output-on-failure