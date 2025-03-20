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

static bool istok (struct ParserData *parser, enum TokenType type, const char *error_msg);

static astprogram_t *parse_program (struct ParserData *parser);
static astfunction_t *parse_function (struct ParserData *parser);
static aststatement_t *parse_statement (struct ParserData *parser);
static astexpression_t *parse_expression (struct ParserData *parser);

static astliteralint_t *parse_literal_int (struct ParserData *parser);
static astidentifier_t *parse_identifier (struct ParserData *parser);

static char strbuffer[256];

int parse (const struct LexerData *lexer,
           struct ParserData *parser)
{
    parser->lexer = lexer;
    parser->ast = parse_program (parser);
    if (parser->ast == NULL)
    {
        return 1;
    }

    return 0;
}


int parser_init (struct ParserData *parser)
{
    parser->lexer = NULL;
    parser->tok_i = 0;
    parser->ast = NULL;
    return 0;
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
            printf ("int<%d>", ((astliteralint_t *) ast_node)->value);
            break;
        case AST_IDENTIFIER:
            strncpy (strbuffer, ((astidentifier_t *) ast_node)->tok->src, ((astidentifier_t *) ast_node)->tok->length);
            strbuffer[((astidentifier_t *) ast_node)->tok->length] = '\0';
            printf ("identifier<%s>", strbuffer);
            break;
        case AST_PROGRAM:
            printf ("program:\n");
            ASTNode_print (((astprogram_t *) ast_node)->function, tabs + 1);
            break;
        case AST_EXPRESSION:
            printf ("expression:\n");
            ASTNode_print (((astexpression_t *) ast_node)->literal_int, tabs + 1);
            printf ("\n");
            break;
        case AST_STATEMENT:
            printf ("statement:\n");
            ASTNode_print (((aststatement_t *) ast_node)->expression, tabs + 1);
            break;
        case AST_FUNCTION:
            printf ("function <int> ");
            ASTNode_print (((astfunction_t *) ast_node)->identifier, 0);
            printf (":\n");
            ASTNode_print (((astfunction_t *) ast_node)->statement, tabs + 1);
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
            fprintf (stderr, "Encountered AST_ERROR node while freeing.\n");
            break;
        case AST_LITERAL_INT:
            break;
        case AST_IDENTIFIER:
            break;
        case AST_PROGRAM:
            ASTNode_free (((astprogram_t *) ast_node)->function);
            break;
        case AST_EXPRESSION:
            ASTNode_free (((astexpression_t *) ast_node)->literal_int);
            break;
        case AST_STATEMENT:
            ASTNode_free (((aststatement_t *) ast_node)->expression);
            break;
        case AST_FUNCTION:
            ASTNode_free (((astfunction_t *) ast_node)->identifier);
            ASTNode_free (((astfunction_t *) ast_node)->statement);
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

    fprintf (stderr, "Unexpected End of File.\n");
    return NULL;
}

static token_t *exp_nxttok (struct ParserData *parser, const char *error_msg)
{
    token_t *tok = nxttok (parser);
    if (!tok)
    {
        fprintf (stderr, error_msg);
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
        fprintf (stderr, "Error at tok %d \'%s\' at line %d, column %d.\n", parser->tok_i - 1, strbuffer, tok->line, tok->column);
        fprintf (stderr, error_msg);
        return NULL;
    }
    return tok;
}

static bool istok (struct ParserData *parser, enum TokenType type, const char *error_msg)
{
    if (parser->tok_i >= parser->lexer->tok_count)
    {
        fprintf (stderr, error_msg);
        return false;
    }

    token_t *tok = &parser->lexer->tokens[parser->tok_i];
    return tok->type == type;
}

static astprogram_t *parse_program (struct ParserData *parser)
{
    astprogram_t *program = calloc (1, sizeof (astprogram_t));
    if (!program)
    {
        fprintf (stderr, "Memory allocation failed.\n");
        fail:
        ASTNode_free (program);
        return NULL;
    }

    program->type = AST_PROGRAM;
    program->function = parse_function (parser);

    if (!program->function) goto fail;
    return program;
}

static astfunction_t *parse_function (struct ParserData *parser)
{
    astfunction_t *function = calloc (1, sizeof (astfunction_t));
    if (!function)
    {
        fprintf (stderr, "Memory allocation failed.\n");
        fail:
        ASTNode_free (function);
        return NULL;
    }

    function->type = AST_FUNCTION;

    token_t *return_type = exp_nxttok_is (parser, TOKEN_KEYWORD_INT,
                                          "Expected an \'int\' return type for function.\n");
    if (!return_type) goto fail;

    function->identifier = parse_identifier (parser);
    if (!function->identifier) goto fail;

    token_t *open_parenthesis = exp_nxttok_is (parser, TOKEN_OPEN_PARENTHESIS,
                                               "Expected \'(\' after function identifier.\n");
    if (!open_parenthesis) goto fail;

    token_t *close_parenthesis = exp_nxttok_is (parser, TOKEN_CLOSE_PARENTHESIS,
                                                "Expected closing \')\' after \'(\'.\n");
    if (!close_parenthesis) goto fail;
    token_t *open_brace = exp_nxttok_is (parser, TOKEN_OPEN_BRACE,
                                         "Expected \'{\' to begin function body.\n");
    if (!open_brace) goto fail;
    function->statement = parse_statement (parser);
    if (!function->statement) goto fail;
    token_t *close_brace = exp_nxttok_is (parser, TOKEN_CLOSE_BRACE,
                                          "Expected \'}\' to close function body.\n");
    if (!close_brace) goto fail;

    return function;
}

static aststatement_t *parse_statement (struct ParserData *parser)
{
    aststatement_t *statement = calloc (1, sizeof (aststatement_t));
    if (!statement)
    {
        fprintf (stderr, "Memory allocation failed.\n");
        fail:
        ASTNode_free (statement);
        return NULL;
    }
    statement->type = AST_STATEMENT;

    token_t *keyword_return = exp_nxttok_is (parser, TOKEN_KEYWORD_RETURN,
                                             "Expected \'return\' statement.\n");
    if (!keyword_return) goto fail;

    statement->expression = parse_expression (parser);
    if (!statement->expression) goto fail;

    token_t *semicolon = exp_nxttok_is (parser, TOKEN_SEMICOLON, "Expected \';\' to end statement.\n");
    if (!semicolon) goto fail;

    return statement;
}

static astexpression_t *parse_expression (struct ParserData *parser)
{
    astexpression_t *expression = calloc (1, sizeof (astexpression_t));
    if (!expression)
    {
        fprintf (stderr, "Memory allocation failed.\n");
        fail:
        ASTNode_free (expression);
        return NULL;
    }
    expression->type = AST_EXPRESSION;

    expression->literal_int = parse_literal_int (parser);
    if (!expression->literal_int) goto fail;

    return expression;
}

static astliteralint_t *parse_literal_int (struct ParserData *parser)
{
    astliteralint_t *literal_int = calloc (1, sizeof (astliteralint_t));
    if (!literal_int)
    {
        fprintf (stderr, "Memory allocation failed. \n");
        fail:
        ASTNode_free (literal_int);
        return NULL;
    }

    literal_int->type = AST_LITERAL_INT;
    literal_int->tok = exp_nxttok_is (parser, TOKEN_LITERAL_INT, "Expected integer literal.\n");
    if (!literal_int->tok) goto fail;

    for (int i = 0; i < literal_int->tok->length; i++)
    {
        literal_int->value *= 10;
        literal_int->value += literal_int->tok->src[i] - '0';
    }

    return literal_int;
}

static astidentifier_t *parse_identifier (struct ParserData *parser)
{
    astidentifier_t *identifier = calloc (1, sizeof (astidentifier_t));
    if (!identifier)
    {
        fprintf (stderr, "Memory allocation failed.\n");
        fail:
        ASTNode_free (identifier);
        return NULL;
    }

    identifier->type = AST_IDENTIFIER;
    identifier->tok = exp_nxttok_is (parser, TOKEN_IDENTIFIER, "Expected identifier.\n");
    if (!identifier->tok) goto fail;

    return identifier;
}
