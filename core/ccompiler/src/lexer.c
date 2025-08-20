#include "ccompiler/lexer.h"

#include "ccompiler/massert.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <stdbool.h>
#include <assert.h>


static void add_token (lexer_data_t *lexer, token_t *tok);

static void process_lexer (lexer_data_t *lexer);
static void phase_1 (lexer_data_t *lexer);
static void phase_2 (lexer_data_t *lexer);
static void phase_3 (lexer_data_t *lexer);

static const int TAB_SIZE = 4;

typedef struct Token_Pattern
{
    enum TokenType type;
    const char *pattern;
} tokpat_t;

static const tokpat_t CTOK_PATTERNS[] =
{
    { TOKEN_KEYWORD_AUTO, "^auto\\b" },
    { TOKEN_KEYWORD_BREAK, "^break\\b" },
    { TOKEN_KEYWORD_CASE, "^case\\b" },
    { TOKEN_KEYWORD_CHAR, "^char\\b" },
    { TOKEN_KEYWORD_CONST, "^const\\b" },
    { TOKEN_KEYWORD_CONTINUE, "^continue\\b" },
    { TOKEN_KEYWORD_DEFAULT, "^default\\b" },
    { TOKEN_KEYWORD_DO, "^do\\b" },
    { TOKEN_KEYWORD_DOUBLE, "^double\\b" },
    { TOKEN_KEYWORD_ELSE, "^else\\b" },
    { TOKEN_KEYWORD_ENUM, "^enum\\b" },
    { TOKEN_KEYWORD_EXTERN, "^extern\\b" },
    { TOKEN_KEYWORD_FLOAT, "^float\\b" },
    { TOKEN_KEYWORD_FOR, "^for\\b" },
    { TOKEN_KEYWORD_GOTO, "^goto\\b" },
    { TOKEN_KEYWORD_IF, "^if\\b" },
    { TOKEN_KEYWORD_INLINE, "^inline\\b" },
    { TOKEN_KEYWORD_INT, "^int\\b" },
    { TOKEN_KEYWORD_LONG, "^long\\b" },
    { TOKEN_KEYWORD_REGISTER, "^register\\b" },
    { TOKEN_KEYWORD_RESTRICT, "^restrict\\b" },
    { TOKEN_KEYWORD_RETURN, "^return\\b" },
    { TOKEN_KEYWORD_SHORT, "^short\\b" },
    { TOKEN_KEYWORD_SIGNED, "^signed\\b" },
    { TOKEN_KEYWORD_SIZEOF, "^sizeof\\b" },
    { TOKEN_KEYWORD_STATIC, "^static\\b" },
    { TOKEN_KEYWORD_STRUCT, "^struct\\b" },
    { TOKEN_KEYWORD_SWITCH, "^switch\\b" },
    { TOKEN_KEYWORD_TYPEDEF, "^typedef\\b" },
    { TOKEN_KEYWORD_UNION, "^union\\b" },
    { TOKEN_KEYWORD_UNSIGNED, "^unsigned\\b" },
    { TOKEN_KEYWORD_VOID, "^void\\b" },
    { TOKEN_KEYWORD_VOLATILE, "^volatile\\b" },
    { TOKEN_KEYWORD_WHILE, "^while\\b" },
    { TOKEN_KEYWORD_ALIGNAS, "^_Alignas\\b" },
    { TOKEN_KEYWORD_ALIGNOF, "^_Alignof\\b" },
    { TOKEN_KEYWORD_ATOMIC, "^_Atomic\\b" },
    { TOKEN_KEYWORD_BOOL, "^_Bool\\b" },
    { TOKEN_KEYWORD_COMPLEX, "^_Complex\\b" },
    { TOKEN_KEYWORD_GENERIC, "^_Generic\\b" },
    { TOKEN_KEYWORD_IMAGINARY, "^_Imaginary\\b" },
    { TOKEN_KEYWORD_NORETURN, "^_Noreturn\\b" },
    { TOKEN_KEYWORD_STATIC_ASSERT, "^_Static_assert\\b" },
    { TOKEN_KEYWORD_THREAD_LOCAL, "^_Thread_local\\b" },
    { TOKEN_KEYWORD_FUNC_NAME, "^__func__\\b" },

    // Identifiers can only start with an alphabet or underscore character then
    // followed by a alphanumeric characters.
    // TODO U/UL/ULL suffixes
    { TOKEN_IDENTIFIER, "^[a-zA-Z_][a-zA-Z0-9_]*\\b" },

    // Float expressed in scientific notation.
    { TOKEN_F_CONSTANT, "^[0-9]+([Ee][+-]?[0-9]+)(f|F|l|L)?\\b" },
    // Float with decimal point.
    { TOKEN_F_CONSTANT, "^[0-9]*\\.[0-9]+([Ee][+-]?[0-9]+)?(f|F|l|L)?\\b" },
    //Float with decimal point but no digits after.
    { TOKEN_F_CONSTANT, "^[0-9]+\\.[Ee][+-]?[0-9]+?(f|F|l|L)?\\b" },

    // Float in hex. Not really within scope.
    // {HP}{H}+{P}{FS}?
    // {HP}{H}*"."{H}+{P}{FS}?
    // {HP}{H}+"."{P}{FS}?

    // Decimal.
    { TOKEN_I_CONSTANT, "^[1-9][0-9]*\\b" },
    // Hexadecimal
    { TOKEN_I_CONSTANT, "^0[xX][0-9a-fA-F]+\\b" },
    // Octal
    { TOKEN_I_CONSTANT, "^0[0-7]*\\b" },

    // TODO what is this
    // {CP}?"'"([^'\\\n]|{ES})+"'"		{ return I_CONSTANT; }
    // Something to do with strings..

    // Strings (Even the concat)
    { TOKEN_STRING_LITERAL, "(\"([^\"\\\n])*\"[ \t\v\n\f]*)+" },

    { TOKEN_ELLIPSIS, "^\\.\\.\\." },
    { TOKEN_RIGHT_ASSIGN, "^>>=" },
    { TOKEN_LEFT_ASSIGN, "^<<=" },
    { TOKEN_ADD_ASSIGN, "^\\+=" },
    { TOKEN_SUB_ASSIGN, "^-=" },
    { TOKEN_MUL_ASSIGN, "^\\*=" },
    { TOKEN_DIV_ASSIGN, "^/=" },
    { TOKEN_MOD_ASSIGN, "^%=" },
    { TOKEN_AND_ASSIGN, "^&=" },
    { TOKEN_XOR_ASSIGN, "^\\^=" },
    { TOKEN_OR_ASSIGN, "^\\|=" },
    { TOKEN_RIGHT_OP, "^>>" },
    { TOKEN_LEFT_OP, "^<<" },
    { TOKEN_INC_OP, "^\\+\\+" },
    { TOKEN_DEC_OP, "^--" },
    { TOKEN_PTR_OP, "^->" },
    { TOKEN_AND_OP, "^&&" },
    { TOKEN_OR_OP, "^\\|\\|" },
    { TOKEN_LE_OP, "^<=" },
    { TOKEN_GE_OP, "^>=" },
    { TOKEN_EQ_OP, "^==" },
    { TOKEN_NE_OP, "^!=" },

    { TOKEN_SEMICOLON, "^;" },
    { TOKEN_OPEN_BRACE, "^\\{" },
    { TOKEN_CLOSE_BRACE, "^\\}" },
    { TOKEN_COMMA, "^," },
    { TOKEN_COLON, "^:" },
    { TOKEN_EQUAL_SIGN, "^=" },
    { TOKEN_OPEN_PARENTHESIS, "^\\(" },
    { TOKEN_CLOSE_PARENTHESIS, "^\\)" },
    { TOKEN_OPEN_BRACKET, "^\\[" },
    { TOKEN_CLOSE_BRACKET, "^\\]" },
    { TOKEN_PERIOD, "^\\." },
    { TOKEN_AMPERSAND, "^&" },
    { TOKEN_EXCLAMATION_MARK, "^!" },
    { TOKEN_TILDE, "^~" },
    { TOKEN_HYPEN, "^-" },
    { TOKEN_PLUS, "^\\+" },
    { TOKEN_ASTERICK, "^\\*" },
    { TOKEN_FORWARD_SLASH, "^/" },
    { TOKEN_PERCENT_SIGN, "^%" },
    { TOKEN_LEFT_ARROW, "^<" },
    { TOKEN_RIGHT_ARROW, "^>" },
    { TOKEN_CARROT, "^\\^" },
    { TOKEN_PIPE, "^\\|" },
    { TOKEN_QUESTION_MARK, "^\\?" },
};

void lex_file (const char* filepath,
               lexer_data_t *lexer)
{
    FILE *file;
    char *buffer;
    long file_size;

    file = fopen (filepath, "rb");
    fseek (file, 0, SEEK_END);
    file_size = ftell (file);
    rewind (file);

    buffer = (char *) calloc (file_size + 1, sizeof (char));
    if (buffer == NULL)
    {
        fprintf (stderr, "ERROR: failed to allocate memory\n");
        fclose (file);
        exit (EXIT_FAILURE);
    }

    if (fread (buffer, 1, file_size, file) != (size_t) file_size)
    {
        fprintf (stderr, "ERROR: failed to read file\n");
        free (buffer);
        fclose (file);
        exit (EXIT_FAILURE);
    }

    lexer->src = buffer;
    lexer->length = strlen (buffer);

    process_lexer (lexer);
}

void lex_str (const char *str,
              lexer_data_t *lexer)
{
    lexer->length = strlen (str);
    lexer->src = (char *) calloc (lexer->length + 1, sizeof (char));
    memcpy (lexer->src, str, lexer->length);

    process_lexer (lexer);
}

void lexer_init (lexer_data_t *lexer)
{
    assert (lexer);

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
    assert(lexer);

    printf ("PRINTING TOKENS");
    for (int i = 0; i < lexer->tok_cnt; i++)
    {
        printf("\n[%d] ", i);
        token_print (&lexer->toks[i]);
    }
    printf ("\n");
}

void lexer_free (lexer_data_t *lexer)
{
    assert(lexer);

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
    if ((long long unsigned int) tok->length >= sizeof (strbuffer))
    {
        fprintf (stderr, "ERROR: failed to print token at \'%s\' of length %d\n", tok->src, tok->length);
        exit (EXIT_FAILURE);
    }

    strncpy (strbuffer, tok->src, tok->length);
    strbuffer[tok->length] = '\0';
    return strbuffer;
}

void token_print (token_t *tok)
{
    char *buffer = token_tostr (tok);

    strncpy (buffer, tok->src, tok->length);
    switch (tok->type)
    {
        case TOKEN_ERROR:
            printf ("TOKEN_ERROR: \'%s\'", buffer);
            break;
        case TOKEN_IDENTIFIER:
            printf ("TOKEN_IDENTIFIER: \'%s\'", buffer);
            break;
        case TOKEN_I_CONSTANT:
            printf ("TOKEN_I_CONSTANT: \'%s\'", buffer);
            break;
        case TOKEN_KEYWORD_INT:
            printf ("TOKEN_KEYWORD_INT: \'%s\'", buffer);
            break;
        case TOKEN_KEYWORD_RETURN:
            printf ("TOKEN_KEYWORD_RETURN: \'%s\'", buffer);
            break;
        case TOKEN_HYPEN:
            printf ("TOKEN_HYPEN: \'%s\'", buffer);
            break;
        case TOKEN_TILDE:
            printf ("TOKEN_TILDE: \'%s\'", buffer);
            break;
        case TOKEN_EXCLAMATION_MARK:
            printf ("TOKEN_EXCLAMATION_MARK: \'%s\'", buffer);
            break;
        case TOKEN_OPEN_PARENTHESIS:
            printf ("TOKEN_OPEN_PARENTHESIS: \'%s\'", buffer);
            break;
        case TOKEN_CLOSE_PARENTHESIS:
            printf ("TOKEN_CLOSE_PARENTHESIS: \'%s\'", buffer);
            break;
        case TOKEN_OPEN_BRACE:
            printf ("TOKEN_OPEN_BRACE: \'%s\'", buffer);
            break;
        case TOKEN_CLOSE_BRACE:
            printf ("TOKEN_CLOSE_BRACE: \'%s\'", buffer);
            break;
        case TOKEN_SEMICOLON:
            printf ("TOKEN_SEMICOLON: \'%s\'", buffer);
            break;
        default:
            M_UNREACHABLE ("\nUnknown token \'%s\'", buffer);
            break;
    }
}

static void add_token (lexer_data_t *lexer, token_t *tok)
{
    if (lexer->tok_cnt + 1 > lexer->tok_cap)
    {
        token_t *prev_toks = lexer->toks;
        lexer->tok_cap += lexer->tok_cap + 10;
        lexer->toks = calloc (lexer->tok_cap, sizeof (*lexer->toks));

        if (lexer->toks == NULL)
        {
            fprintf (stderr, "ERROR: failed to allocate memory\n");
            exit (EXIT_FAILURE);
        }

        memcpy (lexer->toks, prev_toks, lexer->tok_cnt * sizeof (*lexer->toks));
        free (prev_toks);
        prev_toks = NULL;
    }

    lexer->toks[lexer->tok_cnt] = *tok;
    lexer->tok_cnt++;
}

static void process_lexer (lexer_data_t *lexer)
{
    phase_1 (lexer);
    phase_2 (lexer);
    phase_3 (lexer);
}

// https://en.cppreference.com/w/c/language/translation_phases
static void phase_1 (lexer_data_t *lexer)
{
    // will not handle trigraphs
    // handle OS dependent end line characters
    int new_length = 0;
    for (int i = 0; i < lexer->length; i++)
    {
        if (lexer->src[i] == '\r')
        {
            if (lexer->src[i + 1] == '\n')
            {
                continue;
            }

            lexer->src[new_length++] = '\n';
        }
        else
        {
            lexer->src[new_length++] = lexer->src[i];
        }
    }

    lexer->src[new_length] = '\0';
    lexer->length = new_length;
}

static void phase_2 (lexer_data_t *lexer)
{
    // handle '\' to join two lines
    int new_length = 0;
    for (int i = 0; i < lexer->length; i++)
    {
        if (lexer->src[i] == '\\' && lexer->src[i + 1] == '\n')
        {
            i++;
        }
        else
        {
            lexer->src[new_length++] = lexer->src[i];
        }
    }

    lexer->src[new_length] = '\0';
    lexer->length = new_length;
}

// Tokenize (including preprocessor tokens)
static void phase_3 (lexer_data_t *lexer)
{
    const int PATTERN_COUNT = sizeof (CTOK_PATTERNS) / sizeof (CTOK_PATTERNS[0]);

    regex_t regex;
    regmatch_t match;
    int offset = 0;

    int cur_line = 1;
    int cur_column = 1;

    while (lexer->src[offset] != '\0')
    {
        bool matched = false;

        // match to regex pattern
        for (int i = 0; i < PATTERN_COUNT && !matched; i++)
        {
            if (regcomp (&regex, CTOK_PATTERNS[i].pattern, REG_EXTENDED) != 0)
            {

                M_UNREACHABLE ("Failed to compile regex %s\n", CTOK_PATTERNS[i].pattern);
            }

            if (regexec (&regex, lexer->src + offset, 1, &match, 0) == 0)
            {
                // Where the beginning of the regex match starts in the substring.
                // Since we want to tokenize all characters in the string, we should
                // not skip any character.
                assert (match.rm_so == 0);

                int length = match.rm_eo - match.rm_so;

                token_t token;
                token.type = CTOK_PATTERNS[i].type;
                token.line = cur_line;
                token.column = cur_column;
                token.length = length;
                token.src = lexer->src + offset;

                add_token (lexer, &token);

                offset += length;
                cur_column += length;
                matched = true;
            }

            regfree (&regex);
        }

        if (matched)
        {
            continue;
        }


        // check comments
        if (lexer->src[offset] == '/' && offset + 1 < lexer->length)
        {
            if (lexer->src[offset + 1] == '/')
            {
                // single line comment
                offset += 2;
                while (offset < lexer->length)
                {
                    if (lexer->src[offset] == '\n')
                    {
                        offset++;
                        break;
                    }
                    offset++;
                }
                continue;
            }
            else if (lexer->src[offset + 1] == '*')
            {
                // multi-line comment
                offset += 2;
                while (offset + 1 < lexer->length)
                {
                    if (lexer->src[offset] == '*' && lexer->src[offset + 1] == '/')
                    {
                        offset += 2;
                        break;
                    }
                    offset++;
                }
                continue;
            }
        }

        // unmatched, skip if whitespace
        // otherwise, could not tokenize
        switch (lexer->src[offset])
        {
            case '\r':
                offset++;
                break;
            case ' ':
                offset++;
                cur_column++;
                break;
            case '\t':
                offset++;
                cur_column += TAB_SIZE;
                break;
            case '\n':
                offset++;
                cur_column = 1;
                cur_line++;
                break;
            default:
                fprintf (stderr, "ERROR: could not match regex at line %d and column %d.\n>>\n%s\n",
                        cur_line, cur_column, lexer->src + offset);
                exit (EXIT_FAILURE);
        }
    }
}