cmake_minimum_required(VERSION 3.15)
project(EmulatorProject LANGUAGES C CXX)

option(BUILD_TESTS "Build tests" ON)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Set default build type for single-config generators
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()

include(FetchContent)
FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/heads/main.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)
FetchContent_MakeAvailable(googletest)

if (BUILD_TESTS)
    enable_testing()
endif()

add_subdirectory(util)
add_subdirectory(emulator32bit)
add_subdirectory(ccompiler)
add_subdirectory(assembler)
add_subdirectory(app)