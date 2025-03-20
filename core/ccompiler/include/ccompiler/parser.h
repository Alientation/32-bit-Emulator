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


typedef struct ASTLiteralInt
{
    astnodetype_t type;

    token_t *tok;
    int value;
} astliteralint_t;

typedef struct ASTExpression
{
    astnodetype_t type;

    astliteralint_t *literal_int;
} astexpression_t;

typedef struct ASTStatement
{
    astnodetype_t type;

    astexpression_t *expression;
} aststatement_t;

typedef struct ASTIdentifier
{
    astnodetype_t type;

    token_t *tok;
} astidentifier_t;

typedef struct ASTFunction
{
    astnodetype_t type;

    astidentifier_t *identifier;
    aststatement_t *statement;
} astfunction_t;

typedef struct ASTProgram
{
    astnodetype_t type;

    astfunction_t *function;
} astprogram_t;

typedef struct ASTNode
{
    astnodetype_t type;
} astnode_t;

struct ParserData
{
    const struct LexerData *lexer;
    int tok_i;

    astprogram_t *ast;
};


int parse (const struct LexerData *lexer,
           struct ParserData *parser);

struct ParserData parser_init ();
void parser_free (struct ParserData *parser);

void parser_print (struct ParserData *parser);

#endif /* PARSER_H */