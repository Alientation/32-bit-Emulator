#pragma once

#include <ccompiler/ccompiler_options.h>

#include <inttypes.h>
#include <stddef.h>
#include <stdbool.h>

/**
 * TODOS:
 * - Extract Integer(char)/Float/String values from tokens.
 *      - Strings need to be compressed (ex, "\\n" should produce a single character '\n').
 *      - Add these checks to unit tests.
 * - Preprocessors need to be handled inline with lexing.
 * - Ensure line/column information is correct in all cases.
 */

typedef struct Token token_t;
typedef struct TokenArray token_array_t;

typedef struct SrcSpan
{
    // Offset into processed buffer.
    size_t proc_offset;

    // Source file.
    const char *file;

    // Line index in original source.
    size_t orig_line;

    // Starting column index in original source.
    size_t orig_col;
} srcspan_t;


typedef struct SrcMap
{
    // Array of spans.
    srcspan_t *spans;

    // Size of array.
    size_t count;

    // Capacity of array.
    size_t capacity;
} srcmap_t;


typedef struct LexerData
{
    // Compiler options.
    compiler_options_t options;

    // Shared pointer to file that is lexed.
    char *file;

    // NUL terminated string representing the source to run the lexer on.
    char *src;

    // NUL terminated string containing the original source.
    char *src_orig;

    // Length of the string not including the NUL terminator.
    size_t length;

    // Map offsets into processed buffer into an offset in the source file.
    srcmap_t srcmap;

    // Processed array of tokens.
    struct TokenArray_t
    {
        token_t *toks;

        // The number of valid tokens in the array.
        size_t tok_cnt;

        // The amount of memory (in terms of tokens) allocated for the array. Used for resizing.
        size_t tok_cap;
    } tokarr;

} lexer_data_t;

typedef enum TokenType
{
    TOKEN_ERROR,
    TOKEN_KEYWORD_AUTO,
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
    TOKEN_KEYWORD_STRUCT,
    TOKEN_KEYWORD_SWITCH,
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
    TOKEN_KEYWORD_THREAD_LOCAL,

    TOKEN_IDENTIFIER,
    TOKEN_I_CONSTANT,
    TOKEN_F_CONSTANT,
    TOKEN_STRING_LITERAL,

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
    TOKEN_QUESTION_MARK,
} tokentype_t;

#define NUM_TOKEN_TYPES (TOKEN_QUESTION_MARK + 1)

struct Token
{
    tokentype_t type;

    // Pointer into the shared overall source string, a lexeme of sorts.
    const char *src;

    // Length of token in the source string.
    size_t len;

    // Shared pointer to file this token is sourced from.
    const char *file;

    // Line in the source file.
    size_t line;

    // Column in the source file.
    size_t col;

    // Value associated with this token.
    union TokenData
    {
        uint64_t i_constant;
        double f_constant;
        char *s_constant;
    } cval;

    // Flags associated with this token.
    uint64_t flags;
};


// Flags for integer constants.
#define LFLAGS_INT          (0)
#define LFLAGS_LONG         (1)
#define LFLAGS_LONGLONG     (2)
#define LFLAGS_BITINT       (3)
#define LFLAGS_IGET_SIZE    (7)

#define LFLAGS_SIGNED       (1 << 3)
#define LFLAGS_UNSIGNED     (0 << 3)
#define LFLAGS_GET_SIGN     (1 << 3)

// Flags for float constants.
#define LFLAGS_FLOAT        (0)
#define LFLAGS_DOUBLE       (1)
#define LFLAGS_FGET_SIZE    (1)


bool lex_file (const char *filepath,
              lexer_data_t *lexer);
bool lex_str (const char *str,
             lexer_data_t *lexer);

void lexer_print (const lexer_data_t *lexer);
void lexer_init (lexer_data_t *lexer);
void lexer_register_compiler_options (lexer_data_t *lexer, compiler_options_t options);
void lexer_free (lexer_data_t *lexer);

/* Returns a string representation of a token. String is valid until the next token_tostr() call.
   Memory is not dynamically allocated. */
char *token_tostr (token_t *tok);
void token_print (token_t *tok);
