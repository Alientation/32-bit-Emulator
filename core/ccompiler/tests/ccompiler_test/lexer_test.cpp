#include <ccompiler_test/ccompiler_test.h>

#include <stdio.h>

TEST (lexer, lexer_print_empty)
{
    lexer_data_t lexer;
    lexer_init (&lexer);
    lexer_print (&lexer);

    EXPECT_TRUE (lex_str ("", &lexer));
    EXPECT_EQ (lexer.tokarr.tok_cnt, 0);

    lexer_free (&lexer);
}

TEST (lexer, lexer_only_whitespace_produces_no_tokens)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("   \t  \n  \n  ", &lexer));
    EXPECT_EQ (lexer.tokarr.tok_cnt, 0);

    lexer_free (&lexer);
}

TEST (lexer, lexer_newline_in_string)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("\"Hello\\nWorld!\";", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 2);
    EXPECT_EQ (lexer.tokarr.toks[0].line, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].col, 1);

    EXPECT_EQ (lexer.tokarr.toks[1].line, 1);
    EXPECT_EQ (lexer.tokarr.toks[1].col, 16);

    lexer_free (&lexer);
}

TEST (lexer, lexer_os_linebreaks)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("int x = 0;\r\nint y = 0;\nint z = 0;\rreturn x + y + z;\n", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 22);
    EXPECT_EQ (lexer.tokarr.toks[0].col, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].line, 1);

    EXPECT_EQ (lexer.tokarr.toks[5].col, 1);
    EXPECT_EQ (lexer.tokarr.toks[5].line, 2);

    EXPECT_EQ (lexer.tokarr.toks[10].col, 1);
    EXPECT_EQ (lexer.tokarr.toks[10].line, 3);

    EXPECT_EQ (lexer.tokarr.toks[15].col, 1);
    EXPECT_EQ (lexer.tokarr.toks[15].line, 4);

    lexer_free (&lexer);
}

TEST (lexer, lexer_escaping_os_linebreaks)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("int x = 0;\\\r\ni\\\nnt y = 0;\\\nin\\\nt z = 0;\\\rr\\\ne\\\nt\\\nu\\\nr\\\nn x + y + z;\\\n", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 22);
    EXPECT_EQ (lexer.tokarr.toks[0].col, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].line, 1);

    EXPECT_EQ (lexer.tokarr.toks[5].col, 1);
    EXPECT_EQ (lexer.tokarr.toks[5].line, 2);

    EXPECT_EQ (lexer.tokarr.toks[10].col, 1);
    EXPECT_EQ (lexer.tokarr.toks[10].line, 4);

    EXPECT_EQ (lexer.tokarr.toks[15].col, 1);
    EXPECT_EQ (lexer.tokarr.toks[15].line, 6);

    lexer_free (&lexer);
}

TEST (lexer, lexer_whitespace)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("\vint x \f= 0;\f\r\nint \fy \f= 0;\v\n\fint z =\v 0;\v\r\freturn x + y + z;\n\v\f", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 22);
    EXPECT_EQ (lexer.tokarr.toks[0].col, 2);
    EXPECT_EQ (lexer.tokarr.toks[0].line, 1);

    EXPECT_EQ (lexer.tokarr.toks[5].col, 1);
    EXPECT_EQ (lexer.tokarr.toks[5].line, 2);

    EXPECT_EQ (lexer.tokarr.toks[10].col, 2);
    EXPECT_EQ (lexer.tokarr.toks[10].line, 3);

    EXPECT_EQ (lexer.tokarr.toks[15].col, 2);
    EXPECT_EQ (lexer.tokarr.toks[15].line, 4);

    lexer_free (&lexer);
}


TEST (lexer, lexer_file)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    const char *TEMPFILE = "test_lexer_file.s";

    FILE *file = fopen (TEMPFILE, "w");
    ASSERT_TRUE (file != NULL);

    fprintf (file, "int x = 0;\r\nint y = 0;\nint z = 0;\r\nreturn x + y + z;\n");
    ASSERT_EQ (fclose(file), 0);

    EXPECT_TRUE (lex_file (TEMPFILE, &lexer));

    ASSERT_EQ (remove (TEMPFILE), 0);
    lexer_free (&lexer);
}

TEST (lexer, lexer_file_error)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    const char *TEMPFILE = "test_lexer_file_error.s";

    EXPECT_FALSE (lex_file (TEMPFILE, &lexer));
    lexer_free (&lexer);
}

TEST (lexer, lexer_keywords)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    TokenType toks[] =
    {
        TOKEN_KEYWORD_AUTO,
        TOKEN_KEYWORD_BOOL,
        TOKEN_KEYWORD_BREAK,
        TOKEN_KEYWORD_CASE,
        TOKEN_KEYWORD_CHAR,
        TOKEN_KEYWORD_CONST,
        TOKEN_KEYWORD_CONSTEXPR,
        TOKEN_KEYWORD_CONTINUE,
        TOKEN_KEYWORD_DEFAULT,
        TOKEN_KEYWORD_DO,
        TOKEN_KEYWORD_DOUBLE,
        TOKEN_KEYWORD_ELSE,
        TOKEN_KEYWORD_ENUM,
        TOKEN_KEYWORD_EXTERN,
        TOKEN_KEYWORD_FALSE,
        TOKEN_KEYWORD_FLOAT,
        TOKEN_KEYWORD_FOR,
        TOKEN_KEYWORD_GOTO,
        TOKEN_KEYWORD_IF,
        TOKEN_KEYWORD_INLINE,
        TOKEN_KEYWORD_INT,
        TOKEN_KEYWORD_LONG,
        TOKEN_KEYWORD_NULLPTR,
        TOKEN_KEYWORD_REGISTER,
        TOKEN_KEYWORD_RESTRICT,
        TOKEN_KEYWORD_RETURN,
        TOKEN_KEYWORD_SHORT,
        TOKEN_KEYWORD_SIGNED,
        TOKEN_KEYWORD_SIZEOF,
        TOKEN_KEYWORD_STATIC,
        TOKEN_KEYWORD_STATIC_ASSERT,
        TOKEN_KEYWORD_STRUCT,
        TOKEN_KEYWORD_SWITCH,
        TOKEN_KEYWORD_THREAD_LOCAL,
        TOKEN_KEYWORD_TRUE,
        TOKEN_KEYWORD_TYPEDEF,
        TOKEN_KEYWORD_TYPEOF,
        TOKEN_KEYWORD_TYPEOF_UNQUAL,
        TOKEN_KEYWORD_UNION,
        TOKEN_KEYWORD_UNSIGNED,
        TOKEN_KEYWORD_VOID,
        TOKEN_KEYWORD_VOLATILE,
        TOKEN_KEYWORD_WHILE,
        TOKEN_KEYWORD_ALIGNAS,
        TOKEN_KEYWORD_ALIGNOF,
        TOKEN_KEYWORD_ATOMIC,
        TOKEN_KEYWORD_BIGINT,
        TOKEN_KEYWORD_BOOL,
        TOKEN_KEYWORD_COMPLEX,
        TOKEN_KEYWORD_DECIMAL128,
        TOKEN_KEYWORD_DECIMAL32,
        TOKEN_KEYWORD_DECIMAL64,
        TOKEN_KEYWORD_GENERIC,
        TOKEN_KEYWORD_IMAGINARY,
        TOKEN_KEYWORD_NORETURN,
        TOKEN_KEYWORD_STATIC_ASSERT,
        TOKEN_KEYWORD_THREAD_LOCAL
    };

    EXPECT_TRUE (lex_str (
        " auto "
        " bool "
        " break "
        " case "
        " char "
        " const "
        " constexpr "
        " continue "
        " default "
        " do "
        " double "
        " else "
        " enum "
        " extern "
        " false "
        " float "
        " for "
        " goto "
        " if "
        " inline "
        " int "
        " long "
        " nullptr "
        " register "
        " restrict "
        " return "
        " short "
        " signed "
        " sizeof "
        " static "
        " static_assert "
        " struct "
        " switch "
        " thread_local "
        " true "
        " typedef "
        " typeof "
        " typeof_unqual "
        " union "
        " unsigned "
        " void "
        " volatile "
        " while "
        " _Alignas "
        " _Alignof "
        " _Atomic "
        " _BigInt "
        " _Bool "
        " _Complex "
        " _Decimal128 "
        " _Decimal32 "
        " _Decimal64 "
        " _Generic "
        " _Imaginary "
        " _Noreturn "
        " _Static_assert "
        " _Thread_local ",
        &lexer));

    EXPECT_GE (lexer.tokarr.tok_cap, lexer.tokarr.tok_cnt);
    EXPECT_EQ (lexer.tokarr.tok_cnt, sizeof (toks) / sizeof (*toks));

    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, toks[i]);
    }

    lexer_print (&lexer);
    lexer_free (&lexer);
}

TEST (lexer, lexer_identifier)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str (
        "hello\n_this_is_AnOtHer Hello9_ ",
        &lexer));

    const int expected_tok_cnt = 3;

    EXPECT_GE (lexer.tokarr.tok_cap, lexer.tokarr.tok_cnt);
    EXPECT_EQ (lexer.tokarr.tok_cnt, expected_tok_cnt);

    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, TOKEN_IDENTIFIER);
    }

    EXPECT_EQ (lexer.tokarr.toks[0].len, strlen ("hello"));
    EXPECT_EQ (lexer.tokarr.toks[0].col, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].line, 1);
    EXPECT_EQ (strncmp (lexer.tokarr.toks[0].src, "hello", lexer.tokarr.toks[0].len), 0);

    EXPECT_EQ (lexer.tokarr.toks[1].len, strlen ("_this_is_AnOtHer"));
    EXPECT_EQ (lexer.tokarr.toks[1].col, 1);
    EXPECT_EQ (lexer.tokarr.toks[1].line, 2);
    EXPECT_EQ (strncmp (lexer.tokarr.toks[1].src, "_this_is_AnOtHer", lexer.tokarr.toks[1].len), 0);

    EXPECT_EQ (lexer.tokarr.toks[2].len, strlen ("Hello9_"));
    EXPECT_EQ (lexer.tokarr.toks[2].col, 18);
    EXPECT_EQ (lexer.tokarr.toks[2].line, 2);
    EXPECT_EQ (strncmp (lexer.tokarr.toks[2].src, "Hello9_", lexer.tokarr.toks[2].len), 0);

    lexer_free (&lexer);
}

TEST (lexer, lexer_operators)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    TokenType toks[] =
    {
        TOKEN_ELLIPSIS,
        TOKEN_RIGHT_ASSIGN,
        TOKEN_LEFT_ASSIGN,
        TOKEN_ADD_ASSIGN,
        TOKEN_SUB_ASSIGN,
        TOKEN_MUL_ASSIGN,
        TOKEN_DIV_ASSIGN,
        TOKEN_MOD_ASSIGN,
        TOKEN_AND_ASSIGN,
        TOKEN_XOR_ASSIGN,
        TOKEN_OR_ASSIGN,
        TOKEN_RIGHT_OP,
        TOKEN_LEFT_OP,
        TOKEN_INC_OP,
        TOKEN_DEC_OP,
        TOKEN_PTR_OP,
        TOKEN_AND_OP,
        TOKEN_OR_OP,
        TOKEN_LE_OP,
        TOKEN_GE_OP,
        TOKEN_EQ_OP,
        TOKEN_NE_OP,
        TOKEN_SEMICOLON,
        TOKEN_OPEN_BRACE,
        TOKEN_CLOSE_BRACE,
        TOKEN_COMMA,
        TOKEN_COLON,
        TOKEN_EQUAL_SIGN,
        TOKEN_OPEN_PARENTHESIS,
        TOKEN_CLOSE_PARENTHESIS,
        TOKEN_OPEN_BRACKET,
        TOKEN_CLOSE_BRACKET,
        TOKEN_PERIOD,
        TOKEN_AMPERSAND,
        TOKEN_EXCLAMATION_MARK,
        TOKEN_TILDE,
        TOKEN_HYPEN,
        TOKEN_PLUS,
        TOKEN_ASTERICK,
        TOKEN_FORWARD_SLASH,
        TOKEN_PERCENT_SIGN,
        TOKEN_LEFT_ARROW,
        TOKEN_RIGHT_ARROW,
        TOKEN_CARROT,
        TOKEN_PIPE,
        TOKEN_QUESTION_MARK
    };

    EXPECT_TRUE (lex_str (
        "..."
        ">>="
        "<<="
        "+="
        "-="
        "*="
        "/="
        "%="
        "&="
        "^="
        "|="
        ">>"
        "<<"
        "++"
        "--"
        "->"
        "&&"
        "||"
        "<="
        ">="
        "=="
        "!="
        ";"
        "{"
        "}"
        ","
        ":"
        "="
        "("
        ")"
        "["
        "]"
        "."
        "&"
        "!"
        "~"
        "-"
        "+"
        "*"
        "/"
        "%"
        "<"
        ">"
        "^"
        "|"
        "?",
        &lexer));

    EXPECT_GE (lexer.tokarr.tok_cap, lexer.tokarr.tok_cnt);
    EXPECT_EQ (lexer.tokarr.tok_cnt, sizeof (toks) / sizeof (*toks));

    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, toks[i]);
    }

    lexer_print (&lexer);
    lexer_free (&lexer);
}

TEST (lexer, lexer_integer)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str (
        "07213 0x1242fFaB 0XFFFF30 209419340 ",
        &lexer));

    const int expected_tok_cnt = 4;

    EXPECT_GE (lexer.tokarr.tok_cap, lexer.tokarr.tok_cnt);
    EXPECT_EQ (lexer.tokarr.tok_cnt, expected_tok_cnt);

    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, TOKEN_I_CONSTANT);
    }

    EXPECT_EQ (lexer.tokarr.toks[0].len, strlen ("07213"));
    EXPECT_EQ (lexer.tokarr.toks[0].line, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].col, 1);
    EXPECT_EQ (strncmp (lexer.tokarr.toks[0].src, "07213", lexer.tokarr.toks[0].len), 0);

    EXPECT_EQ (lexer.tokarr.toks[1].len, strlen ("0x1242fFaB"));
    EXPECT_EQ (lexer.tokarr.toks[1].line, 1);
    EXPECT_EQ (lexer.tokarr.toks[1].col, 7);
    EXPECT_EQ (strncmp (lexer.tokarr.toks[1].src, "0x1242fFaB", lexer.tokarr.toks[1].len), 0);

    EXPECT_EQ (lexer.tokarr.toks[2].len, strlen ("0XFFFF30"));
    EXPECT_EQ (lexer.tokarr.toks[2].line, 1);
    EXPECT_EQ (lexer.tokarr.toks[2].col, 18);
    EXPECT_EQ (strncmp (lexer.tokarr.toks[2].src, "0XFFFF30", lexer.tokarr.toks[2].len), 0);

    EXPECT_EQ (lexer.tokarr.toks[3].len, strlen ("209419340"));
    EXPECT_EQ (lexer.tokarr.toks[3].line, 1);
    EXPECT_EQ (lexer.tokarr.toks[3].col, 27);
    EXPECT_EQ (strncmp (lexer.tokarr.toks[3].src, "209419340", lexer.tokarr.toks[3].len), 0);

    lexer_free (&lexer);
}

TEST (lexer, lexer_float)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str (
        "0.0901 1. 1e3 1E5 1e+4 7e-6 1.6e+0 2.f 3.4F 1e3l 1e34L",
        &lexer));

    const int expected_tok_cnt = 11;

    EXPECT_GE (lexer.tokarr.tok_cap, lexer.tokarr.tok_cnt);
    EXPECT_EQ (lexer.tokarr.tok_cnt, expected_tok_cnt);

    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, TOKEN_F_CONSTANT);
    }

    EXPECT_EQ (lexer.tokarr.toks[0].len, strlen ("0.0901"));
    EXPECT_EQ (strncmp (lexer.tokarr.toks[0].src, "0.0901", lexer.tokarr.toks[0].len), 0);

    EXPECT_EQ (lexer.tokarr.toks[1].len, strlen ("1."));
    EXPECT_EQ (strncmp (lexer.tokarr.toks[1].src, "1.", lexer.tokarr.toks[1].len), 0);

    EXPECT_EQ (lexer.tokarr.toks[2].len, strlen ("1e3"));
    EXPECT_EQ (strncmp (lexer.tokarr.toks[2].src, "1e3", lexer.tokarr.toks[2].len), 0);

    EXPECT_EQ (lexer.tokarr.toks[3].len, strlen ("1E5"));
    EXPECT_EQ (strncmp (lexer.tokarr.toks[3].src, "1E5", lexer.tokarr.toks[3].len), 0);

    EXPECT_EQ (lexer.tokarr.toks[4].len, strlen ("1e+4"));
    EXPECT_EQ (strncmp (lexer.tokarr.toks[4].src, "1e+4", lexer.tokarr.toks[4].len), 0);

    EXPECT_EQ (lexer.tokarr.toks[5].len, strlen ("7e-6"));
    EXPECT_EQ (strncmp (lexer.tokarr.toks[5].src, "7e-6", lexer.tokarr.toks[5].len), 0);

    EXPECT_EQ (lexer.tokarr.toks[6].len, strlen ("1.6e+0"));
    EXPECT_EQ (strncmp (lexer.tokarr.toks[6].src, "1.6e+0", lexer.tokarr.toks[6].len), 0);

    EXPECT_EQ (lexer.tokarr.toks[7].len, strlen ("2.f"));
    EXPECT_EQ (strncmp (lexer.tokarr.toks[7].src, "2.f", lexer.tokarr.toks[7].len), 0);

    EXPECT_EQ (lexer.tokarr.toks[8].len, strlen ("3.4F"));
    EXPECT_EQ (strncmp (lexer.tokarr.toks[8].src, "3.4F", lexer.tokarr.toks[8].len), 0);

    EXPECT_EQ (lexer.tokarr.toks[9].len, strlen ("1e3l"));
    EXPECT_EQ (strncmp (lexer.tokarr.toks[9].src, "1e3l", lexer.tokarr.toks[9].len), 0);

    EXPECT_EQ (lexer.tokarr.toks[10].len, strlen ("1e34L"));
    EXPECT_EQ (strncmp (lexer.tokarr.toks[10].src, "1e34L", lexer.tokarr.toks[10].len), 0);

    lexer_free (&lexer);
}


TEST (lexer, column_tracking_single_line)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("int x = 42;", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 5);

    EXPECT_EQ (lexer.tokarr.toks[0].type,   TOKEN_KEYWORD_INT);
    EXPECT_EQ (lexer.tokarr.toks[0].line,   1);
    EXPECT_EQ (lexer.tokarr.toks[0].col, 1);

    EXPECT_EQ (lexer.tokarr.toks[1].type,   TOKEN_IDENTIFIER);
    EXPECT_EQ (lexer.tokarr.toks[1].line,   1);
    EXPECT_EQ (lexer.tokarr.toks[1].col, 5);

    EXPECT_EQ (lexer.tokarr.toks[2].type,   TOKEN_EQUAL_SIGN);
    EXPECT_EQ (lexer.tokarr.toks[2].line,   1);
    EXPECT_EQ (lexer.tokarr.toks[2].col, 7);

    EXPECT_EQ (lexer.tokarr.toks[3].type,   TOKEN_I_CONSTANT);
    EXPECT_EQ (lexer.tokarr.toks[3].line,   1);
    EXPECT_EQ (lexer.tokarr.toks[3].col, 9);

    EXPECT_EQ (lexer.tokarr.toks[4].type,   TOKEN_SEMICOLON);
    EXPECT_EQ (lexer.tokarr.toks[4].line,   1);
    EXPECT_EQ (lexer.tokarr.toks[4].col, 11);

    lexer_free (&lexer);
}

TEST (lexer, column_tracking_multiline)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("int\nfloat\n  x", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 3);

    EXPECT_EQ (lexer.tokarr.toks[0].line,   1);
    EXPECT_EQ (lexer.tokarr.toks[0].col, 1);

    EXPECT_EQ (lexer.tokarr.toks[1].line,   2);
    EXPECT_EQ (lexer.tokarr.toks[1].col, 1);

    EXPECT_EQ (lexer.tokarr.toks[2].line,   3);
    EXPECT_EQ (lexer.tokarr.toks[2].col, 3);

    lexer_free (&lexer);
}

TEST (lexer, column_resets_after_newline)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("abc\ndef", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 2);
    EXPECT_EQ (lexer.tokarr.toks[1].line,   2);
    EXPECT_EQ (lexer.tokarr.toks[1].col, 1);

    lexer_free (&lexer);
}

TEST (lexer, single_line_comment_ignored)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("int // this is a comment\nx", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 2);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_KEYWORD_INT);
    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_IDENTIFIER);
    EXPECT_EQ (lexer.tokarr.toks[1].line, 2);

    lexer_free (&lexer);
}

TEST (lexer, multi_line_comment_ignored)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("int /* this spans\nmultiple lines */ x", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 2);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_KEYWORD_INT);
    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_IDENTIFIER);

    lexer_free (&lexer);
}

TEST (lexer, multi_line_comment_updates_line_count)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("/* line1\nline2\nline3 */\nx", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].line, 4);

    lexer_free (&lexer);
}

TEST (lexer, comment_between_tokens_no_space)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("int/*comment*/x", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 2);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_KEYWORD_INT);
    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_IDENTIFIER);

    lexer_free (&lexer);
}

TEST (lexer, slash_not_comment)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("a / b", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 3);
    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_FORWARD_SLASH);

    lexer_free (&lexer);
}

TEST (lexer, character_basic)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("'H' + 'e' + 'l' + 'l' + 'o' + ' ' + 'W' + 'o' + 'r' + 'l' + 'd' + '!' ", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 12 + 11);

    for (size_t i = 0; i < 12 + 11; i += 2)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, TOKEN_I_CONSTANT);
        EXPECT_EQ (lexer.tokarr.toks[i].len, 3);
        EXPECT_EQ (lexer.tokarr.toks[i].src[0], '\'');
        EXPECT_EQ (lexer.tokarr.toks[i].src[2], '\'');
    }
}

TEST (lexer, character_escape_sequences)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("'\\\"'", &lexer));

    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_I_CONSTANT);
    EXPECT_EQ (lexer.tokarr.toks[0].len, strlen ("'\\\"'"));
    EXPECT_EQ (strncmp (lexer.tokarr.toks[0].src, "'\\\"'", strlen ("'\\\"'")), 0);
}

TEST (lexer, string_basic)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("\"hello\"", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_STRING_LITERAL);
    EXPECT_EQ (lexer.tokarr.toks[0].len, strlen ("\"hello\""));

    lexer_free (&lexer);
}

TEST (lexer, string_empty)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("\"\"", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type,   TOKEN_STRING_LITERAL);
    EXPECT_EQ (lexer.tokarr.toks[0].len, 2);

    lexer_free (&lexer);
}

TEST (lexer, string_escape_sequences)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("\"\\n\\t\\r\\a\\b\\f\\v\\\\\\\"\"", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_STRING_LITERAL);

    lexer_free (&lexer);
}

TEST (lexer, string_hex_escape)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("\"\\x41\\xFF\"", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_STRING_LITERAL);

    lexer_free (&lexer);
}

TEST (lexer, string_octal_escape)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("\"\\0\\07\\177\"", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_STRING_LITERAL);

    lexer_free (&lexer);
}

TEST (lexer, string_unicode_escape)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("\"\\u0041\"", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_STRING_LITERAL);

    lexer_free (&lexer);
}

TEST (lexer, string_column_after)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("\"hi\" x", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 2);
    EXPECT_EQ (lexer.tokarr.toks[1].col, 6);

    lexer_free (&lexer);
}

TEST (lexer, adjacent_strings)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("\"hello\" \"world\"", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 2);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_STRING_LITERAL);
    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_STRING_LITERAL);

    lexer_free (&lexer);
}

TEST (lexer, error_unterminated_string)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_FALSE (lex_str ("\"unterminated", &lexer));

    lexer_free (&lexer);
}

TEST (lexer, error_string_with_newline)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    /* Newline inside string without escape is illegal */
    EXPECT_FALSE (lex_str ("\"line1\nline2\"", &lexer));

    lexer_free (&lexer);
}

TEST (lexer, error_unknown_escape)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    /* \q is not a valid escape sequence */
    EXPECT_FALSE (lex_str ("\"\\q\"", &lexer));

    lexer_free (&lexer);
}

TEST (lexer, error_invalid_hex_escape)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    /* \x with no hex digits following */
    EXPECT_FALSE (lex_str ("\"\\xGG\"", &lexer));

    lexer_free (&lexer);
}

TEST (lexer, error_invalid_unicode_escape)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    /* \u must be followed by exactly 4 hex digits */
    EXPECT_FALSE (lex_str ("\"\\u004\"", &lexer));

    lexer_free (&lexer);
}

TEST (lexer, error_unrecognized_character)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    /* '@' is not a valid C token */
    EXPECT_FALSE (lex_str ("@", &lexer));

    lexer_free (&lexer);
}

TEST (lexer, operator_disambiguation)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    /* Each pair should produce the longer token, not two short ones. */
    EXPECT_TRUE (lex_str ("++ -- -> == != <= >= && || << >> += -= *= /= %= &= ^= |= <<= >>=", &lexer));

    TokenType expected[] = {
        TOKEN_INC_OP, TOKEN_DEC_OP, TOKEN_PTR_OP,
        TOKEN_EQ_OP,  TOKEN_NE_OP,  TOKEN_LE_OP,  TOKEN_GE_OP,
        TOKEN_AND_OP, TOKEN_OR_OP,  TOKEN_LEFT_OP, TOKEN_RIGHT_OP,
        TOKEN_ADD_ASSIGN, TOKEN_SUB_ASSIGN, TOKEN_MUL_ASSIGN,
        TOKEN_DIV_ASSIGN, TOKEN_MOD_ASSIGN, TOKEN_AND_ASSIGN,
        TOKEN_XOR_ASSIGN, TOKEN_OR_ASSIGN,
        TOKEN_LEFT_ASSIGN, TOKEN_RIGHT_ASSIGN,
    };

    EXPECT_EQ (lexer.tokarr.tok_cnt, (int)(sizeof (expected) / sizeof (*expected)));
    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, expected[i]);
    }

    lexer_free (&lexer);
}

TEST (lexer, operator_single_chars)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("+ - * / % & | ^ ~ ! < > = ? : ; , . ( ) [ ] { }", &lexer));

    TokenType expected[] = {
        TOKEN_PLUS, TOKEN_HYPEN, TOKEN_ASTERICK, TOKEN_FORWARD_SLASH,
        TOKEN_PERCENT_SIGN, TOKEN_AMPERSAND, TOKEN_PIPE, TOKEN_CARROT,
        TOKEN_TILDE, TOKEN_EXCLAMATION_MARK, TOKEN_LEFT_ARROW, TOKEN_RIGHT_ARROW,
        TOKEN_EQUAL_SIGN, TOKEN_QUESTION_MARK, TOKEN_COLON, TOKEN_SEMICOLON,
        TOKEN_COMMA, TOKEN_PERIOD,
        TOKEN_OPEN_PARENTHESIS, TOKEN_CLOSE_PARENTHESIS,
        TOKEN_OPEN_BRACKET, TOKEN_CLOSE_BRACKET,
        TOKEN_OPEN_BRACE, TOKEN_CLOSE_BRACE,
    };

    EXPECT_EQ (lexer.tokarr.tok_cnt, (int)(sizeof (expected) / sizeof (*expected)));
    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, expected[i]);
    }

    lexer_free (&lexer);
}

TEST (lexer, ellipsis)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("...", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_ELLIPSIS);

    lexer_free (&lexer);
}

TEST (lexer, lexer_three_spaced_periods_are_three_tokens)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str (". . .", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 3);

    for (int i = 0; i < 3; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, TOKEN_PERIOD);
    }

    lexer_free (&lexer);
}

TEST (lexer, keyword_prefix_is_identifier)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("integer", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_IDENTIFIER);

    lexer_free (&lexer);
}

TEST (lexer, keyword_with_trailing_underscore_is_identifier)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("int_ return_ while_loop", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 3);
    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, TOKEN_IDENTIFIER);
    }

    lexer_free (&lexer);
}

TEST (lexer, keyword_with_trailing_digit_is_identifier)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("int2 for3", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 2);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_IDENTIFIER);
    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_IDENTIFIER);

    lexer_free (&lexer);
}

TEST (lexer, keyword_immediately_followed_by_operator)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("int*x", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 3);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_KEYWORD_INT);
    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_ASTERICK);
    EXPECT_EQ (lexer.tokarr.toks[2].type, TOKEN_IDENTIFIER);

    lexer_free (&lexer);
}

TEST (lexer, integer_zero)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("0", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_I_CONSTANT);
    EXPECT_EQ (lexer.tokarr.toks[0].len, 1);

    lexer_free (&lexer);
}

TEST (lexer, integer_suffixes)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("1u 1U 1l 1L 1ll 1LL 1ul 1ULL", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 8);
    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, TOKEN_I_CONSTANT);
    }

    lexer_free (&lexer);
}

TEST (lexer, integer_hex_uppercase_x)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("0XDEADBEEF", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_I_CONSTANT);
    EXPECT_EQ (lexer.tokarr.toks[0].len, strlen ("0XDEADBEEF"));

    lexer_free (&lexer);
}

TEST (lexer, float_leading_dot)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str (".5", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_F_CONSTANT);

    lexer_free (&lexer);
}

TEST (lexer, float_trailing_dot)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("1.", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_F_CONSTANT);

    lexer_free (&lexer);
}

TEST (lexer, float_vs_member_access)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("1 . x", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 3);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_I_CONSTANT);
    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_PERIOD);
    EXPECT_EQ (lexer.tokarr.toks[2].type, TOKEN_IDENTIFIER);

    lexer_free (&lexer);
}

TEST (lexer, line_splice_joins_tokens)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    /* Backslash-newline should be erased, joining "in" and "t" into "int". */
    EXPECT_TRUE (lex_str ("in\\\nt", &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_KEYWORD_INT);

    lexer_free (&lexer);
}

TEST (lexer, snippet_function_decl)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("int add(int a, int b);", &lexer));

    TokenType expected[] = {
        TOKEN_KEYWORD_INT, TOKEN_IDENTIFIER,
        TOKEN_OPEN_PARENTHESIS,
            TOKEN_KEYWORD_INT, TOKEN_IDENTIFIER, TOKEN_COMMA,
            TOKEN_KEYWORD_INT, TOKEN_IDENTIFIER,
        TOKEN_CLOSE_PARENTHESIS,
        TOKEN_SEMICOLON,
    };

    EXPECT_EQ (lexer.tokarr.tok_cnt, (int)(sizeof (expected) / sizeof (*expected)));
    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, expected[i]);
    }

    lexer_free (&lexer);
}

TEST (lexer, snippet_for_loop)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("for (i = 0; i < 10; i++)", &lexer));

    TokenType expected[] = {
        TOKEN_KEYWORD_FOR,
        TOKEN_OPEN_PARENTHESIS,
        TOKEN_IDENTIFIER, TOKEN_EQUAL_SIGN, TOKEN_I_CONSTANT, TOKEN_SEMICOLON,
        TOKEN_IDENTIFIER, TOKEN_LEFT_ARROW,  TOKEN_I_CONSTANT, TOKEN_SEMICOLON,
        TOKEN_IDENTIFIER, TOKEN_INC_OP,
        TOKEN_CLOSE_PARENTHESIS,
    };

    EXPECT_EQ (lexer.tokarr.tok_cnt, (int)(sizeof (expected) / sizeof (*expected)));
    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, expected[i]);
    }

    lexer_free (&lexer);
}

TEST (lexer, snippet_pointer_decl)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("int *p = &x;", &lexer));

    TokenType expected[] = {
        TOKEN_KEYWORD_INT, TOKEN_ASTERICK, TOKEN_IDENTIFIER,
        TOKEN_EQUAL_SIGN, TOKEN_AMPERSAND, TOKEN_IDENTIFIER,
        TOKEN_SEMICOLON,
    };

    EXPECT_EQ (lexer.tokarr.tok_cnt, (int)(sizeof (expected) / sizeof (*expected)));
    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, expected[i]);
    }

    lexer_free (&lexer);
}

TEST (lexer, snippet_struct_member_access)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("p->x = s.y;", &lexer));

    TokenType expected[] = {
        TOKEN_IDENTIFIER, TOKEN_PTR_OP,    TOKEN_IDENTIFIER, TOKEN_EQUAL_SIGN,
        TOKEN_IDENTIFIER, TOKEN_PERIOD,    TOKEN_IDENTIFIER, TOKEN_SEMICOLON,
    };

    EXPECT_EQ (lexer.tokarr.tok_cnt, (int)(sizeof (expected) / sizeof (*expected)));
    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, expected[i]);
    }

    lexer_free (&lexer);
}

TEST (lexer, snippet_ternary)
{
    lexer_data_t lexer;
    lexer_init (&lexer);


    EXPECT_TRUE (lex_str ("x > 0 ? x : -x", &lexer));

    TokenType expected[] = {
        TOKEN_IDENTIFIER, TOKEN_RIGHT_ARROW, TOKEN_I_CONSTANT,
        TOKEN_QUESTION_MARK,
        TOKEN_IDENTIFIER,
        TOKEN_COLON,
        TOKEN_HYPEN, TOKEN_IDENTIFIER,
    };

    EXPECT_EQ (lexer.tokarr.tok_cnt, (int)(sizeof (expected) / sizeof (*expected)));
    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, expected[i]);
    }

    lexer_free (&lexer);
}

TEST (lexer, lexer_operator_src_pointer_single_char)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("+", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);

    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_PLUS);
    EXPECT_EQ (lexer.tokarr.toks[0].len, 1u);
    // src must point at the '+' character in the processed buffer
    ASSERT_NE (lexer.tokarr.toks[0].src, nullptr);
    EXPECT_EQ (lexer.tokarr.toks[0].src[0], '+');

    lexer_free (&lexer);
}

TEST (lexer, lexer_operator_src_pointer_multi_char)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str(">>=", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);

    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_RIGHT_ASSIGN);
    EXPECT_EQ (lexer.tokarr.toks[0].len, 3u);
    ASSERT_NE (lexer.tokarr.toks[0].src, nullptr);
    EXPECT_EQ (strncmp(lexer.tokarr.toks[0].src, ">>=", 3), 0);

    lexer_free (&lexer);
}

TEST (lexer, lexer_operator_col_single_char)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // "int +" — '+' is at column 5
    EXPECT_TRUE (lex_str("int +", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 2);

    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_PLUS);
    EXPECT_EQ (lexer.tokarr.toks[1].col, 5u);
    EXPECT_EQ (lexer.tokarr.toks[1].line, 1u);

    lexer_free (&lexer);
}

TEST (lexer, lexer_token_at_very_end_of_input_no_newline)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // No whitespace or newline after the semicolon.
    EXPECT_TRUE (lex_str("x;", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 2);

    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_IDENTIFIER);
    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_SEMICOLON);
    EXPECT_EQ (lexer.tokarr.toks[1].col, 2u);

    lexer_free (&lexer);
}

TEST (lexer, lexer_integer_at_very_end_of_input_no_newline)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("42", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_I_CONSTANT);
    EXPECT_EQ (lexer.tokarr.toks[0].len, 2u);

    lexer_free (&lexer);
}

TEST (lexer, lexer_negative_number_is_two_tokens)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // -1 must lex as TOKEN_HYPEN followed by TOKEN_I_CONSTANT; there is no
    // negative integer literal in C.
    EXPECT_TRUE (lex_str ("-1", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 2);

    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_HYPEN);
    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_I_CONSTANT);
    EXPECT_EQ (lexer.tokarr.toks[1].len, 1u);

    lexer_free (&lexer);
}

TEST (lexer2, negative_float_is_two_tokens)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str("-3.14", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 2);

    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_HYPEN);
    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_F_CONSTANT);

    lexer_free (&lexer);
}

TEST (lexer, lexer_single_line_comment_at_eof_no_newline)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // The comment runs to EOF; there is no '\n' to terminate it.
    EXPECT_TRUE (lex_str ("int // comment with no newline", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_KEYWORD_INT);

    lexer_free (&lexer);
}

TEST (lexer, lexer_unterminated_block_comment_fails)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_FALSE (lex_str ("/* never closed", &lexer));

    lexer_free (&lexer);
}

TEST (lexer, lexer_vertical_tab_mid_line_stripped)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // \v between two tokens on the same logical line should be treated as
    // whitespace and not alter the line count.
    EXPECT_TRUE (lex_str ("int\vx", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 2);

    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_KEYWORD_INT);
    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_IDENTIFIER);
    // Both tokens are on line 1 because \v does not count as a newline.
    EXPECT_EQ (lexer.tokarr.toks[0].line, 1u);
    EXPECT_EQ (lexer.tokarr.toks[1].line, 1u);

    lexer_free (&lexer);
}

TEST(lexer, lexer_form_feed_mid_line_stripped)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("int\fx", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 2);

    EXPECT_EQ (lexer.tokarr.toks[0].line, 1u);
    EXPECT_EQ (lexer.tokarr.toks[1].line, 1u);

    lexer_free (&lexer);
}

TEST (lexer, lexer_char_escape_alert)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("'\\a'", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_I_CONSTANT);
    // Verify the raw token span covers exactly the 4-character literal.
    EXPECT_EQ (lexer.tokarr.toks[0].len, 4u);

    lexer_free (&lexer);
}

TEST (lexer, lexer_char_escape_backspace)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("'\\b'", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_I_CONSTANT);
    EXPECT_EQ (lexer.tokarr.toks[0].len, 4u);

    lexer_free (&lexer);
}

TEST (lexer, lexer_char_escape_form_feed)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("'\\f'", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_I_CONSTANT);
    EXPECT_EQ (lexer.tokarr.toks[0].len, 4u);

    lexer_free (&lexer);
}

TEST (lexer, lexer_char_escape_vertical_tab)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("'\\v'", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_I_CONSTANT);
    EXPECT_EQ (lexer.tokarr.toks[0].len, 4u);

    lexer_free (&lexer);
}

TEST (lexer, lexer_float_col_tracking_single_line)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // "int 3.14" — float starts at column 5
    EXPECT_TRUE (lex_str ("int 3.14", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 2);

    EXPECT_EQ (lexer.tokarr.toks[0].col, 1u);
    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_F_CONSTANT);
    EXPECT_EQ (lexer.tokarr.toks[1].col, 5u);
    EXPECT_EQ (lexer.tokarr.toks[1].line, 1u);

    lexer_free (&lexer);
}

TEST (lexer, lexer_float_col_tracking_after_newline)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("int\n  1.5", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 2);

    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_F_CONSTANT);
    EXPECT_EQ (lexer.tokarr.toks[1].line, 2u);
    EXPECT_EQ (lexer.tokarr.toks[1].col, 3u);

    lexer_free (&lexer);
}

TEST (lexer, lexer_escaping_os_linebreaks_tok_cnt)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // Same input as the existing test; just pin tok_cnt.
    EXPECT_TRUE (lex_str (
        "int x = 0;\\\r\n"
        "i\\\n"
        "nt y = 0;\\\n"
        "in\\\n"
        "t z = 0;\\\r"
        "r\\\n"
        "e\\\n"
        "t\\\n"
        "u\\\n"
        "r\\\n"
        "n x + y + z;\\\n",
        &lexer));

    EXPECT_EQ (lexer.tokarr.tok_cnt, 22);

    lexer_free (&lexer);
}

TEST (lexer, lexer_escaping_os_linebreaks_last_statement_line)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str (
        "int x = 0;\\\r\n"
        "i\\\n"
        "nt y = 0;\\\n"
        "in\\\n"
        "t z = 0;\\\r"
        "r\\\n"
        "e\\\n"
        "t\\\n"
        "u\\\n"
        "r\\\n"
        "n x + y + z;\\\n",
        &lexer));

    ASSERT_EQ (lexer.tokarr.tok_cnt, 22);

    // Tokens 15–21 belong to "return x + y + z;" which starts on logical
    // line 6 (after the last unescaped newline).
    EXPECT_EQ (lexer.tokarr.toks[15].line, 6u);
    EXPECT_EQ (lexer.tokarr.toks[15].col,  1u);

    lexer_free (&lexer);
}

TEST (lexer, lexer_identifier_leading_underscore_src_len)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("_foo", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);

    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_IDENTIFIER);
    EXPECT_EQ (lexer.tokarr.toks[0].len, 4u);
    EXPECT_EQ (strncmp(lexer.tokarr.toks[0].src, "_foo", 4), 0);

    lexer_free (&lexer);
}

TEST (lexer, lexer_integer_immediately_followed_by_semicolon)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("42;", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 2);

    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_I_CONSTANT);
    EXPECT_EQ (lexer.tokarr.toks[0].len, 2u);
    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_SEMICOLON);

    lexer_free (&lexer);
}

TEST (lexer, lexer_float_immediately_followed_by_semicolon)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("3.14;", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 2);

    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_F_CONSTANT);
    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_SEMICOLON);
    EXPECT_EQ (lexer.tokarr.toks[1].col, 5u);

    lexer_free (&lexer);
}

TEST (lexer, lexer_hex_immediately_followed_by_semicolon)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("0xff;", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 2);

    EXPECT_EQ (lexer.tokarr.toks[0].type, TOKEN_I_CONSTANT);
    EXPECT_EQ (lexer.tokarr.toks[0].len, 4u);
    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_SEMICOLON);

    lexer_free (&lexer);
}

TEST (lexer, lexer_token_col_after_inline_block_comment)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // "x /* hi */ y" — 'y' is at column 12
    EXPECT_TRUE (lex_str ("x /* hi */ y", &lexer));
    ASSERT_EQ (lexer.tokarr.tok_cnt, 2);

    EXPECT_EQ (lexer.tokarr.toks[1].type, TOKEN_IDENTIFIER);
    EXPECT_EQ (lexer.tokarr.toks[1].line, 1u);
    EXPECT_EQ (lexer.tokarr.toks[1].col, 12u);

    lexer_free (&lexer);
}

TEST (lexer, lexer_snippet_minimal_function)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("int main() { return 0; }", &lexer));

    TokenType expected[] =
    {
        TOKEN_KEYWORD_INT, TOKEN_IDENTIFIER,
        TOKEN_OPEN_PARENTHESIS, TOKEN_CLOSE_PARENTHESIS,
        TOKEN_OPEN_BRACE,
            TOKEN_KEYWORD_RETURN, TOKEN_I_CONSTANT, TOKEN_SEMICOLON,
        TOKEN_CLOSE_BRACE,
    };

    ASSERT_EQ (lexer.tokarr.tok_cnt, (int) (sizeof (expected) / sizeof (*expected)));
    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, expected[i]);
    }

    // "return" is at column 16
    EXPECT_EQ (lexer.tokarr.toks[5].col, 14u);

    lexer_free (&lexer);
}

TEST (lexer, lexer_snippet_variable_declaration_and_assign)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("int x = 10 + 20;", &lexer));

    TokenType expected[] =
    {
        TOKEN_KEYWORD_INT, TOKEN_IDENTIFIER, TOKEN_EQUAL_SIGN,
        TOKEN_I_CONSTANT, TOKEN_PLUS, TOKEN_I_CONSTANT,
        TOKEN_SEMICOLON,
    };

    ASSERT_EQ (lexer.tokarr.tok_cnt, (int) (sizeof (expected) / sizeof (*expected)));
    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, expected[i]);
    }

    lexer_free (&lexer);
}

TEST (lexer, lexer_snippet_while_loop)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("while (x != 0) { x--; }", &lexer));

    TokenType expected[] =
    {
        TOKEN_KEYWORD_WHILE,
        TOKEN_OPEN_PARENTHESIS,
            TOKEN_IDENTIFIER, TOKEN_NE_OP, TOKEN_I_CONSTANT,
        TOKEN_CLOSE_PARENTHESIS,
        TOKEN_OPEN_BRACE,
            TOKEN_IDENTIFIER, TOKEN_DEC_OP, TOKEN_SEMICOLON,
        TOKEN_CLOSE_BRACE,
    };

    ASSERT_EQ (lexer.tokarr.tok_cnt, (int) (sizeof (expected) / sizeof (*expected)));
    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, expected[i]);
    }

    lexer_free (&lexer);
}

TEST (lexer, lexer_snippet_bitwise_ops)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("a & b | c ^ ~d", &lexer));

    TokenType expected[] =
    {
        TOKEN_IDENTIFIER, TOKEN_AMPERSAND,
        TOKEN_IDENTIFIER, TOKEN_PIPE,
        TOKEN_IDENTIFIER, TOKEN_CARROT,
        TOKEN_TILDE, TOKEN_IDENTIFIER,
    };

    ASSERT_EQ (lexer.tokarr.tok_cnt, (int) (sizeof (expected) / sizeof (*expected)));
    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, expected[i]);
    }

    lexer_free (&lexer);
}

TEST (lexer, lexer_snippet_shift_ops)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("x <<= 2; y >>= 3;", &lexer));

    TokenType expected[] =
    {
        TOKEN_IDENTIFIER, TOKEN_LEFT_ASSIGN,  TOKEN_I_CONSTANT, TOKEN_SEMICOLON,
        TOKEN_IDENTIFIER, TOKEN_RIGHT_ASSIGN, TOKEN_I_CONSTANT, TOKEN_SEMICOLON,
    };

    ASSERT_EQ (lexer.tokarr.tok_cnt, (int) (sizeof (expected) / sizeof (*expected)));
    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, expected[i]);
    }

    lexer_free (&lexer);
}

TEST (lexer, lexer_snippet_cast_and_sizeof)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    EXPECT_TRUE (lex_str ("(int)sizeof(x)", &lexer));

    TokenType expected[] =
    {
        TOKEN_OPEN_PARENTHESIS, TOKEN_KEYWORD_INT, TOKEN_CLOSE_PARENTHESIS,
        TOKEN_KEYWORD_SIZEOF,
        TOKEN_OPEN_PARENTHESIS, TOKEN_IDENTIFIER, TOKEN_CLOSE_PARENTHESIS,
    };

    ASSERT_EQ (lexer.tokarr.tok_cnt, (int) (sizeof (expected) / sizeof (*expected)));
    for (size_t i = 0; i < lexer.tokarr.tok_cnt; i++)
    {
        EXPECT_EQ (lexer.tokarr.toks[i].type, expected[i]);
    }

    lexer_free (&lexer);
}
