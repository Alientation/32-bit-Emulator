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
    // Float expressed in scientific notation.
    // { TOKEN_F_CONSTANT, "^[0-9]+([Ee][+-]?[0-9]+)(f|F|l|L)?" },
    // Float with decimal point.
    // { TOKEN_F_CONSTANT, "^[0-9]*\\.[0-9]+([Ee][+-]?[0-9]+)?(f|F|l|L)?" },
    //Float with decimal point but no digits after.
    // { TOKEN_F_CONSTANT, "^[0-9]+\\.[Ee][+-]?[0-9]+?(f|F|l|L)?" },

    // Float with leading digits.
    { TOKEN_F_CONSTANT, "^[0-9]+\\.[0-9]*([Ee][+-]?[0-9]+)?[fFlL]?" },

    // Float with leading dot.
    { TOKEN_F_CONSTANT, "^\\.[0-9]+([Ee][+-]?[0-9]+)?[fFlL]?" },

    // Float without dot.
    { TOKEN_F_CONSTANT, "^[0-9]+[Ee][+-]?[0-9]+[fFlL]?" },

    // Float in hex. Not really within scope.
    // {HP}{H}+{P}{FS}?
    // {HP}{H}*"."{H}+{P}{FS}?
    // {HP}{H}+"."{P}{FS}?

    // Decimal.
    { TOKEN_I_CONSTANT, "^[1-9][0-9]*(([uU][lL]{0,2})|([lL]{1,2}[uU]?))?" },
    // Hexadecimal
    { TOKEN_I_CONSTANT, "^0[xX][0-9a-fA-F]+(([uU][lL]{0,2})|([lL]{1,2}[uU]?))?" },
    // Octal
    { TOKEN_I_CONSTANT, "^0[0-7]*(([uU][lL]{0,2})|([lL]{1,2}[uU]?))?" },

    // Character
    { TOKEN_I_CONSTANT, "^[uUL]?'([^'\\\\\n]|(\\\\(['\"?\\\\abfnrtv]|[0-7]{1,3}|x[a-fA-F0-9]+)))'+" },

    // Identifiers can only start with an alphabet or underscore character then
    // followed by a alphanumeric characters.
    { TOKEN_IDENTIFIER, "^[a-zA-Z_][a-zA-Z0-9_]*" },
};

bool lex_file (const char* filepath,
              lexer_data_t *lexer)
{
    _cleanup_free_ char *buffer = NULL;
    _cleanup_fclose_ FILE *file = NULL;
    long file_size = 0;

    if (!(file = fopen (filepath, "rb")))
    {
        fprintf (stderr, "ERROR: failed to find file '%s'\n", filepath);
        return false;
    }

    if (fseek (file, 0, SEEK_END) == -1)
    {
        fprintf (stderr, "ERROR: fseek failed for file '%s'\n", filepath);
        return false;
    }

    if ((file_size = ftell (file)) == -1)
    {
        fprintf (stderr, "ERROR: ftell failed for file '%s'\n", filepath);
        return false;
    }
    rewind (file);

    buffer = (char *) calloc (file_size + 1, sizeof (char));
    if (buffer == NULL)
    {
        fprintf (stderr, "ERROR: failed to allocate memory\n");
        return false;
    }

    if (fread (buffer, 1, file_size, file) != (size_t) file_size)
    {
        fprintf (stderr, "ERROR: failed to read file\n");
        return false;
    }

    lexer->length = strlen (buffer);
    lexer->src = STEAL (buffer);
    return _process_lexer (lexer);
}

bool lex_str (const char *str,
             lexer_data_t *lexer)
{
    lexer->length = strlen (str);
    lexer->src = (char *) calloc (lexer->length + 1, sizeof (char));

    if (!lexer->src)
    {
        fprintf (stderr, "ERROR: failed to allocate memory\n");
        return false;
    }

    memcpy (lexer->src, str, lexer->length);
    return _process_lexer (lexer);
}

void lexer_init (lexer_data_t *lexer)
{
    lexer->src = NULL;
    lexer->toks = NULL;
    lexer->length = 0;

    lexer->tok_cnt = 0;
    lexer->tok_cap = 4;
    lexer->toks = calloc (lexer->tok_cap, sizeof (*lexer->toks));
    if (!lexer->toks)
    {
        fprintf (stderr, "ERROR: failed to allocate memory\n");
        exit (EXIT_FAILURE);
    }
}

void lexer_print (const lexer_data_t *lexer)
{
    printf ("PRINTING TOKENS");
    for (size_t i = 0; i < lexer->tok_cnt; i++)
    {
        printf("\n[%lu] ", i);
        token_print (&lexer->toks[i]);
    }
    printf ("\n");
}

void lexer_free (lexer_data_t *lexer)
{
    free ((void *) lexer->src);
    free (lexer->toks);

    lexer->src = NULL;
    lexer->length = 0;
    lexer->tok_cnt = 0;
    lexer->tok_cap = 0;
    lexer->toks = NULL;
}

char *token_tostr (token_t *tok)
{
    static char strbuffer[256];
    if ((unsigned long) tok->length >= ARRAY_LEN (strbuffer))
    {
        fprintf (stderr, "ERROR: token too long to print, length %zu at line %zu\n", tok->length, tok->line);
        exit (EXIT_FAILURE);
    }

    memcpy (strbuffer, tok->src, tok->length);
    strbuffer[tok->length] = '\0';
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
    if (lexer->tok_cnt + 1 > lexer->tok_cap)
    {
        const int new_cap = lexer->tok_cap * 2 + 10;
        lexer->toks = realloc (lexer->toks, new_cap * sizeof (lexer->toks[0]));
        lexer->tok_cap = new_cap;

        if (lexer->toks == NULL)
        {
            fprintf (stderr, "ERROR: failed to allocate memory\n");
            exit (EXIT_FAILURE);
        }
    }

    lexer->toks[lexer->tok_cnt++] = *tok;
}

static bool _process_lexer (lexer_data_t *lexer)
{
    return _phase_1_2 (lexer) && _phase_3_4 (lexer);
}

// https://en.cppreference.com/w/c/language/translation_phases
static bool _phase_1_2 (lexer_data_t *lexer)
{
    // Note, does not handle trigraphs.
    size_t new_length = 0;
    for (size_t i = 0; i < lexer->length; i++)
    {
        const char cur = lexer->src[i];
        const char nxt = lexer->src[i + 1];
        const char nxtnxt = i + 2 <= lexer->length ? lexer->src[i + 2] : '\0';

        // Handle OS specific linebreaks.
        if (cur == '\r')
        {
            if (nxt == '\n')
            {
                continue;
            }

            lexer->src[new_length++] = '\n';
        }
        else if (cur == '\\' && (nxt == '\n' || nxt == '\r'))
        {
            // Handle linebreak escapes.
            i++;
            if (nxt == '\r' && nxtnxt == '\n')
            {
                i++;
            }
        }
        else
        {
            lexer->src[new_length++] = lexer->src[i];
        }
    }

    lexer->src[new_length] = '\0';
    lexer->length = new_length;
    return true;
}

/* Handle comment if exists otherwise skip. Returns if comment is processed. */
static inline bool _handle_comment (lexer_data_t *lexer, size_t *offset, size_t *cur_line, size_t *cur_column)
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
            (*cur_column)++;
        }

        return true;
    }
    else if (nxt == '*')
    {
        (*offset) += 2;
        (*cur_column) += 2;
        while (*offset < lexer->length)
        {
            if (lexer->src[*offset] == '\n')
            {
                (*cur_line)++;
                (*cur_column) = 1;
            }
            else if (lexer->src[*offset] == '*' && lexer->src[*offset + 1] == '/')
            {
                (*offset) += 2;
                (*cur_column) += 2;
                break;
            }
            else
            {
                (*cur_column)++;
            }
            (*offset)++;
        }

        return true;
    }
    return false;
}

/* Process C string. Returns false if it is not valid. */
static inline bool _handle_c_str (lexer_data_t *lexer, size_t *offset, size_t *cur_line, size_t *cur_column)
{
    assert (lexer->src[*offset] == '\"');

    const size_t start = *offset;
    const size_t start_line = *cur_line;
    const size_t start_column = *cur_column;

    // Skip first quote.
    (*offset)++;
    (*cur_column)++;

    bool closed = false;
    while (*offset <= lexer->length && lexer->src[*offset] != '\n')
    {
        const char c = lexer->src[*offset];

        if (c == '\"')
        {
            // Consume closing quote.
            (*offset)++;
            (*cur_column)++;
            closed = true;
            break;
        }
        else if (c == '\\')
        {
            // Validate and skip the escape sequence.
            (*offset)++;
            (*cur_column)++;
            const char esc = lexer->src[*offset];
            switch (esc)
            {
                // Single character escapes, skip one char.
                case '"': case '\\': case '/':
                case 'a': case 'b':  case 'f':
                case 'n': case 'r':  case 't':
                case 'v':
                    (*offset)++;
                    (*cur_column)++;
                    break;

                // Hex escape \xNN..., skip hex digits.
                case 'x':
                    (*offset)++;
                    (*cur_column)++;
                    if (!isxdigit (lexer->src[*offset]))
                    {
                        fprintf (stderr, "ERROR: invalid hex escape at line %zu col %zu\n",
                                    *cur_line, *cur_column);
                        return false;
                    }
                    while (isxdigit (lexer->src[*offset]))
                    {
                        (*offset)++;
                        (*cur_column)++;
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
                        (*cur_column)++;
                        count++;
                    }
                    break;
                }

                // Unicode \uNNNN.
                case 'u':
                {
                    (*offset)++;
                    (*cur_column)++;
                    for (int i = 0; i < 4; i++)
                    {
                        if (!isxdigit (lexer->src[*offset]))
                        {
                            fprintf (stderr, "ERROR: invalid unicode escape at line %zu col %zu\n",
                                        *cur_line, *cur_column);
                            return false;
                        }
                        (*offset)++;
                        (*cur_column)++;
                    }
                    break;
                }

                default:
                    fprintf (stderr, "ERROR: unknown escape sequence '\\%c' at line %zu col %zu\n",
                                esc, *cur_line, *cur_column);
                    return false;
            }
            continue;
        }

        // Ordinary character.
        (*offset)++;
        (*cur_column)++;
    }

    if (!closed)
    {
        fprintf (stderr, "ERROR: unterminated string literal at line %zu col %zu\n",
                    start_line, start_column);
        return false;
    }

    token_t tok = {
        .type   = TOKEN_STRING_LITERAL,
        .src = lexer->src + start,
        .length = *offset - start,
        .line   = start_line,
        .column = start_column,
    };
    _add_token(lexer, &tok);
    return true;

}

/* Process a preprocessor directive. Assumption is that the '#' at offset is the
   first non-whitespace character in the line. Returns false if invalid preprocessor. */
static bool _handle_preprocessor (lexer_data_t *lexer, size_t *offset, size_t *cur_line, size_t *cur_column)
{
    assert (lexer->src[*offset] == '#');

    UNUSED (lexer);
    UNUSED (offset);
    UNUSED (cur_line);
    UNUSED (cur_column);

    if (strncmp (lexer->src + (*offset), "", strlen ("")) == 0)
    {

    }


    return true;
}

// Tokenize while handling preprocessor directives.
static bool _phase_3_4 (lexer_data_t *lexer)
{
    regex_t regex[ARRAY_LEN (CTOK_PATTERNS)];
    for (size_t i = 0; i < ARRAY_LEN (CTOK_PATTERNS); i++)
    {
        if (regcomp (&regex[i], CTOK_PATTERNS[i].pattern, REG_EXTENDED) != 0)
        {
            M_UNREACHABLE ("Failed to compile regex '%s'\n", CTOK_PATTERNS[i].pattern);
            return false;
        }
    }

    size_t offset = 0;
    size_t cur_line = 1;
    size_t cur_column = 1;

    // Used for checking if preprocessor directive.
    bool first_non_whitespace_char_on_line = true;

    while (offset < lexer->length)
    {
        const char cur = lexer->src[offset];

        if (isspace (cur))
        {
            switch (cur)
            {
                case ' ':
                    cur_column++;
                    break;
                case '\f':
                    break;
                case '\t':
                    cur_column += TAB_SIZE;
                    break;
                case '\v':
                    break;
                case '\n':
                    cur_line++;
                    cur_column = 1;
                    first_non_whitespace_char_on_line = true;
                    break;
                default:
                    UNREACHABLE ();
            }
            offset++;
            continue;
        }
        first_non_whitespace_char_on_line = false;

        if (_handle_comment (lexer, &offset, &cur_line, &cur_column))
        {
            continue;
        }

        // Handle C string.
        if (cur == '\"')
        {
            if (!_handle_c_str (lexer, &offset, &cur_line, &cur_column))
            {
                return false;
            }
            continue;
        }

        // Check if preprocessor directive. '#' must be the first non-whitespace character of the line.
        if (first_non_whitespace_char_on_line && cur == '#')
        {
            if (!_handle_preprocessor (lexer, &offset, &cur_line, &cur_column))
            {
                return false;
            }
            continue;
        }

        bool matched = false;
        regmatch_t regmatch;
        tokentype_t type;

        // Cheap check if we can match prefix to keyword. TODO: Perhaps change to hashtable approach.
        for (size_t i = 0; i < ARRAY_LEN (CTOK_KEYWORDS) && !matched; i++)
        {
            const size_t len = strlen (CTOK_KEYWORDS[i].pattern);
            const char endc = lexer->src[offset + len];
            if (strncmp (CTOK_KEYWORDS[i].pattern, lexer->src + offset,
                strlen (CTOK_KEYWORDS[i].pattern)) == 0 && (endc != '_' && !isalnum (endc)))
            {
                regmatch.rm_eo = len;
                regmatch.rm_so = 0;
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
                        regmatch.rm_so = 0;
                        type = CTOK_OPERATORS[i].type;
                        matched = true;
                    }
                }
            }
        }

        if (!matched)
        {
            fprintf (stderr, "Failed to tokenize \'%.10s\' at line %lu, column %lu\n",
                     lexer->src + offset, cur_line, cur_column);
            return false;
        }

        // Where the beginning of the regex match starts in the substring.
        // Since we want to tokenize all characters in the string, we should
        // not skip any character.
        assert (regmatch.rm_so == 0);
        const int length = regmatch.rm_eo - regmatch.rm_so;

        token_t token =
        {
            .type = type,
            .line = cur_line,
            .column = cur_column,
            .length = length,
            .src = lexer->src + offset,
        };

        _add_token (lexer, &token);

        offset += length;
        cur_column += length;
    }

    for (size_t i = 0; i < ARRAY_LEN (regex); i++)
    {
        regfree (&regex[i]);
    }

    return true;
}