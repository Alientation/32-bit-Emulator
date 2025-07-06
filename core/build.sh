#!/bin/bash
set -e

cmake --build build/debug
cmake --build build/release

# Run tests for both Debug and Release configurations
echo Running Debug tests...
cd build/debug
ctest --build-config Debug --progress --output-on-failure

cd ../../

echo Running Release tests...
cd build/release
ctest --build-config Release --progress --output-on-failure