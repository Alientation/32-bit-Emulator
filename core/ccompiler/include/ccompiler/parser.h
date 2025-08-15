#pragma once

#include "ccompiler/lexer.h"
#include "ccompiler/stringbuffer.h"

#include <stdbool.h>

/*
    C Grammar from ANSI C Yacc updated to C11
    https://www.quut.com/c/ANSI-C-grammar-y.html

    todo
    - will have recursion issues, but that is fine since problems will arise with files of around 10k lines
    - could avoid this problem by having statements be an iterative process within the parser instead of having
      a recursive definition. Then the max file would be something like 10k functions/structs def/global or static vars
      - Which can also be done in an iterative process so..
*/

typedef enum ASTNodeType
{
    AST_ERR,

    AST_LITERAL_INT,
    AST_IDENT,

    AST_EXPR,
    AST_UNARY_EXPR,
    AST_BINARY_EXPR_1,
    AST_BINARY_EXPR_2,
    AST_FACTOR,
    AST_STATEMENT,
    AST_FUNC,
    AST_PROG,
} astnodetype_t;

typedef struct ASTNode
{
    astnodetype_t type;

    union
    {
        struct
        {
            token_t *tok_int;
            int value;
        } literal_int;


        struct
        {
            token_t *tok_op;
            struct ASTNode *operand;        /* <factor> */
        } unary_expr;


        struct
        {
            struct ASTNode *operand_a;
            token_t *tok_op;
            struct ASTNode *operand_b;
        } binary_expr_1;


        struct
        {
            struct ASTNode *operand_a;
            token_t *tok_op;
            struct ASTNode *operand_b;
        } binary_expr_2;


        struct
        {
            struct ASTNode *body;
        } factor;


        struct
        {
            struct ASTNode *body;           /* <factor> | <binary_expr_2> */
        } expr;


        struct
        {
            struct ASTNode *body;           /* <expr> */
        } statement;


        struct
        {
            token_t *tok_id;
        } ident;


        struct
        {
            struct ASTNode *name;           /* <ident> */
            struct ASTNode *body;           /* <statement> */
        } func;


        struct
        {
            struct ASTNode *func;           /* <func> */
        } prog;
    } as;

} astnode_t;

typedef struct ParserState
{
    int tok_i;
} parser_state_t;

typedef struct ParserHistory
{
    parser_state_t *checkpoints;
    int length;
    int capacity;
} parser_history_t;

typedef struct ParserData
{
    const lexer_data_t *lexer;
    int tok_i;

    astnode_t *ast;

    bool had_error;
    stringbuffer_t err_msg_buffer;

    parser_history_t history;
} parser_data_t;

void parse (const lexer_data_t *lexer,
           parser_data_t *parser);

void parser_init (parser_data_t *parser);
void parser_free (parser_data_t *parser);

void parser_print (parser_data_t *parser);

const char *parser_astnode_type_to_str (astnodetype_t type);
