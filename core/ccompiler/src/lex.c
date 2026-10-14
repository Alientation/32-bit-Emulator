#include "ccompiler/lex.h"

#include "ccompiler/util.h"

#include <limits.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// Every keyword of C17, and the ones that C23 adds that this compiler takes (docs/ccompiler.md).
/// The parser decides which of them it supports.
static const char *const keywords[] = {
    "alignas",       "alignof",   "auto",
    "bool",          "break",     "case",
    "char",          "const",     "constexpr",
    "continue",      "default",   "do",
    "double",        "else",      "enum",
    "extern",        "false",     "float",
    "for",           "goto",      "if",
    "inline",        "int",       "long",
    "nullptr",       "register",  "restrict",
    "return",        "short",     "signed",
    "sizeof",        "static",    "static_assert",
    "struct",        "switch",    "thread_local",
    "true",          "typedef",   "typeof",
    "typeof_unqual", "union",     "unsigned",
    "void",          "volatile",  "while",
    "_Alignas",      "_Alignof",  "_Atomic",
    "_Bool",         "_Complex",  "_Generic",
    "_Imaginary",    "_Noreturn", "_Static_assert",
    "_Thread_local",
};

/// The operators and separators, the longer ones first so that the first match is the longest.
static const char *const punctuators[] = {
    "<<=", ">>=", "...", "==", "!=", "<=", ">=", "->", "+=", "-=", "*=", "/=",
    "%=",  "&=",  "|=",  "^=", "&&", "||", "++", "--", "<<", ">>", "##", "+",
    "-",   "*",   "/",   "%",  "&",  "|",  "^",  "~",  "!",  "<",  ">",  "=",
    "?",   ":",   ";",   ",",  ".",  "(",  ")",  "{",  "}",  "[",  "]",  "#",
};

SourceFile *read_source_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
    {
        error("cannot open '%s'", path);
    }

    size_t capacity = 4096;
    size_t length = 0;
    char *contents = arena_alloc(capacity);
    for (;;)
    {
        if (length + 1 == capacity)
        {
            char *bigger = arena_alloc(capacity * 2);
            memcpy(bigger, contents, length);
            contents = bigger;
            capacity *= 2;
        }
        const size_t count = fread(contents + length, 1, capacity - length - 1, fp);
        if (count == 0)
        {
            break;
        }
        length += count;
    }
    if (ferror(fp))
    {
        error("cannot read '%s'", path);
    }
    fclose(fp);

    SourceFile *file = arena_alloc(sizeof(SourceFile));
    file->name = path;
    file->contents = contents;
    return file;
}

bool token_is(const Token *tok, const char *text)
{
    return (tok->kind == TK_PUNCT || tok->kind == TK_KEYWORD) && strlen(text) == tok->len
           && memcmp(tok->loc, text, tok->len) == 0;
}

void error_tok(const Token *tok, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verror_at(tok->file->name, tok->file->contents, tok->loc, fmt, ap);
    va_end(ap);
}

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

static bool is_ident_start(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_ident_char(char c)
{
    return is_ident_start(c) || is_digit(c);
}

/// @return The value of a digit in base 16 and below, or -1 if `c` is no digit.
static int digit_value(char c)
{
    if (is_digit(c))
    {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f')
    {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F')
    {
        return c - 'A' + 10;
    }
    return -1;
}

static Token *new_token(TokenKind kind, SourceFile *file, const char *start, const char *end)
{
    Token *tok = arena_alloc(sizeof(Token));
    tok->kind = kind;
    tok->file = file;
    tok->loc = start;
    tok->len = (size_t) (end - start);
    return tok;
}

/// Reads an integer constant: decimal, `0x` hexadecimal, `0b` binary or octal with a leading 0,
/// with `'` between digits. There are no suffixes yet.
///
/// @param[out] tok The token, its value is set.
/// @param[in] file The file the token is in.
/// @param[in] start The first digit.
/// @return The end of the constant.
static const char *read_number(Token *tok, SourceFile *file, const char *start)
{
    const char *p = start;
    int base = 10;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
    {
        base = 16;
        p += 2;
    }
    else if (p[0] == '0' && (p[1] == 'b' || p[1] == 'B'))
    {
        base = 2;
        p += 2;
    }
    else if (p[0] == '0' && is_digit(p[1]))
    {
        base = 8;
    }

    const char *digits = p;
    unsigned int value = 0;
    for (;;)
    {
        int digit = digit_value(*p);
        if (*p == '\'' && digit_value(p[1]) >= 0 && digit_value(p[1]) < base && p != digits)
        {
            p++;
            continue;
        }
        if (digit < 0 || digit >= base)
        {
            break;
        }
        if (value > (UINT_MAX - (unsigned int) digit) / (unsigned int) base)
        {
            error_at(file->name, file->contents, start, "integer constant is too large");
        }
        value = value * (unsigned int) base + (unsigned int) digit;
        p++;
    }
    if (p == digits || is_ident_char(*p))
    {
        error_at(file->name, file->contents, start, "invalid integer constant");
    }
    if (value > INT_MAX)
    {
        error_at(file->name, file->contents, start,
                 "integer constant does not fit in an int (the other types are not supported yet)");
    }
    tok->value = (int) value;
    return p;
}

Token *tokenize(SourceFile *file)
{
    Token head = {0};
    Token *cur = &head;
    const char *p = file->contents;

    while (*p)
    {
        if (p[0] == '/' && p[1] == '/')
        {
            while (*p && *p != '\n')
            {
                p++;
            }
            continue;
        }
        if (p[0] == '/' && p[1] == '*')
        {
            const char *end = strstr(p + 2, "*/");
            if (!end)
            {
                error_at(file->name, file->contents, p, "unterminated comment");
            }
            p = end + 2;
            continue;
        }
        if (*p == ' ' || *p == '\t' || *p == '\n' || *p == '\r' || *p == '\f' || *p == '\v')
        {
            p++;
            continue;
        }

        if (is_digit(*p))
        {
            cur->next = new_token(TK_NUM, file, p, p);
            cur = cur->next;
            const char *end = read_number(cur, file, p);
            cur->len = (size_t) (end - p);
            p = end;
            continue;
        }

        if (is_ident_start(*p))
        {
            const char *start = p;
            while (is_ident_char(*p))
            {
                p++;
            }
            cur->next = new_token(TK_IDENT, file, start, p);
            cur = cur->next;
            for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++)
            {
                if (strlen(keywords[i]) == cur->len && memcmp(keywords[i], start, cur->len) == 0)
                {
                    cur->kind = TK_KEYWORD;
                    break;
                }
            }
            continue;
        }

        size_t i = 0;
        for (; i < sizeof(punctuators) / sizeof(punctuators[0]); i++)
        {
            const size_t len = strlen(punctuators[i]);
            if (strncmp(p, punctuators[i], len) == 0)
            {
                cur->next = new_token(TK_PUNCT, file, p, p + len);
                cur = cur->next;
                p += len;
                break;
            }
        }
        if (i == sizeof(punctuators) / sizeof(punctuators[0]))
        {
            error_at(file->name, file->contents, p, "invalid token");
        }
    }

    cur->next = new_token(TK_EOF, file, p, p);
    return head.next;
}
