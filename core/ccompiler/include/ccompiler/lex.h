#ifndef CCOMPILER_LEX_H
#define CCOMPILER_LEX_H

#include "ccompiler/type.h"

#include <stdbool.h>
#include <stddef.h>

/// A source file read into memory.
typedef struct
{
    /// The name the file was opened with, which is also what `__FILE__` is.
    const char *name;
    /// The text after the first two phases of translation, ending with a 0 byte: `\r\n` and `\r`
    /// are `\n`, a UTF-8 byte order mark is gone, and so is every backslash that ends a line,
    /// with the newline after it. For each of those the newline is put back after the end of the
    /// logical line, so that the tokens on the following lines keep their line numbers.
    const char *contents;
} SourceFile;

typedef enum
{
    /// A name that is not a keyword. Before convert_pp_tokens() the keywords are names too.
    TK_IDENT,
    /// An operator or a separator, see token_is() for the digraphs.
    TK_PUNCT,
    /// A keyword of C17 or of the C23 subset, see docs/ccompiler.md.
    TK_KEYWORD,
    /// An integer, floating or character constant, see `Token::value` and `Token::ty`.
    TK_NUM,
    /// A string literal, or several that were next to each other, see `Token::str`.
    TK_STR,
    /// The preprocessing tokens that convert_pp_tokens() turns into the kinds above: a number (a
    /// digit, or `.` and a digit, then letters, digits, `_`, `.`, `e+`, `e-`, `p+`, `p-` and `'`
    /// before a letter or digit; checked when it is converted), a character constant and a string
    /// literal, both with their prefix and quotes.
    TK_PP_NUM,
    TK_PP_CHAR,
    TK_PP_STR,
    /// A character that starts no other token, e.g. `@` or a quote without its closing one. It is
    /// only an error when it is still there after preprocessing, since `#if 0` and `#` can take
    /// it.
    TK_PP_OTHER,
    /// An empty macro argument, or an empty `__VA_OPT__`, while the preprocessor replaces a macro
    /// call (C17 6.10.3.3). It is never in the result.
    TK_PLACEMARKER,
    /// The end of a file. Every file ends with one; the preprocessor keeps the one of an
    /// included file as the mark where that file ends.
    TK_EOF,
} TokenKind;

typedef struct Token Token;

/// The names of the macros a token came out of, which may not expand again on it (the preprocessor
/// defines it).
typedef struct Hideset Hideset;

struct Token
{
    TokenKind kind;
    /// The following token. The last one is the `TK_EOF`.
    Token *next;
    SourceFile *file;
    /// The first character of the token in `file->contents`, and the number of characters. A
    /// token that the preprocessor makes (a pasted one, a string out of `#`) is in a file of its
    /// own.
    const char *loc;
    size_t len;
    /// Where the token is for messages and `__LINE__`: the file name and line number, which
    /// `#line` can change, and are those of the macro call for a token that the preprocessor made.
    const char *file_name;
    int line_no;
    /// Whether the token is the first of its line, and whether there is white space (or a
    /// comment) between it and the token before it.
    bool at_bol;
    bool has_space;

    /// The value of an integer or character constant (`TK_NUM`). An `unsigned long long` above
    /// `LLONG_MAX` is its bits, a negative number.
    long long value;
    /// The value of a floating constant (`TK_NUM` of a floating type), rounded to its type: a
    /// `float` constant is a `float` value.
    double fval;
    /// The type of a `TK_NUM`, and of the elements of a `TK_STR`.
    Type *ty;
    /// The bytes of a `TK_STR` in the target's layout, little endian elements of the size of
    /// `ty`, and the number of bytes, both with the 0 element at the end.
    char *str;
    size_t str_size;

    /// Set by the preprocessor: the macros the token may not expand again, and the macro name
    /// whose expansion the token came out of (null for a token of a file).
    Hideset *hideset;
    const Token *origin;
};

/// Reads a file.
///
/// @param[in] path The file to read.
/// @return The file, null if it cannot be opened. Exits with an error if it cannot be read.
SourceFile *read_source_file(const char *path);

/// Makes a source file of a text.
///
/// @param[in] name The name of the file.
/// @param[in] text The text, ending with a 0 byte. It is copied.
/// @return The file, with the first two phases of translation done, see `SourceFile::contents`.
SourceFile *new_source_file(const char *name, const char *text);

/// Splits a file into preprocessing tokens. Comments and white space are dropped, and recorded in
/// `Token::has_space` and `Token::at_bol`.
///
/// @param[in] file The file. It has to stay alive as long as the tokens do.
/// @return The first token. Exits with an error at a comment that is not closed.
Token *tokenize(SourceFile *file);

/// Turns the preprocessing tokens into the tokens of the parser (phases 5 to 7 of translation):
/// the keywords are `TK_KEYWORD`, numbers and character constants are `TK_NUM`, and string
/// literals that are next to each other are one `TK_STR`.
///
/// @param[in,out] tok The first token. The tokens are changed in place.
/// @return The first token. Exits with an error at a number, constant or string that is not
/// valid, and at a `TK_PP_OTHER`.
Token *convert_pp_tokens(Token *tok);

/// Turns a `TK_PP_NUM` or `TK_PP_CHAR` into a `TK_NUM`, as convert_pp_tokens() does.
///
/// @param[in,out] tok The token.
void convert_constant(Token *tok);

/// @param[in] tok A token.
/// @param[in] text A name, operator or keyword, e.g. "x", "+" or "return".
/// @return Whether `tok` is that name, operator or keyword. A digraph is the same as the operator
/// it stands for (`<:` is "[", `%:%:` is "##"). A number, string or other token is never.
bool token_is(const Token *tok, const char *text);

/// @param[in] tok A token.
/// @return Its spelling, a copy with a 0 byte at the end.
char *token_text(const Token *tok);

/// Reports an error at a token and exits with status 1, see verror_at().
///
/// @param[in] tok Where the error is.
/// @param[in] fmt A printf format.
void error_tok(const Token *tok, const char *fmt, ...);

/// Reports a warning at a token and returns, see vwarn_at().
///
/// @param[in] tok Where the problem is.
/// @param[in] fmt A printf format.
void warn_tok(const Token *tok, const char *fmt, ...);

#endif
