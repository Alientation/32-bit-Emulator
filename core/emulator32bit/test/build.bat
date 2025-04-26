@echo off
cmake -S . -B build/test -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo

cmake --build build/test

if errorlevel 1 exit /B

cd build/test && ctest --output-on-failure