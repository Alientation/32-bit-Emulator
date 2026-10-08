aemu_add_gtest(ccompiler_test
    LABEL ccompiler
    LIBS ccompiler
    SOURCES
        ./util_test/stringbuffer_test.cpp
        ./ccompiler_test/lexer_test.cpp
)
