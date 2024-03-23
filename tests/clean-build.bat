@echo off

REM This script cleans the build and install directories and then configures and builds the core application and its dependencies
rmdir "../core/assembler/build" /s /q
rmdir "../core/assembler/install" /s /q
rmdir "../core/emulator6502/build" /s /q 
rmdir "../core/emulator6502/install" /s /q
rmdir "../core/util/build" /s /q
rmdir "../core/util/install" /s /q

rmdir "build" /s /q

REM Configure and build the tests
call build.bat