cmake_minimum_required(VERSION 3.15)
project(util_tests LANGUAGES CXX)

if(BUILD_TESTS)
    # Create the test executable
    add_executable(util_tests
    	./util_tests/short_string_test.cpp
    )

    # Include directories (PRIVATE for test only)
    target_include_directories(util_tests
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    # Link required libraries
    target_link_libraries(util_tests
        PRIVATE
            util
            GTest::gtest_main
    )

    # Compiler flags per target
    target_compile_options(util_tests PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g --coverage>
        $<$<CONFIG:RelWithDebInfo>:-O3 -flto=auto>
        $<$<CONFIG:Release>:-O3 -flto=auto>
    )

    # GCC: link gcov for coverage
    if(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_libraries(util_tests PRIVATE gcov)
    endif()

    # Discover GoogleTest tests automatically
    include(GoogleTest)
    gtest_discover_tests(util_tests)
endif()