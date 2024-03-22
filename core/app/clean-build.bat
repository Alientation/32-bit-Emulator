@echo off

REM This script cleans the build and install directories and then configures and builds the core application and its dependencies
rmdir "%cd%/../assembler/build" /s /q
rmdir "%cd%/../assembler/install" /s /q
rmdir "%cd%/../emulator6502/build" /s /q 
rmdir "%cd%/../emulator6502/install" /s /q
rmdir "%cd%/../util/build" /s /q
rmdir "%cd%/../util/install" /s /q

rmdir "%cd%/build" /s /q


REM Configure and install the application
call configure-and-build-all.bat