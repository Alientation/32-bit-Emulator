#include "ccompiler/lexer.h"

#include "ccompiler/massert.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <stdbool.h>
#include <assert.h>


static void add_token (lexer_data_t *lexer, token_t *tok);
static void phase_1 (lexer_data_t *lexer);
static void phase_2 (lexer_data_t *lexer);
static void phase_3 (lexer_data_t *lexer);
static void phase_7 (lexer_data_t *lexer);

static const int TAB_SIZE = 4;

typedef struct Token_Pattern
{
    enum TokenType type;
    const char *pattern;
} tokpat_t;

static const tokpat_t PRETOK_PATTERNS[] =
{

};

static const tokpat_t CTOK_PATTERNS[] =
{
    { TOKEN_KEYWORD_INT, "^int\\b" },
    { TOKEN_KEYWORD_RETURN, "^return\\b" },

    { TOKEN_PLUS, "^\\+" },
    { TOKEN_ASTERICK, "^\\*" },
    { TOKEN_FORWARD_SLASH, "^/" },

    { TOKEN_HYPEN, "^-" },
    { TOKEN_TILDE, "^~" },
    { TOKEN_EXCLAMATION_MARK, "^!" },

    { TOKEN_OPEN_PARENTHESIS, "^\\(" },
    { TOKEN_CLOSE_PARENTHESIS, "^\\)" },
    { TOKEN_OPEN_BRACE, "^\\{" },
    { TOKEN_CLOSE_BRACE, "^\\}" },

    { TOKEN_SEMICOLON, "^;" },

    { TOKEN_IDENTIFIER, "^[a-zA-Z_][a-zA-Z0-9_]*\\b" },
    { TOKEN_LITERAL_INT, "^[0-9]+\\b" },
};

void lex (const char* filepath,
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

    phase_1 (lexer);
    phase_2 (lexer);
    phase_3 (lexer);


    phase_7 (lexer);
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
        case TOKEN_LITERAL_INT:
            printf ("TOKEN_LITERAL_INT: \'%s\'", buffer);
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

// todo
static void phase_3 (lexer_data_t *lexer)
{
    // convert into preprocessing tokens
    const int PATTERN_COUNT = sizeof (PRETOK_PATTERNS) / sizeof (PRETOK_PATTERNS[0]);

    regex_t regex;
    regmatch_t match;
    int offset = 0;

    // need to maintain line and column information to embed into token metadata
    // for phase 7 to retokenize while keeping this information
    int cur_line = 1;
    int cur_column = 1;

    while (lexer->src[offset] != '\0')
    {
        bool matched = false;

        // match to regex pattern
        for (int i = 0; i < PATTERN_COUNT && !matched; i++)
        {
            if (regcomp (&regex, PRETOK_PATTERNS[i].pattern, REG_EXTENDED) != 0)
            {

                M_UNREACHABLE ("Failed to compile regex %s\n", PRETOK_PATTERNS[i].pattern);
            }

            if (regexec (&regex, lexer->src + offset, 1, &match, 0) == 0)
            {
                assert (match.rm_so == 0);

                int length = match.rm_eo - match.rm_so;

                token_t token;
                token.type = PRETOK_PATTERNS[i].type;
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


// todo, needs to operate on the tokens list that phase_3 will generate and preprocessor is ran
// while maintaining line and column indices respective to original source
static void phase_7 (lexer_data_t *lexer)
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