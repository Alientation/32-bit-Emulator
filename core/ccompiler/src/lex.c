#include "ccompiler/lex.h"

#include "ccompiler/util.h"

#include <stdarg.h>
#include <stdio.h>
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
/// `::` is C23's, for attributes.
static const char *const punctuators[] = {
    "%:%:", "<<=", ">>=", "...", "==", "!=", "<=", ">=", "->", "+=", "-=", "*=", "/=", "%=",
    "&=",   "|=",  "^=",  "&&",  "||", "++", "--", "<<", ">>", "##", "::", "<:", ":>", "<%",
    "%>",   "%:",  "+",   "-",   "*",  "/",  "%",  "&",  "|",  "^",  "~",  "!",  "<",  ">",
    "=",    "?",   ":",   ";",   ",",  ".",  "(",  ")",  "{",  "}",  "[",  "]",  "#",
};

/// The digraphs and the punctuators they stand for.
static const char *const digraphs[][2] = {
    {"<:", "["}, {":>", "]"}, {"<%", "{"}, {"%>", "}"}, {"%:", "#"}, {"%:%:", "##"},
};

/// The encodings of character constants and string literals, by their prefix.
typedef enum
{
    /// No prefix: `char`, the source bytes as they are and UTF-8 for `\u`.
    ENC_PLAIN,
    /// `u8`: UTF-8, of `char` (C17).
    ENC_UTF8,
    /// `u`: UTF-16, of `char16_t` (`unsigned short`).
    ENC_UTF16,
    /// `U`: UTF-32, of `char32_t` (`unsigned int`).
    ENC_UTF32,
    /// `L`: UTF-32, of `wchar_t` (`int`, docs/abi.md).
    ENC_WIDE,
} Encoding;

static bool is_digit(char c)
{
    return c >= '0' && c <= '9';
}

/// A byte of a UTF-8 sequence is a letter: a name can have any character that is not ASCII.
static bool is_ident_start(char c)
{
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_'
           || (unsigned char) c >= 0x80;
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

SourceFile *read_source_file(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (!fp)
    {
        return NULL;
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
    return new_source_file(path, contents);
}

/// @return The length of the line end at `p`: 2 for `\r\n`, 1 for `\n` or `\r`, 0 for none.
static size_t newline_length(const char *p)
{
    if (p[0] == '\r' && p[1] == '\n')
    {
        return 2;
    }
    return p[0] == '\n' || p[0] == '\r' ? 1 : 0;
}

SourceFile *new_source_file(const char *name, const char *text)
{
    if (strncmp(text, "\xEF\xBB\xBF", 3) == 0)
    {
        text += 3;
    }
    // The text only gets shorter: a line end becomes one character, and each spliced line gives
    // back the newline it took.
    char *contents = arena_alloc(strlen(text) + 1);
    size_t length = 0;
    int spliced_lines = 0;
    const char *p = text;
    while (*p)
    {
        if (p[0] == '\\' && newline_length(p + 1))
        {
            p += 1 + newline_length(p + 1);
            spliced_lines++;
            continue;
        }
        if (newline_length(p))
        {
            p += newline_length(p);
            contents[length++] = '\n';
            for (; spliced_lines > 0; spliced_lines--)
            {
                contents[length++] = '\n';
            }
            continue;
        }
        contents[length++] = *p++;
    }
    for (; spliced_lines > 0; spliced_lines--)
    {
        contents[length++] = '\n';
    }

    SourceFile *file = arena_alloc(sizeof(SourceFile));
    file->name = name;
    file->contents = contents;
    return file;
}

/// @return The punctuator that a digraph stands for, or the punctuator itself.
static const char *punct_meaning(const Token *tok, size_t *len)
{
    for (size_t i = 0; i < sizeof(digraphs) / sizeof(digraphs[0]); i++)
    {
        if (strlen(digraphs[i][0]) == tok->len && memcmp(tok->loc, digraphs[i][0], tok->len) == 0)
        {
            *len = strlen(digraphs[i][1]);
            return digraphs[i][1];
        }
    }
    *len = tok->len;
    return tok->loc;
}

bool token_is(const Token *tok, const char *text)
{
    if (tok->kind != TK_PUNCT && tok->kind != TK_KEYWORD && tok->kind != TK_IDENT)
    {
        return false;
    }
    size_t len = tok->len;
    const char *spelling = tok->kind == TK_PUNCT ? punct_meaning(tok, &len) : tok->loc;
    return strlen(text) == len && memcmp(spelling, text, len) == 0;
}

char *token_text(const Token *tok)
{
    return arena_strndup(tok->loc, tok->len);
}

void error_tok(const Token *tok, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verror_at(tok->file_name, tok->line_no, tok->file->contents, tok->loc, fmt, ap);
    va_end(ap);
}

void warn_tok(const Token *tok, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vwarn_at(tok->file_name, tok->line_no, tok->file->contents, tok->loc, fmt, ap);
    va_end(ap);
}

/// Reports an error at a place in a file that is not a token yet.
static void error_in_file(const SourceFile *file, int line_no, const char *loc, const char *fmt,
                          ...)
{
    va_list ap;
    va_start(ap, fmt);
    verror_at(file->name, line_no, file->contents, loc, fmt, ap);
    va_end(ap);
}

/// Reports an error at a place inside a token, e.g. at an escape sequence of a string.
static void error_in_token(const Token *tok, const char *loc, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verror_at(tok->file_name, tok->line_no, tok->file->contents, loc, fmt, ap);
    va_end(ap);
}

/// @return The end of the character constant or string literal whose opening quote is at `p`,
/// after the closing quote; null if the line or the file ends before it.
static const char *quoted_end(const char *p)
{
    const char quote = *p;
    for (p++; *p != quote; p++)
    {
        if (*p == '\\')
        {
            p++;
        }
        if (*p == '\n' || *p == '\0')
        {
            return NULL;
        }
    }
    return p + 1;
}

/// @return The length of the encoding prefix of a string literal (`u8`, `u`, `U`, `L`) or of a
/// character constant (`u`, `U`, `L`) at `p`, 0 if there is none. The quote follows it.
static size_t literal_prefix_length(const char *p)
{
    if (p[0] == 'u' && p[1] == '8' && p[2] == '"')
    {
        return 2;
    }
    if ((p[0] == 'u' || p[0] == 'U' || p[0] == 'L') && (p[1] == '"' || p[1] == '\''))
    {
        return 1;
    }
    return 0;
}

/// @return The end of the preprocessing number that starts at `p` (a digit, or `.` and a digit).
static const char *pp_number_end(const char *p)
{
    p++;
    for (;;)
    {
        if ((*p == 'e' || *p == 'E' || *p == 'p' || *p == 'P') && (p[1] == '+' || p[1] == '-'))
        {
            p += 2;
        }
        else if (is_ident_char(*p) || *p == '.')
        {
            p++;
        }
        else if (*p == '\'' && is_ident_char(p[1]))
        {
            p += 2;
        }
        else
        {
            return p;
        }
    }
}

/// @return The length of the punctuator at `p`, 0 if there is none.
static size_t punctuator_length(const char *p)
{
    for (size_t i = 0; i < sizeof(punctuators) / sizeof(punctuators[0]); i++)
    {
        const size_t len = strlen(punctuators[i]);
        if (strncmp(p, punctuators[i], len) == 0)
        {
            return len;
        }
    }
    return 0;
}

Token *tokenize(SourceFile *file)
{
    Token head = {0};
    Token *cur = &head;
    const char *p = file->contents;
    int line_no = 1;
    bool at_bol = true;
    bool has_space = false;

    while (*p)
    {
        if (p[0] == '/' && p[1] == '/')
        {
            // The newline after it starts the next token's line, so the comment is not a space.
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
                error_in_file(file, line_no, p, "unterminated comment");
            }
            // The comment is one space: a token after it is not at the start of a line, even if
            // the comment spans lines.
            for (; p < end; p++)
            {
                line_no += *p == '\n';
            }
            p = end + 2;
            has_space = true;
            continue;
        }
        if (*p == '\n')
        {
            p++;
            line_no++;
            at_bol = true;
            has_space = false;
            continue;
        }
        if (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\f' || *p == '\v')
        {
            p++;
            has_space = true;
            continue;
        }

        TokenKind kind;
        const char *end;
        const size_t prefix = literal_prefix_length(p);
        const char *quoted = *p == '"' || *p == '\'' || prefix ? quoted_end(p + prefix) : NULL;
        if (is_digit(*p) || (*p == '.' && is_digit(p[1])))
        {
            kind = TK_PP_NUM;
            end = pp_number_end(p);
        }
        else if (quoted)
        {
            kind = p[prefix] == '"' ? TK_PP_STR : TK_PP_CHAR;
            end = quoted;
        }
        else if (is_ident_start(*p))
        {
            kind = TK_IDENT;
            end = p + 1;
            while (is_ident_char(*end))
            {
                end++;
            }
        }
        else
        {
            // A character that starts no token is a token of its own.
            const size_t len = punctuator_length(p);
            kind = len ? TK_PUNCT : TK_PP_OTHER;
            end = p + (len ? len : 1);
        }

        Token *tok = arena_alloc(sizeof(Token));
        tok->kind = kind;
        tok->file = file;
        tok->loc = p;
        tok->len = (size_t) (end - p);
        tok->file_name = file->name;
        tok->line_no = line_no;
        tok->at_bol = at_bol;
        tok->has_space = has_space;
        cur->next = tok;
        cur = tok;
        p = end;
        at_bol = false;
        has_space = false;
    }

    Token *eof = arena_alloc(sizeof(Token));
    eof->kind = TK_EOF;
    eof->file = file;
    eof->loc = p;
    eof->file_name = file->name;
    eof->line_no = line_no;
    eof->at_bol = true;
    cur->next = eof;
    return head.next;
}

/// @return The end of a sequence of digits of `base` that starts at `p`, with a `'` allowed
/// between two digits. `p` itself if there is no digit.
static const char *skip_digits(const char *p, int base)
{
    const char *start = p;
    for (;;)
    {
        if (digit_value(*p) >= 0 && digit_value(*p) < base)
        {
            p++;
        }
        else if (*p == '\'' && p != start && digit_value(p[1]) >= 0 && digit_value(p[1]) < base)
        {
            p++;
        }
        else
        {
            return p;
        }
    }
}

/// The types an integer constant can have, the first one its value fits in (C17 6.4.4.1):
/// by whether it has a `u` suffix, its `l` suffix (none, `l`, `ll`) and whether it is decimal.
static Type **const int_types_signed_decimal[3][4] = {
    {&ty_int, &ty_long, &ty_llong, NULL},
    {&ty_long, &ty_llong, NULL},
    {&ty_llong, NULL},
};
static Type **const int_types_signed_other[3][7] = {
    {&ty_int, &ty_uint, &ty_long, &ty_ulong, &ty_llong, &ty_ullong, NULL},
    {&ty_long, &ty_ulong, &ty_llong, &ty_ullong, NULL},
    {&ty_llong, &ty_ullong, NULL},
};
static Type **const int_types_unsigned[3][4] = {
    {&ty_uint, &ty_ulong, &ty_ullong, NULL},
    {&ty_ulong, &ty_ullong, NULL},
    {&ty_ullong, NULL},
};

/// @return Whether `value` can be represented in the integer type `ty`.
static bool fits_in(Int64 value, const Type *ty)
{
    if (ty->size == 8)
    {
        return ty->is_unsigned || (value.hi >> 31) == 0;
    }
    return value.hi == 0 && (ty->is_unsigned || (value.lo >> 31) == 0);
}

/// Converts an integer constant: decimal, `0x` hexadecimal, `0b` binary or octal with a leading
/// 0, with `'` between digits and the suffixes `u`, `l` and `ll` in either order.
///
/// @return Whether the token is an integer constant, false if it may be a floating one.
static bool convert_integer(Token *tok)
{
    const char *p = tok->loc;
    const char *end = tok->loc + tok->len;
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
    else if (p[0] == '0')
    {
        base = 8;
    }

    const char *digits_end = skip_digits(p, base);
    if (digits_end == p)
    {
        return false;
    }
    Int64 value = {0, 0};
    bool too_large = false;
    const Int64 base64 = int64_from_uint((unsigned int) base);
    for (; p < digits_end; p++)
    {
        if (*p == '\'')
        {
            continue;
        }
        // value * base + digit has to stay below 2^64.
        Int64 remainder;
        const Int64 digit = int64_from_uint((unsigned int) digit_value(*p));
        const Int64 limit =
            int64_div(int64_sub(int64_from_int(-1), digit), base64, false, &remainder);
        too_large = too_large || int64_lt(limit, value, false);
        value = int64_add(int64_mul(value, base64), digit);
    }

    bool is_unsigned = false;
    int longs = 0;
    for (;;)
    {
        if ((*p == 'u' || *p == 'U') && !is_unsigned)
        {
            is_unsigned = true;
            p++;
        }
        else if (longs == 0 && ((p[0] == 'l' && p[1] == 'l') || (p[0] == 'L' && p[1] == 'L')))
        {
            longs = 2;
            p += 2;
        }
        else if (longs == 0 && (*p == 'l' || *p == 'L'))
        {
            longs = 1;
            p++;
        }
        else
        {
            break;
        }
    }
    if (p != end)
    {
        return false;
    }
    if (too_large)
    {
        error_tok(tok, "integer constant is too large");
    }

    Type **const *types = is_unsigned  ? int_types_unsigned[longs]
                          : base == 10 ? int_types_signed_decimal[longs]
                                       : int_types_signed_other[longs];
    for (; *types; types++)
    {
        if (fits_in(value, **types))
        {
            tok->kind = TK_NUM;
            tok->ty = **types;
            tok->value = (int) value.lo;
            tok->value_high = value.hi;
            return true;
        }
    }
    error_tok(tok, "integer constant is too large for its type");
    return false;
}

/// Converts a floating constant, decimal or hexadecimal (`0x1.8p3`), with the suffixes `f` and
/// `l`. It keeps its spelling for now, see `Token::value`.
///
/// @return Whether the token is a floating constant.
static bool convert_float(Token *tok)
{
    const char *p = tok->loc;
    const char *end = tok->loc + tok->len;
    int base = 10;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X'))
    {
        base = 16;
        p += 2;
    }
    const char *mantissa = p;
    p = skip_digits(p, base);
    bool has_digits = p != mantissa;
    bool has_point = false;
    if (*p == '.')
    {
        has_point = true;
        const char *fraction = ++p;
        p = skip_digits(p, base);
        has_digits = has_digits || p != fraction;
    }
    if (!has_digits)
    {
        return false;
    }

    // A hexadecimal constant has to have its exponent; a decimal one has a point or an exponent.
    const bool has_exponent = base == 16 ? (*p == 'p' || *p == 'P') : (*p == 'e' || *p == 'E');
    if (has_exponent)
    {
        p++;
        if (*p == '+' || *p == '-')
        {
            p++;
        }
        const char *exponent = p;
        p = skip_digits(p, 10);
        if (p == exponent)
        {
            return false;
        }
    }
    else if (base == 16 || !has_point)
    {
        return false;
    }

    tok->ty = ty_double;
    if (*p == 'f' || *p == 'F')
    {
        tok->ty = ty_float;
        p++;
    }
    else if (*p == 'l' || *p == 'L')
    {
        tok->ty = ty_ldouble;
        p++;
    }
    if (p != end)
    {
        return false;
    }
    tok->kind = TK_NUM;
    return true;
}

/// @return Whether the spelling of a number that is not valid looks like a floating constant,
/// for the message.
static bool looks_floating(const Token *tok)
{
    const bool hex = tok->len > 1 && (tok->loc[1] == 'x' || tok->loc[1] == 'X');
    for (size_t i = 0; i < tok->len; i++)
    {
        const char c = tok->loc[i];
        if (c == '.' || (hex && (c == 'p' || c == 'P')) || (!hex && (c == 'e' || c == 'E')))
        {
            return true;
        }
    }
    return false;
}

/// @return The largest code unit of an encoding.
static unsigned int max_code_unit(Encoding enc)
{
    if (enc == ENC_UTF16)
    {
        return 0xFFFF;
    }
    return enc == ENC_UTF32 || enc == ENC_WIDE ? 0xFFFFFFFFu : 0xFF;
}

/// Appends a code point in an encoding.
///
/// @param[out] units Where the code units go.
/// @return The number of code units.
static size_t encode_code_point(unsigned int c, Encoding enc, unsigned int *units)
{
    if (enc == ENC_UTF32 || enc == ENC_WIDE)
    {
        units[0] = c;
        return 1;
    }
    if (enc == ENC_UTF16)
    {
        if (c < 0x10000)
        {
            units[0] = c;
            return 1;
        }
        c -= 0x10000;
        units[0] = 0xD800 + (c >> 10);
        units[1] = 0xDC00 + (c & 0x3FF);
        return 2;
    }
    if (c < 0x80)
    {
        units[0] = c;
        return 1;
    }
    if (c < 0x800)
    {
        units[0] = 0xC0 | (c >> 6);
        units[1] = 0x80 | (c & 0x3F);
        return 2;
    }
    if (c < 0x10000)
    {
        units[0] = 0xE0 | (c >> 12);
        units[1] = 0x80 | ((c >> 6) & 0x3F);
        units[2] = 0x80 | (c & 0x3F);
        return 3;
    }
    units[0] = 0xF0 | (c >> 18);
    units[1] = 0x80 | ((c >> 12) & 0x3F);
    units[2] = 0x80 | ((c >> 6) & 0x3F);
    units[3] = 0x80 | (c & 0x3F);
    return 4;
}

/// Reads one UTF-8 character of the source.
///
/// @param[in] tok The token it is in, for the message.
/// @param[in,out] p The first byte; moved past the character.
/// @return The code point. Exits with an error if the bytes are not UTF-8.
static unsigned int decode_utf8(const Token *tok, const char **p)
{
    const unsigned char *s = (const unsigned char *) *p;
    int length = 1;
    unsigned int c = s[0];
    unsigned int min = 0;
    if (s[0] >= 0xF0 && s[0] < 0xF8)
    {
        length = 4;
        c = s[0] & 0x07;
        min = 0x10000;
    }
    else if (s[0] >= 0xE0 && s[0] < 0xF0)
    {
        length = 3;
        c = s[0] & 0x0F;
        min = 0x800;
    }
    else if (s[0] >= 0xC0 && s[0] < 0xE0)
    {
        length = 2;
        c = s[0] & 0x1F;
        min = 0x80;
    }
    else if (s[0] >= 0x80)
    {
        error_in_token(tok, *p, "invalid UTF-8");
    }
    for (int i = 1; i < length; i++)
    {
        if ((s[i] & 0xC0) != 0x80)
        {
            error_in_token(tok, *p, "invalid UTF-8");
        }
        c = c << 6 | (s[i] & 0x3F);
    }
    if (c < min || c > 0x10FFFF || (c >= 0xD800 && c <= 0xDFFF))
    {
        error_in_token(tok, *p, "invalid UTF-8");
    }
    *p += length;
    return c;
}

/// Reads an escape sequence.
///
/// @param[in] tok The token it is in, for the messages.
/// @param[in,out] p The backslash; moved past the sequence.
/// @param[out] is_code_point Whether the value is a code point (`\u`, `\U`) that still has to
/// be encoded, rather than a code unit.
/// @return The value.
static unsigned int read_escape(const Token *tok, const char **p, Encoding enc, bool *is_code_point)
{
    const char *start = *p;
    const char *s = start + 1;
    *is_code_point = false;
    unsigned int value = 0;
    if (*s >= '0' && *s <= '7')
    {
        for (int i = 0; i < 3 && *s >= '0' && *s <= '7'; i++)
        {
            value = value * 8 + (unsigned int) (*s++ - '0');
        }
        if (value > max_code_unit(enc))
        {
            error_in_token(tok, start, "octal escape sequence out of range");
        }
    }
    else if (*s == 'x')
    {
        s++;
        if (digit_value(*s) < 0)
        {
            error_in_token(tok, start, "\\x used with no following hex digits");
        }
        for (; digit_value(*s) >= 0; s++)
        {
            if (value > (max_code_unit(enc) >> 4))
            {
                error_in_token(tok, start, "hex escape sequence out of range");
            }
            value = value * 16 + (unsigned int) digit_value(*s);
        }
        if (value > max_code_unit(enc))
        {
            error_in_token(tok, start, "hex escape sequence out of range");
        }
    }
    else if (*s == 'u' || *s == 'U')
    {
        const int digits = *s == 'u' ? 4 : 8;
        s++;
        for (int i = 0; i < digits; i++, s++)
        {
            if (digit_value(*s) < 0)
            {
                error_in_token(tok, start, "incomplete universal character name");
            }
            value = value * 16 + (unsigned int) digit_value(*s);
        }
        // C17 6.4.3: not below U+00A0 except $, @ and `, no surrogate, nothing beyond Unicode.
        if ((value < 0xA0 && value != '$' && value != '@' && value != '`')
            || (value >= 0xD800 && value <= 0xDFFF) || value > 0x10FFFF)
        {
            error_in_token(tok, start, "invalid universal character name");
        }
        *is_code_point = true;
    }
    else
    {
        // Pairs of the character after the backslash and its value.
        static const char escapes[] = "''\"\"??\\\\a\ab\bf\fn\nr\rt\tv\v";
        size_t i = 0;
        while (escapes[i] && escapes[i] != *s)
        {
            i += 2;
        }
        if (!escapes[i] || *s == '\0')
        {
            error_in_token(tok, start, "unknown escape sequence");
        }
        value = (unsigned char) escapes[i + 1];
        s++;
    }
    *p = s;
    return value;
}

/// Decodes the text between the quotes of a constant or string into the code units of an
/// encoding. A source character or a `\u` becomes the units of its code point, an octal or
/// hexadecimal escape is one unit; a source byte of a plain or `u8` literal is a unit as it is.
///
/// @param[out] units Where the units go: room for one per source character is enough.
/// @return The number of units.
static size_t decode_literal(const Token *tok, const char *p, const char *end, Encoding enc,
                             unsigned int *units)
{
    size_t count = 0;
    while (p < end)
    {
        if (*p == '\\')
        {
            bool is_code_point;
            const unsigned int value = read_escape(tok, &p, enc, &is_code_point);
            count += is_code_point ? encode_code_point(value, enc, units + count) : 0;
            if (!is_code_point)
            {
                units[count++] = value;
            }
        }
        else if (enc == ENC_PLAIN || enc == ENC_UTF8)
        {
            units[count++] = (unsigned char) *p++;
        }
        else
        {
            count += encode_code_point(decode_utf8(tok, &p), enc, units + count);
        }
    }
    return count;
}

/// @return The encoding of a constant or string by its prefix, and the length of the prefix.
static Encoding literal_encoding(const Token *tok, size_t *prefix)
{
    *prefix = literal_prefix_length(tok->loc);
    if (*prefix == 2)
    {
        return ENC_UTF8;
    }
    if (*prefix == 0)
    {
        return ENC_PLAIN;
    }
    return tok->loc[0] == 'u' ? ENC_UTF16 : tok->loc[0] == 'U' ? ENC_UTF32 : ENC_WIDE;
}

/// @return The type of the elements of a string, and of a character constant with a prefix.
static Type *encoding_type(Encoding enc)
{
    if (enc == ENC_UTF16)
    {
        return ty_ushort;
    }
    if (enc == ENC_UTF32)
    {
        return ty_uint;
    }
    return enc == ENC_WIDE ? ty_int : ty_char;
}

/// Converts a character constant. A plain one is an `int`: one character is the value of that
/// `char` (which is signed), and up to four make a multi-character constant whose value is the
/// bytes, the first one highest (as GCC does), with a warning. One with a prefix is one code unit
/// of the type of its encoding.
static void convert_char(Token *tok)
{
    size_t prefix;
    const Encoding enc = literal_encoding(tok, &prefix);
    unsigned int *units = arena_alloc(tok->len * sizeof(unsigned int));
    const size_t count =
        decode_literal(tok, tok->loc + prefix + 1, tok->loc + tok->len - 1, enc, units);
    if (count == 0)
    {
        error_tok(tok, "empty character constant");
    }

    unsigned int value = units[0];
    tok->ty = ty_int;
    if (enc != ENC_PLAIN)
    {
        if (count > 1)
        {
            error_tok(tok, "a character constant with a prefix has to be one code unit");
        }
        tok->ty = encoding_type(enc);
    }
    else if (count == 1)
    {
        value = (unsigned int) (int) (signed char) value;
    }
    else if (count > 4)
    {
        error_tok(tok, "character constant is too long");
    }
    else
    {
        warn_tok(tok, "multi-character character constant");
        for (size_t i = 1; i < count; i++)
        {
            value = value << 8 | units[i];
        }
    }
    tok->kind = TK_NUM;
    tok->value = (int) value;
    tok->value_high = !tok->ty->is_unsigned && (value >> 31) ? ~0u : 0u;
}

void convert_constant(Token *tok)
{
    if (tok->kind == TK_PP_CHAR)
    {
        convert_char(tok);
        return;
    }
    if (!convert_integer(tok) && !convert_float(tok))
    {
        error_tok(tok,
                  looks_floating(tok) ? "invalid floating constant" : "invalid integer constant");
    }
}

/// Converts string literals that are next to each other into one `TK_STR` (C17 6.4.5): the
/// encoding is the prefix they have, they may not have different ones.
///
/// @param[in,out] first The first string literal, which becomes the `TK_STR`.
static void convert_strings(Token *first)
{
    Encoding enc = ENC_PLAIN;
    size_t capacity = 1;
    Token *after = first;
    for (; after->kind == TK_PP_STR; after = after->next)
    {
        size_t prefix;
        const Encoding this_enc = literal_encoding(after, &prefix);
        if (this_enc != ENC_PLAIN && enc != ENC_PLAIN && this_enc != enc)
        {
            error_tok(after, "string literals with different prefixes cannot be concatenated");
        }
        enc = this_enc != ENC_PLAIN ? this_enc : enc;
        capacity += after->len;
    }

    unsigned int *units = arena_alloc(capacity * sizeof(unsigned int));
    size_t count = 0;
    for (Token *t = first; t != after; t = t->next)
    {
        size_t prefix;
        literal_encoding(t, &prefix);
        count += decode_literal(t, t->loc + prefix + 1, t->loc + t->len - 1, enc, units + count);
    }
    units[count++] = 0;

    Type *ty = encoding_type(enc);
    first->kind = TK_STR;
    first->ty = ty;
    first->str_size = count * (size_t) ty->size;
    first->str = arena_alloc(first->str_size);
    for (size_t i = 0; i < count; i++)
    {
        for (int byte = 0; byte < ty->size; byte++)
        {
            first->str[i * (size_t) ty->size + (size_t) byte] = (char) (units[i] >> (8 * byte));
        }
    }
    first->next = after;
}

Token *convert_pp_tokens(Token *tok)
{
    for (Token *t = tok; t->kind != TK_EOF; t = t->next)
    {
        if (t->kind == TK_IDENT)
        {
            for (size_t i = 0; i < sizeof(keywords) / sizeof(keywords[0]); i++)
            {
                if (token_is(t, keywords[i]))
                {
                    t->kind = TK_KEYWORD;
                    break;
                }
            }
        }
        else if (t->kind == TK_PP_NUM || t->kind == TK_PP_CHAR)
        {
            convert_constant(t);
        }
        else if (t->kind == TK_PP_STR)
        {
            convert_strings(t);
        }
        else if (t->kind == TK_PP_OTHER && (t->loc[0] == '"' || t->loc[0] == '\''))
        {
            error_tok(t, "missing terminating %c character", t->loc[0]);
        }
        else if (t->kind == TK_PP_OTHER)
        {
            error_tok(t, "invalid token");
        }
    }
    return tok;
}
