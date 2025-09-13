@echo off

IF NOT EXIST "./build/debug" md "./build/debug"
IF NOT EXIST "./build/release" md "./build/release"

:: Configure Debug and Release if not already done
cmake -S . -B build/debug -G "Ninja" -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build/release -G "Ninja" -DCMAKE_BUILD_TYPE=RelWithDebInfo

:: Build Debug and Release configurations
cmake --build build/debug --config Debug || exit /b
cmake --build build/release --config RelWithDebInfo  || exit /b

:: Run tests for both Debug and Release configurations
echo Running Debug tests...
cd build/debug || exit /b
ctest --build-config Debug --progress --output-on-failure  || exit /b

cd ../../

echo Running Release tests...
cd build/release || exit /b
ctest --build-config Release --progress --output-on-failure || exit /b