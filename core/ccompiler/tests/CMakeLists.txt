cmake_minimum_required(VERSION 3.15)
project(ccompiler_test LANGUAGES CXX)

if(BUILD_TESTS)
    add_executable(ccompiler_test
        ./util_test/stringbuffer_test.cpp
        ./util_test/vector_test.cpp
        ./ccompiler_test/lexer_test.cpp
        ./ccompiler_test/lexer_cval_test.cpp
    )

    target_include_directories(ccompiler_test
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    target_link_libraries(ccompiler_test
        PRIVATE
            ccompiler
            GTest::gtest_main
    )

    target_compile_options(ccompiler_test PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g --coverage>
        $<$<CONFIG:RelWithDebInfo>:-O3 -flto=auto>
        $<$<CONFIG:Release>:-O3 -flto=auto>
    )

    target_link_options(ccompiler_test PRIVATE
        $<$<CONFIG:Debug>:--coverage>
        $<$<CONFIG:RelWithDebInfo>:-flto=auto>
        $<$<CONFIG:Release>:-flto=auto>
    )

    include(GoogleTest)
    gtest_discover_tests(ccompiler_test PROPERTIES LABELS ccompiler)
endif()