cmake_minimum_required(VERSION 3.15)
project(assembler_tests LANGUAGES CXX)

if(BUILD_TESTS)
    # Create the test executable
    add_executable(assembler_tests
        ./preprocessor_test/include.cpp
        ./preprocessor_test/macro.cpp
        ./preprocessor_test/define.cpp
        ./preprocessor_test/conditional.cpp
    )

    # Include directories (PRIVATE for test only)
    target_include_directories(assembler_tests
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    # Link required libraries
    target_link_libraries(assembler_tests
        PRIVATE
            assembler
            util
            emulator32bit
            GTest::gtest_main
    )

    # Compiler flags per target
    target_compile_options(assembler_tests PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g --coverage>
        $<$<CONFIG:RelWithDebInfo>:-O3 -flto=auto>
        $<$<CONFIG:Release>:-O3 -flto=auto>
    )

    # GCC: link gcov for coverage
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_libraries(assembler_tests PRIVATE gcov)
    endif()

    # Discover GoogleTest tests automatically
    include(GoogleTest)
    gtest_discover_tests(assembler_tests)
endif()