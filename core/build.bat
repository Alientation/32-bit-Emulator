@echo off

:: Build Debug and Release configurations
cmake --build build/debug  || exit /b
cmake --build build/release  || exit /b

:: Run tests for both Debug and Release configurations
echo Running Debug tests...
cd build/debug || exit /b
ctest --build-config Debug --progress --output-on-failure  || exit /b

cd ../../

echo Running Release tests...
cd build/release || exit /b
ctest --build-config Release --progress --output-on-failure || exit /b