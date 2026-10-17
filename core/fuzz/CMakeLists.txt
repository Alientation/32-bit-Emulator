# Fuzz targets (docs/internals.md#fuzzing). A target is one .cpp with LLVMFuzzerTestOneInput and is
# built in two ways: <name>_replay runs the saved inputs in corpus/<name> as a test of every build,
# and with AEMU_FUZZ (clang, tools/fuzz.sh) <name>_fuzzer is the libFuzzer binary.
#   name: the .cpp and the corpus directory
#   LIBS: what the target links
function(aemu_add_fuzz_target name)
    cmake_parse_arguments(PARSE_ARGV 1 arg "" "" "LIBS")

    if(BUILD_TESTS AND NOT AEMU_FUZZ)
        add_executable(${name}_replay ${name}.cpp replay_main.cpp)
        target_link_libraries(${name}_replay PRIVATE ${arg_LIBS})
        aemu_target_defaults(${name}_replay)
        add_test(NAME fuzz_corpus.${name}
                 COMMAND ${name}_replay "${CMAKE_CURRENT_SOURCE_DIR}/corpus/${name}")
        # Without the sanitizers' memory limit, an input that once made the program allocate
        # gigabytes is only slow: the timeout is what fails it.
        set_tests_properties(fuzz_corpus.${name} PROPERTIES LABELS fuzz TIMEOUT 60)
    endif()

    if(AEMU_FUZZ)
        add_executable(${name}_fuzzer ${name}.cpp)
        target_link_libraries(${name}_fuzzer PRIVATE ${arg_LIBS})
        aemu_target_defaults(${name}_fuzzer)
        target_link_options(${name}_fuzzer PRIVATE -fsanitize=fuzzer)
    endif()
endfunction()

aemu_add_fuzz_target(bexe LIBS aemu::assembler)
aemu_add_fuzz_target(run_modes LIBS aemu::emulator32bit)
aemu_add_fuzz_target(mmu LIBS aemu::emulator32bit)
aemu_add_fuzz_target(basm LIBS aemu::assembler)
aemu_add_fuzz_target(link LIBS aemu::assembler)
aemu_add_fuzz_target(library LIBS aemu::assembler)
aemu_add_fuzz_target(gdb LIBS aemu::emulator32bit)
