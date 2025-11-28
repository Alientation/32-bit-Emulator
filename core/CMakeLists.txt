cmake_minimum_required(VERSION 3.15)
project(EmulatorProject LANGUAGES C CXX)

option(BUILD_TESTS "Build tests" ON)

# Set C++ standard globally
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_C_STANDARD 11)

# Global warnings
add_compile_options(-Wall -Wextra -Wpedantic)

# Config specific global flags
set(CMAKE_CXX_FLAGS_DEBUG "-g -O0 --coverage")
set(CMAKE_CXX_FLAGS_RELWITHDEBINFO "-O3 -flto=auto")
set(CMAKE_CXX_FLAGS_RELEASE "-O3 -flto=auto")

# Set default build type for single-config generators
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release)
endif()


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
if (BUILD_TESTS)
    enable_testing()
endif()


# External dependency
add_subdirectory(external/cxxopts)

# Submodules
add_subdirectory(util)
add_subdirectory(emulator32bit)

# TODO: Reenable after ccompiler is fixed.
# add_subdirectory(ccompiler)

add_subdirectory(assembler)
add_subdirectory(app)