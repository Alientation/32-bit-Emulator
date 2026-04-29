#include "ccompiler/lexer.h"

#include "ccompiler/massert.h"
#include "ccompiler/util.h"

#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <stdbool.h>
#include <assert.h>

static void srcmap_init (srcmap_t *map);
static void srcmap_free (srcmap_t *map);
static void srcmap_lookup (const srcmap_t *map, size_t proc_offset, const char **out_file,
                           size_t *out_line, size_t *out_col);
static void srcmap_extend (srcmap_t *map);
static void srcmap_push (srcmap_t *map, size_t proc_offset, const char *file,
                         size_t orig_line, size_t orig_col);

static void _find_line(const char *src, size_t target_line,
                       const char **out_start, size_t *out_length);
static void _lex_error_at (const lexer_data_t *lexer, size_t proc_offset,
                           const char *msg_fmt, ...);

static char *_file_read (const char *filepath);

static void _add_token (lexer_data_t *lexer, token_t *tok);

static bool _process_lexer (lexer_data_t *lexer);
static bool _phase_1_2 (lexer_data_t *lexer);
static bool _phase_3_4 (lexer_data_t *lexer);

typedef struct Token_Pattern
{
    enum TokenType type;
    const char *pattern;
} tokpat_t;

static tokpat_t CTOK_KEYWORDS[] =
{
    { TOKEN_KEYWORD_AUTO, "auto" },
    { TOKEN_KEYWORD_BOOL, "bool" },
    { TOKEN_KEYWORD_BREAK, "break" },
    { TOKEN_KEYWORD_CASE, "case" },
    { TOKEN_KEYWORD_CHAR, "char" },
    { TOKEN_KEYWORD_CONST, "const" },
    { TOKEN_KEYWORD_CONSTEXPR, "constexpr" },
    { TOKEN_KEYWORD_CONTINUE, "continue" },
    { TOKEN_KEYWORD_DEFAULT, "default" },
    { TOKEN_KEYWORD_DO, "do" },
    { TOKEN_KEYWORD_DOUBLE, "double" },
    { TOKEN_KEYWORD_ELSE, "else" },
    { TOKEN_KEYWORD_ENUM, "enum" },
    { TOKEN_KEYWORD_EXTERN, "extern" },
    { TOKEN_KEYWORD_FALSE, "false" },
    { TOKEN_KEYWORD_FLOAT, "float" },
    { TOKEN_KEYWORD_FOR, "for" },
    { TOKEN_KEYWORD_GOTO, "goto" },
    { TOKEN_KEYWORD_IF, "if" },
    { TOKEN_KEYWORD_INLINE, "inline" },
    { TOKEN_KEYWORD_INT, "int" },
    { TOKEN_KEYWORD_LONG, "long" },
    { TOKEN_KEYWORD_NULLPTR, "nullptr" },
    { TOKEN_KEYWORD_REGISTER, "register" },
    { TOKEN_KEYWORD_RESTRICT, "restrict" },
    { TOKEN_KEYWORD_RETURN, "return" },
    { TOKEN_KEYWORD_SHORT, "short" },
    { TOKEN_KEYWORD_SIGNED, "signed" },
    { TOKEN_KEYWORD_SIZEOF, "sizeof" },
    { TOKEN_KEYWORD_STATIC, "static" },
    { TOKEN_KEYWORD_STATIC_ASSERT, "static_assert" },
    { TOKEN_KEYWORD_STRUCT, "struct" },
    { TOKEN_KEYWORD_SWITCH, "switch" },
    { TOKEN_KEYWORD_THREAD_LOCAL, "thread_local" },
    { TOKEN_KEYWORD_TRUE, "true" },
    { TOKEN_KEYWORD_TYPEDEF, "typedef" },
    { TOKEN_KEYWORD_TYPEOF, "typeof" },
    { TOKEN_KEYWORD_TYPEOF_UNQUAL, "typeof_unqual" },
    { TOKEN_KEYWORD_UNION, "union" },
    { TOKEN_KEYWORD_UNSIGNED, "unsigned" },
    { TOKEN_KEYWORD_VOID, "void" },
    { TOKEN_KEYWORD_VOLATILE, "volatile" },
    { TOKEN_KEYWORD_WHILE, "while" },
    { TOKEN_KEYWORD_ALIGNAS, "_Alignas" },
    { TOKEN_KEYWORD_ALIGNOF, "_Alignof" },
    { TOKEN_KEYWORD_ATOMIC, "_Atomic" },
    { TOKEN_KEYWORD_BIGINT, "_BigInt" },
    { TOKEN_KEYWORD_BOOL, "_Bool" },
    { TOKEN_KEYWORD_COMPLEX, "_Complex" },
    { TOKEN_KEYWORD_DECIMAL128, "_Decimal128" },
    { TOKEN_KEYWORD_DECIMAL32, "_Decimal32" },
    { TOKEN_KEYWORD_DECIMAL64, "_Decimal64" },
    { TOKEN_KEYWORD_GENERIC, "_Generic" },
    { TOKEN_KEYWORD_IMAGINARY, "_Imaginary" },
    { TOKEN_KEYWORD_NORETURN, "_Noreturn" },
    { TOKEN_KEYWORD_STATIC_ASSERT, "_Static_assert" },
    { TOKEN_KEYWORD_THREAD_LOCAL, "_Thread_local" },
};


static tokpat_t CTOK_OPERATORS[] =
{
    { TOKEN_ELLIPSIS, "..." },
    { TOKEN_RIGHT_ASSIGN, ">>=" },
    { TOKEN_LEFT_ASSIGN, "<<=" },
    { TOKEN_ADD_ASSIGN, "+=" },
    { TOKEN_SUB_ASSIGN, "-=" },
    { TOKEN_MUL_ASSIGN, "*=" },
    { TOKEN_DIV_ASSIGN, "/=" },
    { TOKEN_MOD_ASSIGN, "%=" },
    { TOKEN_AND_ASSIGN, "&=" },
    { TOKEN_XOR_ASSIGN, "^=" },
    { TOKEN_OR_ASSIGN, "|=" },
    { TOKEN_RIGHT_OP, ">>" },
    { TOKEN_LEFT_OP, "<<" },
    { TOKEN_INC_OP, "++" },
    { TOKEN_DEC_OP, "--" },
    { TOKEN_PTR_OP, "->" },
    { TOKEN_AND_OP, "&&" },
    { TOKEN_OR_OP, "||" },
    { TOKEN_LE_OP, "<=" },
    { TOKEN_GE_OP, ">=" },
    { TOKEN_EQ_OP, "==" },
    { TOKEN_NE_OP, "!=" },

    { TOKEN_SEMICOLON, ";" },
    { TOKEN_OPEN_BRACE, "{" },
    { TOKEN_CLOSE_BRACE, "}" },
    { TOKEN_COMMA, "," },
    { TOKEN_COLON, ":" },
    { TOKEN_EQUAL_SIGN, "=" },
    { TOKEN_OPEN_PARENTHESIS, "(" },
    { TOKEN_CLOSE_PARENTHESIS, ")" },
    { TOKEN_OPEN_BRACKET, "[" },
    { TOKEN_CLOSE_BRACKET, "]" },
    { TOKEN_PERIOD, "." },
    { TOKEN_AMPERSAND, "&" },
    { TOKEN_EXCLAMATION_MARK, "!" },
    { TOKEN_TILDE, "~" },
    { TOKEN_HYPEN, "-" },
    { TOKEN_PLUS, "+" },
    { TOKEN_ASTERICK, "*" },
    { TOKEN_FORWARD_SLASH, "/" },
    { TOKEN_PERCENT_SIGN, "%" },
    { TOKEN_LEFT_ARROW, "<" },
    { TOKEN_RIGHT_ARROW, ">" },
    { TOKEN_CARROT, "^" },
    { TOKEN_PIPE, "|" },
    { TOKEN_QUESTION_MARK, "?" },
};

static const tokpat_t CTOK_PATTERNS[] =
{
    // Float with leading digits.
    { TOKEN_F_CONSTANT, "^[0-9]+\\.[0-9]*([Ee][+-]?[0-9]+)?[fFlL]?" },

    // Float with leading dot.
    { TOKEN_F_CONSTANT, "^\\.[0-9]+([Ee][+-]?[0-9]+)?[fFlL]?" },

    // Float without dot.
    { TOKEN_F_CONSTANT, "^[0-9]+[Ee][+-]?[0-9]+[fFlL]?" },

    // Decimal.
    { TOKEN_I_CONSTANT, "^[1-9][0-9]*(([uU][lL]{0,2})|([lL]{1,2}[uU]?))?" },
    // Hexadecimal.
    { TOKEN_I_CONSTANT, "^0[xX][0-9a-fA-F]+(([uU][lL]{0,2})|([lL]{1,2}[uU]?))?" },
    // Octal.
    { TOKEN_I_CONSTANT, "^0[0-7]*(([uU][lL]{0,2})|([lL]{1,2}[uU]?))?" },
    // Binary.
    { TOKEN_I_CONSTANT, "^0[bB][01]+(([uU][lL]{0,2})|([lL]{1,2}[uU]?))?" },

    // Identifiers can only start with an alphabet or underscore character then
    // followed by a alphanumeric characters.
    { TOKEN_IDENTIFIER, "^[a-zA-Z_][a-zA-Z0-9_]*" },
};

static void _lex_error_at (const lexer_data_t * const lexer, const size_t proc_offset,
                           const char * const msg_fmt, ...)
{
    const char *file;
    size_t line, col;
    srcmap_lookup (&lexer->srcmap, proc_offset, &file, &line, &col);

    if (file)
    {
        fprintf (stderr, "[ERROR]: %s:%zu:%zu: ", file, line, col);
    }

    va_list args;
    va_start (args, msg_fmt);

    vfprintf (stderr, msg_fmt, args);
    va_end (args);

    fprintf (stderr, "\n");

    const char *line_start;
    size_t line_length;
    if (file && strcmp (file, lexer->file) != 0)
    {
        // Need to grab the file and reread the contents of it instead.
        _cleanup_free_ const char *new_src = _file_read (file);
        if (!new_src)
        {
            fprintf (stderr, "[ERROR]: failed to read '%s'\n", file);
            return;
        }

        _find_line (new_src, line, &line_start, &line_length);
    }
    else
    {
        _find_line (lexer->src_orig, line, &line_start, &line_length);
    }

    fprintf (stderr, "%-5zu | %.*s\n", line, (int) line_length, line_start);
    fprintf (stderr, "%-5s    ", "");

    for (size_t i = 1; i < col; i++)
    {
        // Preserve tabs so the caret lines up correctly even if the source line has tab characters.
        fprintf (stderr, "%c", line_start[i - 1] == '\t' ? '\t' : ' ');
    }
    fprintf(stderr, "^\n");
}

static void _find_line(const char * const src, const size_t target_line,
                       const char ** const out_start, size_t * const out_length)
{
    size_t cur_line = 1;
    const char *p = src;

    // Walk forward until we reach the target line.
    while (*p != '\0' && cur_line < target_line)
    {
        if (*p == '\n' || (*p == '\r' && *(p + 1) != '\n'))
        {
            cur_line++;
        }
        p++;
    }

    *out_start = p;

    // Walk to end of line.
    const char *end = p;
    while (*end != '\0' && *end != '\n' && *end != '\r')
    {
        end++;
    }

    *out_length = (size_t) (end - p);
}

static char *_file_read (const char * const filepath)
{
    _cleanup_free_ char *buffer = NULL;
    _cleanup_fclose_ FILE *file = NULL;
    long file_size = 0;

    if (!(file = fopen (filepath, "rb")))
    {
        fprintf (stderr, "ERROR: failed to find file '%s'\n", filepath);
        return NULL;
    }

    if (fseek (file, 0, SEEK_END) == -1)
    {
        fprintf (stderr, "ERROR: fseek failed for file '%s'\n", filepath);
        return NULL;
    }

    if ((file_size = ftell (file)) == -1)
    {
        fprintf (stderr, "ERROR: ftell failed for file '%s'\n", filepath);
        return NULL;
    }
    rewind (file);

    buffer = (char *) calloc (file_size + 1, sizeof (char));
    if (buffer == NULL)
    {
        fprintf (stderr, "ERROR: failed to allocate memory\n");
        return NULL;
    }

    if (fread (buffer, 1, file_size, file) != (size_t) file_size)
    {
        fprintf (stderr, "ERROR: failed to read file\n");
        return NULL;
    }

    return STEAL (buffer);
}

bool lex_file (const char *filepath,
              lexer_data_t *lexer)
{
    char *buffer = _file_read (filepath);
    if (!buffer)
    {
        return false;
    }

    lexer->src = buffer;
    lexer->length = strlen (buffer);

    lexer->file = calloc (strlen (filepath), sizeof (char));
    if (!lexer->file)
    {
        fprintf (stderr, "ERROR: failed to allocate memory\n");
        return false;
    }

    memcpy (lexer->file, filepath, strlen (filepath));

    return _process_lexer (lexer);
}

bool lex_str (const char *str,
             lexer_data_t *lexer)
{
    lexer->length = strlen (str);
    lexer->src = (char *) calloc (lexer->length + 1, sizeof (char));
    lexer->file = NULL;

    if (!lexer->src)
    {
        fprintf (stderr, "ERROR: failed to allocate memory\n");
        return false;
    }

    memcpy (lexer->src, str, lexer->length);
    return _process_lexer (lexer);
}

void lexer_print (const lexer_data_t *lexer)
{
    printf ("PRINTING TOKENS");
    for (size_t i = 0; i < lexer->tokarr.tok_cnt; i++)
    {
        printf("\n[%lu] ", i);
        token_print (&lexer->tokarr.toks[i]);
    }
    printf ("\n");
}

void lexer_init (lexer_data_t * const lexer)
{
    *lexer = (lexer_data_t) {0};
    srcmap_init (&lexer->srcmap);
}

void lexer_free (lexer_data_t * const lexer)
{
    free (lexer->file);
    free (lexer->src);
    free (lexer->src_orig);
    free (lexer->tokarr.toks);
    srcmap_free (&lexer->srcmap);

    *lexer = (lexer_data_t) {0};
}

char *token_tostr (token_t *tok)
{
    static char strbuffer[256];
    if ((unsigned long) tok->len >= ARRAY_LEN (strbuffer))
    {
        fprintf (stderr, "ERROR: token too long to print, length %zu at line %zu\n",
                 tok->len, tok->line);
        exit (EXIT_FAILURE);
    }

    memcpy (strbuffer, tok->src, tok->len);
    strbuffer[tok->len] = '\0';
    return strbuffer;
}

void token_print (token_t *tok)
{
    static const char * tokentype_to_str[] =
    {
        "TOKEN_ERROR", "TOKEN_KEYWORD_AUTO", "TOKEN_KEYWORD_BREAK", "TOKEN_KEYWORD_CASE",
        "TOKEN_KEYWORD_CHAR", "TOKEN_KEYWORD_CONST", "TOKEN_KEYWORD_CONSTEXPR",
        "TOKEN_KEYWORD_CONTINUE", "TOKEN_KEYWORD_DEFAULT", "TOKEN_KEYWORD_DO",
        "TOKEN_KEYWORD_DOUBLE", "TOKEN_KEYWORD_ELSE", "TOKEN_KEYWORD_ENUM", "TOKEN_KEYWORD_EXTERN",
        "TOKEN_KEYWORD_FALSE", "TOKEN_KEYWORD_FLOAT", "TOKEN_KEYWORD_FOR", "TOKEN_KEYWORD_GOTO",
        "TOKEN_KEYWORD_IF", "TOKEN_KEYWORD_INLINE", "TOKEN_KEYWORD_INT", "TOKEN_KEYWORD_LONG",
        "TOKEN_KEYWORD_NULLPTR", "TOKEN_KEYWORD_REGISTER", "TOKEN_KEYWORD_RESTRICT",
        "TOKEN_KEYWORD_RETURN", "TOKEN_KEYWORD_SHORT", "TOKEN_KEYWORD_SIGNED",
        "TOKEN_KEYWORD_SIZEOF", "TOKEN_KEYWORD_STATIC", "TOKEN_KEYWORD_STRUCT",
        "TOKEN_KEYWORD_SWITCH", "TOKEN_KEYWORD_TRUE", "TOKEN_KEYWORD_TYPEDEF",
        "TOKEN_KEYWORD_TYPEOF", "TOKEN_KEYWORD_TYPEOF_UNQUAL", "TOKEN_KEYWORD_UNION",
        "TOKEN_KEYWORD_UNSIGNED", "TOKEN_KEYWORD_VOID", "TOKEN_KEYWORD_VOLATILE",
        "TOKEN_KEYWORD_WHILE", "TOKEN_KEYWORD_ALIGNAS", "TOKEN_KEYWORD_ALIGNOF",
        "TOKEN_KEYWORD_ATOMIC", "TOKEN_KEYWORD_BIGINT", "TOKEN_KEYWORD_BOOL",
        "TOKEN_KEYWORD_COMPLEX", "TOKEN_KEYWORD_DECIMAL128", "TOKEN_KEYWORD_DECIMAL32",
        "TOKEN_KEYWORD_DECIMAL64", "TOKEN_KEYWORD_GENERIC", "TOKEN_KEYWORD_IMAGINARY",
        "TOKEN_KEYWORD_NORETURN", "TOKEN_KEYWORD_STATIC_ASSERT", "TOKEN_KEYWORD_THREAD_LOCAL",
        "TOKEN_IDENTIFIER", "TOKEN_I_CONSTANT", "TOKEN_F_CONSTANT", "TOKEN_STRING_LITERAL",
        "TOKEN_ELLIPSIS", "TOKEN_RIGHT_ASSIGN", "TOKEN_LEFT_ASSIGN", "TOKEN_ADD_ASSIGN",
        "TOKEN_SUB_ASSIGN", "TOKEN_MUL_ASSIGN", "TOKEN_DIV_ASSIGN", "TOKEN_MOD_ASSIGN",
        "TOKEN_AND_ASSIGN", "TOKEN_XOR_ASSIGN", "TOKEN_OR_ASSIGN", "TOKEN_RIGHT_OP",
        "TOKEN_LEFT_OP", "TOKEN_INC_OP", "TOKEN_DEC_OP", "TOKEN_PTR_OP", "TOKEN_AND_OP",
        "TOKEN_OR_OP", "TOKEN_LE_OP", "TOKEN_GE_OP", "TOKEN_EQ_OP", "TOKEN_NE_OP",
        "TOKEN_SEMICOLON", "TOKEN_OPEN_BRACE", "TOKEN_CLOSE_BRACE", "TOKEN_COMMA", "TOKEN_COLON",
        "TOKEN_EQUAL_SIGN", "TOKEN_OPEN_PARENTHESIS", "TOKEN_CLOSE_PARENTHESIS",
        "TOKEN_OPEN_BRACKET", "TOKEN_CLOSE_BRACKET", "TOKEN_PERIOD", "TOKEN_AMPERSAND",
        "TOKEN_EXCLAMATION_MARK", "TOKEN_TILDE", "TOKEN_HYPEN", "TOKEN_PLUS", "TOKEN_ASTERICK",
        "TOKEN_FORWARD_SLASH", "TOKEN_PERCENT_SIGN", "TOKEN_LEFT_ARROW", "TOKEN_RIGHT_ARROW",
        "TOKEN_CARROT", "TOKEN_PIPE", "TOKEN_QUESTION_MARK"
    };

    static_assert (NUM_TOKEN_TYPES == ARRAY_LEN (tokentype_to_str));

    char *buffer = token_tostr (tok);
    if (tok->type < ARRAY_LEN (tokentype_to_str))
    {
        printf ("%s: \'%s\'", tokentype_to_str[tok->type], buffer);
    }
    else
    {
        fprintf (stderr, "UNKNOWN_TYPE: \'%s\'\n", buffer);
        exit (EXIT_FAILURE);
    }
}

static void _add_token (lexer_data_t *lexer, token_t *tok)
{
    if (lexer->tokarr.tok_cnt + 1 > lexer->tokarr.tok_cap)
    {
        const int new_cap = lexer->tokarr.tok_cap * 2 + 10;
        token_t * const new_arr = realloc (lexer->tokarr.toks, new_cap * sizeof (token_t));
        if (new_arr == NULL)
        {
            fprintf (stderr, "ERROR: failed to allocate memory\n");
            exit (EXIT_FAILURE);
        }

        lexer->tokarr.toks = new_arr;
        lexer->tokarr.tok_cap = new_cap;
    }

    lexer->tokarr.toks[lexer->tokarr.tok_cnt++] = *tok;
}

static void srcmap_init (srcmap_t * const map)
{
    map->capacity = 0;
    map->count = 0;
    map->spans = NULL;
}

static void srcmap_free (srcmap_t * const map)
{
    free (map->spans);
    map->capacity = 0;
    map->count = 0;
    map->spans = NULL;
}

static void srcmap_lookup (const srcmap_t * const map, const size_t proc_offset,
                           const char ** const out_file, size_t * const out_line,
                           size_t * const out_col)
{
    massert(map->count > 0, "srcmap is empty");

    // Binary search for largest span where span.proc_offset <= proc_offset
    size_t lo = 0;
    size_t hi = map->count - 1;
    while (lo < hi)
    {
        const size_t mid = lo + (hi - lo + 1) / 2;
        if (map->spans[mid].proc_offset <= proc_offset)
        {
            lo = mid;
        }
        else
        {
            hi = mid - 1;
        }
    }

    const srcspan_t *s = &map->spans[lo];
    const size_t delta = proc_offset - s->proc_offset;

    *out_file = s->file;
    *out_line = s->orig_line;
    *out_col = s->orig_col + delta;
}

static void srcmap_extend (srcmap_t * const map)
{
    const size_t new_capacity = map->capacity * 2 + 10;
    srcspan_t * const new_spans = realloc (map->spans, new_capacity * sizeof (srcspan_t));
    if (!new_spans)
    {
        fprintf (stderr, "Error: memory allocation failed.\n");
        exit (EXIT_FAILURE);
    }

    map->spans = new_spans;
    map->capacity = new_capacity;
}

static void srcmap_push (srcmap_t * const map, const size_t proc_offset, const char * const file,
                         const size_t orig_line, const size_t orig_col)
{
    if (map->count + 1 > map->capacity)
    {
        srcmap_extend (map);
    }
    map->spans[map->count++] = (srcspan_t)
    {
        .proc_offset = proc_offset,
        .file = file,
        .orig_line = orig_line,
        .orig_col = orig_col
    };
}

static bool _process_lexer (lexer_data_t *lexer)
{
    lexer->src_orig = calloc (strlen (lexer->src), sizeof (char));
    if (!lexer->src_orig)
    {
        fprintf (stderr, "ERROR: memory allocation failed\n");
        return false;
    }
    memcpy (lexer->src_orig, lexer->src, strlen (lexer->src));

    return _phase_1_2 (lexer) && _phase_3_4 (lexer);
}

// https://en.cppreference.com/w/c/language/translation_phases
static bool _phase_1_2 (lexer_data_t *lexer)
{
    // The first span, processed offset 0 maps to line 1, col 1.
    srcmap_push(&lexer->srcmap, 0, lexer->file, 1, 1);

    size_t idx = 0;
    size_t new_idx = 0;
    size_t orig_line = 1;
    size_t orig_col = 1;

    while (idx < lexer->length)
    {
        const char c  = lexer->src[idx];
        const char c1 = (idx + 1 < lexer->length) ? lexer->src[idx + 1] : '\0';
        const char c2 = (idx + 2 < lexer->length) ? lexer->src[idx + 2] : '\0';

        // Escaped newline (concatenate the two lines together).
        if (c == '\\' && (c1 == '\n' || (c1 == '\r' && c2 != '\n')))
        {
            // Erase '\' and '\n'.
            orig_line++;
            orig_col = 1;
            idx += 2;

            // Record a new span since we are on a new line in source.
            srcmap_push (&lexer->srcmap, new_idx, lexer->file, orig_line, orig_col);
            continue;
        }

        // Escaped newline (concatenate the two lines together).
        if (c == '\\' && c1 == '\r' && c2 == '\n')
        {
            // Erase '\', '\r', '\n'.
            orig_line++;
            orig_col = 1;
            idx += 3;

            srcmap_push (&lexer->srcmap, new_idx, lexer->file, orig_line, orig_col);
            continue;
        }

        // OS dependent newlines.
        if (c == '\r' && c1 == '\n')
        {
            // Skip the '\r', let the '\n' be handled on the next iteration.
            orig_col++;
            idx++;
            continue;
        }

        // OS dependent newlines.
        if (c == '\r')
        {
            lexer->src[new_idx++] = '\n';
            orig_line++;
            orig_col = 1;
            idx++;

            srcmap_push (&lexer->srcmap, new_idx, lexer->file, orig_line, orig_col);
            continue;
        }

        // Strip Vertical tabs and form feeds.
        if (c == '\v' || c == '\f')
        {
            orig_col++;
            idx++;
            continue;
        }

        // Standard character, copy.
        lexer->src[new_idx++] = c;
        idx++;

        if (c == '\n')
        {
            orig_line++;
            orig_col = 1;

            srcmap_push (&lexer->srcmap, new_idx, lexer->file, orig_line, orig_col);
        }
        else
        {
            orig_col++;
        }
    }

    lexer->src[new_idx] = '\0';
    lexer->length = new_idx;
    return true;
}


/* Handle comment if exists otherwise skip. Returns if comment is processed. */
static inline bool _handle_comment (lexer_data_t *lexer, size_t *offset)
{
    const char cur = lexer->src[*offset];
    const char nxt = lexer->src[*offset + 1];

    if (cur != '/')
    {
        return false;
    }

    if (nxt == '/')
    {
        while (*offset < lexer->length && lexer->src[*offset] != '\n')
        {
            (*offset)++;
        }

        return true;
    }
    else if (nxt == '*')
    {
        (*offset) += 2;
        while (*offset < lexer->length)
        {
            if (lexer->src[*offset] == '*' && lexer->src[*offset + 1] == '/')
            {
                (*offset) += 2;
                break;
            }
            (*offset)++;
        }

        return true;
    }
    return false;
}

static inline bool _handle_char (lexer_data_t *lexer, size_t *offset)
{
    if (lexer->src[*offset] == '\"')
    {
        return false;
    }

    if (lexer->src[*offset] == '\\')
    {
        (*offset)++;
        const char esc = lexer->src[*offset];
        switch (esc)
        {
            // Single character escapes, skip one char.
            case '"': case '\\': case '/':
            case 'a': case 'b':  case 'f':
            case 'n': case 'r':  case 't':
            case 'v':
                (*offset)++;
                break;

            // Hex escape \xNN..., skip hex digits.
            case 'x':
                (*offset)++;
                if (!isxdigit (lexer->src[*offset]))
                {
                    _lex_error_at (lexer, *offset, "invalid hex escape");
                    return false;
                }
                while (isxdigit (lexer->src[*offset]))
                {
                    (*offset)++;
                }
                break;

            // Octal escape \NNN — 1 to 3 octal digits
            case '0': case '1': case '2': case '3':
            case '4': case '5': case '6': case '7':
            {
                int count = 0;
                while (count < 3 && lexer->src[*offset] >= '0' && lexer->src[*offset] <= '7')
                {
                    (*offset)++;
                    count++;
                }
                break;
            }

            // Unicode \uNNNN.
            case 'u':
            {
                (*offset)++;
                for (int i = 0; i < 4; i++)
                {
                    if (!isxdigit (lexer->src[*offset]))
                    {
                        _lex_error_at (lexer, *offset, "invalid unicode escape");
                        return false;
                    }
                    (*offset)++;
                }
                break;
            }

            default:
                _lex_error_at (lexer, *offset, "unknown escape sequence '\\%c'", esc);
                return false;
        }
    }
    else
    {
        (*offset)++;
    }

    return true;
}

/* Process C string. Returns false if it is not valid. */
static inline bool _handle_c_str (lexer_data_t *lexer, size_t *offset)
{
    assert (lexer->src[*offset] == '\"');

    const size_t start = *offset;

    // Skip first quote.
    (*offset)++;

    bool closed = false;
    while (*offset <= lexer->length && lexer->src[*offset] != '\n')
    {
        const char c = lexer->src[*offset];

        if (c == '\"')
        {
            // Consume closing quote.
            (*offset)++;
            closed = true;
            break;
        }

        if (!_handle_char (lexer, offset))
        {
            return false;
        }
    }

    if (!closed)
    {
        _lex_error_at (lexer, *offset, "unterminated string literal");
        return false;
    }

    const char *file;
    size_t line, col;
    srcmap_lookup (&lexer->srcmap, start, &file, &line, &col);

    token_t tok = {
        .type   = TOKEN_STRING_LITERAL,
        .src = lexer->src + start,
        .len = *offset - start,
        .file = file,
        .line = line,
        .col = col,
    };
    _add_token(lexer, &tok);
    return true;

}

/* Process a preprocessor directive. Assumption is that the '#' at offset is the
   first non-whitespace character in the line. Returns false if invalid preprocessor. */
static bool _handle_preprocessor (lexer_data_t * const lexer, size_t * const offset)
{
    assert (lexer->src[*offset] == '#');

    // TODO:

    #define STREQ(directive) (strncmp (lexer->src + (*offset), directive, strlen(directive)) == 0)

    if (STREQ("#include"))
    {
        // Expect argument that follows to be of the form '<file>' or '"file"'.
        (*offset) += strlen("#include");
        while (lexer->src[*offset] == ' ')
        {
            (*offset)++;
        }

        if (lexer->src[*offset] != '<' && lexer->src[*offset] != '\"')
        {
            _lex_error_at (lexer, *offset, "invalid #include syntax");
            return false;
        }
        (*offset)++;

        const char * const start = lexer->src + (*offset);
        const char *end = start;
        while (*end != '>' && *end != '\"')
        {
            end++;
        }

        const size_t len = (uintptr_t) end - (uintptr_t) start;
        if (len == 0)
        {
            _lex_error_at (lexer, *offset, "invalid #include syntax");
            return false;
        }

        if (*end != *(start - 1))
        {
            _lex_error_at (lexer, *offset, "invalid #include syntax");
            return false;
        }

        char * const filepath = calloc(len + 1, sizeof(char));
        memcpy (filepath, start, len);

        // TODO:
        // https://gcc.gnu.org/onlinedocs/cpp/Include-Syntax.html
        // #include <file> searches for files in system directories included with -I
        // #include "file" searches first for file in the directory of the current file,
        // then in quote directories, and then finally in system directories.
    }
    else if (STREQ("define"))
    {

    }
    else
    {
        _lex_error_at (lexer, *offset, "unknown preprocessor directive");
        return false;
    }

    #undef STREQ

    return true;
}

// Tokenize while handling preprocessor directives.
static bool _phase_3_4 (lexer_data_t *lexer)
{
    // Compile the regex only once.
    static regex_t regex[ARRAY_LEN (CTOK_PATTERNS)];
    static bool regex_init = false;
    if (!regex_init)
    {
        // TODO: not thread safe.
        for (size_t i = 0; i < ARRAY_LEN (CTOK_PATTERNS); i++)
        {
            if (regcomp (&regex[i], CTOK_PATTERNS[i].pattern, REG_EXTENDED) != 0)
            {
                M_UNREACHABLE ("Failed to compile regex '%s'\n", CTOK_PATTERNS[i].pattern);
                return false;
            }
        }
        regex_init = true;
    }

    size_t offset = 0;
    while (offset < lexer->length)
    {
        const char cur = lexer->src[offset];

        if (isspace (cur))
        {
            offset++;
            continue;
        }

        if (_handle_comment (lexer, &offset))
        {
            continue;
        }

        // Handle C string.
        if (cur == '\"')
        {
            if (!_handle_c_str (lexer, &offset))
            {
                return false;
            }
            continue;
        }

        // Check if preprocessor directive.
        if (cur == '#')
        {
            for (char *ch = lexer->src + offset; ch != lexer->src && *ch != '\n'; ch--)
            {
                // '#' must be the first non-whitespace character of the line.
                if (!isspace (*ch))
                {
                    _lex_error_at (lexer, offset, "preprocessor directive expected on new line");
                    return false;
                }
            }

            if (!_handle_preprocessor (lexer, &offset))
            {
                return false;
            }
            continue;
        }

        bool matched = false;
        regmatch_t regmatch = {0};
        tokentype_t type;

        // Handle character.
        if (cur == '\'')
        {
            // Escape sequence.
            size_t temp_offset = offset + 1;
            if (!_handle_char (lexer, &temp_offset))
            {
                return false;
            }

            if (lexer->src[temp_offset] != '\'')
            {
                _lex_error_at (lexer, offset, "Invalid char");
                return false;
            }

            type = TOKEN_I_CONSTANT;
            regmatch.rm_eo = temp_offset - offset + 1;
            matched = true;
        }

        // Cheap check if we can match prefix to keyword.
        // TODO: Perhaps change to hashtable approach.
        for (size_t i = 0; i < ARRAY_LEN (CTOK_KEYWORDS) && !matched; i++)
        {
            const size_t len = strlen (CTOK_KEYWORDS[i].pattern);
            const char endc = lexer->src[offset + len];
            if (strncmp (CTOK_KEYWORDS[i].pattern, lexer->src + offset,
                strlen (CTOK_KEYWORDS[i].pattern)) == 0 && (endc != '_' && !isalnum (endc)))
            {
                regmatch.rm_eo = len;
                type = CTOK_KEYWORDS[i].type;
                matched = true;
            }
        }

        // Try the regex if not matched to any keyword.
        for (size_t i = 0; i < ARRAY_LEN (regex) && !matched; i++)
        {
            if (regexec (&regex[i], lexer->src + offset, 1, &regmatch, 0) != 0)
            {
                continue;
            }
            type = CTOK_PATTERNS[i].type;
            matched = true;
        }

        // Match to longest operator.
        if (!matched)
        {
            for (size_t i = 0; i < ARRAY_LEN (CTOK_OPERATORS); i++)
            {
                const size_t len = strlen (CTOK_OPERATORS[i].pattern);
                if (strncmp (CTOK_OPERATORS[i].pattern, lexer->src + offset,
                    strlen (CTOK_OPERATORS[i].pattern)) == 0)
                {
                    // Must not match multiple operators of the same length.
                    assert (!matched || len != SIZE_T (regmatch.rm_eo));
                    if (!matched || len > SIZE_T (regmatch.rm_eo) )
                    {
                        regmatch.rm_eo = len;
                        type = CTOK_OPERATORS[i].type;
                        matched = true;
                    }
                }
            }
        }

        if (!matched)
        {
            _lex_error_at (lexer, offset, "failed to tokenize");
            return false;
        }

        // Where the beginning of the regex match starts in the substring.
        // Since we want to tokenize all characters in the string, we should
        // not skip any character.
        assert (regmatch.rm_so == 0);
        const int length = regmatch.rm_eo - regmatch.rm_so;

        const char *file;
        size_t line, col;
        srcmap_lookup (&lexer->srcmap, offset, &file, &line, &col);

        token_t token =
        {
            .type = type,
            .file = file,
            .line = line,
            .col = col,
            .len = length,
            .src = lexer->src + offset,
        };
        _add_token (lexer, &token);

        offset += length;
    }

    return true;
}