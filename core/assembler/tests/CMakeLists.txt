aemu_add_gtest(assembler_tests
    LABEL assembler
    LIBS aemu::assembler
    SOURCES
        ./assembler_test.cpp
        ./build_test.cpp
        ./instruction_table_test.cpp
        ./linker_test.cpp
        ./object_file_test.cpp
        ./preprocessor_programs_test.cpp
        ./preprocessor_test.cpp
        ./relocation_test.cpp
        ./tokenizer_test.cpp
)
