cmake_minimum_required(VERSION 3.15)
project(integration_tests LANGUAGES C CXX)

if(BUILD_TESTS)
    # Assembler pipeline
    add_executable(assembler_integration

    )

    # Include directories (test-private)
    target_include_directories(assembler_integration
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    # Link libraries
    target_link_libraries(assembler_integration
        PRIVATE
            emulator32bit
            assembler
            util
            GTest::gtest_main
    )

    # Compiler flags
    target_compile_options(assembler_integration PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g --coverage>
        $<$<CONFIG:RelWithDebInfo>:-O3 -flto=auto>
        $<$<CONFIG:Release>:-O3 -flto=auto>
    )

    # GCC: link gcov for coverage
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_libraries(assembler_integration PRIVATE gcov)
    endif()

    # Define preprocessor symbols for paths to the respective executables.
    target_compile_definitions(assembler_integration PRIVATE
        EMULATOR_PATH="<TARGET_FILE:emulator32bit>" # TODO: we need a emulator32bit executable
        ASSEMBLER_PATH="<TARGET_FILE:basm>" # assembler executable target
    )

    # Discover GoogleTest tests automatically
    include(GoogleTest)
    gtest_discover_tests(assembler_integration PROPERTIES LABELS assembler_integration)


    # ccompiler Pipeline
    add_executable(ccompiler_integration

    )

    # Include directories (test-private)
    target_include_directories(ccompiler_integration
        PRIVATE
            $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
    )

    # Link libraries
    target_link_libraries(ccompiler_integration
        PRIVATE
            emulator32bit
            assembler
            ccompiler
            util
            GTest::gtest_main
    )

    # Compiler flags
    target_compile_options(ccompiler_integration PRIVATE
        -Wall -Wextra -Wpedantic
        $<$<CONFIG:Debug>:-g --coverage>
        $<$<CONFIG:RelWithDebInfo>:-O3 -flto=auto>
        $<$<CONFIG:Release>:-O3 -flto=auto>
    )

    # GCC: link gcov for coverage
    if (CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
        target_link_libraries(cccompiler_integration PRIVATE gcov)
    endif()

    # Define preprocessor symbols for paths to the respective executables.
    target_compile_definitions(ccompiler_integration PRIVATE
        EMULATOR_PATH="<TARGET_FILE:emulator32bit>" # TODO: we need a emulator32bit executable
        ASSEMBLER_PATH="<TARGET_FILE:basm>" # assembler executable target
        CCOMPILER_PATH="<TARGET_FILE:ccompiler>" # TODO: we need the ccompiler ready
    )

    # Discover GoogleTest tests automatically
    include(GoogleTest)
    gtest_discover_tests(cccompiler_integration PROPERTIES LABELS cccompiler_integration)
endif()
