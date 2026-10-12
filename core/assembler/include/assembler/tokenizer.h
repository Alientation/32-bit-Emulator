#pragma once

/// @file tokenizer.h
/// @brief Shared lexer for basm sources (preprocessor + assembler) and linker scripts.
///
/// Design:
/// - Hand written single pass scanner, O(n) in the size of the source.
/// - The result is a plain, immutable std::vector<Token>. Reading is done through a separate
/// TokenCursor. Nothing is mutated after lexing (no skip flags, no insertion).
/// - Tokens keep their exact source location (file, line, column, byte offset) and point into
/// text owned by a SourceManager, so there are no per token string copies.
/// - NEWLINE tokens are kept, so a line oriented grammar ("statement, then end of line") is
/// possible. The token list always ends with NEWLINE (unless disabled) then END_OF_FILE.
/// - Errors never terminate the process. They are collected as Diagnostics and the lexer
/// resynchronizes, so many errors can be reported in one run.
/// - Whitespace is not a token. Whether a token was preceded by whitespace is recorded in
/// TokenFlag::SPACE_BEFORE, which is what e.g. `#define F(x)` vs `#define F (x)` needs.

#include "assembler/instruction_list.h"
#include "util/types.h"

#include <cstddef>
#include <deque>
#include <initializer_list>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace basm
{

// ---------------------------------------------------------------------------------------------
// Sources and locations
// ---------------------------------------------------------------------------------------------

/// The number of a source in a SourceManager.
using SourceId = U32;

/// The id of no source: the one that SourceLocation starts with, and what add_file () gives for a
/// file that cannot be read.
inline constexpr SourceId kInvalidSource = ~SourceId(0);

/// A place in a source, for the diagnostics.
struct SourceLocation
{
    SourceId source = kInvalidSource;

    /// Byte offset from the start of the source.
    U32 offset = 0;

    /// 1 based line. 0 means unknown (synthetic token).
    U32 line = 0;

    /// 1 based byte column.
    U32 column = 0;

    /// 0 for text written in the source itself. Otherwise the id (SourceManager::add_expansion)
    /// of the macro or symbol expansion this token was produced by.
    U32 expansion = 0;
};

/// Owns the text of every source (files, strings, macro expansions). Tokens hold string_views
/// into this text, so the SourceManager must outlive all tokens produced from it. Storage is
/// stable: adding sources never invalidates existing views.
class SourceManager
{
  public:
    SourceManager() = default;
    SourceManager(const SourceManager &) = delete;
    SourceManager &operator=(const SourceManager &) = delete;
    SourceManager(SourceManager &&) = default;
    SourceManager &operator=(SourceManager &&) = default;

    /// Registers in memory text (also used for text synthesized by the preprocessor).
    ///
    /// @param[in] name What the diagnostics call the source (the path, for a file).
    /// @param[in] text The text.
    /// @return The id of the source.
    SourceId add(std::string name, std::string text);

    /// Reads a file.
    ///
    /// @param[in] path The path of the file.
    /// @return The id of the source, or kInvalidSource if the file cannot be read.
    SourceId add_file(const std::string &path);

    /// @param[in] id The id of a source.
    /// @return The name of the source, empty if the id is unknown.
    std::string_view name(SourceId id) const;

    /// @param[in] id The id of a source.
    /// @return The text of the source, empty if the id is unknown.
    std::string_view text(SourceId id) const;

    /// @param[in] id The id of a source.
    /// @param[in] line A 1 based line number.
    /// @return The text of the line without its line terminator, empty if there is no such line.
    std::string_view line_text(SourceId id, U32 line) const;

    /// Records that tokens were produced by expanding `name` (a macro or a #define) used at
    /// `site`. `site` has its own expansion id if the use was itself inside an expansion, which
    /// links the chain.
    ///
    /// @param[in] name The name of the macro or symbol.
    /// @param[in] is_macro Whether it is a macro, as opposed to a symbol of #define.
    /// @param[in] site Where it was used.
    /// @return The id to store in SourceLocation::expansion.
    U32 add_expansion(std::string name, bool is_macro, const SourceLocation &site);

    /// The expansion of a macro or a symbol, see add_expansion ().
    struct Expansion
    {
        std::string name;
        bool is_macro;
        SourceLocation site;
    };

    /// @param[in] id An id that add_expansion () returned.
    /// @return The expansion, or nullptr if the id is 0 or unknown.
    const Expansion *expansion(U32 id) const;

  private:
    /// One file or string that was added: its text and where its lines start.
    struct Source
    {
        std::string name;
        std::string text;
        std::vector<U32> line_starts;
    };

    /// @param[in] id The id of a source.
    /// @return The source, or nullptr if the id is unknown.
    const Source *get(SourceId id) const;

    std::deque<Source> m_sources;
    std::deque<Expansion> m_expansions;
};

// ---------------------------------------------------------------------------------------------
// Token types
// ---------------------------------------------------------------------------------------------

// The instructions come from their own list (instruction_list.h), one token type per row.
#define BASM_INSTRUCTION_TOKEN(X, NAME, ...) X(INSTRUCTION_##NAME)

// Groups are contiguous so that classification is a range check. Do not reorder inside a group.
// clang-format off
#define BASM_TOKEN_TYPES(X)                                                                      \
    X(END_OF_FILE) X(NEWLINE) X(INVALID)                                                           \
    X(COMMENT_SINGLE_LINE) X(COMMENT_MULTI_LINE)                                                   \
    X(LABEL) X(SYMBOL) X(BACK_SLASH)                                                               \
                                                                                                   \
    X(PREPROCESSOR_INCLUDE) X(PREPROCESSOR_MACRO) X(PREPROCESSOR_MACRET)                           \
    X(PREPROCESSOR_MACEND) X(PREPROCESSOR_INVOKE) X(PREPROCESSOR_DEFINE)                           \
    X(PREPROCESSOR_UNDEF) X(PREPROCESSOR_IFDEF) X(PREPROCESSOR_IFNDEF)                             \
    X(PREPROCESSOR_IFEQU) X(PREPROCESSOR_IFNEQU) X(PREPROCESSOR_IFLESS)                            \
    X(PREPROCESSOR_IFMORE) X(PREPROCESSOR_ELSE) X(PREPROCESSOR_ELSEDEF)                            \
    X(PREPROCESSOR_ELSENDEF) X(PREPROCESSOR_ELSEEQU) X(PREPROCESSOR_ELSENEQU)                      \
    X(PREPROCESSOR_ELSELESS) X(PREPROCESSOR_ELSEMORE) X(PREPROCESSOR_ENDIF)                        \
                                                                                                   \
    X(ASSEMBLER_GLOBAL) X(ASSEMBLER_EXTERN) X(ASSEMBLER_EQU) X(ASSEMBLER_ORG) X(ASSEMBLER_SCOPE)  \
    X(ASSEMBLER_SCEND) X(ASSEMBLER_ADVANCE) X(ASSEMBLER_FILL) X(ASSEMBLER_ALIGN)                   \
    X(ASSEMBLER_SECTION) X(ASSEMBLER_BSS) X(ASSEMBLER_DATA) X(ASSEMBLER_TEXT)                      \
    X(ASSEMBLER_WEAK) X(ASSEMBLER_COMM) X(ASSEMBLER_RODATA) X(ASSEMBLER_INIT_ARRAY)               \
    X(ASSEMBLER_FINI_ARRAY) X(ASSEMBLER_PUSHSECTION) X(ASSEMBLER_POPSECTION)                       \
    X(ASSEMBLER_STOP) X(ASSEMBLER_BYTE) X(ASSEMBLER_DBYTE) X(ASSEMBLER_WORD)                       \
    X(ASSEMBLER_DWORD) X(ASSEMBLER_FLOAT) X(ASSEMBLER_DOUBLE) X(ASSEMBLER_ASCII)                  \
    X(ASSEMBLER_ASCIZ)                                                                             \
                                                                                                   \
    X(RELOCATION_EMU32_O_LO12) X(RELOCATION_EMU32_ADRP_HI20) X(RELOCATION_EMU32_MOV_LO19)          \
    X(RELOCATION_EMU32_MOV_HI13)                                                                   \
                                                                                                   \
    X(REGISTER_X0) X(REGISTER_X1) X(REGISTER_X2) X(REGISTER_X3) X(REGISTER_X4) X(REGISTER_X5)      \
    X(REGISTER_X6) X(REGISTER_X7) X(REGISTER_X8) X(REGISTER_X9) X(REGISTER_X10) X(REGISTER_X11)    \
    X(REGISTER_X12) X(REGISTER_X13) X(REGISTER_X14) X(REGISTER_X15) X(REGISTER_X16)                \
    X(REGISTER_X17) X(REGISTER_X18) X(REGISTER_X19) X(REGISTER_X20) X(REGISTER_X21)                \
    X(REGISTER_X22) X(REGISTER_X23) X(REGISTER_X24) X(REGISTER_X25) X(REGISTER_X26)                \
    X(REGISTER_X27) X(REGISTER_X28) X(REGISTER_X29) X(REGISTER_SP) X(REGISTER_XZR)                 \
                                                                                                   \
    BASM_INSTRUCTION_LIST(BASM_INSTRUCTION_TOKEN, X)                                               \
                                                                                                   \
    X(CONDITION_EQ) X(CONDITION_NE) X(CONDITION_CS) X(CONDITION_HS) X(CONDITION_CC)                \
    X(CONDITION_LO) X(CONDITION_MI) X(CONDITION_PL) X(CONDITION_VS) X(CONDITION_VC)                \
    X(CONDITION_HI) X(CONDITION_LS) X(CONDITION_GE) X(CONDITION_LT) X(CONDITION_GT)                \
    X(CONDITION_LE) X(CONDITION_AL) X(CONDITION_NV)                                                \
                                                                                                   \
    X(LITERAL_FLOAT_32) X(LITERAL_NUMBER_BINARY) X(LITERAL_NUMBER_OCTAL)                           \
    X(LITERAL_NUMBER_DECIMAL) X(LITERAL_NUMBER_HEXADECIMAL) X(LITERAL_CHAR) X(LITERAL_STRING)      \
                                                                                                   \
    X(COLON) X(COMMA) X(PERIOD) X(SEMICOLON) X(OPEN_PARENTHESIS) X(CLOSE_PARENTHESIS)              \
    X(OPEN_BRACKET) X(CLOSE_BRACKET) X(OPEN_BRACE) X(CLOSE_BRACE) X(EQUAL) X(AT)                   \
                                                                                                   \
    X(OPERATOR_ADDITION) X(OPERATOR_SUBTRACTION) X(OPERATOR_MULTIPLICATION)                        \
    X(OPERATOR_DIVISION) X(OPERATOR_MODULUS) X(OPERATOR_BITWISE_LEFT_SHIFT)                        \
    X(OPERATOR_BITWISE_RIGHT_SHIFT) X(OPERATOR_BITWISE_XOR) X(OPERATOR_BITWISE_AND)                \
    X(OPERATOR_BITWISE_OR) X(OPERATOR_BITWISE_COMPLEMENT) X(OPERATOR_LOGICAL_NOT)                  \
    X(OPERATOR_LOGICAL_EQUAL) X(OPERATOR_LOGICAL_NOT_EQUAL) X(OPERATOR_LOGICAL_LESS_THAN)          \
    X(OPERATOR_LOGICAL_GREATER_THAN) X(OPERATOR_LOGICAL_LESS_THAN_OR_EQUAL)                        \
    X(OPERATOR_LOGICAL_GREATER_THAN_OR_EQUAL) X(OPERATOR_LOGICAL_OR) X(OPERATOR_LOGICAL_AND)       \
                                                                                                   \
    X(KEYWORD_ENTRY) X(KEYWORD_SECTIONS)
// clang-format on

enum class TokenType : U16
{
#define BASM_X(name) name,
    BASM_TOKEN_TYPES(BASM_X)
#undef BASM_X
        COUNT
};

/// Name of the enumerator, e.g. "INSTRUCTION_ADD".
///
/// @param[in] type The type.
/// @return The name.
std::string_view to_string(TokenType type);

namespace detail
{
/// @param[in] t The type to look at.
/// @param[in] lo The first type of the range.
/// @param[in] hi The last type of the range.
/// @return Whether `t` is one of the types from `lo` to `hi`.
constexpr bool in_range(const TokenType t, const TokenType lo, const TokenType hi)
{
    return t >= lo && t <= hi;
}
} // namespace detail

///
/// @param[in] t The type.
/// @return Whether the type is a comment.
constexpr bool is_comment(const TokenType t)
{
    return detail::in_range(t, TokenType::COMMENT_SINGLE_LINE, TokenType::COMMENT_MULTI_LINE);
}

///
/// @param[in] t The type.
/// @return Whether the type is a preprocessor directive (`#define`, ...).
constexpr bool is_preprocessor_directive(const TokenType t)
{
    return detail::in_range(t, TokenType::PREPROCESSOR_INCLUDE, TokenType::PREPROCESSOR_ENDIF);
}

///
/// @param[in] t The type.
/// @return Whether the type is an assembler directive (`.global`, ...).
constexpr bool is_assembler_directive(const TokenType t)
{
    return detail::in_range(t, TokenType::ASSEMBLER_GLOBAL, TokenType::ASSEMBLER_ASCIZ);
}

///
/// @param[in] t The type.
/// @return Whether the type is a relocation operator (`:lo12:`, ...).
constexpr bool is_relocation(const TokenType t)
{
    return detail::in_range(t, TokenType::RELOCATION_EMU32_O_LO12,
                            TokenType::RELOCATION_EMU32_MOV_HI13);
}

///
/// @param[in] t The type.
/// @return Whether the type is a register (x0-x29, sp, xzr).
constexpr bool is_register(const TokenType t)
{
    return detail::in_range(t, TokenType::REGISTER_X0, TokenType::REGISTER_XZR);
}

///
/// @param[in] t The type.
/// @return Whether the type is an instruction mnemonic.
constexpr bool is_instruction(const TokenType t)
{
    return detail::in_range(t, TokenType::INSTRUCTION_HLT, TokenType::INSTRUCTION_RET);
}

///
/// @param[in] t The type.
/// @return Whether the type is a condition code (eq, ne, ...).
constexpr bool is_condition(const TokenType t)
{
    return detail::in_range(t, TokenType::CONDITION_EQ, TokenType::CONDITION_NV);
}

///
/// @param[in] t The type.
/// @return Whether the type is an integer literal.
constexpr bool is_integer_literal(const TokenType t)
{
    return detail::in_range(t, TokenType::LITERAL_NUMBER_BINARY,
                            TokenType::LITERAL_NUMBER_HEXADECIMAL);
}

///
/// @param[in] t The type.
/// @return Whether the type is an integer or floating point literal.
constexpr bool is_number_literal(const TokenType t)
{
    return detail::in_range(t, TokenType::LITERAL_FLOAT_32, TokenType::LITERAL_NUMBER_HEXADECIMAL);
}

///
/// @param[in] t The type.
/// @return Whether the type is an operator of an expression.
constexpr bool is_operator(const TokenType t)
{
    return detail::in_range(t, TokenType::OPERATOR_ADDITION, TokenType::OPERATOR_LOGICAL_AND);
}

/// Register number of a register token: x0-x29 are 0-29, sp is 30, xzr is 31.
///
/// @param[in] t The type of a register token.
/// @return The register number.
constexpr byte register_index(const TokenType t)
{
    return byte(U16(t) - U16(TokenType::REGISTER_X0));
}

// ---------------------------------------------------------------------------------------------
// Tokens
// ---------------------------------------------------------------------------------------------

/// Bits of Token::flags.
enum TokenFlag : U8
{
    /// Whitespace, a comment or a line continuation immediately precedes this token.
    SPACE_BEFORE = 1 << 0,

    /// First token on its line (ignoring comments).
    FIRST_ON_LINE = 1 << 1,

    /// Instruction mnemonic carried the `s` suffix (adds, movs, lsls, ...).
    SETS_FLAGS = 1 << 2,

    /// ldrsb / ldrsh / ldursh. A store has no sign, the bytes stored are the low ones of the
    /// register.
    SIGN_EXTEND = 1 << 3,

    /// Not produced by the lexer (see Token::synthetic).
    SYNTHETIC = 1 << 4,
};

/// A piece of the source that the lexer recognised, with where it is.
struct Token
{
    TokenType type = TokenType::END_OF_FILE;
    U8 flags = 0;

    /// Exact source text. Exceptions:
    ///  - LABEL: the name only, without the trailing ':'.
    ///  - Synthetic NEWLINE / END_OF_FILE: empty.
    std::string_view text;

    SourceLocation loc;

    /// Value of integer and character literals (LITERAL_NUMBER_* except float, LITERAL_CHAR).
    U64 int_value = 0;

    /// @param[in] t The type.
    /// @return Whether the token has this type.
    bool is(const TokenType t) const
    {
        return type == t;
    }

    /// @param[in] types The types.
    /// @return Whether the token has one of these types.
    bool is_one_of(const std::initializer_list<TokenType> types) const
    {
        for (const TokenType t : types)
        {
            if (t == type)
            {
                return true;
            }
        }
        return false;
    }

    /// @param[in] flag The flag.
    /// @return Whether the token has this flag.
    bool has(const TokenFlag flag) const
    {
        return (flags & flag) != 0;
    }

    /// @return A copy of the text of the token.
    std::string str() const
    {
        return std::string(text);
    }

    /// Creates a token that is not backed by lexed source, e.g. the `xzr` the assembler injects
    /// for `cmp`.
    ///
    /// @param[in] type The type of the token.
    /// @param[in] static_text The text, which has to outlive the token (use a string literal).
    /// @param[in] origin The token whose location the new one gets for diagnostics, or null.
    /// @return The token.
    static Token synthetic(const TokenType type, const std::string_view static_text,
                           const Token *origin = nullptr)
    {
        Token tok;
        tok.type = type;
        tok.flags = SYNTHETIC;
        tok.text = static_text;
        if (origin != nullptr)
        {
            tok.loc = origin->loc;
        }
        return tok;
    }
};

/// @param[in] token A token.
/// @return A human readable name of the token for error messages: `'add'`, `end of line`,
///     `end of file`.
std::string describe(const Token &token);

/// @param[in] token A LITERAL_STRING token.
/// @return Its contents without the quotes and with the escape sequences resolved.
std::string unescape_string_literal(const Token &token);

// ---------------------------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------------------------

/// How bad a Diagnostic is.
enum class Severity : U8
{
    WARNING,
    ERROR,
};

/// A warning or an error that the lexer found.
struct Diagnostic
{
    Severity severity = Severity::ERROR;
    SourceLocation loc;
    std::string message;
};

/// Writes a diagnostic as `file:line:col: error: message`, followed by the source line and a
/// caret, and a note for each macro or symbol expansion it came from.
///
/// @param[in] sources The sources the diagnostic is about.
/// @param[in] diagnostic The diagnostic.
/// @return The text, which ends with a newline.
std::string format_diagnostic(const SourceManager &sources, const Diagnostic &diagnostic);

// ---------------------------------------------------------------------------------------------
// Lexing
// ---------------------------------------------------------------------------------------------

/// What kind of text is lexed.
enum class LexMode : U8
{
    /// .basm / .binc / .bi sources. `;` comments, `0x` `0b` `0o` literals, `#` directives,
    /// instructions, registers.
    ASSEMBLY,

    /// .ld linker scripts. `//` and `/* */` comments, `0x` / `0b` literals, `;` `=` `@`,
    /// ENTRY / SECTIONS keywords and the names of the sections.
    LINKER_SCRIPT,
};

/// What the lexer keeps and how it reads the text.
struct LexOptions
{
    LexMode mode = LexMode::ASSEMBLY;

    /// Emit COMMENT_* tokens (e.g. for `-C`). Otherwise comments are dropped.
    bool keep_comments = false;

    /// Emit NEWLINE tokens. Disable only for parsers that ignore line structure.
    bool keep_newlines = true;

    /// Emit a single NEWLINE for a run of blank lines. Locations still have exact lines.
    bool collapse_newlines = false;
};

/// The tokens of a source and the diagnostics.
struct LexResult
{
    /// Always ends with END_OF_FILE. Unless `keep_newlines` is false, the token before that is
    /// always a NEWLINE (synthesized at the end of the file if the last line has none).
    std::vector<Token> tokens;

    std::vector<Diagnostic> diagnostics;

    /// True if any diagnostic has Severity::ERROR. INVALID tokens appear in the token list
    /// where the lexer could not produce a valid token.
    bool has_errors = false;
};

/// The tokens of a preprocessed program and the sources they point into. The tokens keep the
/// file, line and column they were lexed at, so errors found after preprocessing point at the
/// original source. Tokens that came out of a macro or symbol expansion record it too
/// (SourceLocation::expansion).
struct PreprocessedSource
{
    std::shared_ptr<SourceManager> sources;

    /// Ends with a NEWLINE and END_OF_FILE.
    std::vector<Token> tokens;
};

/// Lexes a source that is registered already.
///
/// @param[in] sources The sources, which have to outlive the tokens.
/// @param[in] id The source to lex.
/// @param[in] options What to keep, and the kind of text.
/// @return The tokens and the diagnostics.
LexResult lex(const SourceManager &sources, SourceId id, const LexOptions &options = {});

/// Registers `text` and lexes it.
///
/// @param[in,out] sources The sources, which get the text and have to outlive the tokens.
/// @param[in] name What the diagnostics call the text.
/// @param[in] text The text to lex.
/// @param[in] options What to keep, and the kind of text.
/// @return The tokens and the diagnostics.
LexResult lex_text(SourceManager &sources, std::string name, std::string text,
                   const LexOptions &options = {});

/// Logs every diagnostic of `result` (with its source line and caret). If any of them is an
/// error, terminates through AEMU_FATAL (which throws under FatalAction::Throw).
///
/// @param[in] sources The sources the diagnostics are about.
/// @param[in] result The result of lexing.
void fatal_if_errors(const SourceManager &sources, const LexResult &result);

/// Logs `message` at `loc` (file:line:col, source line, caret) and terminates through AEMU_FATAL.
///
/// @param[in] sources The sources the location is in.
/// @param[in] loc Where the error is.
/// @param[in] message What is wrong.
[[noreturn]] void fatal_at(const SourceManager &sources, const SourceLocation &loc,
                           const std::string &message);

// ---------------------------------------------------------------------------------------------
// Reading tokens
// ---------------------------------------------------------------------------------------------

/// Read only cursor over a token list. Does not own the tokens. Cheap to copy, so a stack of
/// cursors is the natural way to implement #include and macro expansion in the preprocessor
/// (push a cursor over the included/expanded tokens, pop it when it is exhausted).
///
/// Reading past the end is safe: peek() returns the END_OF_FILE token and next() stops there.
class TokenCursor
{
  public:
    TokenCursor() = default;

    /// @param[in] tokens The tokens to read, which have to outlive the cursor.
    explicit TokenCursor(const std::span<const Token> tokens) :
        m_tokens(tokens)
    {
    }

    /// Looks at a token without consuming it.
    ///
    /// @param[in] ahead How many tokens after the current one.
    /// @return The token, or the END_OF_FILE one if there are not that many.
    const Token &peek(std::size_t ahead = 0) const;

    /// Consumes the current token. Never moves past END_OF_FILE.
    ///
    /// @return The token.
    const Token &next();

    /// @return Whether the next token is END_OF_FILE.
    bool at_end() const
    {
        return peek().type == TokenType::END_OF_FILE;
    }

    /// @param[in] type The type.
    /// @return Whether the next token has this type, which is not consumed.
    bool check(const TokenType type) const
    {
        return peek().type == type;
    }

    /// @param[in] types The types.
    /// @return Whether the next token has one of these types, which is not consumed.
    bool check_any(const std::initializer_list<TokenType> types) const
    {
        return peek().is_one_of(types);
    }

    /// Consumes the current token if it has this type.
    ///
    /// @param[in] type The type to look for.
    /// @return Whether it had the type.
    bool accept(TokenType type);

    /// Like accept (), but gives the token.
    ///
    /// @param[in] type The type to look for.
    /// @return The consumed token, or nullptr if the type did not match.
    const Token *try_next(TokenType type);

    /// Skips consecutive NEWLINE tokens.
    void skip_newlines();

    /// Consumes tokens up to and including the next NEWLINE (or up to END_OF_FILE). Use it to
    /// resynchronize after a statement level error.
    void skip_line();

    /// Consumes the rest of the current line including its NEWLINE and returns the tokens before
    /// the NEWLINE. The span stays valid as long as the underlying token list does.
    ///
    /// @return The tokens of the line.
    std::span<const Token> take_line();

    /// @return The index of the next token.
    std::size_t position() const
    {
        return m_pos;
    }

    /// @return The position to give to rewind () to go back to this point.
    std::size_t mark() const
    {
        return m_pos;
    }

    /// Goes back to a position that mark() returned.
    ///
    /// @param[in] mark The position to go back to.
    void rewind(const std::size_t mark)
    {
        m_pos = mark;
    }

    /// @return All the tokens that the cursor reads.
    std::span<const Token> tokens() const
    {
        return m_tokens;
    }

  private:
    std::span<const Token> m_tokens;
    std::size_t m_pos = 0;
};

} // namespace basm