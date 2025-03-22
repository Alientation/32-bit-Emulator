#pragma once
#ifndef PARSER_H

#include "ccompiler/lexer.h"

/*
    <program> ::= <function>
    <function> ::= "int" <identifier> "()" "{" <statement> "}"
    <statement> ::= "return" <expression> ";"
    <expression> ::= <literal_int>

 */
typedef enum ASTNodeType
{
    AST_ERROR,

    AST_LITERAL_INT,
    AST_IDENTIFIER,

    AST_EXPRESSION,
    AST_STATEMENT,
    AST_FUNCTION,
    AST_PROGRAM,
} astnodetype_t;

typedef struct ASTNode
{
    astnodetype_t type;

    union
    {
        /* Literal integer */
        struct
        {
            token_t *tok_int;
            int value;
        } literal_int;

        /* Unary operator */
        struct
        {
            token_t *tok_op;
            struct ASTNode *literal_int;
        } unary_op;

        /* Expression */
        struct
        {
            struct ASTNode *val;
        } expression;

        /* Statement */
        struct
        {
            struct ASTNode *expression;
        } statement;

        /* Identifier */
        struct
        {
            token_t *tok_id;
        } identifier;

        /* Function */
        struct
        {
            struct ASTNode *identifier;
            struct ASTNode *statement;
        } function;

        /* Program */
        struct
        {
            struct ASTNode *function;
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