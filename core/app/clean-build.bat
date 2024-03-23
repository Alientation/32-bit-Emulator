@echo off

REM This script cleans the build and install directories and then configures and builds the core application and its dependencies
rmdir "../assembler/build" /s /q
rmdir "../assembler/install" /s /q
rmdir "../emulator6502/build" /s /q 
rmdir "../emulator6502/install" /s /q
rmdir "../util/build" /s /q
rmdir "../util/install" /s /q

rmdir "build" /s /q


REM Configure and build the application
call build.bat