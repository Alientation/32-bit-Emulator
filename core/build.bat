@echo off

:: Configure Debug build
cmake -S . -B build/debug -G Ninja -DCMAKE_BUILD_TYPE=Debug  || exit /b
:: Configure Release build
cmake -S . -B build/release -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo  || exit /b

:: Build Debug and Release configurations
cmake --build build/debug  || exit /b
cmake --build build/release  || exit /b

:: Run tests for both Debug and Release configurations
echo Running Debug tests...
cd build/debug || exit /b
ctest --build-config Debug --output-on-failure  || exit /b

echo Running Release tests...
cd build/release || exit /b
ctest --build-config Release --output-on-failure || exit /b