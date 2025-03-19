@echo off

REM This script cleans the build and install directories and then configures and builds the core application and its dependencies
rmdir "build" /s /q


REM Configure and build the application
call build.bat