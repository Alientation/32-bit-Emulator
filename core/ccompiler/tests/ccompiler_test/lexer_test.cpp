#include <ccompiler_test/ccompiler_test.h>

TEST (lexer, lexer_print_empty)
{
    lexer_data_t lexer;
    lexer_init (&lexer);
    lexer_print (&lexer);
    lexer_free (&lexer);
}

TEST (lexer, lexer_keywords)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    TokenType toks[] =
    {
        TOKEN_KEYWORD_AUTO,
        TOKEN_KEYWORD_BREAK,
        TOKEN_KEYWORD_CASE,
        TOKEN_KEYWORD_CHAR,
        TOKEN_KEYWORD_CONST,
        TOKEN_KEYWORD_CONTINUE,
        TOKEN_KEYWORD_DEFAULT,
        TOKEN_KEYWORD_DO,
        TOKEN_KEYWORD_DOUBLE,
        TOKEN_KEYWORD_ELSE,
        TOKEN_KEYWORD_ENUM,
        TOKEN_KEYWORD_EXTERN,
        TOKEN_KEYWORD_FLOAT,
        TOKEN_KEYWORD_FOR,
        TOKEN_KEYWORD_GOTO,
        TOKEN_KEYWORD_IF,
        TOKEN_KEYWORD_INLINE,
        TOKEN_KEYWORD_INT,
        TOKEN_KEYWORD_LONG,
        TOKEN_KEYWORD_REGISTER,
        TOKEN_KEYWORD_RESTRICT,
        TOKEN_KEYWORD_RETURN,
        TOKEN_KEYWORD_SHORT,
        TOKEN_KEYWORD_SIGNED,
        TOKEN_KEYWORD_SIZEOF,
        TOKEN_KEYWORD_STATIC,
        TOKEN_KEYWORD_STRUCT,
        TOKEN_KEYWORD_SWITCH,
        TOKEN_KEYWORD_TYPEDEF,
        TOKEN_KEYWORD_UNION,
        TOKEN_KEYWORD_UNSIGNED,
        TOKEN_KEYWORD_VOID,
        TOKEN_KEYWORD_VOLATILE,
        TOKEN_KEYWORD_WHILE,
        TOKEN_KEYWORD_ALIGNAS,
        TOKEN_KEYWORD_ALIGNOF,
        TOKEN_KEYWORD_ATOMIC,
        TOKEN_KEYWORD_BOOL,
        TOKEN_KEYWORD_COMPLEX,
        TOKEN_KEYWORD_GENERIC,
        TOKEN_KEYWORD_IMAGINARY,
        TOKEN_KEYWORD_NORETURN,
        TOKEN_KEYWORD_STATIC_ASSERT,
        TOKEN_KEYWORD_THREAD_LOCAL,
        TOKEN_KEYWORD_FUNC_NAME
    };

    lex_str (
        "auto " "break " "case " "char " "const " "continue "
        "default " "do " "double " "else " "enum " "extern "
        "float " "for " "goto " "if " "inline " "int " "long " "register "
        "restrict " "return " "short " "signed " "sizeof " "static " "struct "
        "switch " "typedef " "union " "unsigned " "void " "volatile " "while "
        "_Alignas " "_Alignof " "_Atomic " "_Bool " "_Complex " "_Generic "
        "_Imaginary " "_Noreturn " "_Static_assert " "_Thread_local " "__func__ ",
        &lexer);

    EXPECT_GE (lexer.tok_cap, lexer.tok_cnt);
    EXPECT_EQ (lexer.tok_cnt, sizeof (toks) / sizeof (*toks));

    for (size_t i = 0; i < lexer.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.toks[i].type, toks[i]);
    }

    lexer_free (&lexer);
}

TEST (lexer, lexer_identifier)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    lex_str (
        "hello\n_this_is_AnOtHer Hello9_ ",
        &lexer);

    const int expected_tok_cnt = 3;

    EXPECT_GE (lexer.tok_cap, lexer.tok_cnt);
    EXPECT_EQ (lexer.tok_cnt, expected_tok_cnt);

    for (size_t i = 0; i < lexer.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.toks[i].type, TOKEN_IDENTIFIER);
    }

    EXPECT_EQ (lexer.toks[0].length, strlen ("hello"));
    EXPECT_EQ (lexer.toks[0].column, 1);
    EXPECT_EQ (lexer.toks[0].line, 1);
    EXPECT_EQ (strncmp (lexer.toks[0].src, "hello", lexer.toks[0].length), 0);

    EXPECT_EQ (lexer.toks[1].length, strlen ("_this_is_AnOtHer"));
    EXPECT_EQ (lexer.toks[1].column, 1);
    EXPECT_EQ (lexer.toks[1].line, 2);
    EXPECT_EQ (strncmp (lexer.toks[1].src, "_this_is_AnOtHer", lexer.toks[1].length), 0);

    EXPECT_EQ (lexer.toks[2].length, strlen ("Hello9_"));
    EXPECT_EQ (lexer.toks[2].column, 18);
    EXPECT_EQ (lexer.toks[2].line, 2);
    EXPECT_EQ (strncmp (lexer.toks[2].src, "Hello9_", lexer.toks[2].length), 0);

    lexer_free (&lexer);
}

TEST (lexer, lexer_integer)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    lex_str (
        "07213 0x1242fFaB 0XFFFF30 209419340 ",
        &lexer);

    const int expected_tok_cnt = 4;

    EXPECT_GE (lexer.tok_cap, lexer.tok_cnt);
    EXPECT_EQ (lexer.tok_cnt, expected_tok_cnt);

    for (size_t i = 0; i < lexer.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.toks[i].type, TOKEN_I_CONSTANT);
    }

    EXPECT_EQ (lexer.toks[0].length, strlen ("07213"));
    EXPECT_EQ (lexer.toks[0].line, 1);
    EXPECT_EQ (lexer.toks[0].column, 1);
    EXPECT_EQ (strncmp (lexer.toks[0].src, "07213", lexer.toks[0].length), 0);

    EXPECT_EQ (lexer.toks[1].length, strlen ("0x1242fFaB"));
    EXPECT_EQ (lexer.toks[1].line, 1);
    EXPECT_EQ (lexer.toks[1].column, 7);
    EXPECT_EQ (strncmp (lexer.toks[1].src, "0x1242fFaB", lexer.toks[1].length), 0);

    EXPECT_EQ (lexer.toks[2].length, strlen ("0XFFFF30"));
    EXPECT_EQ (lexer.toks[2].line, 1);
    EXPECT_EQ (lexer.toks[2].column, 18);
    EXPECT_EQ (strncmp (lexer.toks[2].src, "0XFFFF30", lexer.toks[2].length), 0);

    EXPECT_EQ (lexer.toks[3].length, strlen ("209419340"));
    EXPECT_EQ (lexer.toks[3].line, 1);
    EXPECT_EQ (lexer.toks[3].column, 27);
    EXPECT_EQ (strncmp (lexer.toks[3].src, "209419340", lexer.toks[3].length), 0);

    lexer_free (&lexer);
}

TEST (lexer, lexer_float)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    lex_str (
        "0.0901 1. 1e3 1E5 1e+4 7e-6 1.6e+0 2.f 3.4F 1e3l 1e4L",
        &lexer);

    const int expected_tok_cnt = 11;

    EXPECT_GE (lexer.tok_cap, lexer.tok_cnt);
    EXPECT_EQ (lexer.tok_cnt, expected_tok_cnt);

    for (size_t i = 0; i < lexer.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.toks[i].type, TOKEN_F_CONSTANT);
    }

    EXPECT_EQ (lexer.toks[0].length, strlen ("0.0901"));
    EXPECT_EQ (strncmp (lexer.toks[0].src, "0.0901", lexer.toks[0].length), 0);

    EXPECT_EQ (lexer.toks[1].length, strlen ("1."));
    EXPECT_EQ (strncmp (lexer.toks[1].src, "1.", lexer.toks[1].length), 0);

    EXPECT_EQ (lexer.toks[2].length, strlen ("1e3"));
    EXPECT_EQ (strncmp (lexer.toks[2].src, "1e3", lexer.toks[2].length), 0);

    EXPECT_EQ (lexer.toks[3].length, strlen ("1E5"));
    EXPECT_EQ (strncmp (lexer.toks[3].src, "1E5", lexer.toks[3].length), 0);

    EXPECT_EQ (lexer.toks[4].length, strlen ("1e+4"));
    EXPECT_EQ (strncmp (lexer.toks[4].src, "1e+4", lexer.toks[4].length), 0);

    EXPECT_EQ (lexer.toks[5].length, strlen ("7e-6"));
    EXPECT_EQ (strncmp (lexer.toks[5].src, "7e-6", lexer.toks[5].length), 0);

    EXPECT_EQ (lexer.toks[6].length, strlen ("1.6e+0"));
    EXPECT_EQ (strncmp (lexer.toks[6].src, "1.6e+0", lexer.toks[6].length), 0);

    EXPECT_EQ (lexer.toks[7].length, strlen ("2.f"));
    EXPECT_EQ (strncmp (lexer.toks[7].src, "2.f", lexer.toks[7].length), 0);

    EXPECT_EQ (lexer.toks[8].length, strlen ("3.4F"));
    EXPECT_EQ (strncmp (lexer.toks[8].src, "3.4F", lexer.toks[8].length), 0);

    EXPECT_EQ (lexer.toks[9].length, strlen ("1e3l"));
    EXPECT_EQ (strncmp (lexer.toks[9].src, "1e3l", lexer.toks[9].length), 0);

    EXPECT_EQ (lexer.toks[10].length, strlen ("1e34L"));
    EXPECT_EQ (strncmp (lexer.toks[10].src, "1e34L", lexer.toks[10].length), 0);

    lexer_free (&lexer);
}