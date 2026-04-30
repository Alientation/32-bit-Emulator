#include <ccompiler_test/ccompiler_test.h>

#define LEX_OK(str, lexer) ASSERT_TRUE (lex_str (str, lexer))
#define TOK(lexer, i) ((lexer).tokarr.toks[i])

// Decimal literal.
TEST (lexer_cval_int, decimal_zero)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("0", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).type, TOKEN_I_CONSTANT);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 0ULL);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, decimal_simple)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("42", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 42ULL);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, decimal_large)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("1000000", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 1000000ULL);

    lexer_free (&lexer);
}

// Hexadecimal literal.
TEST (lexer_cval_int, hex_lowercase)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("0xff", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 0xffULL);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, hex_uppercase_prefix)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("0XDEADBEEF", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 0xDEADBEEFULL);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, hex_mixed_case_digits)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("0x1a2B3c", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 0x1a2b3cULL);

    lexer_free (&lexer);
}

// Octal literal.
TEST (lexer_cval_int, octal_zero)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // A bare "0" is octal zero in the regex path.
    LEX_OK ("00", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 0ULL);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, octal_value)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // 0777 == 511
    LEX_OK ("0777", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 0777ULL);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, octal_small)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // 010 == 8
    LEX_OK ("010", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 8ULL);

    lexer_free (&lexer);
}

// Binary literals.
TEST (lexer_cval_int, binary_zero_lowercase)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("0b0", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).type, TOKEN_I_CONSTANT);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 0ULL);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, binary_zero_uppercase)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("0B0", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).type, TOKEN_I_CONSTANT);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 0ULL);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, binary_one)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("0b1", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 1ULL);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, binary_simple_byte)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // 0b10101010 == 170
    LEX_OK ("0b10101010", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 170ULL);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, binary_all_ones_byte)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // 0b11111111 == 255
    LEX_OK ("0b11111111", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 255ULL);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, binary_power_of_two)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // 0b100 == 4
    LEX_OK ("0b100", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 4ULL);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, binary_uppercase_prefix)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // 0B1010 == 10
    LEX_OK ("0B1010", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 10ULL);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, binary_16bit)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // 0b1111000010101010 == 0xF0AA == 61610
    LEX_OK ("0b1111000010101010", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 0xF0AAULL);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, binary_same_value_as_decimal)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // 0b1101 == 13
    LEX_OK ("0b1101 13", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 2);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, TOK (lexer, 1).cval.i_constant);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, binary_same_value_as_hex)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // 0b11111111 == 0xff == 255
    LEX_OK ("0b11111111 0xff", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 2);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, TOK (lexer, 1).cval.i_constant);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, binary_with_unsigned_suffix)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("0b101u", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 5ULL);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_GET_SIGN, (uint64_t) LFLAGS_UNSIGNED);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, binary_with_long_suffix)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("0b1010l", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 10ULL);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_IGET_SIZE, (uint64_t) LFLAGS_LONG);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, binary_with_longlong_suffix)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("0b1111ll", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 15ULL);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_IGET_SIZE, (uint64_t) LFLAGS_LONGLONG);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, multiple_binary_literals)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("0b0001 0b0010 0b0100 0b1000", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 4);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 1ULL);
    EXPECT_EQ (TOK (lexer, 1).cval.i_constant, 2ULL);
    EXPECT_EQ (TOK (lexer, 2).cval.i_constant, 4ULL);
    EXPECT_EQ (TOK (lexer, 3).cval.i_constant, 8ULL);

    lexer_free (&lexer);
}

// Integer suffix.
TEST (lexer_cval_int_flags, no_suffix_signed)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("1", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    // Small value: should be SIGNED INT
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_GET_SIGN, (uint64_t) LFLAGS_SIGNED);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_IGET_SIZE, (uint64_t) LFLAGS_INT);

    lexer_free (&lexer);
}

TEST (lexer_cval_int_flags, unsigned_suffix_u)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("1u", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_GET_SIGN, (uint64_t) LFLAGS_UNSIGNED);

    lexer_free (&lexer);
}

TEST (lexer_cval_int_flags, unsigned_suffix_U)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("1U", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_GET_SIGN, (uint64_t) LFLAGS_UNSIGNED);

    lexer_free (&lexer);
}

TEST (lexer_cval_int_flags, long_suffix_l)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("1l", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_IGET_SIZE, (uint64_t) LFLAGS_LONG);

    lexer_free (&lexer);
}

TEST (lexer_cval_int_flags, long_suffix_L)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("1L", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_IGET_SIZE, (uint64_t) LFLAGS_LONG);

    lexer_free (&lexer);
}

TEST (lexer_cval_int_flags, longlong_suffix_ll)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("1ll", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_IGET_SIZE, (uint64_t) LFLAGS_LONGLONG);

    lexer_free (&lexer);
}

TEST (lexer_cval_int_flags, longlong_suffix_LL)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("1LL", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_IGET_SIZE, (uint64_t) LFLAGS_LONGLONG);

    lexer_free (&lexer);
}

TEST (lexer_cval_int_flags, unsigned_long_suffix_ul)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("1ul", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_GET_SIGN, (uint64_t) LFLAGS_UNSIGNED);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_IGET_SIZE, (uint64_t) LFLAGS_LONG);

    lexer_free (&lexer);
}

TEST (lexer_cval_int_flags, unsigned_longlong_suffix_ULL)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("1ULL", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_GET_SIGN, (uint64_t) LFLAGS_UNSIGNED);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_IGET_SIZE, (uint64_t) LFLAGS_LONGLONG);

    lexer_free (&lexer);
}

// Multiple integers.
TEST (lexer_cval_int, multiple_values)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("1 2 3 10 255", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 5);

    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 1ULL);
    EXPECT_EQ (TOK (lexer, 1).cval.i_constant, 2ULL);
    EXPECT_EQ (TOK (lexer, 2).cval.i_constant, 3ULL);
    EXPECT_EQ (TOK (lexer, 3).cval.i_constant, 10ULL);
    EXPECT_EQ (TOK (lexer, 4).cval.i_constant, 255ULL);

    lexer_free (&lexer);
}

TEST (lexer_cval_int, mixed_bases_same_value)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // All three represent 255.
    LEX_OK ("255 0377 0xff", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 3);

    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 255ULL);
    EXPECT_EQ (TOK (lexer, 1).cval.i_constant, 255ULL);
    EXPECT_EQ (TOK (lexer, 2).cval.i_constant, 255ULL);

    lexer_free (&lexer);
}

// Float literal.
TEST (lexer_cval_float, simple_decimal)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("3.14", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).type, TOKEN_F_CONSTANT);
    EXPECT_DOUBLE_EQ (TOK (lexer, 0).cval.f_constant, 3.14);

    lexer_free (&lexer);
}

TEST (lexer_cval_float, trailing_dot)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("1.", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_DOUBLE_EQ (TOK (lexer, 0).cval.f_constant, 1.0);

    lexer_free (&lexer);
}

TEST (lexer_cval_float, leading_dot)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK (".5", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_DOUBLE_EQ (TOK (lexer, 0).cval.f_constant, 0.5);

    lexer_free (&lexer);
}

TEST (lexer_cval_float, exponent_positive)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("1e3", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_DOUBLE_EQ (TOK (lexer, 0).cval.f_constant, 1000.0);

    lexer_free (&lexer);
}

TEST (lexer_cval_float, exponent_negative)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("5e-2", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_DOUBLE_EQ (TOK (lexer, 0).cval.f_constant, 0.05);

    lexer_free (&lexer);
}

TEST (lexer_cval_float, exponent_uppercase_E)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("2E4", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_DOUBLE_EQ (TOK (lexer, 0).cval.f_constant, 20000.0);

    lexer_free (&lexer);
}

TEST (lexer_cval_float, zero)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("0.0", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_DOUBLE_EQ (TOK (lexer, 0).cval.f_constant, 0.0);

    lexer_free (&lexer);
}

// Float suffix.
TEST (lexer_cval_float_flags, no_suffix_is_double)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("1.0", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_FGET_SIZE, (uint64_t) LFLAGS_DOUBLE);

    lexer_free (&lexer);
}

TEST (lexer_cval_float_flags, f_suffix_is_float)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("1.0f", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_FGET_SIZE, (uint64_t) LFLAGS_FLOAT);

    lexer_free (&lexer);
}

TEST (lexer_cval_float_flags, F_suffix_is_float)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("2.f", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_FGET_SIZE, (uint64_t) LFLAGS_FLOAT);

    lexer_free (&lexer);
}

TEST (lexer_cval_float_flags, l_suffix_is_double)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("1e3l", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_FGET_SIZE, (uint64_t) LFLAGS_DOUBLE);

    lexer_free (&lexer);
}

TEST (lexer_cval_float_flags, L_suffix_is_double)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("1e34L", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).flags & LFLAGS_FGET_SIZE, (uint64_t) LFLAGS_DOUBLE);

    lexer_free (&lexer);
}

// Character literal.
TEST (lexer_cval_char, simple_ascii)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("'A'", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).type, TOKEN_I_CONSTANT);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, (uint64_t) 'A');

    lexer_free (&lexer);
}

TEST (lexer_cval_char, lowercase)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("'z'", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, (uint64_t) 'z');

    lexer_free (&lexer);
}

TEST (lexer_cval_char, digit_char)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("'0'", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, (uint64_t) '0');

    lexer_free (&lexer);
}

TEST (lexer_cval_char, space_char)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("' '", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, (uint64_t) ' ');

    lexer_free (&lexer);
}

TEST (lexer_cval_char, escape_newline)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("'\\n'", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, (uint64_t) '\n');

    lexer_free (&lexer);
}

TEST (lexer_cval_char, escape_tab)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("'\\t'", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, (uint64_t) '\t');

    lexer_free (&lexer);
}

TEST (lexer_cval_char, escape_carriage_return)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("'\\r'", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, (uint64_t) '\r');

    lexer_free (&lexer);
}

TEST (lexer_cval_char, escape_backslash)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("'\\\\'", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, (uint64_t) '\\');

    lexer_free (&lexer);
}

TEST (lexer_cval_char, escape_single_quote)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("'\\''", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, (uint64_t) '\'');

    lexer_free (&lexer);
}

TEST (lexer_cval_char, escape_null)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // Octal \0 == NUL byte.
    LEX_OK ("'\\0'", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 0ULL);

    lexer_free (&lexer);
}

TEST (lexer_cval_char, hex_escape)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // \x41 == 'A'.
    LEX_OK ("'\\x41'", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 0x41ULL);

    lexer_free (&lexer);
}

TEST (lexer_cval_char, octal_escape)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // \101 == 65 == 'A'.
    LEX_OK ("'\\101'", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, 65ULL);

    lexer_free (&lexer);
}

TEST (lexer_cval_char, multiple_chars_distinct_values)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("'A' 'B' 'C'", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 3);
    EXPECT_EQ (TOK (lexer, 0).cval.i_constant, (uint64_t) 'A');
    EXPECT_EQ (TOK (lexer, 1).cval.i_constant, (uint64_t) 'B');
    EXPECT_EQ (TOK (lexer, 2).cval.i_constant, (uint64_t) 'C');

    lexer_free (&lexer);
}

// String literal.
TEST (lexer_cval_string, simple_content)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("\"hello\"", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    EXPECT_EQ (TOK (lexer, 0).type, TOKEN_STRING_LITERAL);
    ASSERT_NE (TOK (lexer, 0).cval.s_constant, nullptr);
    EXPECT_STREQ (TOK (lexer, 0).cval.s_constant, "hello");

    lexer_free (&lexer);
}

TEST (lexer_cval_string, empty_string)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("\"\"", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    ASSERT_NE (TOK (lexer, 0).cval.s_constant, nullptr);
    EXPECT_STREQ (TOK (lexer, 0).cval.s_constant, "");

    lexer_free (&lexer);
}

TEST (lexer_cval_string, escape_newline_in_string)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("\"\\n\"", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    ASSERT_NE (TOK (lexer, 0).cval.s_constant, nullptr);
    // The evaluated string should contain a real newline character.
    EXPECT_EQ (TOK (lexer, 0).cval.s_constant[0], '\n');
    EXPECT_EQ (TOK (lexer, 0).cval.s_constant[1], '\0');

    lexer_free (&lexer);
}

TEST (lexer_cval_string, escape_tab_in_string)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("\"\\t\"", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    ASSERT_NE (TOK (lexer, 0).cval.s_constant, nullptr);
    EXPECT_EQ (TOK (lexer, 0).cval.s_constant[0], '\t');

    lexer_free (&lexer);
}

TEST (lexer_cval_string, escape_backslash_in_string)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("\"\\\\\"", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    ASSERT_NE (TOK (lexer, 0).cval.s_constant, nullptr);
    EXPECT_EQ (TOK (lexer, 0).cval.s_constant[0], '\\');
    EXPECT_EQ (TOK (lexer, 0).cval.s_constant[1], '\0');

    lexer_free (&lexer);
}

TEST (lexer_cval_string, escape_quote_in_string)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("\"\\\"\"", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    ASSERT_NE (TOK (lexer, 0).cval.s_constant, nullptr);
    EXPECT_EQ (TOK (lexer, 0).cval.s_constant[0], '"');
    EXPECT_EQ (TOK (lexer, 0).cval.s_constant[1], '\0');

    lexer_free (&lexer);
}

TEST (lexer_cval_string, mixed_escapes)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // "a\nb" -> 'a', '\n', 'b'
    LEX_OK ("\"a\\nb\"", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    ASSERT_NE (TOK (lexer, 0).cval.s_constant, nullptr);
    EXPECT_EQ (TOK (lexer, 0).cval.s_constant[0], 'a');
    EXPECT_EQ (TOK (lexer, 0).cval.s_constant[1], '\n');
    EXPECT_EQ (TOK (lexer, 0).cval.s_constant[2], 'b');
    EXPECT_EQ (TOK (lexer, 0).cval.s_constant[3], '\0');

    lexer_free (&lexer);
}

TEST (lexer_cval_string, hex_escape_in_string)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // "\x41" should evaluate to "A"
    LEX_OK ("\"\\x41\"", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    ASSERT_NE (TOK (lexer, 0).cval.s_constant, nullptr);
    EXPECT_EQ (TOK (lexer, 0).cval.s_constant[0], 'A');
    EXPECT_EQ (TOK (lexer, 0).cval.s_constant[1], '\0');

    lexer_free (&lexer);
}

TEST (lexer_cval_string, length_matches_evaluated_content)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // Source is 5 chars "hello", evaluated s_constant should be 5 bytes + NUL.
    LEX_OK ("\"hello\"", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    ASSERT_NE (TOK (lexer, 0).cval.s_constant, nullptr);
    EXPECT_EQ (strlen (TOK (lexer, 0).cval.s_constant), 5u);

    lexer_free (&lexer);
}

TEST (lexer_cval_string, escape_shrinks_evaluated_length)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    // Source token is 4 chars: \, n — but evaluated is 1 char '\n'.
    LEX_OK ("\"\\n\"", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 1);
    ASSERT_NE (TOK (lexer, 0).cval.s_constant, nullptr);
    // Evaluated content is 1 byte.
    EXPECT_EQ (strlen(TOK (lexer, 0).cval.s_constant), 1u);
    // Raw source token span covers the escape sequence characters.
    EXPECT_GT (TOK (lexer, 0).len, 1u);

    lexer_free (&lexer);
}

TEST (lexer_cval_string, two_adjacent_strings_independent_cvals)
{
    lexer_data_t lexer;
    lexer_init (&lexer);

    LEX_OK ("\"foo\" \"bar\"", &lexer);
    ASSERT_EQ (lexer.tokarr.tok_cnt, 2);
    ASSERT_NE (TOK (lexer, 0).cval.s_constant, nullptr);
    ASSERT_NE (TOK (lexer, 1).cval.s_constant, nullptr);
    EXPECT_STREQ (TOK (lexer, 0).cval.s_constant, "foo");
    EXPECT_STREQ (TOK (lexer, 1).cval.s_constant, "bar");

    lexer_free (&lexer);
}