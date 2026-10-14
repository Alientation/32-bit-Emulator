#ifndef CCOMPILER_LEX_H
#define CCOMPILER_LEX_H

#include <stdbool.h>
#include <stddef.h>

/// A source file read into memory.
typedef struct
{
    const char *name;
    /// The text, ending with a 0 byte.
    const char *contents;
} SourceFile;

typedef enum
{
    /// A name that is not a keyword.
    TK_IDENT,
    /// A keyword of C17 or of the C23 subset, see docs/ccompiler.md.
    TK_KEYWORD,
    /// An operator or a separator.
    TK_PUNCT,
    /// An integer constant. Its value is in `Token::value`.
    TK_NUM,
    TK_EOF,
} TokenKind;

typedef struct Token Token;

struct Token
{
    TokenKind kind;
    /// The following token. The last one is the `TK_EOF`.
    Token *next;
    SourceFile *file;
    /// The first character of the token in `file->contents`, and the number of characters.
    const char *loc;
    size_t len;
    /// The value of a `TK_NUM`.
    int value;
};

/// Reads a file.
///
/// @param[in] path The file to read.
/// @return The file. Exits with an error if it cannot be read.
SourceFile *read_source_file(const char *path);

/// Splits a file into tokens. Comments and white space are dropped.
///
/// @param[in] file The file. It has to stay alive as long as the tokens do.
/// @return The first token. Exits with an error at a character that starts no token and at an
/// integer constant that is not valid or does not fit in an `int`.
Token *tokenize(SourceFile *file);

/// @param[in] tok A token.
/// @param[in] text An operator or keyword, e.g. "+" or "return".
/// @return Whether `tok` is that operator or keyword. An identifier or number is never.
bool token_is(const Token *tok, const char *text);

/// Reports an error at a token and exits with status 1, see error_at().
///
/// @param[in] tok Where the error is.
/// @param[in] fmt A printf format.
void error_tok(const Token *tok, const char *fmt, ...);

#endif
