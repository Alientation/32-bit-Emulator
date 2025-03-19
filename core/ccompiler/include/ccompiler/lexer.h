#pragma once
#ifndef LEXER_H

struct Token;

struct LexerData
{
    const char *src;
    int length;

    struct Token *tokens;
    int tok_count;
    int tok_total_alloc;
};

enum TokenType
{
    TOKEN_ERROR = -1,

    TOKEN_IDENTIFIER,
    TOKEN_LITERAL_INTEGER,

    TOKEN_KEYWORD_INT,
    TOKEN_KEYWORD_RETURN,

    TOKEN_OPEN_PARENTHESIS, TOKEN_CLOSE_PARENTHESIS,
    TOKEN_OPEN_BRACE, TOKEN_CLOSE_BRACE,
    TOKEN_SEMICOLON,
};

struct Token
{
    enum TokenType type;
    const char *tok;
    int length;
    int line;
    int column;
};

int lex (const char* filepath,
        struct LexerData *lexer);

struct LexerData lexer_init ();
void lexer_print (const struct LexerData *lexer);
void lexer_free (struct LexerData *lexer);

void token_print (struct Token tok);

#endif /* LEXER_H */