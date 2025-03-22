#include "ccompiler/parser.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>

static void ASTNode_free (void *node);
static void ASTNode_print (void *node, int tabs);

static void print_tokerr (struct ParserData *parser, token_t *tok);
static token_t *nxttok (struct ParserData *parser);
static token_t *exp_nxttok (struct ParserData *parser, const char *error_msg);
static token_t *exp_nxttok_is (struct ParserData *parser, enum TokenType type, const char *error_msg);

static token_t *tok (struct ParserData *parser);
static tokentype_t toktype (struct ParserData *parser);
static bool istok (struct ParserData *parser, enum TokenType type);

static astnode_t *allocate_astnode (astnodetype_t type);
static astnode_t *parse_program (struct ParserData *parser);
static astnode_t *parse_function (struct ParserData *parser);
static astnode_t *parse_statement (struct ParserData *parser);
static astnode_t *parse_expression (struct ParserData *parser);

static astnode_t *parse_literal_int (struct ParserData *parser);
static astnode_t *try_parse_literal_int (struct ParserData *parser);
static astnode_t *parse_unary_op (struct ParserData *parser);
static astnode_t *try_parse_unary_op (struct ParserData *parser);
static astnode_t *parse_identifier (struct ParserData *parser);

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
            printf ("identifier<%s>", token_tostr (ast_node->as.identifier.tok_id));
            break;
        case AST_EXPRESSION:
            printf ("expression:\n");
            ASTNode_print (ast_node->as.expression.expr, tabs + 1);
            printf ("\n");
            break;
        case AST_UNARY_OP:
            break;
        case AST_STATEMENT:
            printf ("statement:\n");
            ASTNode_print (ast_node->as.statement.body, tabs + 1);
            break;
        case AST_FUNCTION:
            printf ("function <int> ");
            ASTNode_print (ast_node->as.function.name, 0);
            printf (":\n");
            ASTNode_print (ast_node->as.function.body, tabs + 1);
            break;
        case AST_PROGRAM:
            printf ("program:\n");
            ASTNode_print (ast_node->as.program.function, tabs + 1);
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
        case AST_EXPRESSION:
            ASTNode_free (ast_node->as.expression.expr);
            break;
        case AST_UNARY_OP:
            ASTNode_free (ast_node->as.unary_op.operand);
            break;
        case AST_STATEMENT:
            ASTNode_free (ast_node->as.statement.body);
            break;
        case AST_FUNCTION:
            ASTNode_free (ast_node->as.function.name);
            ASTNode_free (ast_node->as.function.body);
            break;
        case AST_PROGRAM:
            ASTNode_free (ast_node->as.program.function);
            break;
    }

    free (ast_node);
    ast_node = NULL;
}

static void print_tokerr (struct ParserData *parser, token_t *tok)
{
    fprintf (stderr, "ERROR: at tok %d \'%s\' at line %d, column %d\n", parser->tok_i - 1,
             token_tostr (tok), tok->line, tok->column);
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
        print_tokerr (parser, tok);
        fprintf (stderr, error_msg);
        exit (EXIT_FAILURE);
    }
    return tok;
}

static bool istok (struct ParserData *parser, enum TokenType type)
{
    if (parser->tok_i >= parser->lexer->tok_count)
    {
        return false;
    }

    token_t *tok = &parser->lexer->tokens[parser->tok_i];
    return tok->type == type;
}

static token_t *tok (struct ParserData *parser)
{
    if (parser->tok_i >= parser->lexer->tok_count)
    {
        fprintf (stderr, "ERROR: unexpected end of file\n");
        exit (EXIT_FAILURE);
    }

    return &parser->lexer->tokens[parser->tok_i];
}

static tokentype_t toktype (struct ParserData *parser)
{
    if (parser->tok_i >= parser->lexer->tok_count)
    {
        fprintf (stderr, "ERROR: unexpected end of file\n");
        exit (EXIT_FAILURE);
    }

    return parser->lexer->tokens[parser->tok_i].type;
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
    function->as.function.name = parse_identifier (parser);

    exp_nxttok_is (parser, TOKEN_OPEN_PARENTHESIS, "Expected \'(\' after function identifier.\n");
    exp_nxttok_is (parser, TOKEN_CLOSE_PARENTHESIS, "Expected closing \')\' after \'(\'.\n");
    exp_nxttok_is (parser, TOKEN_OPEN_BRACE, "Expected \'{\' to begin function body.\n");
    function->as.function.body = parse_statement (parser);
    exp_nxttok_is (parser, TOKEN_CLOSE_BRACE, "Expected \'}\' to close function body.\n");
    return function;
}

static astnode_t *parse_statement (struct ParserData *parser)
{
    astnode_t *statement = allocate_astnode (AST_STATEMENT);
    exp_nxttok_is (parser, TOKEN_KEYWORD_RETURN, "Expected \'return\' statement.\n");
    statement->as.statement.body = parse_expression (parser);
    exp_nxttok_is (parser, TOKEN_SEMICOLON, "Expected \';\' to end statement.\n");
    return statement;
}

static astnode_t *parse_expression (struct ParserData *parser)
{
    astnode_t *expression = allocate_astnode (AST_EXPRESSION);

    expression->as.expression.expr = try_parse_unary_op (parser);
    if (expression->as.expression.expr) return expression;

    expression->as.expression.expr = parse_literal_int (parser);
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

static astnode_t *try_parse_literal_int (struct ParserData *parser)
{
    if (toktype (parser) == TOKEN_LITERAL_INT)
    {
        return parse_literal_int (parser);
    }
    return NULL;
}

static astnode_t *parse_unary_op (struct ParserData *parser)
{
    astnode_t *unary_op = allocate_astnode (AST_UNARY_OP);
    switch (toktype (parser))
    {
        case TOKEN_HYPEN:
        case TOKEN_EXCLAMATION_MARK:
        case TOKEN_TILDE:
            break;
        default:
            print_tokerr (parser, tok (parser));
            fprintf (stderr, "unexpected token when parsing unary op\n");
            exit (EXIT_FAILURE);
    }

    unary_op->as.unary_op.tok_op = nxttok (parser);
    unary_op->as.unary_op.operand = parse_expression (parser);
    return unary_op;
}

static astnode_t *try_parse_unary_op (struct ParserData *parser)
{
    switch (toktype (parser))
    {
        case TOKEN_HYPEN:
        case TOKEN_EXCLAMATION_MARK:
        case TOKEN_TILDE:
            return parse_unary_op (parser);
        default:
            break;
    }

    return NULL;
}

static astnode_t *parse_identifier (struct ParserData *parser)
{
    astnode_t *identifier = allocate_astnode (AST_IDENTIFIER);
    identifier->as.identifier.tok_id = exp_nxttok_is (parser, TOKEN_IDENTIFIER, "Expected identifier.\n");
    return identifier;
}
