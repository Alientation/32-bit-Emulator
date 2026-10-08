cmake_minimum_required(VERSION 3.15)
project(EmulatorProject LANGUAGES C CXX)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

option(BUILD_TESTS "Build tests" ON)
option(AEMU_ENABLE_COVERAGE "Instrument Debug builds with --coverage" ON)
option(AEMU_WARNINGS_AS_ERRORS "Treat compiler warnings as errors" OFF)
option(AEMU_ENABLE_LTO "Enable link-time optimization for Release/RelWithDebInfo" ON)

# Project-wide defaults (also applied to third-party targets). Our own libraries additionally
# declare cxx_std_20 as a usage requirement for installed consumers.
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)

# Set default build type for single-config generators
if(NOT CMAKE_BUILD_TYPE AND NOT CMAKE_CONFIGURATION_TYPES)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
    set_property(CACHE CMAKE_BUILD_TYPE PROPERTY STRINGS Debug Release RelWithDebInfo MinSizeRel)
endif()

if(AEMU_ENABLE_LTO)
    include(CheckIPOSupported)
    check_ipo_supported(RESULT ipo_supported OUTPUT ipo_output LANGUAGES C CXX)
    if(ipo_supported)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELEASE ON)
        set(CMAKE_INTERPROCEDURAL_OPTIMIZATION_RELWITHDEBINFO ON)
    else()
        message(WARNING "LTO requested but not supported: ${ipo_output}")
    endif()
endif()

list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")
include(AemuHelpers)

if(BUILD_TESTS)
    enable_testing()

    include(FetchContent)
    set(INSTALL_GTEST OFF CACHE BOOL "" FORCE)
    set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
    FetchContent_Declare(
        googletest
        URL https://github.com/google/googletest/archive/refs/tags/v1.17.0.zip
        URL_HASH SHA256=40d4ec942217dcc84a9ebe2a68584ada7d4a33a8ee958755763278ea1c5e18ff
        DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    )
    FetchContent_MakeAvailable(googletest)
endif()

# External dependency
if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/external/cxxopts/CMakeLists.txt")
    message(FATAL_ERROR
        "cxxopts submodule is missing. Run: git submodule update --init --recursive")
endif()
set(CXXOPTS_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(CXXOPTS_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(CXXOPTS_ENABLE_INSTALL OFF CACHE BOOL "" FORCE)
add_subdirectory(external/cxxopts)

# Submodules
add_subdirectory(util)
add_subdirectory(emulator32bit)
# Comment out ccompiler for now until it is fixed.
# add_subdirectory(ccompiler)
add_subdirectory(assembler)
add_subdirectory(app)

# Integration tests
if(BUILD_TESTS)
    add_subdirectory(integration_tests)
endif()
