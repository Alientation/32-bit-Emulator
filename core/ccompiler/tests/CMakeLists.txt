cmake_minimum_required(VERSION 3.15)
project(ccompiler_test LANGUAGES CXX)

if(BUILD_TESTS)
    # Create the test executable
    add_executable(ccompiler_test
        ./util_test/stringbuffer_test.cpp
        ./ccompiler_test/lexer_test.cpp
    )

    # Include directories
    target_include_directories(
        ccompiler_test
        PUBLIC
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
            $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
    )

    # Link libraries
    target_link_libraries(ccompiler_test
        PRIVATE
            ccompiler
            GTest::gtest_main
    )

    # GCC-specific code coverage library
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_libraries(ccompiler_test PRIVATE gcov)
    endif()

    # Auto-discover tests with GoogleTest
    include(GoogleTest)
    gtest_discover_tests(ccompiler_test PROPERTIES LABELS ccompiler)
endif()