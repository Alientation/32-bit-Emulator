#pragma once

/// @file tokenizer.h
/// @brief Shared lexer for basm sources (preprocessor + assembler) and linker scripts.
///
/// Design:
///  - Hand written single pass scanner, O(n) in the size of the source.
///  - The result is a plain, immutable std::vector<Token>. Reading is done through a separate
///    TokenCursor. Nothing is mutated after lexing (no skip flags, no insertion).
///  - Tokens keep their exact source location (file, line, column, byte offset) and point into
///    text owned by a SourceManager, so there are no per token string copies.
///  - NEWLINE tokens are kept, so a line oriented grammar ("statement, then end of line") is
///    possible. The token list always ends with NEWLINE (unless disabled) then END_OF_FILE.
///  - Errors never terminate the process. They are collected as Diagnostics and the lexer
///    resynchronizes, so many errors can be reported in one run.
///  - Whitespace is not a token. Whether a token was preceded by whitespace is recorded in
///    TokenFlag::SPACE_BEFORE, which is what e.g. `#define F(x)` vs `#define F (x)` needs.

#include "util/types.h"
#include "assembler/instruction_list.h"

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

using SourceId = U32;
inline constexpr SourceId kInvalidSource = ~SourceId(0);

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
    SourceId add(std::string name, std::string text);

    /// Reads a file. Returns nullopt if it cannot be read.
    SourceId add_file(const std::string &path);

    std::string_view name(SourceId id) const;
    std::string_view text(SourceId id) const;

    /// Text of a 1 based line without its line terminator. Empty if out of range.
    std::string_view line_text(SourceId id, U32 line) const;

    /// Records that tokens were produced by expanding `name` (a macro or a #define) used at
    /// `site`. Returns the id to store in SourceLocation::expansion. `site` has its own
    /// expansion id if the use was itself inside an expansion, which links the chain.
    U32 add_expansion(std::string name, bool is_macro, const SourceLocation &site);

    struct Expansion
    {
        std::string name;
        bool is_macro;
        SourceLocation site;
    };

    /// Expansion with the given id (never 0). nullptr if unknown.
    const Expansion *expansion(U32 id) const;

  private:
    struct Source
    {
        std::string name;
        std::string text;
        std::vector<U32> line_starts;
    };

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
    X(ASSEMBLER_DWORD) X(ASSEMBLER_ASCII) X(ASSEMBLER_ASCIZ)                                       \
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
std::string_view to_string(TokenType type);

namespace detail
{
constexpr bool in_range(TokenType t, TokenType lo, TokenType hi)
{
    return t >= lo && t <= hi;
}
} // namespace detail

constexpr bool is_comment(TokenType t)
{
    return detail::in_range(t, TokenType::COMMENT_SINGLE_LINE, TokenType::COMMENT_MULTI_LINE);
}

constexpr bool is_preprocessor_directive(TokenType t)
{
    return detail::in_range(t, TokenType::PREPROCESSOR_INCLUDE, TokenType::PREPROCESSOR_ENDIF);
}

constexpr bool is_assembler_directive(TokenType t)
{
    return detail::in_range(t, TokenType::ASSEMBLER_GLOBAL, TokenType::ASSEMBLER_ASCIZ);
}

constexpr bool is_relocation(TokenType t)
{
    return detail::in_range(t, TokenType::RELOCATION_EMU32_O_LO12,
                            TokenType::RELOCATION_EMU32_MOV_HI13);
}

constexpr bool is_register(TokenType t)
{
    return detail::in_range(t, TokenType::REGISTER_X0, TokenType::REGISTER_XZR);
}

constexpr bool is_instruction(TokenType t)
{
    return detail::in_range(t, TokenType::INSTRUCTION_HLT, TokenType::INSTRUCTION_RET);
}

constexpr bool is_condition(TokenType t)
{
    return detail::in_range(t, TokenType::CONDITION_EQ, TokenType::CONDITION_NV);
}

constexpr bool is_integer_literal(TokenType t)
{
    return detail::in_range(t, TokenType::LITERAL_NUMBER_BINARY,
                            TokenType::LITERAL_NUMBER_HEXADECIMAL);
}

constexpr bool is_number_literal(TokenType t)
{
    return detail::in_range(t, TokenType::LITERAL_FLOAT_32, TokenType::LITERAL_NUMBER_HEXADECIMAL);
}

constexpr bool is_operator(TokenType t)
{
    return detail::in_range(t, TokenType::OPERATOR_ADDITION, TokenType::OPERATOR_LOGICAL_AND);
}

/// Register number of a register token: x0-x29 are 0-29, sp is 30, xzr is 31.
constexpr byte register_index(TokenType t)
{
    return byte(U16(t) - U16(TokenType::REGISTER_X0));
}

// ---------------------------------------------------------------------------------------------
// Tokens
// ---------------------------------------------------------------------------------------------

enum TokenFlag : U8
{
    /// Whitespace, a comment or a line continuation immediately precedes this token.
    SPACE_BEFORE = 1 << 0,

    /// First token on its line (ignoring comments).
    FIRST_ON_LINE = 1 << 1,

    /// Instruction mnemonic carried the `s` suffix (adds, movs, lsls, ...).
    SETS_FLAGS = 1 << 2,

    /// ldrsb / ldrsh. A store has no sign, the bytes stored are the low ones of the register.
    SIGN_EXTEND = 1 << 3,

    /// Not produced by the lexer (see Token::synthetic).
    SYNTHETIC = 1 << 4,
};

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

    bool is(TokenType t) const
    {
        return type == t;
    }

    bool is_one_of(std::initializer_list<TokenType> types) const
    {
        for (TokenType t : types)
        {
            if (t == type)
            {
                return true;
            }
        }
        return false;
    }

    bool has(TokenFlag flag) const
    {
        return (flags & flag) != 0;
    }

    std::string str() const
    {
        return std::string(text);
    }

    /// Creates a token that is not backed by lexed source, e.g. the `xzr` the assembler injects
    /// for `cmp`. `static_text` must outlive the token (use a string literal). Pass `origin` to
    /// inherit its location for diagnostics.
    static Token synthetic(TokenType type, std::string_view static_text,
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

/// Human readable token for error messages: `'add'`, `end of line`, `end of file`.
std::string describe(const Token &token);

/// Contents of a LITERAL_STRING token without the quotes and with escape sequences resolved.
std::string unescape_string_literal(const Token &token);

// ---------------------------------------------------------------------------------------------
// Diagnostics
// ---------------------------------------------------------------------------------------------

enum class Severity : U8
{
    WARNING,
    ERROR,
};

struct Diagnostic
{
    Severity severity = Severity::ERROR;
    SourceLocation loc;
    std::string message;
};

/// `file:line:col: error: message`, followed by the source line and a caret.
std::string format_diagnostic(const SourceManager &sources, const Diagnostic &diagnostic);

// ---------------------------------------------------------------------------------------------
// Lexing
// ---------------------------------------------------------------------------------------------

enum class LexMode : U8
{
    /// .basm / .binc / .bi sources. `;` comments, `$hex %bin @oct` literals, `#` directives,
    /// instructions, registers.
    ASSEMBLY,

    /// .ld linker scripts. `//` and `/* */` comments, `0x` / `0b` literals, `;` `=` `@`,
    /// ENTRY / SECTIONS keywords and the `.text` `.data` `.bss` section names.
    LINKER_SCRIPT,
};

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

struct LexResult
{
    /// Always ends with END_OF_FILE. Without `keep_newlines=false` the token before that is
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

/// Lexes the already registered source `id`.
LexResult lex(const SourceManager &sources, SourceId id, const LexOptions &options = {});

/// Convenience: registers `text` and lexes it.
LexResult lex_text(SourceManager &sources, std::string name, std::string text,
                   const LexOptions &options = {});

/// Logs every diagnostic of `result` (with its source line and caret). If any of them is an
/// error, terminates through AEMU_FATAL (which throws under FatalAction::Throw).
void fatal_if_errors(const SourceManager &sources, const LexResult &result);

/// Logs `message` at `loc` (file:line:col, source line, caret) and terminates through AEMU_FATAL.
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

    explicit TokenCursor(std::span<const Token> tokens) :
        m_tokens(tokens)
    {
    }

    /// Token `ahead` positions from the current one without consuming.
    const Token &peek(std::size_t ahead = 0) const;

    /// Consumes and returns the current token. Never moves past END_OF_FILE.
    const Token &next();

    bool at_end() const
    {
        return peek().type == TokenType::END_OF_FILE;
    }

    bool check(TokenType type) const
    {
        return peek().type == type;
    }

    bool check_any(std::initializer_list<TokenType> types) const
    {
        return peek().is_one_of(types);
    }

    /// Consumes the current token if it has this type.
    bool accept(TokenType type);

    /// Like accept but returns the consumed token, nullptr if the type did not match.
    const Token *try_next(TokenType type);

    /// Skips consecutive NEWLINE tokens.
    void skip_newlines();

    /// Consumes tokens up to and including the next NEWLINE (or up to END_OF_FILE). Use it to
    /// resynchronize after a statement level error.
    void skip_line();

    /// Consumes the rest of the current line including its NEWLINE and returns the tokens before
    /// the NEWLINE. The span stays valid as long as the underlying token list does.
    std::span<const Token> take_line();

    std::size_t position() const
    {
        return m_pos;
    }

    /// Save/restore for backtracking.
    std::size_t mark() const
    {
        return m_pos;
    }

    void rewind(std::size_t mark)
    {
        m_pos = mark;
    }

    std::span<const Token> tokens() const
    {
        return m_tokens;
    }

  private:
    std::span<const Token> m_tokens;
    std::size_t m_pos = 0;
};

} // namespace basm