#pragma once
#ifndef PARSER_H

#include "ccompiler/lexer.h"

/*
    <program> ::= <function>
    <function> ::= "int" <identifier> "()" "{" <statement> "}"
    <statement> ::= "return" <expression>
    <expression> ::= <literal_int>

 */

typedef struct LiteralInt
{
    struct Token *tok;
    int value;
} literalint_t;

typedef struct Identifier
{
    struct Token *tok;
} identifier_t;



typedef enum ASTNodeType
{
    AST_ERROR,

    AST_EXPRESSION,
    AST_STATEMENT,
    AST_FUNCTION,
    AST_PROGRAM,
} astnodetype_t;

typedef struct ASTNode
{
    astnodetype_t type;
} astnode_t;

typedef struct ASTExpression
{
    astnodetype_t type;

    literalint_t literal_int;
} astexpression_t;

typedef struct ASTStatement
{
    astnodetype_t type;

    astexpression_t *expression;
} aststatement_t;

typedef struct ASTFunction
{
    astnodetype_t type;

    identifier_t Identifier;
    aststatement_t *statement;
} astfunction_t;

typedef struct ASTProgram
{
    astnodetype_t type;

    astfunction_t *function;
} astprogram_t;

struct ParserData
{
    struct LexerData lexer;
    int tok_i;

    astprogram_t *ast;
};


int parse (struct LexerData *lexer,
           struct ParserData *parser);

struct ParserData parser_init ();
void parser_free (struct ParserData *parser);

#endif /* PARSER_H */