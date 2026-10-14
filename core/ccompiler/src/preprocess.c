#include "ccompiler/preprocess.h"

#include "ccompiler/util.h"

#include <string.h>

/// The macros that are defined before the file, as `#define` would have them: the version of C,
/// what is left out of it, and the data model of docs/abi.md.
static const char *const predefined_macros[] = {
    "__STDC__ 1",
    "__STDC_VERSION__ 201710L",
    "__STDC_HOSTED__ 1",
    "__STDC_UTF_16__ 1",
    "__STDC_UTF_32__ 1",
    "__STDC_NO_ATOMICS__ 1",
    "__STDC_NO_COMPLEX__ 1",
    "__STDC_NO_THREADS__ 1",
    "__STDC_NO_VLA__ 1",
    "__bcc__ 1",
    "__ILP32__ 1",
    "_ILP32 1",
    "__CHAR_BIT__ 8",
    "__SIZEOF_SHORT__ 2",
    "__SIZEOF_INT__ 4",
    "__SIZEOF_LONG__ 4",
    "__SIZEOF_LONG_LONG__ 8",
    "__SIZEOF_POINTER__ 4",
    "__SIZEOF_FLOAT__ 4",
    "__SIZEOF_DOUBLE__ 8",
    "__SIZEOF_LONG_DOUBLE__ 8",
    "__SIZEOF_SIZE_T__ 4",
    "__SIZEOF_WCHAR_T__ 4",
    "__SIZEOF_PTRDIFF_T__ 4",
    "__SIZE_TYPE__ unsigned int",
    "__PTRDIFF_TYPE__ int",
    "__WCHAR_TYPE__ int",
    "__CHAR16_TYPE__ unsigned short",
    "__CHAR32_TYPE__ unsigned int",
    "__INTPTR_TYPE__ int",
    "__UINTPTR_TYPE__ unsigned int",
    "__SCHAR_MAX__ 127",
    "__SHRT_MAX__ 32767",
    "__INT_MAX__ 2147483647",
    "__LONG_MAX__ 2147483647L",
    "__LONG_LONG_MAX__ 9223372036854775807LL",
    "__WCHAR_MAX__ 2147483647",
    "__SIZE_MAX__ 4294967295U",
    "__ORDER_LITTLE_ENDIAN__ 1234",
    "__ORDER_BIG_ENDIAN__ 4321",
    "__BYTE_ORDER__ __ORDER_LITTLE_ENDIAN__",
};

/// How deep `#include` may nest, so that a file that includes itself ends with an error.
enum
{
    MAX_INCLUDE_DEPTH = 200,
};

struct Hideset
{
    Hideset *next;
    const char *name;
};

typedef struct MacroParam MacroParam;

struct MacroParam
{
    MacroParam *next;
    const char *name;
};

/// The tokens a macro call gives for one parameter, ending with a `TK_EOF`: as they are written
/// (for `#` and `##`), and with their macros expanded, worked out when they are first needed.
typedef struct MacroArg MacroArg;

struct MacroArg
{
    MacroArg *next;
    const char *name;
    Token *raw;
    Token *expanded;
};

/// Makes the token of a macro that has no fixed text (`__LINE__`), at the place of the macro name.
typedef Token *(*BuiltinMacro)(const Token *name);

typedef struct
{
    const char *name;
    bool is_objlike;
    /// The parameters of a function-like macro; a `...` is the last one, named `__VA_ARGS__`.
    MacroParam *params;
    bool is_variadic;
    /// The replacement list, ending with a `TK_EOF`.
    Token *body;
    /// Set instead of `body` for the macros of builtin_macros.
    BuiltinMacro builtin;
} Macro;

/// An `#if` (or `#ifdef`, `#ifndef`) whose `#endif` has not come yet.
typedef struct CondIncl CondIncl;

struct CondIncl
{
    CondIncl *next;
    /// Whether the `#else` was there, after which no `#elif` or `#else` may come.
    bool in_else;
    /// The directive name, for messages and to check that the `#endif` is in the same file.
    Token *tok;
    /// Whether one of the groups was taken, so the ones after it are skipped.
    bool included;
};

/// A directive, by the name after the `#`. The handler gets the name and returns the token
/// after the directive.
typedef struct
{
    const char *name;
    Token *(*handler)(Token *tok);
} Directive;

/// A value of an `#if` expression: every signed type is `intmax_t` there and every unsigned one
/// `uintmax_t` (C17 6.10.1), which are 64 bit.
typedef struct
{
    Int64 value;
    bool is_unsigned;
} IfValue;

/// The state of one preprocess() call.
static HashMap macros;
static CondIncl *cond_incl;
/// The files that had `#pragma once`, by the name they were opened with.
static HashMap pragma_once;
static const PreprocessOptions *options;
static int include_depth;
static unsigned int counter;

static Token *expand_tokens(Token *tok);

static Token *copy_token(const Token *tok)
{
    Token *copy = arena_alloc(sizeof(Token));
    *copy = *tok;
    copy->next = NULL;
    return copy;
}

/// @return A `TK_EOF` just after a token, which ends a list of tokens that were copied.
static Token *new_eof(const Token *after)
{
    Token *eof = copy_token(after);
    eof->kind = TK_EOF;
    eof->loc = after->loc + after->len;
    eof->len = 0;
    eof->at_bol = true;
    return eof;
}

/// @return Copies of the tokens up to the `TK_EOF`, without it; null for none.
static Token *copy_until_eof(const Token *tok)
{
    Token head = {0};
    Token *cur = &head;
    for (; tok->kind != TK_EOF; tok = tok->next)
    {
        cur = cur->next = copy_token(tok);
    }
    return head.next;
}

/// @return The last token of a list that ends with a null `next`.
static Token *last_token(Token *tok)
{
    while (tok->next)
    {
        tok = tok->next;
    }
    return tok;
}

/// Splits a text into tokens that the preprocessor made (a pasted token, a string out of `#`, a
/// `__LINE__`). They are in a file of their own and are placed at `tmpl` for messages.
static Token *tokenize_text(const char *text, const Token *tmpl)
{
    Token *tok = tokenize(new_source_file(tmpl->file_name, text));
    for (Token *t = tok; t; t = t->next)
    {
        t->file_name = tmpl->file_name;
        t->line_no = tmpl->line_no;
        t->at_bol = tmpl->at_bol;
        t->has_space = tmpl->has_space;
        t->hideset = tmpl->hideset;
        t->origin = tmpl->origin;
    }
    return tok;
}

/// @return `text` in double quotes, with a backslash before each `"` and `\`.
static char *quote_string(const char *text)
{
    char *quoted = arena_alloc(strlen(text) * 2 + 3);
    size_t len = 0;
    quoted[len++] = '"';
    for (; *text; text++)
    {
        if (*text == '"' || *text == '\\')
        {
            quoted[len++] = '\\';
        }
        quoted[len++] = *text;
    }
    quoted[len] = '"';
    return quoted;
}

/// @return A number token at `tmpl`.
static Token *new_number(unsigned int value, const Token *tmpl)
{
    char digits[16];
    size_t len = 0;
    do
    {
        digits[len++] = (char) ('0' + value % 10);
        value /= 10;
    } while (value);
    char *text = arena_alloc(len + 1);
    for (size_t i = 0; len; i++)
    {
        text[i] = digits[--len];
    }
    return tokenize_text(text, tmpl);
}

/// @return The tokens from `tok` up to `end` (or a `TK_EOF`) as text: each token with a space
/// before it if it has space before it (but not the first one).
static char *join_tokens(const Token *tok, const Token *end)
{
    size_t len = 1;
    for (const Token *t = tok; t != end && t->kind != TK_EOF; t = t->next)
    {
        len += t->len + 1;
    }
    char *text = arena_alloc(len);
    size_t pos = 0;
    for (const Token *t = tok; t != end && t->kind != TK_EOF; t = t->next)
    {
        if (t != tok && t->has_space)
        {
            text[pos++] = ' ';
        }
        memcpy(text + pos, t->loc, t->len);
        pos += t->len;
    }
    return text;
}

/// The `#` operator (C17 6.10.3.2): the spelling of the tokens as a string literal, with one
/// space where there was white space, and `"` and `\` escaped in the strings and character
/// constants among them.
///
/// @param[in] tok The tokens, up to a `TK_EOF` or null.
/// @param[in] tmpl The `#`, where the string is placed.
static Token *stringize(const Token *tok, const Token *tmpl)
{
    size_t len = 3;
    for (const Token *t = tok; t && t->kind != TK_EOF; t = t->next)
    {
        len += t->len * 2 + 1;
    }
    char *text = arena_alloc(len);
    size_t pos = 0;
    text[pos++] = '"';
    for (const Token *t = tok; t && t->kind != TK_EOF; t = t->next)
    {
        if (pos > 1 && t->has_space && t->kind != TK_PLACEMARKER)
        {
            text[pos++] = ' ';
        }
        const bool escape = t->kind == TK_PP_STR || t->kind == TK_PP_CHAR;
        for (size_t i = 0; i < t->len; i++)
        {
            if (escape && (t->loc[i] == '"' || t->loc[i] == '\\'))
            {
                text[pos++] = '\\';
            }
            text[pos++] = t->loc[i];
        }
    }
    text[pos] = '"';

    Token *str = tokenize_text(text, tmpl);
    if (str->kind != TK_PP_STR || str->next->kind != TK_EOF)
    {
        error_tok(tmpl, "'#' does not give a valid string literal: %s", text);
    }
    return str;
}

/// The `##` operator (C17 6.10.3.3): the token whose spelling is the two spellings together.
static Token *paste(const Token *lhs, const Token *rhs)
{
    char *text = arena_alloc(lhs->len + rhs->len + 1);
    memcpy(text, lhs->loc, lhs->len);
    memcpy(text + lhs->len, rhs->loc, rhs->len);
    Token *tok = tokenize_text(text, lhs);
    if (tok->kind == TK_EOF || tok->next->kind != TK_EOF)
    {
        error_tok(lhs, "pasting \"%s\" and \"%s\" does not give a valid preprocessing token",
                  token_text(lhs), token_text(rhs));
    }
    tok->at_bol = false;
    return tok;
}

static Hideset *new_hideset(const char *name)
{
    Hideset *hs = arena_alloc(sizeof(Hideset));
    hs->name = name;
    return hs;
}

static bool hideset_contains(const Hideset *hs, const Token *tok)
{
    for (; hs; hs = hs->next)
    {
        if (token_is(tok, hs->name))
        {
            return true;
        }
    }
    return false;
}

static Hideset *hideset_union(const Hideset *a, Hideset *b)
{
    Hideset head = {0};
    Hideset *cur = &head;
    for (; a; a = a->next)
    {
        cur = cur->next = new_hideset(a->name);
    }
    cur->next = b;
    return head.next;
}

static Hideset *hideset_intersection(const Hideset *a, const Hideset *b)
{
    Hideset head = {0};
    Hideset *cur = &head;
    for (; a; a = a->next)
    {
        for (const Hideset *t = b; t; t = t->next)
        {
            if (strcmp(a->name, t->name) == 0)
            {
                cur = cur->next = new_hideset(a->name);
                break;
            }
        }
    }
    return head.next;
}

static Macro *find_macro(const Token *tok)
{
    if (tok->kind != TK_IDENT)
    {
        return NULL;
    }
    return hashmap_get(&macros, tok->loc, tok->len);
}

/// @return Whether `tok` is a `#` that starts a directive: the first token of a line of a file,
/// not one that a macro gave.
static bool is_hash(const Token *tok)
{
    return tok->at_bol && !tok->origin && token_is(tok, "#");
}

/// @return Whether `tok` starts the directive `name`.
static bool is_directive(const Token *tok, const char *name)
{
    return is_hash(tok) && !tok->next->at_bol && token_is(tok->next, name);
}

/// @return The first token of the next line. Warns about tokens before it, which a directive does
/// not take.
static Token *skip_line(Token *tok)
{
    if (tok->at_bol)
    {
        return tok;
    }
    warn_tok(tok, "extra tokens at the end of the directive");
    while (!tok->at_bol)
    {
        tok = tok->next;
    }
    return tok;
}

/// @param[out] rest The first token of the next line.
/// @return Copies of the tokens up to the end of the line, with a `TK_EOF` after them.
static Token *read_line(Token **rest, Token *tok)
{
    Token head = {0};
    Token *cur = &head;
    const Token *last = tok;
    for (; !tok->at_bol; tok = tok->next)
    {
        cur = cur->next = copy_token(tok);
        last = tok;
    }
    cur->next = new_eof(last);
    *rest = tok;
    return head.next;
}

/// Skips to the `#elif`, `#elifdef`, `#elifndef`, `#else` or `#endif` that belongs to the `#if`
/// before `tok`, over the conditionals nested in it.
///
/// @return The `#` of that directive, or the `TK_EOF` of the file.
static Token *skip_cond_incl(Token *tok)
{
    int depth = 0;
    for (; tok->kind != TK_EOF; tok = tok->next)
    {
        if (is_directive(tok, "if") || is_directive(tok, "ifdef") || is_directive(tok, "ifndef"))
        {
            depth++;
        }
        else if (depth > 0 && is_directive(tok, "endif"))
        {
            depth--;
        }
        else if (depth == 0
                 && (is_directive(tok, "elif") || is_directive(tok, "elifdef")
                     || is_directive(tok, "elifndef") || is_directive(tok, "else")
                     || is_directive(tok, "endif")))
        {
            return tok;
        }
    }
    return tok;
}

static void push_cond_incl(Token *tok, bool included)
{
    CondIncl *ci = arena_alloc(sizeof(CondIncl));
    ci->next = cond_incl;
    ci->tok = tok;
    ci->included = included;
    cond_incl = ci;
}

/// @return The `#if` that the `#elif`, `#else` or `#endif` at `tok` belongs to. Exits with an
/// error if there is none in the same file, or if it already had its `#else`.
static CondIncl *current_cond_incl(const Token *tok)
{
    if (!cond_incl || cond_incl->tok->file != tok->file)
    {
        error_tok(tok, "#%s without #if", token_text(tok));
    }
    if (cond_incl->in_else && !token_is(tok, "endif"))
    {
        error_tok(tok, "#%s after #else", token_text(tok));
    }
    return cond_incl;
}

/// @return The name after `#ifdef`, `#define` and the others. Exits with an error if there is
/// no name.
static Token *read_macro_name(Token *directive)
{
    Token *name = directive->next;
    if (name->at_bol || name->kind != TK_IDENT)
    {
        error_tok(name->at_bol ? directive : name, "macro names must be identifiers");
    }
    if (token_is(name, "defined"))
    {
        error_tok(name, "'defined' cannot be used as a macro name");
    }
    return name;
}

/// @return The name of the directory a file is in, with a `/` at the end; "" for the current
/// directory.
static char *directory_of(const char *path)
{
    size_t len = strlen(path);
    while (len > 0 && path[len - 1] != '/')
    {
        len--;
    }
    return arena_strndup(path, len);
}

/// @return `dir`, a `/` if it does not end with one, and `name`.
static char *join_path(const char *dir, const char *name)
{
    const size_t dir_len = strlen(dir);
    const bool slash = dir_len > 0 && dir[dir_len - 1] != '/';
    char *path = arena_alloc(dir_len + strlen(name) + 2);
    memcpy(path, dir, dir_len);
    if (slash)
    {
        path[dir_len] = '/';
    }
    memcpy(path + dir_len + slash, name, strlen(name));
    return path;
}

static bool file_exists(const char *path)
{
    FILE *fp = fopen(path, "rb");
    if (fp)
    {
        fclose(fp);
    }
    return fp != NULL;
}

/// Finds a file to include.
///
/// @param[in] name The name between the quotes or the angle brackets.
/// @param[in] is_quoted Whether it was `"name"`.
/// @param[in] includer The token of the directive, whose file's directory is searched first.
/// @return The path, null if the file is nowhere.
static char *search_include(const char *name, bool is_quoted, const Token *includer)
{
    if (name[0] == '/')
    {
        return file_exists(name) ? arena_strndup(name, strlen(name)) : NULL;
    }
    if (is_quoted)
    {
        char *path = join_path(directory_of(includer->file->name), name);
        if (file_exists(path))
        {
            return path;
        }
    }
    for (size_t i = 0; i < options->include_path_count; i++)
    {
        char *path = join_path(options->include_paths[i], name);
        if (file_exists(path))
        {
            return path;
        }
    }
    return NULL;
}

/// Reads the file name of `#include` and `__has_include`: `"name"`, `<name>` (whose tokens are
/// put together with their spaces), or tokens whose macros expand to one of the two.
///
/// @param[out] rest The token after the name.
/// @param[out] is_quoted Whether the name was in double quotes.
/// @return The name.
static char *read_include_name(Token **rest, Token *tok, bool *is_quoted)
{
    if (tok->kind == TK_PP_STR && tok->loc[0] == '"' && !tok->at_bol)
    {
        *is_quoted = true;
        *rest = tok->next;
        return arena_strndup(tok->loc + 1, tok->len - 2);
    }
    if (token_is(tok, "<") && !tok->at_bol)
    {
        Token *start = tok->next;
        Token *end = start;
        for (; !token_is(end, ">"); end = end->next)
        {
            if (end->at_bol)
            {
                error_tok(tok, "expected '>'");
            }
        }
        *is_quoted = false;
        *rest = end->next;
        return join_tokens(start, end);
    }
    if (tok->kind == TK_IDENT && !tok->at_bol)
    {
        Token *line_end;
        Token *expanded = expand_tokens(read_line(&line_end, tok));
        Token *after;
        char *name = read_include_name(&after, expanded, is_quoted);
        if (after->kind != TK_EOF)
        {
            warn_tok(after, "extra tokens after the file name");
        }
        // The name and the rest of its line are used up.
        *rest = line_end;
        return name;
    }
    error_tok(tok, "expected a file name in quotes or angle brackets");
    return NULL;
}

/// Puts the tokens of a file in front of `rest`, with the `TK_EOF` of the file between them.
static Token *include_file(Token *rest, const char *path, const Token *where)
{
    if (hashmap_get(&pragma_once, path, strlen(path)))
    {
        return rest;
    }
    if (include_depth >= MAX_INCLUDE_DEPTH)
    {
        error_tok(where, "#include nested more than %d deep", MAX_INCLUDE_DEPTH);
    }
    SourceFile *file = read_source_file(path);
    if (!file)
    {
        error_tok(where, "cannot open '%s'", path);
    }
    Token *tok = tokenize(file);
    last_token(tok)->next = rest;
    include_depth++;
    return tok;
}

static Token *directive_include(Token *tok)
{
    bool is_quoted;
    Token *rest;
    char *name = read_include_name(&rest, tok->next, &is_quoted);
    rest = skip_line(rest);
    char *path = search_include(name, is_quoted, tok);
    if (!path)
    {
        error_tok(tok->next, "'%s' not found", name);
    }
    return include_file(rest, path, tok->next);
}

/// @return Whether two definitions of a macro are the same (C17 6.10.3p2): the same parameters
/// and the same tokens with white space at the same places.
static bool same_definition(const Macro *a, const Macro *b)
{
    if (a->builtin || b->builtin || a->is_objlike != b->is_objlike
        || a->is_variadic != b->is_variadic)
    {
        return false;
    }
    const MacroParam *pa = a->params;
    const MacroParam *pb = b->params;
    for (; pa && pb; pa = pa->next, pb = pb->next)
    {
        if (strcmp(pa->name, pb->name) != 0)
        {
            return false;
        }
    }
    if (pa || pb)
    {
        return false;
    }
    const Token *ta = a->body;
    const Token *tb = b->body;
    for (; ta->kind != TK_EOF && tb->kind != TK_EOF; ta = ta->next, tb = tb->next)
    {
        if (ta->len != tb->len || memcmp(ta->loc, tb->loc, ta->len) != 0
            || (ta != a->body && ta->has_space != tb->has_space))
        {
            return false;
        }
    }
    return ta->kind == TK_EOF && tb->kind == TK_EOF;
}

static bool is_param(const Macro *m, const Token *tok)
{
    for (const MacroParam *p = m->params; p; p = p->next)
    {
        if (token_is(tok, p->name))
        {
            return true;
        }
    }
    return false;
}

/// Reads the parameters of a function-like macro, after its `(`.
///
/// @param[out] rest The token after the `)`.
static void read_macro_params(Token **rest, Token *tok, Macro *m)
{
    MacroParam head = {0};
    MacroParam *cur = &head;
    if (token_is(tok, ")"))
    {
        *rest = tok->next;
        return;
    }
    for (;;)
    {
        if (tok->at_bol)
        {
            error_tok(tok, "expected ')'");
        }
        if (token_is(tok, "..."))
        {
            m->is_variadic = true;
            cur = cur->next = arena_alloc(sizeof(MacroParam));
            cur->name = "__VA_ARGS__";
            tok = tok->next;
            if (!token_is(tok, ")") || tok->at_bol)
            {
                error_tok(tok, "expected ')' after '...'");
            }
            break;
        }
        if (tok->kind != TK_IDENT || token_is(tok, "__VA_ARGS__") || token_is(tok, "__VA_OPT__"))
        {
            error_tok(tok, "expected a parameter name");
        }
        m->params = head.next;
        if (is_param(m, tok))
        {
            error_tok(tok, "duplicate macro parameter '%s'", token_text(tok));
        }
        cur = cur->next = arena_alloc(sizeof(MacroParam));
        cur->name = token_text(tok);
        tok = tok->next;
        if (token_is(tok, ")") && !tok->at_bol)
        {
            break;
        }
        if (!token_is(tok, ",") || tok->at_bol)
        {
            error_tok(tok, "expected ',' or ')'");
        }
        tok = tok->next;
    }
    m->params = head.next;
    *rest = tok->next;
}

/// @return The `)` that closes the `(` at `tok`, in a list that ends with a `TK_EOF`; null if
/// there is none.
static Token *matching_paren(Token *tok)
{
    int depth = 0;
    for (; tok->kind != TK_EOF; tok = tok->next)
    {
        depth += token_is(tok, "(") - token_is(tok, ")");
        if (depth == 0)
        {
            return tok;
        }
    }
    return NULL;
}

/// Checks the rules of a replacement list (C17 6.10.3, C23 6.10.5): `##` is not at either end,
/// `#` in a function-like macro is followed by a parameter, and `__VA_ARGS__` and `__VA_OPT__`
/// are only in a variadic macro, the latter with its tokens in parentheses and not nested.
static void check_macro_body(const Macro *m)
{
    if (token_is(m->body, "##"))
    {
        error_tok(m->body, "'##' cannot be at either end of a macro");
    }
    Token *va_opt_end = NULL;
    for (Token *tok = m->body; tok->kind != TK_EOF; tok = tok->next)
    {
        if (token_is(tok, "##") && tok->next->kind == TK_EOF)
        {
            error_tok(tok, "'##' cannot be at either end of a macro");
        }
        if (token_is(tok, "#") && !m->is_objlike && !is_param(m, tok->next)
            && !(m->is_variadic && token_is(tok->next, "__VA_OPT__")))
        {
            error_tok(tok, "'#' is not followed by a macro parameter");
        }
        if ((token_is(tok, "__VA_ARGS__") || token_is(tok, "__VA_OPT__")) && !m->is_variadic)
        {
            error_tok(tok, "%s can only be in a variadic macro", token_text(tok));
        }
        if (tok == va_opt_end)
        {
            va_opt_end = NULL;
        }
        if (va_opt_end && token_is(tok, "##") && tok->next == va_opt_end)
        {
            error_tok(tok, "'##' cannot be at either end of __VA_OPT__");
        }
        if (token_is(tok, "__VA_OPT__"))
        {
            if (va_opt_end)
            {
                error_tok(tok, "__VA_OPT__ cannot be nested");
            }
            va_opt_end = token_is(tok->next, "(") ? matching_paren(tok->next) : NULL;
            if (!va_opt_end)
            {
                error_tok(tok, "__VA_OPT__ needs its tokens in parentheses");
            }
            if (token_is(tok->next->next, "##"))
            {
                error_tok(tok->next->next, "'##' cannot be at either end of __VA_OPT__");
            }
        }
    }
}

static Token *directive_define(Token *tok)
{
    Token *name = read_macro_name(tok);
    Macro *m = arena_alloc(sizeof(Macro));
    m->name = token_text(name);
    m->is_objlike = true;
    tok = name->next;
    if (token_is(tok, "(") && !tok->has_space && !tok->at_bol)
    {
        m->is_objlike = false;
        read_macro_params(&tok, tok->next, m);
    }
    else if (!tok->at_bol && !tok->has_space)
    {
        warn_tok(tok, "missing white space after the macro name");
    }
    Token *rest;
    m->body = read_line(&rest, tok);
    check_macro_body(m);

    Macro *old = hashmap_get(&macros, name->loc, name->len);
    if (old && !same_definition(old, m))
    {
        error_tok(name, "'%s' is redefined differently", m->name);
    }
    hashmap_put(&macros, m->name, strlen(m->name), m);
    return rest;
}

static Token *directive_undef(Token *tok)
{
    Token *name = read_macro_name(tok);
    if (hashmap_get(&macros, name->loc, name->len))
    {
        hashmap_put(&macros, name->loc, name->len, NULL);
    }
    return skip_line(name->next);
}

/// The value of an `#if` expression, by precedence climbing. `evaluate` is false in an operand
/// that is not evaluated (`0 && x`), where division by 0 is not an error.
static IfValue if_expr(Token **rest, Token *tok, bool evaluate);

static IfValue if_value(Int64 value, bool is_unsigned)
{
    IfValue result = {value, is_unsigned};
    return result;
}

static IfValue if_bool(bool value)
{
    return if_value(int64_from_int(value), false);
}

static IfValue if_primary(Token **rest, Token *tok, bool evaluate)
{
    if (token_is(tok, "("))
    {
        IfValue value = if_expr(&tok, tok->next, evaluate);
        if (!token_is(tok, ")"))
        {
            error_tok(tok, "expected ')' in the #if expression");
        }
        *rest = tok->next;
        return value;
    }
    if (tok->kind == TK_PP_NUM || tok->kind == TK_PP_CHAR)
    {
        Token *constant = copy_token(tok);
        convert_constant(constant);
        if (constant->ty->kind == TY_FLOAT || constant->ty->kind == TY_DOUBLE
            || constant->ty->kind == TY_LDOUBLE)
        {
            error_tok(tok, "floating constant in an #if expression");
        }
        Int64 value = {(unsigned int) constant->value, constant->value_high};
        *rest = tok->next;
        return if_value(value, constant->ty->is_unsigned);
    }
    if (tok->kind == TK_IDENT)
    {
        // A name that is left after the macros are expanded is 0, except C23's true.
        *rest = tok->next;
        return if_bool(token_is(tok, "true"));
    }
    error_tok(tok, tok->kind == TK_EOF ? "expected an expression after this in #if"
                                       : "expected an expression in #if");
    return if_bool(false);
}

static IfValue if_unary(Token **rest, Token *tok, bool evaluate)
{
    if (token_is(tok, "+"))
    {
        return if_unary(rest, tok->next, evaluate);
    }
    if (token_is(tok, "-"))
    {
        IfValue operand = if_unary(rest, tok->next, evaluate);
        return if_value(int64_neg(operand.value), operand.is_unsigned);
    }
    if (token_is(tok, "~"))
    {
        IfValue operand = if_unary(rest, tok->next, evaluate);
        return if_value(int64_not(operand.value), operand.is_unsigned);
    }
    if (token_is(tok, "!"))
    {
        return if_bool(int64_is_zero(if_unary(rest, tok->next, evaluate).value));
    }
    return if_primary(rest, tok, evaluate);
}

typedef enum
{
    OP_MUL,
    OP_DIV,
    OP_MOD,
    OP_ADD,
    OP_SUB,
    OP_SHL,
    OP_SHR,
    OP_LT,
    OP_LE,
    OP_GT,
    OP_GE,
    OP_EQ,
    OP_NE,
    OP_BITAND,
    OP_BITXOR,
    OP_BITOR,
    OP_LOGAND,
    OP_LOGOR,
} IfOperator;

typedef struct
{
    const char *text;
    IfOperator op;
} IfBinaryOperator;

/// The binary operators of `#if`, a table for each precedence level, the loosest first. Each
/// table ends with an entry without text.
static const IfBinaryOperator if_logor_ops[] = {{"||", OP_LOGOR}, {NULL, OP_MUL}};
static const IfBinaryOperator if_logand_ops[] = {{"&&", OP_LOGAND}, {NULL, OP_MUL}};
static const IfBinaryOperator if_bitor_ops[] = {{"|", OP_BITOR}, {NULL, OP_MUL}};
static const IfBinaryOperator if_bitxor_ops[] = {{"^", OP_BITXOR}, {NULL, OP_MUL}};
static const IfBinaryOperator if_bitand_ops[] = {{"&", OP_BITAND}, {NULL, OP_MUL}};
static const IfBinaryOperator if_equality_ops[] = {{"==", OP_EQ}, {"!=", OP_NE}, {NULL, OP_MUL}};
static const IfBinaryOperator if_relational_ops[] = {
    {"<", OP_LT}, {"<=", OP_LE}, {">", OP_GT}, {">=", OP_GE}, {NULL, OP_MUL}};
static const IfBinaryOperator if_shift_ops[] = {{"<<", OP_SHL}, {">>", OP_SHR}, {NULL, OP_MUL}};
static const IfBinaryOperator if_additive_ops[] = {{"+", OP_ADD}, {"-", OP_SUB}, {NULL, OP_MUL}};
static const IfBinaryOperator if_multiplicative_ops[] = {
    {"*", OP_MUL}, {"/", OP_DIV}, {"%", OP_MOD}, {NULL, OP_MUL}};

static const IfBinaryOperator *const if_binary_levels[] = {
    if_logor_ops,    if_logand_ops,     if_bitor_ops, if_bitxor_ops,   if_bitand_ops,
    if_equality_ops, if_relational_ops, if_shift_ops, if_additive_ops, if_multiplicative_ops,
};

enum
{
    NUM_IF_BINARY_LEVELS = sizeof(if_binary_levels) / sizeof(if_binary_levels[0]),
};

/// Applies a binary operator other than `&&` and `||`, with the usual arithmetic conversions:
/// the operation is unsigned if an operand is (the left one for a shift).
static IfValue if_apply(IfOperator op, IfValue lhs, IfValue rhs, const Token *tok, bool evaluate)
{
    const bool is_unsigned = lhs.is_unsigned || rhs.is_unsigned;
    const Int64 a = lhs.value;
    const Int64 b = rhs.value;
    Int64 remainder;
    switch (op)
    {
    case OP_MUL:
        return if_value(int64_mul(a, b), is_unsigned);
    case OP_DIV:
    case OP_MOD:
        if (int64_is_zero(b))
        {
            if (evaluate)
            {
                error_tok(tok, "division by zero in #if");
            }
            return if_value(b, is_unsigned);
        }
        {
            const Int64 quotient = int64_div(a, b, !is_unsigned, &remainder);
            return if_value(op == OP_DIV ? quotient : remainder, is_unsigned);
        }
    case OP_ADD:
        return if_value(int64_add(a, b), is_unsigned);
    case OP_SUB:
        return if_value(int64_sub(a, b), is_unsigned);
    case OP_SHL:
    case OP_SHR:
        if (b.hi != 0 || b.lo >= 64)
        {
            if (evaluate)
            {
                error_tok(tok, "shift count out of range in #if");
            }
            return if_value(int64_from_int(0), lhs.is_unsigned);
        }
        return if_value(op == OP_SHL ? int64_shl(a, (int) b.lo)
                                     : int64_shr(a, (int) b.lo, !lhs.is_unsigned),
                        lhs.is_unsigned);
    case OP_LT:
        return if_bool(int64_lt(a, b, !is_unsigned));
    case OP_LE:
        return if_bool(!int64_lt(b, a, !is_unsigned));
    case OP_GT:
        return if_bool(int64_lt(b, a, !is_unsigned));
    case OP_GE:
        return if_bool(!int64_lt(a, b, !is_unsigned));
    case OP_EQ:
        return if_bool(int64_eq(a, b));
    case OP_NE:
        return if_bool(!int64_eq(a, b));
    case OP_BITAND:
        return if_value(int64_and(a, b), is_unsigned);
    case OP_BITXOR:
        return if_value(int64_xor(a, b), is_unsigned);
    case OP_BITOR:
        return if_value(int64_or(a, b), is_unsigned);
    case OP_LOGAND:
    case OP_LOGOR:
        break;
    }
    return if_bool(false);
}

static IfValue if_binary(Token **rest, Token *tok, size_t level, bool evaluate)
{
    if (level == NUM_IF_BINARY_LEVELS)
    {
        return if_unary(rest, tok, evaluate);
    }
    IfValue lhs = if_binary(&tok, tok, level + 1, evaluate);
    for (;;)
    {
        const IfBinaryOperator *op = if_binary_levels[level];
        while (op->text && !token_is(tok, op->text))
        {
            op++;
        }
        if (!op->text)
        {
            break;
        }
        const Token *op_tok = tok;
        if (op->op == OP_LOGAND || op->op == OP_LOGOR)
        {
            // The right operand is only evaluated if the left one does not decide.
            const bool decided = int64_is_zero(lhs.value) == (op->op == OP_LOGAND);
            IfValue rhs = if_binary(&tok, tok->next, level + 1, evaluate && !decided);
            lhs = if_bool(decided ? op->op == OP_LOGOR : !int64_is_zero(rhs.value));
            continue;
        }
        IfValue rhs = if_binary(&tok, tok->next, level + 1, evaluate);
        lhs = if_apply(op->op, lhs, rhs, op_tok, evaluate);
    }
    *rest = tok;
    return lhs;
}

/// `a ? b : c`, where only the operand that is chosen is evaluated.
static IfValue if_conditional(Token **rest, Token *tok, bool evaluate)
{
    IfValue cond = if_binary(&tok, tok, 0, evaluate);
    if (!token_is(tok, "?"))
    {
        *rest = tok;
        return cond;
    }
    const bool take_first = !int64_is_zero(cond.value);
    IfValue first = if_expr(&tok, tok->next, evaluate && take_first);
    if (!token_is(tok, ":"))
    {
        error_tok(tok, "expected ':' in the #if expression");
    }
    IfValue second = if_conditional(&tok, tok->next, evaluate && !take_first);
    *rest = tok;
    return if_value(take_first ? first.value : second.value,
                    first.is_unsigned || second.is_unsigned);
}

static IfValue if_expr(Token **rest, Token *tok, bool evaluate)
{
    IfValue value = if_conditional(&tok, tok, evaluate);
    while (token_is(tok, ","))
    {
        value = if_conditional(&tok, tok->next, evaluate);
    }
    *rest = tok;
    return value;
}

/// Replaces `defined X`, `defined(X)` and `__has_include(name)` with 1 or 0, before the macros of
/// the line are expanded.
static Token *replace_defined(Token *tok)
{
    Token head = {0};
    Token *cur = &head;
    while (tok->kind != TK_EOF)
    {
        Token *start = tok;
        if (token_is(tok, "defined"))
        {
            tok = tok->next;
            const bool paren = token_is(tok, "(");
            tok = paren ? tok->next : tok;
            if (tok->kind != TK_IDENT)
            {
                error_tok(start, "'defined' needs a macro name");
            }
            const bool is_defined = find_macro(tok) != NULL;
            tok = tok->next;
            if (paren && !token_is(tok, ")"))
            {
                error_tok(tok, "expected ')' after the name of 'defined'");
            }
            tok = paren ? tok->next : tok;
            cur = cur->next = new_number(is_defined, start);
            continue;
        }
        if (token_is(tok, "__has_include"))
        {
            Token *close = token_is(tok->next, "(") ? matching_paren(tok->next) : NULL;
            if (!close)
            {
                error_tok(start, "__has_include needs a file name in parentheses");
            }
            // The name is read from a list of its own, which ends where the parentheses do.
            Token name_head = {0};
            Token *name_cur = &name_head;
            for (Token *t = tok->next->next; t != close; t = t->next)
            {
                name_cur = name_cur->next = copy_token(t);
            }
            name_cur->next = new_eof(name_cur == &name_head ? tok->next : name_cur);
            Token *name_tokens = name_head.next ? name_head.next : name_cur->next;
            Token *after;
            bool is_quoted;
            char *name = read_include_name(&after, name_tokens, &is_quoted);
            if (after->kind != TK_EOF)
            {
                error_tok(after, "expected ')' after the file name");
            }
            cur = cur->next = new_number(search_include(name, is_quoted, start) != NULL, start);
            tok = close->next;
            continue;
        }
        cur = cur->next = tok;
        tok = tok->next;
    }
    cur->next = tok;
    return head.next;
}

/// Evaluates the expression of `#if` or `#elif`.
///
/// @param[in] directive The directive name.
/// @param[out] rest The first token of the next line.
static bool eval_if(Token *directive, Token **rest)
{
    Token *line = read_line(rest, directive->next);
    if (line->kind == TK_EOF)
    {
        error_tok(directive, "#%s with no expression", token_text(directive));
    }
    line = expand_tokens(replace_defined(line));
    Token *end;
    IfValue value = if_expr(&end, line, true);
    if (end->kind != TK_EOF)
    {
        error_tok(end, "missing an operator before this in #if");
    }
    return !int64_is_zero(value.value);
}

static Token *directive_if(Token *tok)
{
    Token *rest;
    const bool value = eval_if(tok, &rest);
    push_cond_incl(tok, value);
    return value ? rest : skip_cond_incl(rest);
}

/// `#ifdef` and `#ifndef`, and `#elifdef` and `#elifndef` after the check of their `#if`.
///
/// @param[out] rest The first token of the next line.
/// @return Whether the macro is defined.
static bool eval_ifdef(Token *tok, Token **rest)
{
    Token *name = read_macro_name(tok);
    *rest = skip_line(name->next);
    return find_macro(name) != NULL;
}

static Token *directive_ifdef(Token *tok)
{
    Token *rest;
    const bool value = eval_ifdef(tok, &rest);
    push_cond_incl(tok, value);
    return value ? rest : skip_cond_incl(rest);
}

static Token *directive_ifndef(Token *tok)
{
    Token *rest;
    const bool value = !eval_ifdef(tok, &rest);
    push_cond_incl(tok, value);
    return value ? rest : skip_cond_incl(rest);
}

/// `#elif`, `#elifdef` and `#elifndef`. Once a group was taken, the condition is not looked at.
static Token *directive_elif(Token *tok)
{
    CondIncl *ci = current_cond_incl(tok);
    if (ci->included)
    {
        return skip_cond_incl(tok);
    }
    Token *rest;
    bool value;
    if (token_is(tok, "elif"))
    {
        value = eval_if(tok, &rest);
    }
    else
    {
        value = eval_ifdef(tok, &rest) == token_is(tok, "elifdef");
    }
    ci->included = value;
    return value ? rest : skip_cond_incl(rest);
}

static Token *directive_else(Token *tok)
{
    CondIncl *ci = current_cond_incl(tok);
    ci->in_else = true;
    Token *rest = skip_line(tok->next);
    return ci->included ? skip_cond_incl(rest) : rest;
}

static Token *directive_endif(Token *tok)
{
    current_cond_incl(tok);
    cond_incl = cond_incl->next;
    return skip_line(tok->next);
}

/// `#line number "name"`, whose tokens are macro expanded: the line after it is line `number`,
/// and the file is called `name` from there on.
static Token *directive_line(Token *tok)
{
    Token *rest;
    Token *line = expand_tokens(read_line(&rest, tok->next));
    const bool digits_only = line->kind == TK_PP_NUM && line->loc[0] >= '0' && line->loc[0] <= '9';
    unsigned int number = 0;
    for (size_t i = 0; digits_only && i < line->len; i++)
    {
        const unsigned int digit = (unsigned int) (line->loc[i] - '0');
        if (line->loc[i] < '0' || line->loc[i] > '9' || number > (2147483647u - digit) / 10)
        {
            number = 0;
            break;
        }
        number = number * 10 + digit;
    }
    if (number == 0)
    {
        error_tok(line->kind == TK_EOF ? tok : line,
                  "#line needs a line number from 1 to 2147483647");
    }
    const char *name = NULL;
    Token *after = line->next;
    if (after->kind == TK_PP_STR && after->loc[0] == '"')
    {
        name = arena_strndup(after->loc + 1, after->len - 2);
        after = after->next;
    }
    else if (after->kind != TK_EOF)
    {
        error_tok(after, "expected a file name in double quotes");
    }
    if (after->kind != TK_EOF)
    {
        warn_tok(after, "extra tokens at the end of the directive");
    }

    // The rest of the file follows up to its TK_EOF: the files it includes are not in yet.
    const int delta = (int) number - (tok->line_no + 1);
    for (Token *t = rest; t && t->file == tok->file; t = t->next)
    {
        t->line_no += delta;
        t->file_name = name ? name : t->file_name;
    }
    return rest;
}

static Token *directive_pragma(Token *tok)
{
    // The pragmas of C (STDC FP_CONTRACT and the others) do not change anything here, and the
    // other ones are not this compiler's. Only `once` does something.
    if (token_is(tok->next, "once") && !tok->next->at_bol)
    {
        hashmap_put(&pragma_once, tok->file->name, strlen(tok->file->name), tok);
        return skip_line(tok->next->next);
    }
    while (!tok->next->at_bol)
    {
        tok = tok->next;
    }
    return tok->next;
}

static Token *directive_error(Token *tok)
{
    Token *rest;
    error_tok(tok, "#error %s", join_tokens(read_line(&rest, tok->next), NULL));
    return rest;
}

static Token *directive_warning(Token *tok)
{
    Token *rest;
    warn_tok(tok, "#warning %s", join_tokens(read_line(&rest, tok->next), NULL));
    return rest;
}

static const Directive directives[] = {
    {"include", directive_include}, {"define", directive_define}, {"undef", directive_undef},
    {"if", directive_if},           {"ifdef", directive_ifdef},   {"ifndef", directive_ifndef},
    {"elif", directive_elif},       {"elifdef", directive_elif},  {"elifndef", directive_elif},
    {"else", directive_else},       {"endif", directive_endif},   {"line", directive_line},
    {"pragma", directive_pragma},   {"error", directive_error},   {"warning", directive_warning},
};

/// Reads the arguments of a function-like macro call, from the token after its `(`.
///
/// @param[out] rest The `)` that ends the call.
/// @return The arguments, one for each parameter (an empty one for `...` that has none).
static MacroArg *read_macro_args(Token **rest, Token *tok, const Macro *m, const Token *name)
{
    MacroArg head = {0};
    MacroArg *cur = &head;
    for (const MacroParam *param = m->params; param; param = param->next)
    {
        const bool is_va_args = m->is_variadic && !param->next;
        if (param != m->params)
        {
            if (is_va_args && token_is(tok, ")"))
            {
                // C23: the variable arguments can be left out, with their comma.
                cur = cur->next = arena_alloc(sizeof(MacroArg));
                cur->name = param->name;
                cur->raw = new_eof(tok);
                break;
            }
            if (!token_is(tok, ","))
            {
                error_tok(tok, "too few arguments for the macro '%s'", m->name);
            }
            tok = tok->next;
        }

        Token arg_head = {0};
        Token *arg_cur = &arg_head;
        const Token *last = tok;
        int depth = 0;
        for (; depth > 0 || (!token_is(tok, ")") && (is_va_args || !token_is(tok, ",")));
             tok = tok->next)
        {
            if (tok->kind == TK_EOF)
            {
                error_tok(name, "unterminated call of the macro '%s'", m->name);
            }
            depth += token_is(tok, "(") - token_is(tok, ")");
            arg_cur = arg_cur->next = copy_token(tok);
            last = tok;
        }
        arg_cur->next = new_eof(last);
        cur = cur->next = arena_alloc(sizeof(MacroArg));
        cur->name = param->name;
        cur->raw = arg_head.next;
    }
    if (!token_is(tok, ")"))
    {
        if (tok->kind == TK_EOF)
        {
            error_tok(name, "unterminated call of the macro '%s'", m->name);
        }
        error_tok(tok, "too many arguments for the macro '%s'", m->name);
    }
    *rest = tok;
    return head.next;
}

static MacroArg *find_arg(MacroArg *args, const Token *tok)
{
    for (; args; args = args->next)
    {
        if (tok->kind == TK_IDENT && token_is(tok, args->name))
        {
            return args;
        }
    }
    return NULL;
}

/// @return The tokens of an argument with its macros expanded (C17 6.10.3.1), ending with a
/// `TK_EOF`.
static Token *expanded_arg(MacroArg *arg)
{
    if (!arg->expanded)
    {
        Token *copy = copy_until_eof(arg->raw);
        if (copy)
        {
            last_token(copy)->next = new_eof(last_token(copy));
        }
        arg->expanded = expand_tokens(copy ? copy : new_eof(arg->raw));
    }
    return arg->expanded;
}

static Token *new_placemarker(const Token *tmpl)
{
    Token *tok = copy_token(tmpl);
    tok->kind = TK_PLACEMARKER;
    tok->len = 0;
    return tok;
}

/// The replacement of a macro call (C17 6.10.3.1 to 6.10.3.3, C23 6.10.5.2): copies of the tokens
/// of the replacement list from `tok` up to `end`, with the parameters replaced and `#` and `##`
/// done. An empty operand is a placemarker, which the caller removes.
///
/// @param[in] end Where the tokens end, or null for the `TK_EOF` of the list.
/// @return The tokens, ending with a null `next`; null for none.
static Token *subst(Token *tok, const Token *end, MacroArg *args, const Macro *m);

/// The tokens of `__VA_OPT__(...)` at `tok`: the replacement of the tokens in the parentheses
/// if the variable arguments have tokens, else a placemarker. The placemarkers stay, so that the
/// result as an operand of `##` is pasted at its ends as they are (C23 6.10.5.2).
///
/// @param[out] rest The token after the `)`.
static Token *va_opt(Token **rest, Token *tok, MacroArg *args, const Macro *m)
{
    Token *close = tok->next;
    for (int depth = 0;; close = close->next)
    {
        depth += token_is(close, "(") - token_is(close, ")");
        if (depth == 0)
        {
            break;
        }
    }
    *rest = close->next;
    MacroArg *va_args = args;
    while (va_args->next)
    {
        va_args = va_args->next;
    }
    Token *result = NULL;
    if (expanded_arg(va_args)->kind != TK_EOF)
    {
        result = subst(tok->next->next, close, args, m);
    }
    return result ? result : new_placemarker(tok);
}

static Token *subst(Token *tok, const Token *end, MacroArg *args, const Macro *m)
{
    Token head = {0};
    Token *cur = &head;
    // Whether a ## comes before the operand that is read now.
    bool paste_next = false;
    while (tok != end && tok->kind != TK_EOF)
    {
        if (token_is(tok, "##"))
        {
            paste_next = true;
            tok = tok->next;
            continue;
        }

        Token *item;
        const Token *first = tok;
        MacroArg *arg = m->is_objlike ? NULL : find_arg(args, tok);
        if (!m->is_objlike && token_is(tok, "#"))
        {
            if (token_is(tok->next, "__VA_OPT__"))
            {
                item = stringize(va_opt(&tok, tok->next, args, m), first);
            }
            else
            {
                item = stringize(find_arg(args, tok->next)->raw, first);
                tok = tok->next->next;
            }
            item->next = NULL;
        }
        else if (arg)
        {
            const bool next_is_paste = tok->next != end && token_is(tok->next, "##");
            item = copy_until_eof(paste_next || next_is_paste ? arg->raw : expanded_arg(arg));
            item = item ? item : new_placemarker(tok);
            tok = tok->next;
        }
        else if (m->is_variadic && token_is(tok, "__VA_OPT__"))
        {
            item = va_opt(&tok, tok, args, m);
        }
        else
        {
            item = copy_token(tok);
            tok = tok->next;
        }
        item->has_space = first->has_space;
        item->at_bol = false;

        if (paste_next)
        {
            // A placemarker on either side leaves the other operand (C17 6.10.3.3p3).
            Token *after = item->next;
            if (cur->kind == TK_PLACEMARKER)
            {
                *cur = *item;
            }
            else if (item->kind != TK_PLACEMARKER)
            {
                *cur = *paste(cur, item);
            }
            cur->next = after;
            paste_next = false;
        }
        else
        {
            cur->next = item;
        }
        cur = last_token(cur);
    }
    return head.next;
}

/// @return The tokens without the placemarkers.
static Token *remove_placemarkers(Token *tok)
{
    Token head = {0};
    Token *cur = &head;
    for (; tok; tok = tok->next)
    {
        if (tok->kind != TK_PLACEMARKER)
        {
            cur = cur->next = tok;
        }
    }
    cur->next = NULL;
    return head.next;
}

/// @return The token of the file that a token came out of, through the macros it was expanded
/// from.
static const Token *root_origin(const Token *tok)
{
    while (tok->origin)
    {
        tok = tok->origin;
    }
    return tok;
}

static Token *builtin_file(const Token *name)
{
    return tokenize_text(quote_string(root_origin(name)->file_name), name);
}

static Token *builtin_line(const Token *name)
{
    return new_number((unsigned int) root_origin(name)->line_no, name);
}

static Token *builtin_counter(const Token *name)
{
    return new_number(counter++, name);
}

/// `__DATE__` and `__TIME__` are fixed, which the standard allows when the date is not
/// available: the output of the compiler depends on nothing but its input, and the libc needs no
/// clock.
static Token *builtin_date(const Token *name)
{
    return tokenize_text("\"Jan  1 1970\"", name);
}

static Token *builtin_time(const Token *name)
{
    return tokenize_text("\"00:00:00\"", name);
}

/// `__has_include` is a macro only so that `#ifdef __has_include` is true; replace_defined()
/// does its work.
static Token *builtin_has_include(const Token *name)
{
    error_tok(name, "__has_include can only be in #if and #elif");
    return NULL;
}

static const struct
{
    const char *name;
    BuiltinMacro builtin;
} builtin_macros[] = {
    {"__FILE__", builtin_file}, {"__LINE__", builtin_line}, {"__COUNTER__", builtin_counter},
    {"__DATE__", builtin_date}, {"__TIME__", builtin_time}, {"__has_include", builtin_has_include},
};

/// Expands the macro at `tok`, if it is one and its name is not in the token's hide set: the
/// tokens of the replacement go in front of the rest, so that they are scanned again (C17
/// 6.10.3.4), with the name added to their hide sets (Prosser's algorithm: for a function-like
/// macro the hide set is the intersection of those of the name and of the `)`).
///
/// @param[out] rest The first token of the replacement, or the token after the call.
/// @return Whether a macro was expanded.
static bool expand_macro(Token **rest, Token *tok)
{
    if (hideset_contains(tok->hideset, tok))
    {
        return false;
    }
    Macro *m = find_macro(tok);
    if (!m)
    {
        return false;
    }
    if (m->builtin)
    {
        Token *value = m->builtin(tok);
        value->origin = tok;
        value->next = tok->next;
        *rest = value;
        return true;
    }

    Hideset *hs;
    Token *body;
    Token *after;
    if (m->is_objlike)
    {
        hs = hideset_union(tok->hideset, new_hideset(m->name));
        body = remove_placemarkers(subst(m->body, NULL, NULL, m));
        after = tok->next;
    }
    else
    {
        if (!token_is(tok->next, "("))
        {
            return false;
        }
        Token *rparen;
        MacroArg *args = read_macro_args(&rparen, tok->next->next, m, tok);
        hs = hideset_union(hideset_intersection(tok->hideset, rparen->hideset),
                           new_hideset(m->name));
        body = remove_placemarkers(subst(m->body, NULL, args, m));
        after = rparen->next;
    }

    if (!body)
    {
        *rest = after;
        return true;
    }
    for (Token *t = body; t; t = t->next)
    {
        t->hideset = hideset_union(t->hideset, hs);
        t->origin = tok;
    }
    body->at_bol = tok->at_bol;
    body->has_space = tok->has_space;
    last_token(body)->next = after;
    *rest = body;
    return true;
}

/// Expands the macros of a list of tokens that ends with a `TK_EOF`: an argument, or the line of
/// an `#if`, `#include` or `#line`. There are no directives in it.
static Token *expand_tokens(Token *tok)
{
    Token head = {0};
    Token *cur = &head;
    while (tok->kind != TK_EOF)
    {
        if (expand_macro(&tok, tok))
        {
            continue;
        }
        cur = cur->next = tok;
        tok = tok->next;
    }
    cur->next = tok;
    return head.next;
}

/// `_Pragma("...")` (C17 6.10.9), which does what `#pragma ...` does, and is removed.
///
/// @param[out] rest The token after the `)`.
/// @return Whether `tok` is `_Pragma`.
static bool pragma_operator(Token **rest, Token *tok)
{
    if (!token_is(tok, "_Pragma"))
    {
        return false;
    }
    Token *str = tok->next->next;
    if (!token_is(tok->next, "(") || str->kind != TK_PP_STR || !token_is(str->next, ")"))
    {
        error_tok(tok, "_Pragma needs a string literal in parentheses");
    }
    // The string without its prefix and quotes, and without the backslash of \" and \\.
    const char *p = str->loc;
    while (*p != '"')
    {
        p++;
    }
    char *text = arena_alloc(str->len);
    size_t len = 0;
    for (p++; p < str->loc + str->len - 1; p++)
    {
        if (p[0] == '\\' && (p[1] == '"' || p[1] == '\\'))
        {
            p++;
        }
        text[len++] = *p;
    }
    if (token_is(tokenize_text(text, str), "once"))
    {
        const char *file = root_origin(tok)->file->name;
        hashmap_put(&pragma_once, file, strlen(file), tok);
    }
    *rest = str->next->next;
    return true;
}

/// The end of an included file: its conditionals have to be closed.
static void end_of_include(const Token *eof)
{
    if (cond_incl && cond_incl->tok->file == eof->file)
    {
        error_tok(cond_incl->tok, "unterminated #%s", token_text(cond_incl->tok));
    }
    include_depth--;
}

/// The preprocessor proper: carries out the directives and expands the macros of the tokens of
/// a file, and of the files they include.
static Token *preprocess_tokens(Token *tok)
{
    Token head = {0};
    Token *cur = &head;
    // A TK_EOF that is not the last token ends an included file.
    while (tok->kind != TK_EOF || tok->next)
    {
        if (tok->kind == TK_EOF)
        {
            end_of_include(tok);
            tok = tok->next;
            continue;
        }
        if (expand_macro(&tok, tok) || pragma_operator(&tok, tok))
        {
            continue;
        }
        if (!is_hash(tok))
        {
            cur = cur->next = tok;
            tok = tok->next;
            continue;
        }

        Token *name = tok->next;
        if (name->at_bol)
        {
            // A # alone on its line does nothing.
            tok = name;
            continue;
        }
        size_t i = 0;
        while (i < sizeof(directives) / sizeof(directives[0])
               && !token_is(name, directives[i].name))
        {
            i++;
        }
        if (i == sizeof(directives) / sizeof(directives[0]))
        {
            error_tok(name, "invalid preprocessing directive #%s", token_text(name));
        }
        tok = directives[i].handler(name);
    }
    cur->next = tok;
    return head.next;
}

/// Runs the directives of a text that is not part of the file, and keeps nothing else of it.
static void preprocess_text(const char *name, const char *text)
{
    preprocess_tokens(tokenize(new_source_file(name, text)));
    if (cond_incl)
    {
        error_tok(cond_incl->tok, "unterminated #%s", token_text(cond_incl->tok));
    }
}

Token *preprocess(Token *tok, const PreprocessOptions *opts)
{
    const HashMap empty = {0};
    macros = empty;
    pragma_once = empty;
    cond_incl = NULL;
    options = opts;
    include_depth = 0;
    counter = 0;

    size_t len = 1;
    for (size_t i = 0; i < sizeof(predefined_macros) / sizeof(predefined_macros[0]); i++)
    {
        len += strlen("#define \n") + strlen(predefined_macros[i]);
    }
    char *text = arena_alloc(len);
    size_t pos = 0;
    for (size_t i = 0; i < sizeof(predefined_macros) / sizeof(predefined_macros[0]); i++)
    {
        memcpy(text + pos, "#define ", 8);
        memcpy(text + pos + 8, predefined_macros[i], strlen(predefined_macros[i]));
        pos += 8 + strlen(predefined_macros[i]);
        text[pos++] = '\n';
    }
    preprocess_text("<built-in>", text);
    for (size_t i = 0; i < sizeof(builtin_macros) / sizeof(builtin_macros[0]); i++)
    {
        Macro *m = arena_alloc(sizeof(Macro));
        m->name = builtin_macros[i].name;
        m->is_objlike = true;
        m->builtin = builtin_macros[i].builtin;
        hashmap_put(&macros, m->name, strlen(m->name), m);
    }
    if (opts->command_line)
    {
        preprocess_text("<command line>", opts->command_line);
    }

    tok = preprocess_tokens(tok);
    if (cond_incl)
    {
        error_tok(cond_incl->tok, "unterminated #%s", token_text(cond_incl->tok));
    }
    return tok;
}

/// @return Whether two tokens, written together without a space, would be read as other tokens
/// (`+` `+`, `1e` `+`, `L` `"s"`, `/` `/`).
static bool needs_space(const Token *prev, const Token *tok)
{
    // Tokens that were next to each other in a file were read as these two already. This only
    // saves the work below.
    if (prev->file == tok->file && prev->loc + prev->len == tok->loc)
    {
        return false;
    }
    char *text = arena_alloc(prev->len + tok->len + 1);
    memcpy(text, prev->loc, prev->len);
    memcpy(text + prev->len, tok->loc, tok->len);
    const Token *relexed = tokenize(new_source_file("", text));
    return relexed->len != prev->len || relexed->next->len != tok->len;
}

void print_tokens(const Token *tok, FILE *out)
{
    const Token *prev = NULL;
    for (; tok->kind != TK_EOF; tok = tok->next)
    {
        if (prev && tok->at_bol)
        {
            fputc('\n', out);
        }
        else if (prev && (tok->has_space || needs_space(prev, tok)))
        {
            fputc(' ', out);
        }
        fwrite(tok->loc, 1, tok->len, out);
        prev = tok;
    }
    if (prev)
    {
        fputc('\n', out);
    }
}
