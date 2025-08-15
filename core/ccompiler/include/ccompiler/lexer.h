#pragma once

typedef struct Token token_t;

typedef struct LexerData
{
    char *src;
    int length;

    token_t *toks;
    int tok_cnt;
    int tok_cap;
} lexer_data_t;

typedef enum TokenType
{
    TOKEN_ERROR = -1,

    TOKEN_IDENTIFIER,
    TOKEN_LITERAL_INT,

    TOKEN_KEYWORD_INT,
    TOKEN_KEYWORD_RETURN,

    TOKEN_PLUS, TOKEN_ASTERICK, TOKEN_FORWARD_SLASH,
    TOKEN_HYPEN, TOKEN_TILDE, TOKEN_EXCLAMATION_MARK,

    TOKEN_OPEN_PARENTHESIS, TOKEN_CLOSE_PARENTHESIS,
    TOKEN_OPEN_BRACE, TOKEN_CLOSE_BRACE,
    TOKEN_SEMICOLON,
} tokentype_t;

struct Token
{
    tokentype_t type;

    const char *src;
    int length;

    const char *file;
    int line;
    int column;
};

void lex (const char* filepath,
          lexer_data_t *lexer);

void lexer_init (lexer_data_t *lexer);
void lexer_print (const lexer_data_t *lexer);
void lexer_free (lexer_data_t *lexer);

char *token_tostr (token_t *tok);
void token_print (token_t *tok);
