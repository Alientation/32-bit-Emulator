cmake_minimum_required(VERSION 3.15)
project(assembler_tests LANGUAGES CXX)

if(BUILD_TESTS)
    add_executable(assembler_tests
        ./preprocessor_test/include.cpp
        ./preprocessor_test/macro.cpp
        ./preprocessor_test/define.cpp
        ./preprocessor_test/conditional.cpp
    )

    target_include_directories(assembler_tests
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    target_link_libraries(assembler_tests
        PRIVATE
            assembler
            util
            emulator32bit
            GTest::gtest_main
    )

    target_compile_options(assembler_tests PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g --coverage>
        $<$<CONFIG:RelWithDebInfo>:-O3 -flto=auto>
        $<$<CONFIG:Release>:-O3 -flto=auto>
    )

    target_link_options(assembler_tests PRIVATE
        $<$<CONFIG:Debug>:--coverage>
        $<$<CONFIG:RelWithDebInfo>:-flto=auto>
        $<$<CONFIG:Release>:-flto=auto>
    )

    include(GoogleTest)
    gtest_discover_tests(assembler_tests PROPERTIES LABELS assembler)
endif()