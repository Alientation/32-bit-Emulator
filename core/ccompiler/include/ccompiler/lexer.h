#pragma once
#ifndef LEXER_H

typedef struct Token token_t;

struct LexerData
{
    const char *src;
    int length;

    token_t *tokens;
    int tok_count;
    int tok_total_alloc;
};

typedef enum TokenType
{
    TOKEN_ERROR = -1,

    TOKEN_IDENTIFIER,
    TOKEN_LITERAL_INT,

    TOKEN_KEYWORD_INT,
    TOKEN_KEYWORD_RETURN,

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
    int line;
    int column;
};

void lex (const char* filepath,
        struct LexerData *lexer);

void lexer_init (struct LexerData *lexer);
void lexer_print (const struct LexerData *lexer);
void lexer_free (struct LexerData *lexer);

char *token_tostr (token_t *tok);
void token_print (token_t *tok);

#endif /* LEXER_H */