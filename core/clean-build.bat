@echo off

rmdir "build" /s /q

:: Configure Debug build
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug  || exit /b
:: Configure Release build
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo  || exit /b

call build.bat