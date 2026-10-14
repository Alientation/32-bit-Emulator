# Unit tests of the lexer and the preprocessor, which call the C library directly. The rest of
# the compiler is tested end to end (integration_tests/ccompiler_integration).
aemu_add_gtest(ccompiler_tests
    LABEL ccompiler
    LIBS ccompiler
    SOURCES
        ./lex_test.cpp
        ./preprocess_test.cpp
)
