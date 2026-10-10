// Unit tests for the preprocessor. Each test preprocesses a small source and compares the .bi text
// (whitespace is normalized: no indentation, comments or blank lines, and one space between
// tokens that had whitespace between them). Errors are checked through their message.

#include "assembler_test/toolchain_fixture.h"

#include <utility>
#include <vector>

class PreprocessorUnit : public ToolchainFixture
{
  protected:
    /// The preprocessed text of `source`, written to `<name>` in the scratch directory.
    std::string preprocess(const std::string &source, const std::string &name = "main.basm")
    {
        const std::string path = write(name, source);
        Preprocessor preprocessor(File(path), (m_dir / "out" / (name + ".bi")).string(), m_options);
        const File output = preprocessor.preprocess();
        EXPECT_EQ(preprocessor.get_state(), Preprocessor::PROCESSED_SUCCESS);
        return read(output.get_path());
    }

    std::string error(const std::string &source, const std::string &name = "main.basm")
    {
        return error_of([&] { preprocess(source, name); });
    }

    using TypedText = std::vector<std::pair<basm::TokenType, std::string>>;

    static TypedText lexed(const std::string &text)
    {
        basm::SourceManager sources;
        basm::LexOptions options;
        options.collapse_newlines = true;
        const basm::LexResult result = basm::lex_text(sources, "relex", text, options);

        TypedText tokens;
        for (const basm::Token &token : result.tokens)
        {
            tokens.emplace_back(token.type, token.str());
        }
        return tokens;
    }
};

// ---------------------------------------------------------------------------------------------
// Plain text
// ---------------------------------------------------------------------------------------------

TEST_F(PreprocessorUnit, plain_source_is_normalized)
{
    EXPECT_EQ(preprocess("  .text\n_start:   ; entry\n    add x0, x1, 5   ; sum\n\n\n    hlt\n"),
              ".text\n_start:\nadd x0, x1, 5\nhlt\n");
}

TEST_F(PreprocessorUnit, empty_source_gives_empty_output)
{
    EXPECT_EQ(preprocess(""), "");
    EXPECT_EQ(preprocess("; only a comment\n"), "");
}

TEST_F(PreprocessorUnit, missing_final_newline_is_added)
{
    EXPECT_EQ(preprocess("hlt"), "hlt\n");
}

TEST_F(PreprocessorUnit, comments_are_removed)
{
    EXPECT_EQ(preprocess(";* block\ncomment *;\nhlt ; done\n"), "hlt\n");
}

TEST_F(PreprocessorUnit, labels_keep_their_colon_and_branch_conditions_stay_attached)
{
    EXPECT_EQ(preprocess("loop: b.eq loop\n"), "loop: b.eq loop\n");
}

TEST_F(PreprocessorUnit, strings_and_characters_are_not_touched)
{
    EXPECT_EQ(preprocess("#define N 4\n.asciz \"N ; not a comment\"\n.byte ';', 'N'\n"),
              ".asciz \"N ; not a comment\"\n.byte ';', 'N'\n");
}

TEST_F(PreprocessorUnit, output_lexes_like_the_input)
{
    // Spacing decides how some of these lex (`.5` is a float, `10 .5` is not), so it has to survive.
    const std::string source = "mov x0, 0b101\n"
                               "add x1, x1, 0xFF\n"
                               "ldr x0, [sp, -4]!\n"
                               "ldrsb x2, [x1], 1\n"
                               "adds x0, x0, :lo12:buf\n"
                               "b.ne loop\n"
                               ".word 10 % 3, 0o17, 1<<2\n"
                               "fadd.f32 x0, x1\n";
    EXPECT_EQ(lexed(preprocess(source)), lexed(source));
}

// ---------------------------------------------------------------------------------------------
// #define / #undef
// ---------------------------------------------------------------------------------------------

TEST_F(PreprocessorUnit, define_without_parameters)
{
    EXPECT_EQ(preprocess("#define N 4\nmov x0, N\n"), "mov x0, 4\n");
}

TEST_F(PreprocessorUnit, define_with_empty_value)
{
    EXPECT_EQ(preprocess("#define NOTHING\nhlt NOTHING\n"), "hlt\n");
}

TEST_F(PreprocessorUnit, define_is_looked_up_when_it_is_used)
{
    EXPECT_EQ(preprocess("#define A B\n#define B 7\n.word A\n"), ".word 7\n");
}

// The -D flags of the command line.
TEST_F(PreprocessorUnit, symbols_defined_by_the_options)
{
    m_options.defines = {{"N", "4"}, {"FLAG", ""}, {"EXPR", "1 + 2"}};
    EXPECT_EQ(preprocess("mov x0, N\n.word EXPR\n"), "mov x0, 4\n.word 1 + 2\n");
    EXPECT_EQ(preprocess("#ifdef FLAG\nhlt\n#else\nnop\n#endif\n"), "hlt\n");
}

TEST_F(PreprocessorUnit, the_source_can_redefine_a_symbol_of_the_options)
{
    m_options.defines = {{"N", "4"}};
    EXPECT_EQ(preprocess("#define N 9\nmov x0, N\n"), "mov x0, 9\n");
    EXPECT_EQ(preprocess("#undef N\n#ifdef N\nhlt\n#else\nnop\n#endif\n"), "nop\n");
}

TEST_F(PreprocessorUnit, a_symbol_of_the_options_with_a_bad_name_is_an_error)
{
    // An instruction is not a symbol, and neither is something that does not even lex.
    m_options.defines = {{"mov", "4"}};
    EXPECT_TRUE(contains(error("nop\n"), "expected a symbol after #define"));

    m_options.defines = {{"1bad", "4"}};
    EXPECT_TRUE(contains(error("nop\n"), "lexical error"));
}

TEST_F(PreprocessorUnit, define_with_parameters)
{
    // Parameter names cannot be keywords, and `b` is the branch instruction.
    EXPECT_EQ(preprocess("#define ADD(p, q) p + q\n.word ADD(1, 2)\n"), ".word 1 + 2\n");
}

TEST_F(PreprocessorUnit, define_parameter_list_must_touch_the_name)
{
    // `F (x)` is a definition without parameters whose value is `(x)`.
    EXPECT_EQ(preprocess("#define F (x)\n.word F\n"), ".word (x)\n");
}

TEST_F(PreprocessorUnit, define_arguments_can_hold_commas_inside_brackets)
{
    EXPECT_EQ(preprocess("#define FIRST(p, q) p\n.word FIRST((1, 2), 3)\n"), ".word (1, 2)\n");
    EXPECT_EQ(preprocess("#define SECOND(p, q) q\nldr x0, SECOND(0, [x1, 4])\n"),
              "ldr x0, [x1, 4]\n");
}

TEST_F(PreprocessorUnit, define_arguments_are_replaced_once)
{
    // The value of an argument that is also a parameter name is not substituted again.
    EXPECT_EQ(preprocess("#define SWAP(p, q) q p\n.word SWAP(q, p)\n"), ".word p q\n");
}

TEST_F(PreprocessorUnit, define_empty_parameter_list)
{
    EXPECT_EQ(preprocess("#define ZERO() 0\n.word ZERO()\n"), ".word 0\n");
}

TEST_F(PreprocessorUnit, define_overloaded_by_parameter_count)
{
    EXPECT_EQ(preprocess("#define M(p) one\n#define M(p, q) two\n.word M(1)\n.word M(1, 2)\n"),
              ".word one\n.word two\n");
}

TEST_F(PreprocessorUnit, define_redefinition_replaces_the_value)
{
    EXPECT_EQ(preprocess("#define X 1\n#define X 2\n.word X\n"), ".word 2\n");
}

TEST_F(PreprocessorUnit, define_continues_on_the_next_line)
{
    EXPECT_EQ(preprocess("#define ADD(p, q) \\\n    add x0, p, q\nADD(1, 2)\n"), "add x0, 1, 2\n");
}

TEST_F(PreprocessorUnit, define_function_like_symbol_without_parentheses_is_left_alone)
{
    EXPECT_EQ(preprocess("#define F(a) a\n.word F\n"), ".word F\n");
}

TEST_F(PreprocessorUnit, define_does_not_replace_labels)
{
    EXPECT_EQ(preprocess("#define N 4\nN:\n"), "N:\n");
}

TEST_F(PreprocessorUnit, undef_removes_every_definition)
{
    EXPECT_EQ(preprocess("#define X 1\n#define X(a) 2\n#undef X\n.word X\n"), ".word X\n");
}

TEST_F(PreprocessorUnit, undef_with_parameter_count_removes_one_definition)
{
    EXPECT_EQ(preprocess("#define X 1\n#define X(a) 2\n#undef X 0\n.word X\n.word X(0)\n"),
              ".word X\n.word 2\n");
}

TEST_F(PreprocessorUnit, undef_of_unknown_symbol_is_fine)
{
    EXPECT_EQ(preprocess("#undef NOPE\n#undef NOPE 3\nhlt\n"), "hlt\n");
}

TEST_F(PreprocessorUnit, define_that_refers_to_itself_is_an_error)
{
    EXPECT_TRUE(contains(error("#define A A\n.word A\n"), "'A' expands too deeply"));
}

TEST_F(PreprocessorUnit, define_errors)
{
    EXPECT_TRUE(contains(error("#define\n"), "expected a symbol after #define, got end of line"));
    EXPECT_TRUE(contains(error("#define add 1\n"), "expected a symbol after #define, got 'add'"));
    EXPECT_TRUE(contains(error("#define F(a, a) a\n"), "duplicate parameter 'a'"));
    EXPECT_TRUE(contains(error("#define F(a b) a\n"), "expected ',' or ')'"));
    EXPECT_TRUE(contains(error("#define F(1) a\n"), "expected a parameter name, got '1'"));
    EXPECT_TRUE(contains(error("#define F(a) a\n.word F(1, 2)\n"),
                         "'F' is not defined with 2 argument(s)"));
    EXPECT_TRUE(
        contains(error("#define F(a) a\n.word F(1\n"), "missing ')' after the arguments of 'F'"));
    EXPECT_TRUE(contains(error("#undef\n"), "expected a symbol after #undef"));
    EXPECT_TRUE(contains(error("#undef X Y\n"), "unexpected 'Y' after #undef"));
}

// ---------------------------------------------------------------------------------------------
// Conditionals
// ---------------------------------------------------------------------------------------------

TEST_F(PreprocessorUnit, ifdef_and_ifndef)
{
    const std::string defined = "#define A\n";
    EXPECT_EQ(preprocess(defined + "#ifdef A\n.word 1\n#else\n.word 2\n#endif\n"), ".word 1\n");
    EXPECT_EQ(preprocess(defined + "#ifndef A\n.word 1\n#else\n.word 2\n#endif\n"), ".word 2\n");
    EXPECT_EQ(preprocess("#ifdef A\n.word 1\n#endif\nhlt\n"), "hlt\n");
    EXPECT_EQ(preprocess("#ifndef A\n.word 1\n#endif\nhlt\n"), ".word 1\nhlt\n");
}

TEST_F(PreprocessorUnit, first_true_branch_of_a_chain_is_taken)
{
    EXPECT_EQ(preprocess("#define MODE 4\n"
                         "#ifequ MODE 3\n.word 3\n"
                         "#elseequ MODE 4\n.word 4\n"
                         "#elseequ MODE 4\n.word 44\n"
                         "#else\n.word 0\n"
                         "#endif\n"),
              ".word 4\n");
}

TEST_F(PreprocessorUnit, else_branch_is_taken_when_nothing_else_was)
{
    EXPECT_EQ(preprocess("#define MODE 9\n"
                         "#ifequ MODE 3\n.word 3\n"
                         "#elsedef NOPE\n.word 4\n"
                         "#else\n.word 0\n"
                         "#endif\n"),
              ".word 0\n");
}

TEST_F(PreprocessorUnit, value_comparisons)
{
    const std::string define = "#define V 5\n";
    EXPECT_EQ(preprocess(define + "#ifnequ V 5\n.word 1\n#elseless V 6\n.word 2\n#endif\n"),
              ".word 2\n");
    EXPECT_EQ(preprocess(define + "#ifmore V 4\n.word 1\n#endif\n"), ".word 1\n");
    EXPECT_EQ(preprocess(define + "#ifless V 4\n.word 1\n#elsemore V 4\n.word 2\n#endif\n"),
              ".word 2\n");
}

// Numbers are compared as numbers: as text "9" would be more than "10".
TEST_F(PreprocessorUnit, numbers_are_compared_by_value)
{
    EXPECT_EQ(preprocess("#define V 9\n#ifless V 10\n.word 1\n#endif\n"), ".word 1\n");
    EXPECT_EQ(preprocess("#define V 9\n#ifmore V 10\n.word 1\n#endif\n"), "");
    EXPECT_EQ(preprocess("#define V 10\n#ifmore V 9\n.word 1\n#endif\n"), ".word 1\n");
    EXPECT_EQ(preprocess("#define V 0xA\n#ifequ V 10\n.word 1\n#endif\n"), ".word 1\n");
    EXPECT_EQ(preprocess("#define V 0b1010\n#ifequ V 10\n.word 1\n#endif\n"), ".word 1\n");
    EXPECT_EQ(preprocess("#define V 0o12\n#ifequ V 10\n.word 1\n#endif\n"), ".word 1\n");
    EXPECT_EQ(preprocess("#define V -0x10\n#ifless V 0\n.word 1\n#endif\n"), ".word 1\n");
    EXPECT_EQ(preprocess("#define V -2\n#ifless V 1\n.word 1\n#endif\n"), ".word 1\n");
    EXPECT_EQ(preprocess("#define V 010\n#ifequ V 10\n.word 1\n#endif\n"), ".word 1\n");
}

TEST_F(PreprocessorUnit, comparisons_are_on_the_text_of_the_value)
{
    // Whitespace between tokens counts as one space, none stays none.
    EXPECT_EQ(preprocess("#define E 1 + 2\n#ifequ E 1  +   2\n.word 1\n#endif\n"), ".word 1\n");
    EXPECT_EQ(preprocess("#define E 1 + 2\n#ifequ E 1+2\n.word 1\n#endif\n"), "");
}

TEST_F(PreprocessorUnit, undefined_symbol_has_the_empty_value)
{
    EXPECT_EQ(preprocess("#ifequ NOPE\n.word 1\n#endif\n"), ".word 1\n");
    EXPECT_EQ(preprocess("#ifequ NOPE 3\n.word 1\n#endif\n"), "");
}

TEST_F(PreprocessorUnit, blocks_nest_and_dead_blocks_are_not_looked_at)
{
    // The `#ifdef` with no symbol is in a branch that is not taken, so it is not an error.
    EXPECT_EQ(preprocess("#define A\n"
                         "#ifdef A\n"
                         "#ifdef B\n.word 1\n#else\n.word 2\n#endif\n"
                         "#else\n"
                         "#ifdef\n.word 3\n#endif\n"
                         "#endif\n"),
              ".word 2\n");
}

TEST_F(PreprocessorUnit, nothing_in_a_dead_branch_happens)
{
    EXPECT_EQ(preprocess("#ifdef NOPE\n#define X 1\n#include \"missing.binc\"\n#invoke nope()\n"
                         "#endif\n#ifdef X\n.word 1\n#else\n.word 2\n#endif\n"),
              ".word 2\n");
}

TEST_F(PreprocessorUnit, conditional_errors)
{
    EXPECT_TRUE(contains(error("#endif\n"), "#endif without a matching #if"));
    EXPECT_TRUE(contains(error("#else\n"), "#else without a matching #if"));
    EXPECT_TRUE(contains(error("#elsedef A\n"), "#elsedef without a matching #if"));
    EXPECT_TRUE(contains(error("#ifdef A\n.word 1\n"), "conditional block is never closed"));
    EXPECT_TRUE(contains(error("#ifdef A\n#else\n#elsedef A\n#endif\n"), "after #else"));
    EXPECT_TRUE(contains(error("#ifdef A\n#else\n#else\n#endif\n"), "after #else"));
    EXPECT_TRUE(
        contains(error("#ifdef\n#endif\n"), "expected a symbol after #ifdef, got end of line"));
    EXPECT_TRUE(contains(error("#ifdef 5\n#endif\n"), "expected a symbol after #ifdef, got '5'"));
    EXPECT_TRUE(contains(error("#ifdef A B\n#endif\n"), "unexpected 'B' after #ifdef"));
    EXPECT_TRUE(contains(error("#ifdef A\n#else x\n#endif\n"), "unexpected 'x' after #else"));
    EXPECT_TRUE(contains(error("#ifdef A\n#endif x\n"), "unexpected 'x' after #endif"));
}

// ---------------------------------------------------------------------------------------------
// Macros
// ---------------------------------------------------------------------------------------------

TEST_F(PreprocessorUnit, macro_expands_inside_a_scope)
{
    EXPECT_EQ(preprocess("#macro load(a)\nmov x0, a\n#macend\n#invoke load(5)\nhlt\n"),
              ".scope\nmov x0, 5\n.scend\nhlt\n");
}

TEST_F(PreprocessorUnit, macro_without_parameters)
{
    EXPECT_EQ(preprocess("#macro nothing()\nnop\n#macend\n#invoke nothing()\n"),
              ".scope\nnop\n.scend\n");
}

TEST_F(PreprocessorUnit, macro_is_not_expanded_where_it_is_defined)
{
    EXPECT_EQ(preprocess("#macro m()\nnop\n#macend\nhlt\n"), "hlt\n");
}

TEST_F(PreprocessorUnit, macro_arguments_replace_parameters)
{
    EXPECT_EQ(preprocess("#macro add3(d, x, y)\nadd d, x, y\n#macend\n#invoke add3(x0, x1, 3)\n"),
              ".scope\nadd x0, x1, 3\n.scend\n");
}

TEST_F(PreprocessorUnit, macro_arguments_can_hold_commas_inside_brackets)
{
    EXPECT_EQ(preprocess("#macro get(a)\nldr x0, a\n#macend\n#invoke get([x1, 4])\n"),
              ".scope\nldr x0, [x1, 4]\n.scend\n");
}

TEST_F(PreprocessorUnit, macro_overloaded_by_parameter_count)
{
    EXPECT_EQ(preprocess("#macro m(p)\n.word 1\n#macend\n#macro m(p, q)\n.word 2\n#macend\n"
                         "#invoke m(0)\n#invoke m(0, 0)\n"),
              ".scope\n.word 1\n.scend\n.scope\n.word 2\n.scend\n");
}

TEST_F(PreprocessorUnit, macro_parameter_hides_a_define_of_the_same_name)
{
    EXPECT_EQ(preprocess("#define va 5\n#macro m(va)\nmov x0, va\n#macend\n"
                         "#invoke m(7)\nmov x1, va\n"),
              ".scope\nmov x0, 7\n.scend\nmov x1, 5\n");
}

TEST_F(PreprocessorUnit, macro_body_sees_definitions_made_after_it)
{
    EXPECT_EQ(preprocess("#macro m()\n.word N\n#macend\n#define N 1\n#invoke m()\n"),
              ".scope\n.word 1\n.scend\n");
}

TEST_F(PreprocessorUnit, macro_can_invoke_a_macro)
{
    EXPECT_EQ(preprocess("#macro inner(v)\nadd x0, x0, v\n#macend\n"
                         "#macro outer(w)\n#invoke inner(w)\n#macend\n#invoke outer(3)\n"),
              ".scope\n.scope\nadd x0, x0, 3\n.scend\n.scend\n");
}

TEST_F(PreprocessorUnit, macro_body_can_define_symbols)
{
    EXPECT_EQ(
        preprocess("#macro m(a)\n#define TMP a\n.word TMP\n#macend\n#invoke m(9)\n.word TMP\n"),
        ".scope\n.word 9\n.scend\n.word 9\n");
}

TEST_F(PreprocessorUnit, macret_sets_the_output_symbol)
{
    EXPECT_EQ(preprocess("#macro sum(p, q)\n#macret p + q\n#macend\n"
                         "#invoke sum(1, 9) total\nmov x0, total\n"),
              ".scope\n.scend\nmov x0, 1 + 9\n");
}

TEST_F(PreprocessorUnit, macret_leaves_the_macro)
{
    EXPECT_EQ(preprocess("#macro m(a)\n#macret a\n.word 99\n#macend\n#invoke m(5) r\n.word r\n"),
              ".scope\n.scend\n.word 5\n");
}

TEST_F(PreprocessorUnit, macret_leaves_open_conditionals_of_the_macro)
{
    EXPECT_EQ(preprocess("#macro m()\n#ifndef NOPE\n#macret 1\n.word 99\n#endif\n#macend\n"
                         "#invoke m() r\n.word r\n"),
              ".scope\n.scend\n.word 1\n");
}

TEST_F(PreprocessorUnit, macret_without_a_value_is_allowed)
{
    EXPECT_EQ(preprocess("#macro m()\nnop\n#macret\n.word 99\n#macend\n#invoke m()\n"),
              ".scope\nnop\n.scend\n");
}

TEST_F(PreprocessorUnit, macro_parameter_can_be_compared_in_a_conditional)
{
    EXPECT_EQ(preprocess("#macro pick(mode)\n#ifequ mode 1\nmov x0, 10\n#else\nmov x0, 20\n"
                         "#endif\n#macend\n#invoke pick(1)\n#invoke pick(2)\n"),
              ".scope\nmov x0, 10\n.scend\n.scope\nmov x0, 20\n.scend\n");
}

TEST_F(PreprocessorUnit, macro_errors)
{
    EXPECT_TRUE(
        contains(error("#invoke nope()\n"), "no macro 'nope' is defined with 0 argument(s)"));
    EXPECT_TRUE(contains(error("#macro m(a)\nhlt\n#macend\n#invoke m()\n"),
                         "no macro 'm' is defined with 0 argument(s)"));
    EXPECT_TRUE(contains(error("#macro m(a)\n#macend\n#macro m(q)\n#macend\n"),
                         "macro 'm' with 1 parameter(s) is already defined"));
    EXPECT_TRUE(contains(error("#macro m()\nhlt\n"), "#macro without a matching #macend"));
    EXPECT_TRUE(contains(error("#macro m()\n#macro n()\n#macend\n#macend\n"),
                         "macro definitions cannot be nested"));
    EXPECT_TRUE(contains(error("#macend\n"), "#macend without a matching #macro"));
    EXPECT_TRUE(contains(error("#macret 1\n"), "#macret outside of a macro"));
    EXPECT_TRUE(contains(error("#macro m()\n#macret 1\n#macend\n#invoke m()\n"),
                         "#macret has a value but the #invoke has no symbol to put it in"));
    EXPECT_TRUE(contains(error("#macro m()\n#invoke m()\n#macend\n#invoke m()\n"),
                         "macro expansion nested too deeply"));
    EXPECT_TRUE(contains(error("#macro\n"), "expected a macro name after #macro"));
    EXPECT_TRUE(contains(error("#macro m\n"), "expected '(' after the macro name"));
    EXPECT_TRUE(
        contains(error("#macro m() x\n#macend\n"), "unexpected 'x' after the macro header"));
    EXPECT_TRUE(contains(error("#macro m()\n#macend x\n"), "unexpected 'x' after #macend"));
    EXPECT_TRUE(contains(error("#macro m()\n#macend\n#invoke m(1\n"),
                         "missing ')' after the macro arguments"));
    EXPECT_TRUE(contains(error("#macro m()\n#macend\n#invoke m() r s\n"),
                         "unexpected 's' after the macro arguments"));
    EXPECT_TRUE(contains(error("#invoke\n"), "expected a macro name after #invoke"));
}

TEST_F(PreprocessorUnit, conditional_cannot_stay_open_across_a_macro)
{
    EXPECT_TRUE(contains(error("#macro m()\n#ifdef A\n#macend\n#invoke m()\n#endif\n"),
                         "conditional block is never closed"));
}

// ---------------------------------------------------------------------------------------------
// #include
// ---------------------------------------------------------------------------------------------

TEST_F(PreprocessorUnit, include_relative_to_the_file)
{
    write("lib/defs.binc", "#define VALUE 42\n");
    EXPECT_EQ(preprocess("#include \"lib/defs.binc\"\n.word VALUE\n"), ".word 42\n");
}

TEST_F(PreprocessorUnit, include_inside_an_included_file_is_relative_to_that_file)
{
    write("lib/a.binc", "#include \"b.binc\"\n.word 1\n");
    write("lib/b.binc", ".word 2\n");
    EXPECT_EQ(preprocess("#include \"lib/a.binc\"\n"), ".word 2\n.word 1\n");
}

TEST_F(PreprocessorUnit, include_continues_where_it_was_included)
{
    write("x.binc", "add x0, x0, 1\n");
    EXPECT_EQ(preprocess("hlt\n#include \"x.binc\"\nnop\n"), "hlt\nadd x0, x0, 1\nnop\n");
}

TEST_F(PreprocessorUnit, include_from_the_include_directories)
{
    write("include/util.binc", "#define FROM_INCLUDE 7\n");
    EXPECT_EQ(preprocess("#include <\"util.binc\">\n.word FROM_INCLUDE\n"), ".word 7\n");
}

TEST_F(PreprocessorUnit, the_same_include_directory_twice_is_not_an_ambiguity)
{
    write("include/util.binc", "#define FROM_INCLUDE 7\n");
    const std::string dir = (m_dir / "include").string();
    m_options.system_dirs = {Directory(dir), Directory(dir), Directory(dir + "/../include")};
    EXPECT_EQ(preprocess("#include <\"util.binc\">\n.word FROM_INCLUDE\n"), ".word 7\n");
}

TEST_F(PreprocessorUnit, a_file_in_two_include_directories_is_an_ambiguity)
{
    write("include/util.binc", "#define FROM_INCLUDE 7\n");
    write("other/util.binc", "#define FROM_INCLUDE 8\n");
    m_options.system_dirs = {Directory((m_dir / "include").string()),
                             Directory((m_dir / "other").string())};
    EXPECT_TRUE(contains(error("#include <\"util.binc\">\n"),
                         "'util.binc' found in more than one include directory"));
}

TEST_F(PreprocessorUnit, include_guard_keeps_a_file_from_being_included_twice)
{
    write("guard.binc", "#ifndef GUARD\n#define GUARD\n.word 1\n#endif\n");
    EXPECT_EQ(preprocess("#include \"guard.binc\"\n#include \"guard.binc\"\n"), ".word 1\n");
}

TEST_F(PreprocessorUnit, included_file_can_define_macros)
{
    write("macros.binc", "#macro m()\nnop\n#macend\n");
    EXPECT_EQ(preprocess("#include \"macros.binc\"\n#invoke m()\n"), ".scope\nnop\n.scend\n");
}

TEST_F(PreprocessorUnit, include_errors)
{
    EXPECT_TRUE(contains(error("#include \"nope.binc\"\n"), "does not exist"));
    EXPECT_TRUE(contains(error("#include <\"nope.binc\">\n"),
                         "'nope.binc' not found in the include directories"));
    EXPECT_TRUE(contains(error("#include foo\n"), "#include expects \"file\" or <\"file\">"));
    EXPECT_TRUE(contains(error("#include\n"), "#include expects \"file\" or <\"file\">"));
    EXPECT_TRUE(contains(error("#include <\"a.binc\"\n"), "expected '>' after the file name"));
    EXPECT_TRUE(contains(error("#include \"a.binc\" x\n"), "unexpected 'x' after #include"));
}

TEST_F(PreprocessorUnit, include_that_includes_itself_is_an_error)
{
    write("loop.binc", "#include \"loop.binc\"\n");
    EXPECT_TRUE(contains(error("#include \"loop.binc\"\n"), "#include nested too deeply"));
}

TEST_F(PreprocessorUnit, conditional_cannot_stay_open_across_files)
{
    write("open.binc", "#ifdef A\n");
    const std::string message = error("#include \"open.binc\"\n#endif\n");
    EXPECT_TRUE(contains(message, "open.binc:1:1"));
    EXPECT_TRUE(contains(message, "conditional block is never closed"));
}

// ---------------------------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------------------------

TEST_F(PreprocessorUnit, errors_name_the_file_line_and_column)
{
    const std::string message = error("hlt\n  #define\n");
    EXPECT_TRUE(contains(message, "main.basm:2:3: error: expected a symbol after #define"));

    // The source line and a caret under the column.
    EXPECT_TRUE(contains(message, "  #define\n    ^"));
}

TEST_F(PreprocessorUnit, errors_in_an_included_file_name_that_file)
{
    write("bad.binc", "hlt\n#undef\n");
    EXPECT_TRUE(contains(error("#include \"bad.binc\"\n"), "bad.binc:2:1"));
}

TEST_F(PreprocessorUnit, lexical_errors_end_the_preprocessor)
{
    EXPECT_TRUE(contains(error("mov x0, $G1\n"), "lexical error"));
    EXPECT_TRUE(contains(error(".asciz \"never closed\n"), "lexical error"));
}

TEST_F(PreprocessorUnit, state_is_error_after_a_failure)
{
    const std::string path = write("fail.basm", "#endif\n");
    Preprocessor preprocessor(File(path), (m_dir / "out" / "fail.bi").string(), m_options);
    EXPECT_EQ(preprocessor.get_state(), Preprocessor::UNPROCESSED);
    EXPECT_THROW(preprocessor.preprocess(), aemu::log::FatalError);
    EXPECT_EQ(preprocessor.get_state(), Preprocessor::PROCESSED_ERROR);
}

// ---------------------------------------------------------------------------------------------
// Tokens handed to the assembler
// ---------------------------------------------------------------------------------------------

namespace
{

std::string texts(const std::vector<basm::Token> &tokens)
{
    std::string out;
    for (const basm::Token &token : tokens)
    {
        if (token.is(basm::TokenType::NEWLINE)) out += "|";
        else if (!token.is(basm::TokenType::END_OF_FILE)) out += token.str() + " ";
    }
    return out;
}

} // namespace

TEST_F(PreprocessorUnit, result_tokens_end_with_a_newline_and_end_of_file)
{
    const std::string path = write("main.basm", "hlt");
    Preprocessor preprocessor(File(path), (m_dir / "out" / "main.bi").string(), m_options);
    preprocessor.preprocess();
    const basm::PreprocessedSource result = preprocessor.take_result();

    ASSERT_GE(result.tokens.size(), 3u);
    EXPECT_EQ(result.tokens[result.tokens.size() - 2].type, basm::TokenType::NEWLINE);
    EXPECT_EQ(result.tokens.back().type, basm::TokenType::END_OF_FILE);

    // An empty program is just the end.
    const std::string empty_path = write("empty.basm", "; nothing\n");
    Preprocessor empty(File(empty_path), (m_dir / "out" / "empty.bi").string(), m_options);
    empty.preprocess();
    EXPECT_EQ(empty.take_result().tokens.back().type, basm::TokenType::END_OF_FILE);
}

TEST_F(PreprocessorUnit, result_tokens_keep_their_original_locations)
{
    write("inc.binc", "nop\n");
    const std::string path =
        write("main.basm", "#define N 4\n\n  mov x0, N\n#include \"inc.binc\"\n");
    Preprocessor preprocessor(File(path), (m_dir / "out" / "main.bi").string(), m_options);
    preprocessor.preprocess();
    const basm::PreprocessedSource result = preprocessor.take_result();
    EXPECT_EQ(texts(result.tokens), "mov x0 , 4 |nop |");

    const basm::Token &mov = result.tokens[0];
    EXPECT_TRUE(contains(std::string(result.sources->name(mov.loc.source)), "main.basm"));
    EXPECT_EQ(mov.loc.line, 3u);
    EXPECT_EQ(mov.loc.column, 3u);
    EXPECT_EQ(mov.loc.expansion, 0u);

    const basm::Token &nop = result.tokens[6];
    EXPECT_TRUE(contains(std::string(result.sources->name(nop.loc.source)), "inc.binc"));
    EXPECT_EQ(nop.loc.line, 1u);
}

TEST_F(PreprocessorUnit, result_tokens_name_the_symbol_that_produced_them)
{
    const std::string path = write("main.basm", "#define N 4\nmov x0, N\n");
    Preprocessor preprocessor(File(path), (m_dir / "out" / "main.bi").string(), m_options);
    preprocessor.preprocess();
    const basm::PreprocessedSource result = preprocessor.take_result();

    const basm::Token &four = result.tokens[3];
    ASSERT_EQ(four.str(), "4");
    EXPECT_EQ(four.loc.line, 1u); // where the value was written
    const basm::SourceManager::Expansion *expansion = result.sources->expansion(four.loc.expansion);
    ASSERT_NE(expansion, nullptr);
    EXPECT_EQ(expansion->name, "N");
    EXPECT_FALSE(expansion->is_macro);
    EXPECT_EQ(expansion->site.line, 2u); // where it was used
    EXPECT_EQ(expansion->site.column, 9u);
}

TEST_F(PreprocessorUnit, macro_arguments_keep_the_location_they_were_written_at)
{
    const std::string path = write("main.basm", "#macro m(r)\nmov r, 1\n#macend\n#invoke m(x5)\n");
    Preprocessor preprocessor(File(path), (m_dir / "out" / "main.bi").string(), m_options);
    preprocessor.preprocess();
    const basm::PreprocessedSource result = preprocessor.take_result();

    const basm::Token *mov = nullptr;
    const basm::Token *x5 = nullptr;
    for (const basm::Token &token : result.tokens)
    {
        if (token.is(basm::TokenType::INSTRUCTION_MOV)) mov = &token;
        if (token.is(basm::TokenType::REGISTER_X5)) x5 = &token;
    }
    ASSERT_NE(mov, nullptr);
    ASSERT_NE(x5, nullptr);

    // The body belongs to the macro, the argument to the #invoke line.
    EXPECT_EQ(mov->loc.line, 2u);
    ASSERT_NE(result.sources->expansion(mov->loc.expansion), nullptr);
    EXPECT_EQ(result.sources->expansion(mov->loc.expansion)->name, "m");
    EXPECT_TRUE(result.sources->expansion(mov->loc.expansion)->is_macro);
    EXPECT_EQ(result.sources->expansion(mov->loc.expansion)->site.line, 4u);
    EXPECT_EQ(x5->loc.line, 4u);
    EXPECT_EQ(x5->loc.expansion, 0u);
}

TEST_F(PreprocessorUnit, result_tokens_match_the_text_of_the_output_file)
{
    const std::string path = write("main.basm", "#macro twice(r)\nadd r, r, r\n#macend\n"
                                                "#define K 3\n.text\n_start:\n  #invoke twice(x1)\n"
                                                "  mov x2, K\n#ifdef K\n  hlt\n#endif\n");
    Preprocessor preprocessor(File(path), (m_dir / "out" / "main.bi").string(), m_options);
    const File output = preprocessor.preprocess();
    const basm::PreprocessedSource result = preprocessor.take_result();

    // Lexing the .bi gives the same tokens as the in memory result.
    basm::SourceManager sources;
    const basm::LexResult relexed = basm::lex_text(sources, "relex", read(output.get_path()));
    ASSERT_EQ(relexed.tokens.size(), result.tokens.size());
    for (std::size_t i = 0; i < result.tokens.size(); i++)
    {
        EXPECT_EQ(relexed.tokens[i].type, result.tokens[i].type) << i;
        EXPECT_EQ(relexed.tokens[i].text, result.tokens[i].text) << i;
    }
}
