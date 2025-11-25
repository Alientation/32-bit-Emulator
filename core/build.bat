@echo off
setlocal enabledelayedexpansion

:: Optional: pass "clean" as first argument to remove previous build
set CLEAN=%1

set BUILD_DIR=build
set DEBUG_DIR=%BUILD_DIR%\debug
set RELEASE_DIR=%BUILD_DIR%\release

if /I "%CLEAN%"=="clean" (
    echo Cleaning previous build directories...
    rmdir /s /q "%BUILD_DIR%"
)

:: Create build directories
if not exist "%DEBUG_DIR%" md "%DEBUG_DIR%"
if not exist "%RELEASE_DIR%" md "%RELEASE_DIR%"

:: Configure builds
echo Configuring Debug build...
cmake -S . -B "%DEBUG_DIR%" -G "Ninja" -DCMAKE_BUILD_TYPE=Debug

echo Configuring Release build...
cmake -S . -B "%RELEASE_DIR%" -G "Ninja" -DCMAKE_BUILD_TYPE=RelWithDebInfo

:: Build
echo Building Debug...
cmake --build "%DEBUG_DIR%" --config Debug

echo Building Release...
cmake --build "%RELEASE_DIR%" --config RelWithDebInfo

:: Run tests
echo Running Debug tests...
ctest --test-dir "%DEBUG_DIR%" --build-config Debug --progress --output-on-failure

echo Running Release tests...
ctest --test-dir "%RELEASE_DIR%" --build-config RelWithDebInfo --progress --output-on-failure

echo Build and test completed successfully!
endlocal