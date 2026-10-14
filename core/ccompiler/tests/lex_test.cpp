// Unit tests for the lexer of the C compiler: the preprocessing tokens of tokenize() and their
// conversion by convert_pp_tokens(). An error exits the process, so the errors are death tests
// that match the message on stderr.

extern "C"
{
#include "ccompiler/lex.h"
}

#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace
{

Token *lex(const char *text)
{
    return tokenize(new_source_file("test.c", text));
}

/// The tokens of `text`, converted for the parser.
Token *convert(const char *text)
{
    return convert_pp_tokens(lex(text));
}

std::vector<std::string> spellings(const Token *tok)
{
    std::vector<std::string> out;
    for (; tok->kind != TK_EOF; tok = tok->next)
    {
        out.emplace_back(tok->loc, tok->len);
    }
    return out;
}

std::vector<TokenKind> kinds(const Token *tok)
{
    std::vector<TokenKind> out;
    for (; tok->kind != TK_EOF; tok = tok->next)
    {
        out.push_back(tok->kind);
    }
    return out;
}

/// The bytes of a converted string literal, without the 0 element at its end.
std::string string_bytes(const Token *tok)
{
    return std::string(tok->str, tok->str_size - static_cast<size_t>(tok->ty->size));
}

void expect_constant(const char *text, Type *ty, unsigned int value, unsigned int value_high)
{
    const Token *tok = convert(text);
    ASSERT_EQ(tok->kind, TK_NUM) << text;
    EXPECT_EQ(tok->ty, ty) << text << " is a " << tok->ty->name;
    EXPECT_EQ(static_cast<unsigned int>(tok->value), value) << text;
    EXPECT_EQ(tok->value_high, value_high) << text;
}

void expect_error(const char *text, const char *message)
{
    EXPECT_EXIT(convert(text), testing::ExitedWithCode(1), message) << text;
}

} // namespace

TEST(Lexer, splits_into_preprocessing_tokens)
{
    const Token *tok = lex("int x1 = 0x1F + 'a' - \"s\\\"\" ... @ 1.2.3e+4_x;");
    EXPECT_EQ(spellings(tok),
              (std::vector<std::string>{"int", "x1", "=", "0x1F", "+", "'a'", "-", "\"s\\\"\"",
                                        "...", "@", "1.2.3e+4_x", ";"}));
    EXPECT_EQ(kinds(tok), (std::vector<TokenKind>{TK_IDENT, TK_IDENT, TK_PUNCT, TK_PP_NUM, TK_PUNCT,
                                                  TK_PP_CHAR, TK_PUNCT, TK_PP_STR, TK_PUNCT,
                                                  TK_PP_OTHER, TK_PP_NUM, TK_PUNCT}));
}

TEST(Lexer, punctuators_are_the_longest_match)
{
    EXPECT_EQ(spellings(lex("a<<=b>>c->d...e##f::g&&=h+++i")),
              (std::vector<std::string>{"a", "<<=", "b", ">>", "c", "->", "d", "...", "e", "##",
                                        "f", "::", "g", "&&", "=", "h", "++", "+", "i"}));
}

TEST(Lexer, digraphs_are_the_punctuators_they_stand_for)
{
    const Token *tok = lex("<: :> <% %> %: %:%:");
    for (const char *meaning : {"[", "]", "{", "}", "#", "##"})
    {
        EXPECT_TRUE(token_is(tok, meaning)) << meaning;
        tok = tok->next;
    }
    EXPECT_FALSE(token_is(lex("<:"), "<:"));
}

TEST(Lexer, literals_with_prefixes_and_without_closing_quotes)
{
    const Token *tok = lex("u8\"a\" u\"b\" U'c' L'd' u8'e' x\"f\" \"open\ndon't");
    EXPECT_EQ(spellings(tok),
              (std::vector<std::string>{"u8\"a\"", "u\"b\"", "U'c'", "L'd'", "u8", "'e'", "x",
                                        "\"f\"", "\"", "open", "don", "'", "t"}));
    EXPECT_EQ(kinds(tok),
              (std::vector<TokenKind>{TK_PP_STR, TK_PP_STR, TK_PP_CHAR, TK_PP_CHAR, TK_IDENT,
                                      TK_PP_CHAR, TK_IDENT, TK_PP_STR, TK_PP_OTHER, TK_IDENT,
                                      TK_IDENT, TK_PP_OTHER, TK_IDENT}));
}

TEST(Lexer, pp_numbers_take_letters_signs_after_exponents_and_separators)
{
    EXPECT_EQ(spellings(lex("1e+5 0x1p-3 1'000 .5 1+2 0x1e+1 12uLL")),
              (std::vector<std::string>{"1e+5", "0x1p-3", "1'000", ".5", "1", "+", "2", "0x1e+1",
                                        "12uLL"}));
}

TEST(Lexer, flags_line_starts_white_space_and_line_numbers)
{
    const Token *tok = lex("a b/**/c\n  d/* two\nlines */e // comment\nf");
    std::vector<bool> at_bol;
    std::vector<bool> has_space;
    std::vector<int> lines;
    for (; tok->kind != TK_EOF; tok = tok->next)
    {
        at_bol.push_back(tok->at_bol);
        has_space.push_back(tok->has_space);
        lines.push_back(tok->line_no);
    }
    // A comment is one space, even one that spans lines.
    EXPECT_EQ(at_bol, (std::vector<bool>{true, false, false, true, false, true}));
    EXPECT_EQ(has_space, (std::vector<bool>{false, true, true, true, true, false}));
    EXPECT_EQ(lines, (std::vector<int>{1, 1, 1, 2, 3, 4}));
}

TEST(Lexer, line_splices_crlf_and_byte_order_mark)
{
    // A backslash at the end of a line joins the lines, also inside a name.
    const Token *tok = lex("\xEF\xBB\xBFre\\\r\nturn x\\\ny\r\nz\rw");
    EXPECT_EQ(spellings(tok), (std::vector<std::string>{"return", "xy", "z", "w"}));
    // The lines after a splice keep their numbers.
    std::vector<int> lines;
    for (; tok->kind != TK_EOF; tok = tok->next)
    {
        lines.push_back(tok->line_no);
    }
    EXPECT_EQ(lines, (std::vector<int>{1, 1, 4, 5}));
    EXPECT_EQ(tok->line_no, 5); // The TK_EOF.
    // A splice at the end of the file gives its line back too.
    EXPECT_EQ(lex("a\\\n")->next->line_no, 2);
    EXPECT_EXIT(convert("int a = \\\n  1;\n@"), testing::ExitedWithCode(1), "test.c:3: @");
}

TEST(Lexer, keywords_are_converted_after_preprocessing)
{
    EXPECT_EQ(kinds(lex("int integer _Bool")),
              (std::vector<TokenKind>{TK_IDENT, TK_IDENT, TK_IDENT}));
    const Token *tok = convert("int integer _Bool typeof");
    EXPECT_EQ(kinds(tok), (std::vector<TokenKind>{TK_KEYWORD, TK_IDENT, TK_KEYWORD, TK_KEYWORD}));
    EXPECT_TRUE(token_is(tok, "int"));
    EXPECT_TRUE(token_is(tok->next, "integer"));
}

TEST(Lexer, integer_constants_take_the_first_type_their_value_fits)
{
    expect_constant("0", ty_int, 0, 0);
    expect_constant("2147483647", ty_int, 0x7FFFFFFF, 0);
    // Decimal goes on to long long, the other bases to unsigned first (C17 6.4.4.1).
    expect_constant("2147483648", ty_llong, 0x80000000, 0);
    expect_constant("0x80000000", ty_uint, 0x80000000, 0);
    expect_constant("0x100000000", ty_llong, 0, 1);
    expect_constant("0xFFFFFFFFFFFFFFFF", ty_ullong, 0xFFFFFFFF, 0xFFFFFFFF);
    expect_constant("9223372036854775807", ty_llong, 0xFFFFFFFF, 0x7FFFFFFF);
    expect_constant("4294967296u", ty_ullong, 0, 1);
    expect_constant("1l", ty_long, 1, 0);
    expect_constant("0xFFFFFFFFL", ty_ulong, 0xFFFFFFFF, 0);
    expect_constant("1Lu", ty_ulong, 1, 0);
    expect_constant("1LL", ty_llong, 1, 0);
    expect_constant("1llU", ty_ullong, 1, 0);
    expect_constant("0b1010", ty_int, 10, 0);
    expect_constant("0777", ty_int, 511, 0);
    expect_constant("0'7'7", ty_int, 63, 0);
    expect_constant("1'000'000", ty_int, 1000000, 0);
}

TEST(Lexer, rejects_integer_constants_that_are_not_valid)
{
    expect_error("09", "invalid integer constant");
    expect_error("0x", "invalid integer constant");
    expect_error("0b2", "invalid integer constant");
    expect_error("12uu", "invalid integer constant");
    expect_error("1lL", "invalid integer constant");
    expect_error("0x'1", "invalid integer constant");
    expect_error("18446744073709551616", "integer constant is too large");
    // The value wraps to 0 at the last digit: it was too large before that.
    expect_error("184467440737095516160", "integer constant is too large");
    expect_error("9223372036854775808", "too large for its type");
}

TEST(Lexer, floating_constants_have_a_type_and_no_value_yet)
{
    expect_constant("1.5", ty_double, 0, 0);
    expect_constant(".5e-3f", ty_float, 0, 0);
    expect_constant("1.", ty_double, 0, 0);
    expect_constant("1e10", ty_double, 0, 0);
    expect_constant("0x1.8p1L", ty_ldouble, 0, 0);
    expect_constant("09.5", ty_double, 0, 0);
    expect_constant("1'0.0'1", ty_double, 0, 0);
    expect_error("1e", "invalid floating constant");
    expect_error("0x1.8", "invalid floating constant");
    expect_error("1.5x", "invalid floating constant");
}

TEST(Lexer, character_constants)
{
    expect_constant("'a'", ty_int, 'a', 0);
    expect_constant("'\\n'", ty_int, '\n', 0);
    expect_constant("'\\''", ty_int, '\'', 0);
    expect_constant("'\\101'", ty_int, 'A', 0);
    expect_constant("'\\x41'", ty_int, 'A', 0);
    // A plain char is signed.
    expect_constant("'\\377'", ty_int, 0xFFFFFFFF, 0xFFFFFFFF);
    expect_constant("L'\\xFFFFFFFF'", ty_int, 0xFFFFFFFF, 0xFFFFFFFF);
    expect_constant("u'\\u00e9'", ty_ushort, 0xE9, 0);
    expect_constant("u'\xC3\xA9'", ty_ushort, 0xE9, 0);
    expect_constant("U'\\U0001F600'", ty_uint, 0x1F600, 0);
    expect_constant("U'\\xFFFFFFFF'", ty_uint, 0xFFFFFFFF, 0);
    // A multi-character constant has its bytes, the first one highest, as in GCC.
    expect_constant("'ab'", ty_int, 0x6162, 0);
}

TEST(Lexer, rejects_character_constants_that_are_not_valid)
{
    expect_error("''", "empty character constant");
    expect_error("'abcde'", "character constant is too long");
    expect_error("u'ab'", "has to be one code unit");
    expect_error("u'\\U0001F600'", "has to be one code unit");
    expect_error("'\\q'", "unknown escape sequence");
    expect_error("'\\x'", "no following hex digits");
    expect_error("'\\x100'", "hex escape sequence out of range");
    expect_error("'\\400'", "octal escape sequence out of range");
    expect_error("'\\u0041'", "invalid universal character name");
    expect_error("'\\uD800'", "invalid universal character name");
    expect_error("'\\u12'", "incomplete universal character name");
    expect_error("U'\xC3'", "invalid UTF-8");
}

TEST(Lexer, string_literals_are_bytes_in_the_target_layout)
{
    const Token *tok = convert("\"a\\tb\\0c\"");
    EXPECT_EQ(tok->kind, TK_STR);
    EXPECT_EQ(tok->ty, ty_char);
    EXPECT_EQ(tok->str_size, 6u);
    EXPECT_EQ(string_bytes(tok), std::string("a\tb\0c", 5));

    // \u is UTF-8 in a plain string; an escape is one byte.
    EXPECT_EQ(string_bytes(convert("\"\\u00e9\\xff\"")), "\xC3\xA9\xFF");
    EXPECT_EQ(string_bytes(convert("u8\"\xC3\xA9\"")), "\xC3\xA9");

    // UTF-16 with a surrogate pair, and UTF-32, little endian.
    tok = convert("u\"a\\U0001F600\"");
    EXPECT_EQ(tok->ty, ty_ushort);
    EXPECT_EQ(string_bytes(tok), std::string("a\0\x3D\xD8\x00\xDE", 6));
    tok = convert("U\"\xE2\x82\xAC\"");
    EXPECT_EQ(tok->ty, ty_uint);
    EXPECT_EQ(tok->str_size, 8u);
    EXPECT_EQ(string_bytes(tok), std::string("\xAC\x20\0\0", 4));
}

TEST(Lexer, adjacent_string_literals_are_one)
{
    const Token *tok = convert("\"ab\" \"c\" x");
    EXPECT_EQ(string_bytes(tok), "abc");
    EXPECT_TRUE(token_is(tok->next, "x"));

    // A prefix applies to the whole string.
    tok = convert("\"a\" L\"b\" \"c\"");
    EXPECT_EQ(tok->ty, ty_int);
    EXPECT_EQ(string_bytes(tok), std::string("a\0\0\0b\0\0\0c\0\0\0", 12));
    EXPECT_EQ(tok->next->kind, TK_EOF);

    expect_error("u\"a\" U\"b\"", "different prefixes");
}

TEST(Lexer, other_tokens_are_errors_after_preprocessing)
{
    expect_error("1 @ 2", "invalid token");
    expect_error("\\", "invalid token");
    expect_error("\"abc", "missing terminating \" character");
    expect_error("x = 'a;", "missing terminating ' character");
    EXPECT_EXIT(lex("a /* never closed"), testing::ExitedWithCode(1), "unterminated comment");
}
