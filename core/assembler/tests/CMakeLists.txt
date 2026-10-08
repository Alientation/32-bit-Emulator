aemu_add_gtest(assembler_tests
    LABEL assembler
    LIBS aemu::assembler
    SOURCES
        ./preprocessor_test/include.cpp
        ./preprocessor_test/macro.cpp
        ./preprocessor_test/define.cpp
        ./preprocessor_test/conditional.cpp
        ./preprocessor_test/preprocessor_test.cpp
        ./assembler_test/assembler_test.cpp
        ./linker_test/linker_test.cpp
        ./object_file_test/object_file_test.cpp
        ./relocation_test/relocation_test.cpp
        ./build_test/build_test.cpp
        ./instruction_table_test/instruction_table_test.cpp
        ./tokenizer_test/tokenizer_test.cpp
)
