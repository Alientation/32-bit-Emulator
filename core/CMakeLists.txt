cmake_minimum_required(VERSION 3.15)
project(EmulatorProject LANGUAGES CXX)

# Set C++ standard globally
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Setup FetchContent to grab external libraries like GoogleTest
include(FetchContent)

FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
# Actually fetch and make available
FetchContent_MakeAvailable(googletest)

# Enable testing support (CTest)
enable_testing()

# Add subdirectories (these will also define their own libraries, tests, etc.)
add_subdirectory(util)
add_subdirectory(emulator32bit)
add_subdirectory(ccompiler)
add_subdirectory(assembler)
add_subdirectory(app)