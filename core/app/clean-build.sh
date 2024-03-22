#!/bin/bash

# This script cleans the build and install directories and then configures and builds the core application and its dependencies
rm -rf ../assembler/build
rm -rf ../assembler/install
rm -rf ../emulator6502/build
rm -rf ../emulator6502/install
rm -rf ../util/build
rm -rf ../util/install

rm -rf /build


# Configure and install the application
source /build.bat