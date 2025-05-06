#include "ccompiler/lexer.h"

#include "ccompiler/massert.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <stdbool.h>
#include <assert.h>


static void add_token (lexer_data_t *lexer, token_t *tok);
static void tokenize (lexer_data_t *lexer);

static const int TAB_SIZE = 4;

struct Token_Pattern
{
    enum TokenType type;
    const char *pattern;
};

static const struct Token_Pattern TOKEN_PATTERNS[] =
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

    tokenize (lexer);
}

void lexer_print (const lexer_data_t *lexer)
{
    assert(lexer);

    printf ("PRINTING TOKENS");
    for (int i = 0; i < lexer->tok_count; i++)
    {
        printf("\n[%d] ", i);
        token_print (&lexer->tokens[i]);
    }
    printf ("\n");
}

void lexer_free (lexer_data_t *lexer)
{
    assert(lexer);

    free ((void *) lexer->src);
    free (lexer->tokens);

    lexer->src = NULL;
    lexer->length = 0;
    lexer->tok_count = 0;
    lexer->tok_total_alloc = 0;
    lexer->tokens = NULL;
}


void lexer_init (lexer_data_t *lexer)
{
    assert (lexer);

    lexer->src = NULL;
    lexer->length = 0;
    lexer->tok_count = 0;
    lexer->tok_total_alloc = 4;
    lexer->tokens = calloc (lexer->tok_total_alloc, sizeof (token_t));
    if (!lexer->tokens)
    {
        fprintf (stderr, "ERROR: failed to allocate memory\n");
        exit (EXIT_FAILURE);
    }
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

static void tokenize (lexer_data_t *lexer)
{
    const int PATTERN_COUNT = sizeof (TOKEN_PATTERNS) / sizeof (TOKEN_PATTERNS[0]);

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
            if (regcomp (&regex, TOKEN_PATTERNS[i].pattern, REG_EXTENDED) != 0)
            {

                M_UNREACHABLE ("Failed to compile regex %s\n", TOKEN_PATTERNS[i].pattern);
            }

            if (regexec (&regex, lexer->src + offset, 1, &match, 0) == 0)
            {
                assert (match.rm_so == 0);

                int length = match.rm_eo - match.rm_so;

                token_t token;
                token.type = TOKEN_PATTERNS[i].type;
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

static void add_token (lexer_data_t *lexer, token_t *tok)
{
    if (lexer->tok_count + 1 > lexer->tok_total_alloc)
    {
        token_t *prev_tokens = lexer->tokens;
        lexer->tok_total_alloc += lexer->tok_total_alloc + 10;
        lexer->tokens = calloc (lexer->tok_total_alloc, sizeof (token_t));

        if (lexer->tokens == NULL)
        {
            fprintf (stderr, "ERROR: failed to allocate memory\n");
            exit (EXIT_FAILURE);
        }

        memcpy (lexer->tokens, prev_tokens, lexer->tok_count * sizeof (token_t));
        free (prev_tokens);
        prev_tokens = NULL;
    }

    lexer->tokens[lexer->tok_count] = *tok;
    lexer->tok_count++;
}