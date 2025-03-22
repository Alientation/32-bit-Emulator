#pragma once
#ifndef PARSER_H

#include "ccompiler/lexer.h"

typedef enum ASTNodeType
{
    AST_ERROR,

    AST_LITERAL_INT,
    AST_IDENTIFIER,

    AST_EXPRESSION,
    AST_UNARY_OP,
    AST_STATEMENT,
    AST_FUNCTION,
    AST_PROGRAM,
} astnodetype_t;

typedef struct ASTNode
{
    astnodetype_t type;

    union
    {
        /* <literal_int> */
        struct
        {
            token_t *tok_int;
            int value;
        } literal_int;


        /* <unary_op> ::= ("!" | "-" | "~") <expression> */
        struct
        {
            token_t *tok_op;
            struct ASTNode *operand;        /* <expression> */
        } unary_op;


        /* <expression> ::= <unary_op> | <literal_int> */
        struct
        {
            struct ASTNode *expr;           /* <unary_op> | <literal_int> */
        } expression;


        /* <statement> ::= "return" <expression> */
        struct
        {
            struct ASTNode *body;           /* <expression> */
        } statement;


        /* <identifier> */
        struct
        {
            token_t *tok_id;
        } identifier;


        /* <function> ::= "int" <identifier> "()" "{" <statement> "}" */
        struct
        {
            struct ASTNode *name;           /* <identifier> */
            struct ASTNode *body;           /* <statement> */
        } function;


        /* <program> ::= <function> */
        struct
        {
            struct ASTNode *function;       /* <function> */
        } program;
    } as;

} astnode_t;

struct ParserData
{
    const struct LexerData *lexer;
    int tok_i;

    astnode_t *ast;
};

void  parse (const struct LexerData *lexer,
           struct ParserData *parser);

void parser_init (struct ParserData *parser);
void parser_free (struct ParserData *parser);

void parser_print (struct ParserData *parser);

#endif /* PARSER_H */