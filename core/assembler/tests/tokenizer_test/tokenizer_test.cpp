#include "assembler/tokenizer.h"
#include "util/logger.h"

#include <gtest/gtest.h>

using namespace basm;
using T = TokenType;

namespace
{

std::vector<TokenType> types(const LexResult &r)
{
    std::vector<TokenType> out;
    for (const Token &t : r.tokens)
    {
        out.push_back(t.type);
    }
    return out;
}

LexResult lex_asm(SourceManager &sm, const std::string &text, LexOptions opts = {})
{
    return lex_text(sm, "test.basm", text, opts);
}

LexResult lex_ld(SourceManager &sm, const std::string &text)
{
    LexOptions opts;
    opts.mode = LexMode::LINKER_SCRIPT;
    return lex_text(sm, "test.ld", text, opts);
}

} // namespace

TEST(tokenizer_v2, instruction_line_with_newline_and_eof)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "add x0, x1, 5\n");

    EXPECT_FALSE(r.has_errors);
    EXPECT_EQ(types(r), (std::vector<TokenType>{T::INSTRUCTION_ADD, T::REGISTER_X0, T::COMMA,
                                                T::REGISTER_X1, T::COMMA, T::LITERAL_NUMBER_DECIMAL,
                                                T::NEWLINE, T::END_OF_FILE}));
    EXPECT_EQ(r.tokens[5].int_value, 5u);
}

TEST(tokenizer_v2, missing_final_newline_is_synthesized)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "hlt");
    EXPECT_EQ(types(r), (std::vector<TokenType>{T::INSTRUCTION_HLT, T::NEWLINE, T::END_OF_FILE}));
}

TEST(tokenizer_v2, empty_source_is_just_eof)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "");
    EXPECT_EQ(types(r), (std::vector<TokenType>{T::END_OF_FILE}));
}

TEST(tokenizer_v2, exact_locations)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "  add x0, x1\n\tsub x2\n");

    EXPECT_EQ(r.tokens[0].loc.line, 1u);
    EXPECT_EQ(r.tokens[0].loc.column, 3u);
    EXPECT_EQ(r.tokens[1].loc.column, 7u);

    // second line, tab counts as one byte
    const Token &sub = r.tokens[5];
    ASSERT_EQ(sub.type, T::INSTRUCTION_SUB);
    EXPECT_EQ(sub.loc.line, 2u);
    EXPECT_EQ(sub.loc.column, 2u);
    EXPECT_TRUE(sub.has(FIRST_ON_LINE));
    EXPECT_TRUE(sub.has(SPACE_BEFORE));
    EXPECT_EQ(sm.line_text(sub.loc.source, 2), "\tsub x2");
}

TEST(tokenizer_v2, crlf_and_cr_line_endings)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "hlt\r\nnop\rhlt\n");
    EXPECT_EQ(types(r),
              (std::vector<TokenType>{T::INSTRUCTION_HLT, T::NEWLINE, T::INSTRUCTION_NOP,
                                      T::NEWLINE, T::INSTRUCTION_HLT, T::NEWLINE, T::END_OF_FILE}));
    EXPECT_EQ(r.tokens[2].loc.line, 2u);
    EXPECT_EQ(r.tokens[4].loc.line, 3u);
    EXPECT_EQ(r.tokens[4].loc.column, 1u);
}

TEST(tokenizer_v2, collapse_newlines_option)
{
    SourceManager sm;
    LexOptions opts;
    opts.collapse_newlines = true;
    const LexResult r = lex_asm(sm, "\n\nhlt\n\n\nnop\n", opts);
    EXPECT_EQ(types(r), (std::vector<TokenType>{T::INSTRUCTION_HLT, T::NEWLINE, T::INSTRUCTION_NOP,
                                                T::NEWLINE, T::END_OF_FILE}));
    EXPECT_EQ(r.tokens[2].loc.line, 6u);
}

TEST(tokenizer_v2, labels_and_contextual_conditions)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "loop:\n  b.eq loop\n  add x0, x0, eq\n");

    ASSERT_FALSE(r.has_errors);
    EXPECT_EQ(r.tokens[0].type, T::LABEL);
    EXPECT_EQ(r.tokens[0].text, "loop");

    // b . eq loop
    EXPECT_EQ(r.tokens[2].type, T::INSTRUCTION_B);
    EXPECT_EQ(r.tokens[3].type, T::PERIOD);
    EXPECT_EQ(r.tokens[4].type, T::CONDITION_EQ);
    EXPECT_EQ(r.tokens[5].type, T::SYMBOL);

    // `eq` outside of a branch condition is an ordinary symbol
    EXPECT_EQ(r.tokens[r.tokens.size() - 3].type, T::SYMBOL);
    EXPECT_EQ(r.tokens[r.tokens.size() - 3].text, "eq");
}

TEST(tokenizer_v2, keyword_named_label)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "b:\n");
    EXPECT_EQ(r.tokens[0].type, T::LABEL);
    EXPECT_EQ(r.tokens[0].text, "b");
}

TEST(tokenizer_v2, instruction_suffix_flags)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "adds x0, x1, x2\nldrsb x0, [x1]\nadd x0, x1, x2\n");

    EXPECT_EQ(r.tokens[0].type, T::INSTRUCTION_ADD);
    EXPECT_TRUE(r.tokens[0].has(SETS_FLAGS));

    const Token &ldrsb = r.tokens[7];
    EXPECT_EQ(ldrsb.type, T::INSTRUCTION_LDRB);
    EXPECT_TRUE(ldrsb.has(SIGN_EXTEND));
    EXPECT_FALSE(ldrsb.has(SETS_FLAGS));

    const Token &add = r.tokens[14];
    EXPECT_EQ(add.type, T::INSTRUCTION_ADD);
    EXPECT_FALSE(add.has(SETS_FLAGS));
}

TEST(tokenizer_v2, registers)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "x0 x29 sp xzr x30 x01 x5a\n");
    EXPECT_EQ(register_index(r.tokens[0].type), 0);
    EXPECT_EQ(register_index(r.tokens[1].type), 29);
    EXPECT_EQ(register_index(r.tokens[2].type), 30);
    EXPECT_EQ(register_index(r.tokens[3].type), 31);
    EXPECT_EQ(r.tokens[4].type, T::SYMBOL);
    EXPECT_EQ(r.tokens[5].type, T::SYMBOL);
    EXPECT_EQ(r.tokens[6].type, T::SYMBOL);
}

TEST(tokenizer_v2, float_mnemonics)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "vadd.f32 x0, x1, x2\nvcint.u32.f32 x0, x1\n");
    EXPECT_FALSE(r.has_errors);
    EXPECT_EQ(r.tokens[0].type, T::INSTRUCTION_VADD);
    EXPECT_EQ(r.tokens[0].text, "vadd.f32");
    EXPECT_EQ(r.tokens[7].type, T::INSTRUCTION_VCINT);
}

TEST(tokenizer_v2, number_literals)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, ".byte $2A, %101010, @52, 42, 1.5, .25\n");
    ASSERT_FALSE(r.has_errors);
    EXPECT_EQ(r.tokens[0].type, T::ASSEMBLER_BYTE);
    EXPECT_EQ(r.tokens[1].type, T::LITERAL_NUMBER_HEXADECIMAL);
    EXPECT_EQ(r.tokens[1].int_value, 42u);
    EXPECT_EQ(r.tokens[3].type, T::LITERAL_NUMBER_BINARY);
    EXPECT_EQ(r.tokens[3].int_value, 42u);
    EXPECT_EQ(r.tokens[5].type, T::LITERAL_NUMBER_OCTAL);
    EXPECT_EQ(r.tokens[5].int_value, 42u);
    EXPECT_EQ(r.tokens[7].type, T::LITERAL_NUMBER_DECIMAL);
    EXPECT_EQ(r.tokens[7].int_value, 42u);
    EXPECT_EQ(r.tokens[9].type, T::LITERAL_FLOAT_32);
    EXPECT_EQ(r.tokens[9].text, "1.5");
    EXPECT_EQ(r.tokens[11].type, T::LITERAL_FLOAT_32);
    EXPECT_EQ(r.tokens[11].text, ".25");
}

TEST(tokenizer_v2, percent_is_modulus_after_an_operand)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "10 %1\nx %101\n");
    EXPECT_EQ(r.tokens[1].type, T::OPERATOR_MODULUS);
    EXPECT_EQ(r.tokens[2].type, T::LITERAL_NUMBER_DECIMAL);
}

TEST(tokenizer_v2, bad_numbers_are_diagnosed)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "mov x0, 0x1F\nmov x1, %12\nmov x2, 99999999999999999999\n");
    EXPECT_TRUE(r.has_errors);
    EXPECT_EQ(r.diagnostics.size(), 3u);
    EXPECT_NE(r.diagnostics[0].message.find("hexadecimal is written"), std::string::npos);
    EXPECT_EQ(r.diagnostics[0].loc.line, 1u);
    EXPECT_EQ(r.diagnostics[0].loc.column, 9u);
    EXPECT_EQ(r.diagnostics[1].loc.line, 2u);
    EXPECT_NE(r.diagnostics[2].message.find("too large"), std::string::npos);
    // lexing continued after the errors
    EXPECT_EQ(r.tokens.back().type, T::END_OF_FILE);
}

TEST(tokenizer_v2, comments)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "add ; comment\n;* multi\nline *; sub\n");
    EXPECT_EQ(types(r), (std::vector<TokenType>{T::INSTRUCTION_ADD, T::NEWLINE, T::INSTRUCTION_SUB,
                                                T::NEWLINE, T::END_OF_FILE}));
    // the multi line comment still advanced the line counter
    EXPECT_EQ(r.tokens[2].loc.line, 3u);
    EXPECT_TRUE(r.tokens[2].has(SPACE_BEFORE));

    LexOptions opts;
    opts.keep_comments = true;
    const LexResult kept = lex_asm(sm, "add ; c\n", opts);
    EXPECT_EQ(kept.tokens[1].type, T::COMMENT_SINGLE_LINE);
    EXPECT_EQ(kept.tokens[1].text, "; c");
}

TEST(tokenizer_v2, unterminated_comment)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "nop\n;* never closed\nhlt\n");
    EXPECT_TRUE(r.has_errors);
    EXPECT_EQ(r.diagnostics[0].loc.line, 2u);
}

TEST(tokenizer_v2, directives_and_preprocessor)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, ".global _start\n#define X 4\n#ifdef X\n#endif\n");
    ASSERT_FALSE(r.has_errors);
    EXPECT_EQ(r.tokens[0].type, T::ASSEMBLER_GLOBAL);
    EXPECT_EQ(r.tokens[1].type, T::SYMBOL);
    EXPECT_EQ(r.tokens[3].type, T::PREPROCESSOR_DEFINE);
    EXPECT_EQ(r.tokens[7].type, T::PREPROCESSOR_IFDEF);
    EXPECT_TRUE(is_preprocessor_directive(r.tokens[7].type));
    EXPECT_TRUE(is_assembler_directive(r.tokens[0].type));
}

TEST(tokenizer_v2, unknown_directives_are_errors)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, ".bogus\n#bogus\n");
    EXPECT_EQ(r.diagnostics.size(), 2u);
    EXPECT_EQ(r.tokens[0].type, T::INVALID);
    EXPECT_EQ(r.tokens[2].type, T::INVALID);
}

TEST(tokenizer_v2, define_whitespace_before_paren_is_recorded)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "#define F(x) x\n#define G (x)\n");
    // F ( ...
    EXPECT_FALSE(r.tokens[2].has(SPACE_BEFORE));
    // G (
    EXPECT_TRUE(r.tokens[9].has(SPACE_BEFORE));
}

TEST(tokenizer_v2, line_continuation)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "#define ADD \\\n   add x0, xzr, 4\nhlt\n");
    EXPECT_EQ(types(r), (std::vector<TokenType>{
                            T::PREPROCESSOR_DEFINE, T::SYMBOL, T::INSTRUCTION_ADD, T::REGISTER_X0,
                            T::COMMA, T::REGISTER_XZR, T::COMMA, T::LITERAL_NUMBER_DECIMAL,
                            T::NEWLINE, T::INSTRUCTION_HLT, T::NEWLINE, T::END_OF_FILE}));
    EXPECT_EQ(r.tokens[2].loc.line, 2u);
    EXPECT_EQ(r.tokens[9].loc.line, 3u);
}

TEST(tokenizer_v2, strings_and_chars)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, ".asciz \"hi\\n\\\"x\"\n.byte 'a', '\\n'\n");
    ASSERT_FALSE(r.has_errors);
    EXPECT_EQ(r.tokens[1].type, T::LITERAL_STRING);
    EXPECT_EQ(unescape_string_literal(r.tokens[1]), "hi\n\"x");
    EXPECT_EQ(r.tokens[4].type, T::LITERAL_CHAR);
    EXPECT_EQ(r.tokens[4].int_value, static_cast<std::uint64_t>('a'));
    EXPECT_EQ(r.tokens[6].int_value, static_cast<std::uint64_t>('\n'));
}

TEST(tokenizer_v2, unterminated_string_recovers_on_next_line)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, ".ascii \"oops\nhlt\n");
    EXPECT_TRUE(r.has_errors);
    EXPECT_EQ(r.tokens[1].type, T::INVALID);
    EXPECT_EQ(r.tokens[3].type, T::INSTRUCTION_HLT);
}

TEST(tokenizer_v2, operators_and_relocations)
{
    SourceManager sm;
    const LexResult r =
        lex_asm(sm, "add x0, x0, :lo12:sym\n1 << 2 >= 3 && 4 != 5 || ~6 / 7\n[x0, 4]!\n");
    ASSERT_FALSE(r.has_errors);
    EXPECT_EQ(r.tokens[5].type, T::RELOCATION_EMU32_O_LO12);
    EXPECT_EQ(r.tokens[6].type, T::SYMBOL);

    const std::vector<TokenType> all = types(r);
    const std::vector<TokenType> line2(all.begin() + 8, all.begin() + 8 + 14);
    EXPECT_EQ(
        line2,
        (std::vector<TokenType>{
            T::LITERAL_NUMBER_DECIMAL, T::OPERATOR_BITWISE_LEFT_SHIFT, T::LITERAL_NUMBER_DECIMAL,
            T::OPERATOR_LOGICAL_GREATER_THAN_OR_EQUAL, T::LITERAL_NUMBER_DECIMAL,
            T::OPERATOR_LOGICAL_AND, T::LITERAL_NUMBER_DECIMAL, T::OPERATOR_LOGICAL_NOT_EQUAL,
            T::LITERAL_NUMBER_DECIMAL, T::OPERATOR_LOGICAL_OR, T::OPERATOR_BITWISE_COMPLEMENT,
            T::LITERAL_NUMBER_DECIMAL, T::OPERATOR_DIVISION, T::LITERAL_NUMBER_DECIMAL}));
}

TEST(tokenizer_v2, unexpected_character_is_one_error_and_lexing_continues)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "add ` x0\nhlt\n");
    EXPECT_TRUE(r.has_errors);
    ASSERT_EQ(r.diagnostics.size(), 1u);
    EXPECT_EQ(r.tokens[1].type, T::INVALID);
    EXPECT_EQ(r.tokens[2].type, T::REGISTER_X0);

    const std::string text = format_diagnostic(sm, r.diagnostics[0]);
    EXPECT_NE(text.find("test.basm:1:5: error: unexpected character '`'"), std::string::npos);
    EXPECT_NE(text.find("^"), std::string::npos);
}

TEST(tokenizer_v2, synthetic_tokens)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "cmp x0, 1\n");
    const Token xzr = Token::synthetic(T::REGISTER_XZR, "xzr", &r.tokens[0]);
    EXPECT_TRUE(xzr.has(SYNTHETIC));
    EXPECT_EQ(xzr.loc.line, 1u);
    EXPECT_EQ(register_index(xzr.type), 31);
}

TEST(tokenizer_v2, linker_script)
{
    SourceManager sm;
    const LexResult r = lex_ld(sm, "ENTRY(_start)\n"
                                   "/* c */ SECTIONS (\n"
                                   "  @P;\n"
                                   "  .text = 0x1000; // c\n"
                                   "  .data = 4096;\n"
                                   "  .bss;\n"
                                   ")\n");
    ASSERT_FALSE(r.has_errors);
    EXPECT_EQ(r.tokens[0].type, T::KEYWORD_ENTRY);
    EXPECT_EQ(r.tokens[1].type, T::OPEN_PARENTHESIS);
    EXPECT_EQ(r.tokens[2].type, T::SYMBOL);
    EXPECT_EQ(r.tokens[2].text, "_start");

    int hex = 0;
    int dec = 0;
    bool saw_at = false;
    bool saw_semicolon = false;
    bool saw_equal = false;
    for (const Token &t : r.tokens)
    {
        if (t.type == T::LITERAL_NUMBER_HEXADECIMAL)
        {
            hex++;
            EXPECT_EQ(t.int_value, 0x1000u);
        }
        if (t.type == T::LITERAL_NUMBER_DECIMAL)
        {
            dec++;
            EXPECT_EQ(t.int_value, 4096u);
        }
        saw_at |= t.type == T::AT;
        saw_semicolon |= t.type == T::SEMICOLON;
        saw_equal |= t.type == T::EQUAL;
    }
    EXPECT_EQ(hex, 1);
    EXPECT_EQ(dec, 1);
    EXPECT_TRUE(saw_at && saw_semicolon && saw_equal);

    // section names are shared with the assembler directive token types
    bool saw_text = false;
    for (const Token &t : r.tokens)
    {
        saw_text |= t.type == T::ASSEMBLER_TEXT;
    }
    EXPECT_TRUE(saw_text);
}

TEST(tokenizer_v2, linker_mode_does_not_reserve_instruction_names)
{
    SourceManager sm;
    const LexResult r = lex_ld(sm, "add x0\n");
    EXPECT_EQ(r.tokens[0].type, T::SYMBOL);
    EXPECT_EQ(r.tokens[1].type, T::SYMBOL);
}

TEST(tokenizer_v2, cursor_basics)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "add x0, x1\n\nsub x2\n");
    TokenCursor cur(r.tokens);

    EXPECT_EQ(cur.peek().type, T::INSTRUCTION_ADD);
    EXPECT_EQ(cur.peek(1).type, T::REGISTER_X0);

    const std::size_t mark = cur.mark();
    EXPECT_TRUE(cur.accept(T::INSTRUCTION_ADD));
    EXPECT_FALSE(cur.accept(T::INSTRUCTION_ADD));
    cur.rewind(mark);
    EXPECT_EQ(cur.next().type, T::INSTRUCTION_ADD);

    const std::span<const Token> rest = cur.take_line();
    EXPECT_EQ(rest.size(), 3u);
    EXPECT_EQ(rest[0].type, T::REGISTER_X0);

    cur.skip_newlines();
    EXPECT_EQ(cur.peek().type, T::INSTRUCTION_SUB);
    cur.skip_line();
    EXPECT_TRUE(cur.at_end());

    // reading past the end is safe
    EXPECT_EQ(cur.next().type, T::END_OF_FILE);
    EXPECT_EQ(cur.peek(100).type, T::END_OF_FILE);
}

TEST(tokenizer_v2, empty_cursor_is_safe)
{
    TokenCursor cur;
    EXPECT_TRUE(cur.at_end());
    EXPECT_EQ(cur.next().type, T::END_OF_FILE);
}

TEST(tokenizer_v2, token_type_names)
{
    EXPECT_EQ(to_string(T::INSTRUCTION_ADD), "INSTRUCTION_ADD");
    EXPECT_EQ(to_string(T::KEYWORD_SECTIONS), "KEYWORD_SECTIONS");
    EXPECT_EQ(to_string(T::END_OF_FILE), "END_OF_FILE");
}

TEST(tokenizer_v2, source_manager_survives_many_sources)
{
    SourceManager sm;
    const LexResult first = lex_asm(sm, "hlt\n");
    for (int i = 0; i < 1000; i++)
    {
        sm.add("n" + std::to_string(i), "nop\n");
    }
    // the view taken from the first source is still valid
    EXPECT_EQ(first.tokens[0].text, "hlt");
}

TEST(tokenizer_v2, fatal_if_errors_is_quiet_without_errors)
{
    aemu::log::ScopedLevel quiet(aemu::log::Level::Off);
    aemu::log::ScopedFatalAction guard(aemu::log::FatalAction::Throw);

    SourceManager sm;
    const LexResult r = lex_asm(sm, "add x0, x1, 5\n");
    EXPECT_NO_THROW(fatal_if_errors(sm, r));
}

TEST(tokenizer_v2, fatal_if_errors_counts_the_errors)
{
    aemu::log::ScopedLevel quiet(aemu::log::Level::Off);
    aemu::log::ScopedFatalAction guard(aemu::log::FatalAction::Throw);

    SourceManager sm;
    const LexResult r = lex_asm(sm, "mov x0, $G\n.bogus\n");
    ASSERT_EQ(r.diagnostics.size(), 2u);
    try
    {
        fatal_if_errors(sm, r);
        FAIL() << "expected an error";
    }
    catch (const aemu::log::FatalError &error)
    {
        EXPECT_NE(std::string(error.what()).find("2 lexical error(s)"), std::string::npos);
    }
}

TEST(tokenizer_v2, fatal_at_reports_the_location)
{
    aemu::log::ScopedLevel quiet(aemu::log::Level::Off);
    aemu::log::ScopedFatalAction guard(aemu::log::FatalAction::Throw);

    SourceManager sm;
    const LexResult r = lex_asm(sm, "hlt\n  add x0\n");
    const Token &add = r.tokens[2];
    ASSERT_EQ(add.type, T::INSTRUCTION_ADD);

    try
    {
        fatal_at(sm, add.loc, "not good");
        FAIL() << "expected an error";
    }
    catch (const aemu::log::FatalError &error)
    {
        // The message has no trailing newline.
        EXPECT_EQ(std::string(error.what()), "test.basm:2:3: error: not good\n    add x0\n    ^");
    }
}

TEST(tokenizer_v2, diagnostics_list_the_expansions_a_token_came_from)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "use\n  inner\n");
    const Token &use = r.tokens[0];
    const Token &inner = r.tokens[2];

    // `inner` was produced by expanding macro `m`, used at `use`, which itself came out of
    // expanding the symbol `S` used at `inner`.
    SourceLocation outer_site = inner.loc;
    SourceLocation site = use.loc;
    site.expansion = sm.add_expansion("S", false, outer_site);
    SourceLocation at = inner.loc;
    at.expansion = sm.add_expansion("m", true, site);

    const std::string text = format_diagnostic(sm, {Severity::ERROR, at, "bad"});
    EXPECT_EQ(text, "test.basm:2:3: error: bad\n"
                    "    inner\n"
                    "    ^\n"
                    "test.basm:1:1: note: in expansion of macro 'm'\n"
                    "  use\n"
                    "  ^\n"
                    "test.basm:2:3: note: in expansion of symbol 'S'\n"
                    "    inner\n"
                    "    ^\n");
}

TEST(tokenizer_v2, tokens_written_in_the_source_have_no_expansion)
{
    SourceManager sm;
    const LexResult r = lex_asm(sm, "hlt\n");
    EXPECT_EQ(r.tokens[0].loc.expansion, 0u);
    EXPECT_EQ(sm.expansion(0), nullptr);
    EXPECT_EQ(sm.expansion(7), nullptr);
}
