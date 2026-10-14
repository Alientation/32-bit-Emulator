// Unit tests for the preprocessor of the C compiler. Each test preprocesses a small source and
// compares the text that print_tokens() makes of the result (one space where there was white
// space, a line break before a token that started a line). The expected texts of the macro
// expansions are those of the host's `cpp -P`, with the white space normalized. An error exits
// the process, so the errors are death tests that match the message on stderr.

extern "C"
{
#include "ccompiler/lex.h"
#include "ccompiler/preprocess.h"
}

#include <cfloat>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <gtest/gtest.h>
#include <string>
#include <vector>

namespace fs = std::filesystem;

namespace
{

/// Each test works in its own scratch directory, for the files it includes.
class Preprocessor : public ::testing::Test
{
  protected:
    fs::path m_dir;
    std::vector<std::string> m_include_paths;
    std::string m_command_line;
    /// The name of main.c, which the tokens keep a pointer to.
    std::string m_main_path;

    void SetUp() override
    {
        const ::testing::TestInfo *info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_dir = fs::temp_directory_path() / "aemu_ccompiler_tests"
                / (std::string(info->test_suite_name()) + "." + info->name());
        fs::remove_all(m_dir);
        fs::create_directories(m_dir);
    }

    void TearDown() override
    {
        fs::remove_all(m_dir);
    }

    void write_file(const std::string &name, const std::string &contents)
    {
        fs::create_directories((m_dir / name).parent_path());
        std::ofstream(m_dir / name) << contents;
    }

    /// Preprocesses `source`, written to main.c in the scratch directory.
    std::string preprocess_source(const std::string &source)
    {
        write_file("main.c", source);
        m_main_path = (m_dir / "main.c").string();
        std::vector<const char *> paths;
        for (const std::string &p : m_include_paths)
        {
            paths.push_back(p.c_str());
        }
        PreprocessOptions options = {paths.data(), paths.size(),
                                     m_command_line.empty() ? nullptr : m_command_line.c_str()};
        Token *tok = preprocess(tokenize(read_source_file(m_main_path.c_str())), &options);

        std::FILE *out = std::tmpfile();
        print_tokens(tok, out);
        std::string text(static_cast<size_t>(std::ftell(out)), '\0');
        std::rewind(out);
        EXPECT_EQ(std::fread(text.data(), 1, text.size(), out), text.size());
        std::fclose(out);
        return text;
    }

    void expect_error(const std::string &source, const char *message)
    {
        EXPECT_EXIT(preprocess_source(source), testing::ExitedWithCode(1), message) << source;
    }
};

} // namespace

TEST_F(Preprocessor, text_without_directives_is_kept)
{
    EXPECT_EQ(preprocess_source("int  main ( void )\n{   return 0; } // done\n"),
              "int main ( void )\n{ return 0; }\n");
    EXPECT_EQ(preprocess_source(""), "");
}

TEST_F(Preprocessor, object_and_function_like_macros)
{
    EXPECT_EQ(preprocess_source("#define N 10\n"
                                "#define SQUARE(x) ((x) * (x))\n"
                                "#define NONE()\n"
                                "#define TWO(a, b) a - b\n"
                                "N SQUARE(N + 1) NONE() TWO((1, 2), [3]) SQUARE\n"),
              "10 ((10 + 1) * (10 + 1)) (1, 2) - [3] SQUARE\n");
    // The arguments can span lines, and the name and its ( can be on different lines.
    EXPECT_EQ(preprocess_source("#define F(a, b) a + b\nF\n(1,\n2)\n"), "1 + 2\n");
}

TEST_F(Preprocessor, standard_example_of_rescanning)
{
    // C17 6.10.3.5 EXAMPLE 3, whose result the standard prints with this spacing.
    EXPECT_EQ(preprocess_source("#define x 3\n"
                                "#define f(a) f(x * (a))\n"
                                "#undef x\n"
                                "#define x 2\n"
                                "#define g f\n"
                                "#define z z[0]\n"
                                "#define h g(~\n"
                                "#define m(a) a(w)\n"
                                "#define w 0,1\n"
                                "#define t(a) a\n"
                                "#define p() int\n"
                                "#define q(x) x\n"
                                "#define r(x,y) x ## y\n"
                                "#define str(x) # x\n"
                                "f(y+1) + f(f(z)) % t(t(g)(0) + t)(1);\n"
                                "g(x+(3,4)-w) | h 5) & m\n"
                                "(f)^m(m);\n"
                                "p() i[q()] = { q(1), r(2,3), r(4,), r(,5), r(,) };\n"
                                "char c[2][6] = { str(hello), str() };\n"),
              "f(2 * (y+1)) + f(2 * (f(2 * (z[0])))) % f(2 * (0)) + t(1);\n"
              "f(2 * (2+(3,4)-0,1)) | f(2 * (~ 5)) & f(2 * (0,1))^m(0,1);\n"
              "int i[] = { 1, 23, 4, 5, };\n"
              "char c[2][6] = { \"hello\", \"\" };\n");
}

TEST_F(Preprocessor, hide_sets_stop_recursion)
{
    EXPECT_EQ(preprocess_source("#define f f\n#define AA BB\n#define BB AA\nf AA BB\n"),
              "f AA BB\n");
    EXPECT_EQ(preprocess_source("#define a a b\n#define b a c\na\n"), "a a c\n");
    // The name of a function-like macro is hidden by its ), not by its name: g expands again.
    EXPECT_EQ(preprocess_source("#define f(a) a*g\n#define g(a) f(a)\nf(2)(9)\n"), "2*9*g\n");
}

TEST_F(Preprocessor, arguments_are_expanded_except_for_hash_and_paste)
{
    EXPECT_EQ(preprocess_source("#define FOO 1\n"
                                "#define str(s) #s\n"
                                "#define xstr(s) str(s)\n"
                                "#define cat(a, b) a ## b\n"
                                "#define xcat(a, b) cat(a, b)\n"
                                "#define FOOBAR 3\n"
                                "str(FOO) xstr(FOO) cat(FOO, BAR) xcat(FOO, BAR)\n"),
              "\"FOO\" \"1\" 3 1BAR\n");
}

TEST_F(Preprocessor, stringizing_escapes_strings_and_collapses_space)
{
    // C17 6.10.3.5 EXAMPLE 4.
    EXPECT_EQ(
        preprocess_source("#define str(s) # s\n"
                          "#define xstr(s) str(s)\n"
                          "#define debug(s, t) printf(\"x\" # s \"= %d, x\" # t \"= %s\", \\\n"
                          " x ## s, x ## t)\n"
                          "#define INCFILE(n) vers ## n\n"
                          "debug(1, 2);\n"
                          "fputs(str(strncmp(\"abc\\0d\", \"abc\", '\\4') // this goes away\n"
                          " == 0) str(: @\\n), s);\n"
                          "xstr(INCFILE(2).h)\n"),
        "printf(\"x\" \"1\" \"= %d, x\" \"2\" \"= %s\", x1, x2);\n"
        "fputs(\"strncmp(\\\"abc\\\\0d\\\", \\\"abc\\\", '\\\\4') == 0\" \": @\\n\", s);\n"
        "\"vers2.h\"\n");
}

TEST_F(Preprocessor, pasting_and_placemarkers)
{
    // C17 6.10.3.5 EXAMPLE 5 and the hash_hash one of 6.10.3.3.
    EXPECT_EQ(preprocess_source("#define t(x,y,z) x ## y ## z\n"
                                "int j[] = { t(1,2,3), t(,4,5), t(6,,7), t(8,9,),\n"
                                " t(10,,), t(,11,), t(,,12), t(,,) };\n"
                                "#define hash_hash # ## #\n"
                                "#define mkstr(a) # a\n"
                                "#define in_between(a) mkstr(a)\n"
                                "#define join(c, d) in_between(c hash_hash d)\n"
                                "char p[] = join(x, y);\n"),
              "int j[] = { 123, 45, 67, 89,\n10, 11, 12, };\nchar p[] = \"x ## y\";\n");
    // A pasted digraph is the punctuator; an empty argument before ## is not pasted to the token
    // before it.
    EXPECT_EQ(preprocess_source("#define cat(x, y) x ## y\n#define pre(x, y) 1 x ## y\n"
                                "cat(<, :) cat(%:, %:) cat(-, >) cat(1, e)+3 pre(, 2)\n"),
              "<: %:%: -> 1e +3 1 2\n");
    expect_error("#define cat(x, y) x ## y\ncat(+, /)\n", "does not give a valid");
}

TEST_F(Preprocessor, variadic_macros_and_va_opt)
{
    // C17 6.10.3.5 EXAMPLE 7 and the __VA_OPT__ examples of C23 6.10.5.2.
    EXPECT_EQ(preprocess_source("#define debug(...) fprintf(stderr, __VA_ARGS__)\n"
                                "#define showlist(...) puts(#__VA_ARGS__)\n"
                                "#define report(test, ...) ((test)?puts(#test):\\\n"
                                " printf(__VA_ARGS__))\n"
                                "debug(\"X = %d\\n\", x);\n"
                                "showlist(The first, second, and third items.);\n"
                                "report(x>y, \"x is %d but y is %d\", x, y);\n"),
              "fprintf(stderr, \"X = %d\\n\", x);\n"
              "puts(\"The first, second, and third items.\");\n"
              "((x>y)?puts(\"x>y\"): printf(\"x is %d but y is %d\", x, y));\n");
    EXPECT_EQ(
        preprocess_source("#define F(...) f(0 __VA_OPT__(,) __VA_ARGS__)\n"
                          "#define G(X, ...) f(0, X __VA_OPT__(,) __VA_ARGS__)\n"
                          "#define SDEF(sname, ...) S sname __VA_OPT__(= { __VA_ARGS__ })\n"
                          "#define EMP\n"
                          "#define H3(X, ...) #__VA_OPT__(X##X X##X)\n"
                          "#define H4(X, ...) __VA_OPT__(a X ## X) ## b\n"
                          "#define H2(X, Y, ...) __VA_OPT__(X ## Y,) __VA_ARGS__\n"
                          "#define H5A(...) __VA_OPT__()/**/__VA_OPT__()\n"
                          "#define H5B(X) a ## X ## b\n"
                          "#define H5C(X) H5B(X)\n"
                          "F(a,b,c) F() F(EMP) G(a,b,c) G(a,) G(a) SDEF(foo); SDEF(bar, 1, 2);\n"
                          "H3(, 0) H4(, 1) H2(a, b, c, d) H5C(H5A())\n"),
        "f(0 , a,b,c) f(0) f(0) f(0, a , b,c) f(0, a) f(0, a) S foo; S bar = { 1, 2 };\n"
        "\"\" a b ab, c, d ab\n");
}

TEST_F(Preprocessor, rejects_macro_definitions_that_are_not_valid)
{
    expect_error("#define\n", "macro names must be identifiers");
    expect_error("#define 1 2\n", "macro names must be identifiers");
    expect_error("#define defined 1\n", "'defined' cannot be used as a macro name");
    expect_error("#define F(a, a) a\n", "duplicate macro parameter 'a'");
    expect_error("#define F(a, 1) a\n", "expected a parameter name");
    expect_error("#define F(a\n", "expected ',' or '\\)'");
    expect_error("#define F(..., a)\n", "expected '\\)' after '...'");
    expect_error("#define F(a) #b\n", "'#' is not followed by a macro parameter");
    expect_error("#define F ## a\n", "'##' cannot be at either end");
    expect_error("#define F a ##\n", "'##' cannot be at either end");
    expect_error("#define F(a) __VA_ARGS__\n", "__VA_ARGS__ can only be in a variadic macro");
    expect_error("#define F(...) __VA_OPT__ x\n", "needs its tokens in parentheses");
    expect_error("#define F(...) __VA_OPT__(## x)\n", "'##' cannot be at either end of __VA_OPT__");
    expect_error("#define F(...) __VA_OPT__(__VA_OPT__())\n", "cannot be nested");
    expect_error("#define A 1\n#define A 2\n", "'A' is redefined differently");
    expect_error("#define A(x) x\n#define A(y) y\n", "'A' is redefined differently");
    // The same definition again is allowed, and so is a definition after #undef.
    EXPECT_EQ(
        preprocess_source("#define A 1 /* */ + 2\n#define A 1 + 2\nA\n#undef A\n#define A 3\nA\n"),
        "1 + 2\n3\n");
}

TEST_F(Preprocessor, rejects_macro_calls_that_are_not_valid)
{
    expect_error("#define F(a, b) a\nF(1)\n", "too few arguments for the macro 'F'");
    expect_error("#define F(a) a\nF(1, 2)\n", "too many arguments for the macro 'F'");
    expect_error("#define F() 1\nF(1)\n", "too many arguments for the macro 'F'");
    expect_error("#define F(a) a\nF((1)\n", "unterminated call of the macro 'F'");
}

TEST_F(Preprocessor, conditionals)
{
    EXPECT_EQ(preprocess_source("#define ONE 1\n"
                                "#if ONE\na\n#else\nb\n#endif\n"
                                "#if 0\nc\n#elif ONE + 1 == 2\nd\n#elif 1\ne\n#else\nf\n#endif\n"
                                "#ifdef ONE\ng\n#endif\n"
                                "#ifndef ONE\nh\n#elifdef TWO\ni\n#elifndef TWO\nj\n#endif\n"
                                "#if 1\n#if 0\nk\n#else\nl\n#endif\n#endif\n"),
              "a\nd\ng\nj\nl\n");
    // A group that is skipped is only read for its directives: neither its tokens nor the
    // expressions of its #elif have to be valid.
    EXPECT_EQ(preprocess_source("#if 0\ndon't \"\n#if garbage ((\n#else\n#bogus\n#endif\n"
                                "#elif 1\nyes\n#elif 1 / 0\n#endif\n"),
              "yes\n");
    // Names left after expansion are 0, true is 1; defined and __has_include are replaced first.
    write_file("there.h", "");
    EXPECT_EQ(preprocess_source("#define D\n"
                                "#if defined D && defined(D) && !defined E && UNDEFINED == 0 "
                                "&& true && !false\nyes\n#endif\n"
                                "#define THERE \"there.h\"\n"
                                "#if __has_include(\"there.h\") && !__has_include(<missing.h>) && "
                                "__has_include(THERE)\n"
                                "found\n#endif\n"),
              "yes\nfound\n");
}

TEST_F(Preprocessor, if_arithmetic_is_64_bit)
{
    EXPECT_EQ(
        preprocess_source("#if 2147483647 + 1 > 0 && 4294967296 > 4294967295 && "
                          "-9223372036854775807 - 1 < 0\na\n#endif\n"
                          // Unsigned when an operand is; a signed overflow wraps.
                          "#if -1 > 0u && 0xFFFFFFFFFFFFFFFF == -1 && "
                          "0x7FFFFFFFFFFFFFFF * 2 == -2 && 5u - 10 > 0 && 5 - 10 < 0\nb\n#endif\n"
                          "#if 18446744073709551615u / 3 == 6148914691236517205 && "
                          "10000000000 * 10000000000 / 10000000000 != 10000000000 && "
                          "3 * 0x10000 == 0x30000 && 123456789 * 987654321 == "
                          "121932631112635269\nc\n#endif\n"
                          // Division rounds toward zero, >> of a negative value keeps the sign.
                          "#if -7 / 2 == -3 && -7 % 2 == -1 && 7 % -3 == 1 && -1 >> 1 == -1 "
                          "&& 1u << 63 > 0 && 1 << 62 > 0 && -0x200000000 >> 32 == -2 && "
                          "0x123456789 >> 32 == 1\nd\n#endif\n"
                          // INT64_MIN / -1 wraps (the host's division would trap).
                          "#if (-9223372036854775807 - 1) / -1 == -9223372036854775807 - 1 && "
                          "(-9223372036854775807 - 1) % -1 == 0 && -7 / -1 == 7\nh\n#endif\n"
                          "#if 'A' == 65 && '\\377' < 0 && L'\\xFFFFFFFF' < 0 && u'x' == 120\n"
                          "e\n#endif\n"
                          "#if (1 ? 2 : 3) == 2 && (0 ? 2 : 3) == 3 && (3, 4) == 4 && ~0 == -1 "
                          "&& +1 == 1 && (-1 > 0u ? 1u : -1) > 0\nf\n#endif\n"
                          "#if 3 * 4 % 5 == 2 && (6 & 3 | 8 ^ 1) == 11 && 1 != 2 && 2 <= 2 && "
                          "3 >= 4 == 0\ng\n#endif\n"),
        "a\nb\nc\nd\nh\ne\nf\ng\n");
    // An operand that is not evaluated may divide by 0.
    EXPECT_EQ(
        preprocess_source("#if 0 && 1 / 0 || 1 || 1 % 0\nyes\n#endif\n#if 1 ? 1 : 1 / 0\nyes\n"
                          "#endif\n"),
        "yes\nyes\n");
}

TEST_F(Preprocessor, rejects_conditionals_that_are_not_valid)
{
    expect_error("#if 1 / 0\n#endif\n", "division by zero in #if");
    expect_error("#if 1 << 64\n#endif\n", "shift count out of range in #if");
    expect_error("#if\n#endif\n", "#if with no expression");
    expect_error("#if (1\n#endif\n", "expected '\\)' in the #if expression");
    expect_error("#if 1 ? 2\n#endif\n", "expected ':' in the #if expression");
    expect_error("#if 1 2\n#endif\n", "missing an operator before this in #if");
    expect_error("#if 1 +\n#endif\n", "expected an expression after this in #if");
    expect_error("#if 1.0\n#endif\n", "floating constant in an #if expression");
    expect_error("#if \"s\"\n#endif\n", "expected an expression in #if");
    expect_error("#if defined\n#endif\n", "'defined' needs a macro name");
    expect_error("#if defined(X\n#endif\n", "expected '\\)' after the name of 'defined'");
    expect_error("#if __has_include\n#endif\n", "__has_include needs a file name in parentheses");
    expect_error("#if 1\n", "unterminated #if");
    expect_error("#ifdef X\n#if 0\n#endif\n", "unterminated #ifdef");
    expect_error("#endif\n", "#endif without #if");
    expect_error("#else\n", "#else without #if");
    expect_error("#if 1\n#else\n#else\n#endif\n", "#else after #else");
    expect_error("#if 1\n#else\n#elif 1\n#endif\n", "#elif after #else");
    expect_error("#ifdef 1\n#endif\n", "macro names must be identifiers");
}

TEST_F(Preprocessor, include_searches_the_directory_of_the_file_then_the_paths)
{
    write_file("a.h", "int a; // a.h\n#include \"sub/b.h\"\n");
    write_file("sub/b.h", "int b;\n#include \"c.h\"\n");
    write_file("sub/c.h", "int c;\n");
    write_file("inc/d.h", "int d;\n");
    write_file("sub/d.h", "int wrong;\n");
    m_include_paths = {(m_dir / "inc").string()};
    EXPECT_EQ(preprocess_source("#include \"a.h\"\n#include <d.h>\n#define E \"inc/d.h\"\n"
                                "#include E\n#define F <d.h>\n#include F\nint main;\n"),
              "int a;\nint b;\nint c;\nint d;\nint d;\nint d;\nint main;\n");
}

TEST_F(Preprocessor, include_guards_and_pragma_once)
{
    write_file("guard.h", "#ifndef GUARD_H\n#define GUARD_H\nint guarded;\n#endif\n");
    write_file("once.h", "#pragma once\nint once;\n");
    write_file("once2.h", "_Pragma(\"once\") int once2;\n");
    EXPECT_EQ(preprocess_source("#include \"guard.h\"\n#include \"guard.h\"\n#include \"once.h\"\n"
                                "#include \"once.h\"\n#include \"once2.h\"\n#include \"once2.h\"\n"
                                "#pragma STDC FP_CONTRACT ON\n_Pragma(\"whatever\") x\n"),
              "int guarded;\nint once; int once2; x\n");
}

TEST_F(Preprocessor, include_errors_name_the_place)
{
    write_file("bad.h", "int ok;\n#error stop in \"bad.h\"\n");
    write_file("open.h", "#if 1\n");
    write_file("close.h", "#endif\n");
    write_file("self.h", "#include \"self.h\"\n");
    expect_error("#include \"missing.h\"\n", "'missing.h' not found");
    expect_error("#include <main.c>\n", "'main.c' not found");
    expect_error("#include\n", "expected a file name");
    expect_error("#include <a.h\n", "expected '>'");
    expect_error("\n#include \"bad.h\"\n", "bad.h:2: #error stop in \"bad.h\"");
    expect_error("#include \"open.h\"\n#endif\n", "unterminated #if");
    expect_error("#if 1\n#include \"close.h\"\n", "#endif without #if");
    expect_error("#include \"self.h\"\n", "#include nested more than 200 deep");
}

TEST_F(Preprocessor, predefined_and_builtin_macros)
{
    EXPECT_EQ(
        preprocess_source("#define L __LINE__\n__FILE__ __LINE__\nL __COUNTER__ __COUNTER__\n"
                          "__DATE__ __TIME__ __STDC__ __STDC_VERSION__ __STDC_HOSTED__ "
                          "__SIZE_TYPE__ __CHAR_BIT__\n"
                          "#if defined __bcc__ && __SIZEOF_POINTER__ == 4 && __BYTE_ORDER__ "
                          "== __ORDER_LITTLE_ENDIAN__ && defined __has_include\nok\n#endif\n"),
        "\"" + (m_dir / "main.c").string()
            + "\" 2\n3 0 1\n\"Jan  1 1970\" \"00:00:00\" 1 "
              "201710L 1 unsigned int 8\nok\n");
    expect_error("__has_include(\"x\")\n", "__has_include can only be in #if");
    expect_error("#define __LINE__ 1\n", "'__LINE__' is redefined differently");
}

TEST_F(Preprocessor, floating_limits_are_those_of_ieee_754)
{
    // The host's float and double are binary32 and binary64 as well.
    const std::string text = preprocess_source(
        "__FLT_MAX__ __FLT_MIN__ __FLT_EPSILON__ __FLT_DENORM_MIN__ __DBL_MAX__ __DBL_MIN__ "
        "__DBL_EPSILON__ __DBL_DENORM_MIN__ __LDBL_MAX__ __LDBL_DENORM_MIN__\n");
    const Token *tok = convert_pp_tokens(tokenize(new_source_file("limits.c", text.c_str())));
    const double limits[] = {FLT_MAX, FLT_MIN,     FLT_EPSILON,  FLT_TRUE_MIN, DBL_MAX,
                             DBL_MIN, DBL_EPSILON, DBL_TRUE_MIN, DBL_MAX,      DBL_TRUE_MIN};
    for (double limit : limits)
    {
        EXPECT_EQ(tok->fval, limit) << std::string(tok->loc, tok->len);
        tok = tok->next;
    }
    EXPECT_EQ(preprocess_source("__FLT_MANT_DIG__ __FLT_DIG__ __FLT_DECIMAL_DIG__ __FLT_MIN_EXP__ "
                                "__FLT_MIN_10_EXP__ __FLT_MAX_EXP__ __FLT_MAX_10_EXP__\n"
                                "__DBL_MANT_DIG__ __DBL_DIG__ __DBL_DECIMAL_DIG__ __DBL_MIN_EXP__ "
                                "__DBL_MIN_10_EXP__ __DBL_MAX_EXP__ __DBL_MAX_10_EXP__\n"),
              std::to_string(FLT_MANT_DIG) + " " + std::to_string(FLT_DIG) + " "
                  + std::to_string(FLT_DECIMAL_DIG) + " (" + std::to_string(FLT_MIN_EXP) + ") ("
                  + std::to_string(FLT_MIN_10_EXP) + ") " + std::to_string(FLT_MAX_EXP) + " "
                  + std::to_string(FLT_MAX_10_EXP) + "\n" + std::to_string(DBL_MANT_DIG) + " "
                  + std::to_string(DBL_DIG) + " " + std::to_string(DBL_DECIMAL_DIG) + " ("
                  + std::to_string(DBL_MIN_EXP) + ") (" + std::to_string(DBL_MIN_10_EXP) + ") "
                  + std::to_string(DBL_MAX_EXP) + " " + std::to_string(DBL_MAX_10_EXP) + "\n");
    EXPECT_EQ(preprocess_source("#if __INTMAX_MAX__ == 0x7FFFFFFFFFFFFFFF && __UINTMAX_MAX__ == -1 "
                                "&& __FLT_EVAL_METHOD__ == 0 && __FLT_RADIX__ == 2\nok\n#endif\n"
                                "__INTMAX_TYPE__ __UINTMAX_TYPE__\n"),
              "ok\nlong long unsigned long long\n");
}

TEST_F(Preprocessor, line_changes_the_line_number_and_the_file_name)
{
    EXPECT_EQ(preprocess_source("#line 100\n__LINE__\n#define N 200\n#line N \"other.c\"\n"
                                "__LINE__ __FILE__\n\n__LINE__\n"),
              "100\n200 \"other.c\"\n202\n");
    expect_error("#line 10 \"named.c\"\n@\n#if\n", "named.c:11: #if");
    expect_error("#line 0\n", "#line needs a line number from 1 to 2147483647");
    expect_error("#line 2147483648\n", "#line needs a line number from 1 to 2147483647");
    expect_error("#line 0x10\n", "#line needs a line number from 1 to 2147483647");
    expect_error("#line 1 x\n", "expected a file name in double quotes");
}

TEST_F(Preprocessor, other_directives)
{
    // A # alone on its line is nothing; a # that a macro gives does not start a directive.
    EXPECT_EQ(preprocess_source("#\n# /* comment */\n#define H #\n#define EMPTY\nH define X 1\n"
                                "EMPTY # define Y 2\nX Y\n"),
              "# define X 1 # define Y 2\nX Y\n");
    expect_error("#bogus\n", "invalid preprocessing directive #bogus");
    expect_error("#error a  b\n", "#error a b");
    expect_error("#endif x\n", "#endif without #if");
    expect_error("_Pragma(x)\n", "_Pragma needs a string literal in parentheses");
    // A warning goes on.
    EXPECT_EQ(preprocess_source("#warning careful\nx\n"), "x\n");
}

TEST_F(Preprocessor, command_line_macros)
{
    m_command_line = "#define A 1\n#define B\n#undef __STDC_HOSTED__\n";
    EXPECT_EQ(preprocess_source("A B __STDC_HOSTED__\n"), "1 __STDC_HOSTED__\n");
    // Each call starts with only the predefined macros.
    m_command_line.clear();
    EXPECT_EQ(preprocess_source("A __STDC_HOSTED__\n"), "A 1\n");
}

TEST_F(Preprocessor, printed_tokens_are_not_glued_together)
{
    EXPECT_EQ(
        preprocess_source("#define P +\n#define E\n#define I(x) x\nP+ -E- I(1)I(2) x I(\"y\") "
                          "a/I(/)b .I(5) I(1e)I(+)2 I(L)I(\"s\")\n"),
        "+ + - - 1 2 x \"y\" a/ /b . 5 1e +2 L \"s\"\n");
}
