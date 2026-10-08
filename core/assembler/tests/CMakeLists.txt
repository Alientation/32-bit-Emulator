aemu_add_gtest(assembler_tests
    LABEL assembler
    LIBS aemu::assembler
    SOURCES
        ./preprocessor_test/include.cpp
        ./preprocessor_test/macro.cpp
        ./preprocessor_test/define.cpp
        ./preprocessor_test/conditional.cpp
)
