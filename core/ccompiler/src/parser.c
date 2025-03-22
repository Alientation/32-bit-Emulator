#include "ccompiler/parser.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

static void ASTNode_free (void *node);
static void ASTNode_print (void *node, int tabs);

static token_t *nxttok (struct ParserData *parser);
static token_t *exp_nxttok (struct ParserData *parser, const char *error_msg);
static token_t *exp_nxttok_is (struct ParserData *parser, enum TokenType type, const char *error_msg);

static bool istok (struct ParserData *parser, enum TokenType type);

static astnode_t *allocate_astnode (astnodetype_t type);
static astnode_t *parse_program (struct ParserData *parser);
static astnode_t *parse_function (struct ParserData *parser);
static astnode_t *parse_statement (struct ParserData *parser);
static astnode_t *parse_expression (struct ParserData *parser);

static astnode_t *parse_literal_int (struct ParserData *parser);
static astnode_t *parse_identifier (struct ParserData *parser);

static char strbuffer[256];

void parse (const struct LexerData *lexer,
           struct ParserData *parser)
{
    parser->lexer = lexer;
    parser->ast = parse_program (parser);
}


void parser_init (struct ParserData *parser)
{
    parser->lexer = NULL;
    parser->tok_i = 0;
    parser->ast = NULL;
}

void parser_free (struct ParserData *parser)
{
    ASTNode_free (parser->ast);
}


void parser_print (struct ParserData *parser)
{
    printf ("PRINTING AST\n");
    ASTNode_print (parser->ast, 0);
}

static void ASTNode_print (void *node, int tabs)
{
    for (int i = 0; i < tabs; i++)
    {
        printf ("\t");
    }

    if (node == NULL)
    {
        printf ("NULL\n");
        return;
    }

    astnode_t *ast_node = node;
    switch (ast_node->type)
    {
        case AST_ERROR:
            printf ("ERROR\n");
            break;
        case AST_LITERAL_INT:
            printf ("int<%d>", ast_node->as.literal_int.value);
            break;
        case AST_IDENTIFIER:
            strncpy (strbuffer, ast_node->as.identifier.tok_id->src, ast_node->as.identifier.tok_id->length);
            strbuffer[ast_node->as.identifier.tok_id->length] = '\0';
            printf ("identifier<%s>", strbuffer);
            break;
        case AST_PROGRAM:
            printf ("program:\n");
            ASTNode_print (ast_node->as.program.function, tabs + 1);
            break;
        case AST_EXPRESSION:
            printf ("expression:\n");
            ASTNode_print (ast_node->as.expression.val, tabs + 1);
            printf ("\n");
            break;
        case AST_STATEMENT:
            printf ("statement:\n");
            ASTNode_print (ast_node->as.statement.expression, tabs + 1);
            break;
        case AST_FUNCTION:
            printf ("function <int> ");
            ASTNode_print (ast_node->as.function.identifier, 0);
            printf (":\n");
            ASTNode_print (ast_node->as.function.statement, tabs + 1);
            break;
    }
}

static void ASTNode_free (void *node)
{
    if (node == NULL)
    {
        return;
    }

    astnode_t *ast_node = node;

    switch (ast_node->type)
    {
        case AST_ERROR:
            fprintf (stderr, "ERROR: encountered AST_ERROR node while freeing\n");
            exit (EXIT_FAILURE);
            break;
        case AST_LITERAL_INT:
            break;
        case AST_IDENTIFIER:
            break;
        case AST_PROGRAM:
            ASTNode_free (ast_node->as.program.function);
            break;
        case AST_EXPRESSION:
            ASTNode_free (ast_node->as.expression.val);
            break;
        case AST_STATEMENT:
            ASTNode_free (ast_node->as.statement.expression);
            break;
        case AST_FUNCTION:
            ASTNode_free (ast_node->as.function.identifier);
            ASTNode_free (ast_node->as.function.statement);
            break;
    }

    free (ast_node);
    ast_node = NULL;
}

static token_t *nxttok (struct ParserData *parser)
{
    if (parser->tok_i < parser->lexer->tok_count)
    {
        return &parser->lexer->tokens[parser->tok_i++];
    }

    fprintf (stderr, "ERROR: unexpected end of file\n");
    exit (EXIT_FAILURE);
    return NULL;
}

static token_t *exp_nxttok (struct ParserData *parser, const char *error_msg)
{
    token_t *tok = nxttok (parser);
    if (!tok)
    {
        fprintf (stderr, error_msg);
        exit (EXIT_FAILURE);
    }
    return tok;
}

static token_t *exp_nxttok_is (struct ParserData *parser, enum TokenType type,
                                           const char *error_msg)
{
    token_t *tok = exp_nxttok (parser, error_msg);
    if (tok && tok->type != type)
    {
        strncpy (strbuffer, tok->src, tok->length);
        strbuffer[tok->length] = '\0';
        fprintf (stderr, "ERROR: at tok %d \'%s\' at line %d, column %d\n", parser->tok_i - 1, strbuffer, tok->line, tok->column);
        fprintf (stderr, error_msg);
        exit (EXIT_FAILURE);
    }
    return tok;
}

static bool istok (struct ParserData *parser, enum TokenType type)
{
    if (parser->tok_i >= parser->lexer->tok_count)
    {
        fprintf (stderr, "ERROR: unexpected end of file\n");
        return false;
    }

    token_t *tok = &parser->lexer->tokens[parser->tok_i];
    return tok->type == type;
}



static astnode_t *allocate_astnode (astnodetype_t type)
{
    astnode_t *node = calloc (1, sizeof (astnode_t));
    if (!node)
    {
        fprintf (stderr, "ERROR: failed to allocate memory\n");
        exit (EXIT_FAILURE);
    }

    node->type = type;
    return node;
}


static astnode_t *parse_program (struct ParserData *parser)
{
    astnode_t *program = allocate_astnode (AST_PROGRAM);
    program->as.program.function = parse_function (parser);
    return program;
}

static astnode_t *parse_function (struct ParserData *parser)
{
    astnode_t *function = allocate_astnode (AST_FUNCTION);
    exp_nxttok_is (parser, TOKEN_KEYWORD_INT, "Expected an \'int\' return type for function.\n");
    function->as.function.identifier = parse_identifier (parser);

    exp_nxttok_is (parser, TOKEN_OPEN_PARENTHESIS, "Expected \'(\' after function identifier.\n");
    exp_nxttok_is (parser, TOKEN_CLOSE_PARENTHESIS, "Expected closing \')\' after \'(\'.\n");
    exp_nxttok_is (parser, TOKEN_OPEN_BRACE, "Expected \'{\' to begin function body.\n");
    function->as.function.statement = parse_statement (parser);
    exp_nxttok_is (parser, TOKEN_CLOSE_BRACE, "Expected \'}\' to close function body.\n");
    return function;
}

static astnode_t *parse_statement (struct ParserData *parser)
{
    astnode_t *statement = allocate_astnode (AST_STATEMENT);
    exp_nxttok_is (parser, TOKEN_KEYWORD_RETURN, "Expected \'return\' statement.\n");
    statement->as.statement.expression = parse_expression (parser);
    exp_nxttok_is (parser, TOKEN_SEMICOLON, "Expected \';\' to end statement.\n");
    return statement;
}

static astnode_t *parse_expression (struct ParserData *parser)
{
    astnode_t *expression = allocate_astnode (AST_EXPRESSION);
    if (istok (parser, TOKEN_LITERAL_INT))
    {
        expression->as.expression.val = parse_literal_int (parser);
    }
    else
    {
        // todo
    }
    return expression;
}

static astnode_t *parse_literal_int (struct ParserData *parser)
{
    astnode_t*literal_int = allocate_astnode (AST_LITERAL_INT);
    literal_int->as.literal_int.tok_int = exp_nxttok_is (parser, TOKEN_LITERAL_INT, "Expected integer literal.\n");

    for (int i = 0; i < literal_int->as.literal_int.tok_int->length; i++)
    {
        literal_int->as.literal_int.value *= 10;
        literal_int->as.literal_int.value += literal_int->as.literal_int.tok_int->src[i] - '0';
    }
    return literal_int;
}

static astnode_t *parse_identifier (struct ParserData *parser)
{
    astnode_t *identifier = allocate_astnode (AST_IDENTIFIER);
    identifier->as.identifier.tok_id = exp_nxttok_is (parser, TOKEN_IDENTIFIER, "Expected identifier.\n");
    return identifier;
}
