#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <regex.h>
#include <stdbool.h>
#include <assert.h>

#include "ccompiler/lexer.h"

static int add_token (struct LexerData *lexer, token_t tok);
static int tokenize (struct LexerData *lexer);

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

    { TOKEN_OPEN_PARENTHESIS, "^\\(" },
    { TOKEN_CLOSE_PARENTHESIS, "^\\)" },
    { TOKEN_OPEN_BRACE, "^\\{" },
    { TOKEN_CLOSE_BRACE, "^\\}" },

    { TOKEN_SEMICOLON, "^;" },

    { TOKEN_IDENTIFIER, "^[a-zA-Z_][a-zA-Z0-9_]*\\b" },
    { TOKEN_LITERAL_INT, "^[1-9][0-9]*\\b" },
};

int lex (const char* filepath,
        struct LexerData *lexer)
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
        fprintf (stderr, "Memory allocation failed\n");
        fclose (file);
        return 1;
    }

    if (fread (buffer, 1, file_size, file) != (size_t) file_size)
    {
        fprintf (stderr, "Error reading file\n");
        free (buffer);
        fclose (file);
        return 1;
    }

    lexer->src = buffer;
    lexer->length = strlen (buffer);

    tokenize (lexer);
    return 0;
}

void lexer_print (const struct LexerData *lexer)
{
    printf ("PRINTING TOKENS");
    for (int i = 0; i < lexer->tok_count; i++)
    {
        printf("\n[%d] ", i);
        token_print (lexer->tokens[i]);
    }
    printf ("\n");
}

void lexer_free (struct LexerData *lexer)
{
    free ((void *) lexer->src);
    free (lexer->tokens);

    lexer->src = NULL;
    lexer->length = 0;
    lexer->tok_count = 0;
    lexer->tok_total_alloc = 0;
    lexer->tokens = NULL;
}


struct LexerData lexer_init ()
{
    struct LexerData lexer;
    lexer.src = NULL;
    lexer.length = 0;
    lexer.tok_count = 0;
    lexer.tok_total_alloc = 4;
    lexer.tokens = calloc (lexer.tok_total_alloc, sizeof (token_t));
    return lexer;
}

void token_print (token_t tok)
{
    char *buffer = calloc (tok.length + 1, sizeof (char));
    strncpy (buffer, tok.src, tok.length);
    switch (tok.type)
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
            printf ("UNKNOWN: \'%s\'", buffer);
            break;
    }
}

static int tokenize (struct LexerData *lexer)
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

        for (int i = 0; i < PATTERN_COUNT && !matched; i++)
        {
            if (regcomp (&regex, TOKEN_PATTERNS[i].pattern, REG_EXTENDED) != 0)
            {
                fprintf (stderr, "Error compiling regex %s\n", TOKEN_PATTERNS[i].pattern);
                return 1;
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

                add_token (lexer, token);

                offset += length;
                cur_column += length;
                matched = true;
            }

            regfree (&regex);
        }

        if (!matched)
        {
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
                    fprintf (stderr, "Could not match regex at line %d and column %d.\n>>%s\n",
                            cur_line, cur_column, lexer->src + offset);
                    return 1;
            }
        }
    }

    return 0;
}

static int add_token (struct LexerData *lexer, token_t tok)
{
    if (lexer->tok_count + 1 > lexer->tok_total_alloc)
    {
        token_t *prev_tokens = lexer->tokens;
        int prev_alloc = lexer->tok_total_alloc;
        lexer->tok_total_alloc *= 2;
        lexer->tokens = calloc (lexer->tok_total_alloc, sizeof (token_t));

        if (lexer->tokens == NULL)
        {
            fprintf (stderr, "Memory allocation failed\n");
            return 1;
        }

        memcpy (lexer->tokens, prev_tokens, prev_alloc * sizeof (token_t));
        free (prev_tokens);
    }

    lexer->tokens[lexer->tok_count] = tok;
    lexer->tok_count++;
    return 0;
}