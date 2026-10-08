# Shared target configuration for the project.
#
# This is a function rather than an INTERFACE "options" library: a PRIVATE link to an INTERFACE
# library still appears as $<LINK_ONLY:...> in the export sets of installed static libraries,
# which would make install(EXPORT) fail.

# Applies the project-wide warning, optimization, and coverage settings to a target.
#   target: name of an existing library or executable target
function(aemu_target_defaults target)
    set(gcc_like "$<OR:$<C_COMPILER_ID:GNU,Clang,AppleClang>,$<CXX_COMPILER_ID:GNU,Clang,AppleClang>>")
    set(msvc "$<OR:$<C_COMPILER_ID:MSVC>,$<CXX_COMPILER_ID:MSVC>>")
    set(optimized "$<OR:$<CONFIG:Release>,$<CONFIG:RelWithDebInfo>>")

    target_compile_options(${target} PRIVATE
        "$<${gcc_like}:-Wall;-Wextra;-Wpedantic>"
        $<${msvc}:/W4>
        $<$<BOOL:${AEMU_WARNINGS_AS_ERRORS}>:$<${gcc_like}:-Werror>$<${msvc}:/WX>>

        # CMake's RelWithDebInfo default is -O2; the emulator is tuned for -O3.
        $<$<AND:${gcc_like},${optimized}>:-O3>
    )
    # With LTO, code generation happens at link time, so the link step needs -O3 as well.
    target_link_options(${target} PRIVATE $<$<AND:${gcc_like},${optimized}>:-O3>)

    if(AEMU_ENABLE_COVERAGE)
        target_compile_options(${target} PRIVATE $<$<AND:${gcc_like},$<CONFIG:Debug>>:--coverage>)
        # --coverage at link time pulls in libgcov, so no explicit gcov link is needed.
        target_link_options(${target} PRIVATE $<$<AND:${gcc_like},$<CONFIG:Debug>>:--coverage>)
    endif()
endfunction()

# Creates a GoogleTest executable and registers its tests with CTest.
#   target:  name of the executable target
#   LABEL:   CTest label for filtering (ctest -L <label>)
#   SOURCES: test source files
#   LIBS:    libraries under test (linked PRIVATE alongside GTest::gtest_main)
function(aemu_add_gtest target)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "LABEL" "SOURCES;LIBS")

    add_executable(${target} ${arg_SOURCES})

    # Test-private headers (fixtures, helpers)
    if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/include")
        target_include_directories(${target} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/include")
    endif()

    target_link_libraries(${target} PRIVATE ${arg_LIBS} GTest::gtest_main)
    aemu_target_defaults(${target})

    include(GoogleTest)
    gtest_discover_tests(${target} PROPERTIES LABELS ${arg_LABEL})
endfunction()
