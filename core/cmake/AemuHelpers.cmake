# Shared target configuration for the project.
#
# This is a function rather than an INTERFACE "options" library: a PRIVATE link to an INTERFACE
# library still appears as $<LINK_ONLY:...> in the export sets of installed static libraries,
# which would make install(EXPORT) fail.

# Turns AEMU_SANITIZE into the -fsanitize flag that aemu_target_defaults adds (empty when off).
set(AEMU_SANITIZE_FLAG "")
if(AEMU_SANITIZE)
    foreach(sanitizer IN LISTS AEMU_SANITIZE)
        if(NOT sanitizer MATCHES "^(address|undefined|leak|thread)$")
            message(FATAL_ERROR
                "AEMU_SANITIZE: unknown sanitizer '${sanitizer}' (address, undefined, leak, thread)")
        endif()
    endforeach()
    # The thread sanitizer cannot be combined with the address and leak sanitizers.
    if("thread" IN_LIST AEMU_SANITIZE AND ("address" IN_LIST AEMU_SANITIZE
                                           OR "leak" IN_LIST AEMU_SANITIZE))
        message(FATAL_ERROR "AEMU_SANITIZE: thread cannot be combined with address or leak")
    endif()
    list(JOIN AEMU_SANITIZE "," sanitizer_list)
    set(AEMU_SANITIZE_FLAG "-fsanitize=${sanitizer_list}")
    message(STATUS "Sanitizers: ${sanitizer_list}")
endif()

# Applies the project-wide warning, optimization, sanitizer and coverage settings to a target.
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

    if(AEMU_SANITIZE_FLAG)
        # A report has to fail the test: by default the undefined behavior sanitizer prints the
        # error and carries on. The frame pointer gives the reports usable stack traces, and the
        # assertions of the standard library check the indices of vector, array and string.
        target_compile_options(${target} PRIVATE
            "$<${gcc_like}:${AEMU_SANITIZE_FLAG};-fno-sanitize-recover=all;-fno-omit-frame-pointer>")
        target_compile_definitions(${target} PRIVATE "$<${gcc_like}:_GLIBCXX_ASSERTIONS>")
        target_link_options(${target} PRIVATE "$<${gcc_like}:${AEMU_SANITIZE_FLAG}>")
    endif()

    if(AEMU_ENABLE_COVERAGE)
        target_compile_options(${target} PRIVATE $<$<AND:${gcc_like},$<CONFIG:Debug>>:--coverage>)
        # --coverage at link time pulls in libgcov, so no explicit gcov link is needed.
        target_link_options(${target} PRIVATE $<$<AND:${gcc_like},$<CONFIG:Debug>>:--coverage>)

        # Whenever the target is relinked/re-archived (some object was recompiled), drop the
        # old run data of its objects: it no longer matches the new .gcno and libgcov would
        # report "profiling error" at the next test run.
        add_custom_command(TARGET ${target} PRE_LINK
            COMMAND ${CMAKE_COMMAND} "-DDIR=${CMAKE_CURRENT_BINARY_DIR}/CMakeFiles/${target}.dir"
                    -P "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/remove_gcda.cmake"
            VERBATIM)
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
