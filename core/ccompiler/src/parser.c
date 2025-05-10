#include "ccompiler/parser.h"

#include "ccompiler/massert.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <stdbool.h>
#include <stdarg.h>

static void ASTNode_free (void *node);
static void ASTNode_print (void *node, int tabs);

static void tokerr (parser_data_t *parser, token_t *tok);
static token_t *nxttok (parser_data_t *parser);
static token_t *exp_nxttok_is (parser_data_t *parser, tokentype_t type, const char *error_msg);

static token_t *tok (parser_data_t *parser);
static tokentype_t toktype (parser_data_t *parser);
static bool istok (parser_data_t *parser, tokentype_t type);

static void err (parser_data_t *parser, const char * fmt, ...);

static void parser_commit (parser_data_t *parser);
static void parser_checkpoint (parser_data_t *parser);
static void parser_rollback (parser_data_t *parser);

static astnode_t *allocate_astnode (astnodetype_t type);

static astnode_t *parse_prog (parser_data_t *parser);
static astnode_t *parse_func (parser_data_t *parser);
static astnode_t *parse_statement (parser_data_t *parser);
static astnode_t *parse_expr (parser_data_t *parser);

static bool to_int (parser_data_t *parser, token_t *tok, int *out);
static astnode_t *parse_literal_int (parser_data_t *parser);
static token_t *get_unary_op (parser_data_t *parser);
static astnode_t *parse_unary_expr (parser_data_t *parser);
static astnode_t *parse_binary_expr_1 (parser_data_t *parser);
static token_t *get_binary_op_1 (parser_data_t *parser);
static astnode_t *parse_binary_expr_2 (parser_data_t *parser);
static token_t *get_binary_op_2 (parser_data_t *parser);
static astnode_t *parse_factor (parser_data_t *parser);
static astnode_t *parse_ident (parser_data_t *parser);

void parse (const lexer_data_t *lexer,
           parser_data_t *parser)
{
    parser->lexer = lexer;
    parser->ast = parse_prog (parser);

    if (parser->history.length != 0)
    {
        fprintf (stderr, "ERROR: parser is malformed, expected checkpoint history to be empty but has %d elements\n",
                parser->history.length);
        exit (EXIT_FAILURE);
    }
}


void parser_init (parser_data_t *parser)
{
    parser->lexer = NULL;
    parser->tok_i = 0;
    parser->ast = NULL;
    parser->history.checkpoints = NULL;
    parser->history.length = 0;
    parser->history.capacity = 0;

    stringbuffer_init (&parser->err_msg_buffer);
}

void parser_free (parser_data_t *parser)
{
    ASTNode_free (parser->ast);
    stringbuffer_free (&parser->err_msg_buffer);
}


void parser_print (parser_data_t *parser)
{
    printf ("PRINTING AST\n");
    ASTNode_print (parser->ast, 0);
}

const char *parser_astnode_type_to_str (astnodetype_t type)
{
    switch (type)
    {
        case AST_ERR:
            return "AST_ERR";
        case AST_LITERAL_INT:
            return "AST_LITERAL_INT";
        case AST_IDENT:
            return "AST_IDENT";
        case AST_EXPR:
            return "AST_EXPR";
        case AST_UNARY_EXPR:
            return "AST_UNARY_EXPR";
        case AST_BINARY_EXPR_1:
            return "AST_BINARY_EXPR_1";
        case AST_BINARY_EXPR_2:
            return "AST_BINARY_EXPR_2";
        case AST_FACTOR:
            return "AST_FACTOR";
        case AST_STATEMENT:
            return "AST_STATEMENT";
        case AST_FUNC:
            return "AST_FUNC";
        case AST_PROG:
            return "AST_PROG";
        default:
            UNREACHABLE ();
    }
    return "ERR";
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
        case AST_ERR:
            printf ("ERROR\n");
            break;
        case AST_LITERAL_INT:
            printf ("int<%d>", ast_node->as.literal_int.value);
            break;
        case AST_IDENT:
            printf ("ident<%s>", token_tostr (ast_node->as.ident.tok_id));
            break;
        case AST_EXPR:
            printf ("expr:\n");
            ASTNode_print (ast_node->as.expr.body, tabs + 1);
            printf ("\n");
            break;
        case AST_UNARY_EXPR:
            printf ("unary_expr<%s>:\n", token_tostr (ast_node->as.unary_expr.tok_op));
            ASTNode_print (ast_node->as.unary_expr.operand, tabs + 1);
            break;
        case AST_BINARY_EXPR_1:
            printf ("binary_expr_1<%s>:\n", token_tostr (ast_node->as.binary_expr_1.tok_op));
            ASTNode_print (ast_node->as.binary_expr_1.operand_a, tabs + 1);
            ASTNode_print (ast_node->as.binary_expr_1.operand_b, tabs + 1);
            break;
        case AST_BINARY_EXPR_2:
            printf ("binary_expr_2<%s>:\n", token_tostr (ast_node->as.binary_expr_2.tok_op));
            ASTNode_print (ast_node->as.binary_expr_2.operand_a, tabs + 1);
            ASTNode_print (ast_node->as.binary_expr_2.operand_b, tabs + 1);
            break;
        case AST_FACTOR:
            printf ("factor:\n");
            ASTNode_print (ast_node->as.factor.body, tabs + 1);
            break;
        case AST_STATEMENT:
            printf ("statement:\n");
            ASTNode_print (ast_node->as.statement.body, tabs + 1);
            break;
        case AST_FUNC:
            printf ("func <int> ");
            ASTNode_print (ast_node->as.func.name, 0);
            printf (" () ");
            printf (":\n");
            ASTNode_print (ast_node->as.func.body, tabs + 1);
            break;
        case AST_PROG:
            printf ("prog:\n");
            ASTNode_print (ast_node->as.prog.func, tabs + 1);
            break;
        default:
            UNREACHABLE ();
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
        case AST_ERR:
            fprintf (stderr, "ERROR: encountered AST_ERR node while freeing\n");
            exit (EXIT_FAILURE);
            break;
        case AST_LITERAL_INT:
            break;
        case AST_IDENT:
            break;
        case AST_EXPR:
            ASTNode_free (ast_node->as.expr.body);
            break;
        case AST_UNARY_EXPR:
            ASTNode_free (ast_node->as.unary_expr.operand);
            break;
        case AST_BINARY_EXPR_1:
            ASTNode_free (ast_node->as.binary_expr_1.operand_a);
            ASTNode_free (ast_node->as.binary_expr_1.operand_b);
            break;
        case AST_BINARY_EXPR_2:
            ASTNode_free (ast_node->as.binary_expr_2.operand_a);
            ASTNode_free (ast_node->as.binary_expr_2.operand_b);
            break;
        case AST_FACTOR:
            ASTNode_free (ast_node->as.factor.body);
            break;
        case AST_STATEMENT:
            ASTNode_free (ast_node->as.statement.body);
            break;
        case AST_FUNC:
            ASTNode_free (ast_node->as.func.name);
            ASTNode_free (ast_node->as.func.body);
            break;
        case AST_PROG:
            ASTNode_free (ast_node->as.prog.func);
            break;
        default:
            UNREACHABLE ();
    }

    free (ast_node);
    ast_node = NULL;
}

static void tokerr (parser_data_t *parser, token_t *tok)
{
    err (parser, "ERROR: at tok %d \'%s\' at line %d, column %d\n", parser->tok_i - 1,
               token_tostr (tok), tok->line, tok->column);
}

static void parser_commit (parser_data_t *parser)
{
    if (parser->history.length <= 0)
    {
        fprintf (stderr, "ERROR: failed to commit parser state, no checkpoint in history\n");
        exit (EXIT_FAILURE);
    }

    parser->history.length--;
    stringbuffer_clear (&parser->err_msg_buffer);
}

static void parser_checkpoint (parser_data_t *parser)
{
    if (parser->history.length + 1 > parser->history.capacity)
    {
        int capacity = parser->history.capacity * 2;
        if (capacity < parser->history.length + 1)
        {
            capacity = parser->history.length + 1;
        }

        parser_state_t *old = parser->history.checkpoints;
        parser->history.checkpoints = calloc (capacity, sizeof (parser_state_t));

        if (!parser->history.checkpoints)
        {
            fprintf (stderr, "ERROR: failed to allocate memory\n");
            exit (EXIT_FAILURE);
        }

        if (old)
        {
            memcpy (parser->history.checkpoints, old, parser->history.length * sizeof (parser_state_t));
            free (old);
        }
    }

    parser->history.checkpoints[parser->history.length].tok_i = parser->tok_i;
    parser->history.length++;
}

static void parser_rollback (parser_data_t *parser)
{
    if (parser->history.length <= 0)
    {
        fprintf (stderr, "ERROR: failed to rollback parser, no checkpoint saved\n");
        exit (EXIT_FAILURE);
    }

    parser->history.length--;
    parser->tok_i = parser->history.checkpoints[parser->history.length].tok_i;
}

static token_t *nxttok (parser_data_t *parser)
{
    if (parser->tok_i < parser->lexer->tok_cnt)
    {
        return &parser->lexer->toks[parser->tok_i++];
    }

    err (parser, "ERROR: Unexpected end of file\n");
    return NULL;
}

static token_t *exp_nxttok_is (parser_data_t *parser, tokentype_t type,
                                           const char *err_msg)
{
    token_t *tok = nxttok (parser);
    if (tok && tok->type != type)
    {
        tokerr (parser, tok);
        err (parser, err_msg);
        return NULL;
    }
    return tok;
}

static bool istok (parser_data_t *parser, tokentype_t type)
{
    if (parser->tok_i >= parser->lexer->tok_cnt)
    {
        return false;
    }

    token_t *tok = &parser->lexer->toks[parser->tok_i];
    return tok->type == type;
}

static token_t *tok (parser_data_t *parser)
{
    if (parser->tok_i >= parser->lexer->tok_cnt)
    {
        err (parser, "ERROR: unexpected end of file\n");
        return NULL;
    }

    return &parser->lexer->toks[parser->tok_i];
}

static tokentype_t toktype (parser_data_t *parser)
{
    if (parser->tok_i >= parser->lexer->tok_cnt)
    {
        err (parser, "ERROR: unexpected end of file\n");
        return TOKEN_ERROR;
    }

    return parser->lexer->toks[parser->tok_i].type;
}

static void err (parser_data_t *parser, const char * fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    stringbuffer_appendf (&parser->err_msg_buffer, fmt, args);
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


static astnode_t *parse_prog (parser_data_t *parser)
{
    parser_checkpoint (parser);
    astnode_t *prog = allocate_astnode (AST_PROG);

    if (!(prog->as.prog.func = parse_func (parser)))
    {
        ASTNode_free (prog);
        parser_rollback (parser);
        return NULL;
    }

    parser_commit (parser);
    return prog;
}

static astnode_t *parse_func (parser_data_t *parser)
{
    parser_checkpoint (parser);
    astnode_t *func = allocate_astnode (AST_FUNC);

    if (!exp_nxttok_is (parser, TOKEN_KEYWORD_INT, "Expected an 'int' return type for function\n") ||
        !(func->as.func.name = parse_ident (parser)) ||
        !exp_nxttok_is (parser, TOKEN_OPEN_PARENTHESIS, "Expected '(' after function identifier\n") ||
        !exp_nxttok_is (parser, TOKEN_CLOSE_PARENTHESIS, "Expected ')' after '('\n") ||
        !exp_nxttok_is (parser, TOKEN_OPEN_BRACE, "Expected '{' to begin function body\n") ||
        !(func->as.func.body = parse_statement (parser)) ||
        !exp_nxttok_is (parser, TOKEN_CLOSE_BRACE, "Expected '}' to close function body\n"))
    {

        parser_rollback (parser);
        ASTNode_free (func);
        return NULL;
    }

    parser_commit (parser);
    return func;
}

static astnode_t *parse_statement (parser_data_t *parser)
{
    parser_checkpoint (parser);
    astnode_t *statement = allocate_astnode (AST_STATEMENT);

    if (!exp_nxttok_is (parser, TOKEN_KEYWORD_RETURN, "Expected \'return\' statement\n") ||
        !(statement->as.statement.body = parse_expr (parser)) ||
        !exp_nxttok_is (parser, TOKEN_SEMICOLON, "Expected \';\' to end statement\n"))
    {
        parser_rollback (parser);
        ASTNode_free (statement);
        return NULL;
    }

    parser_commit(parser);
    return statement;
}

static astnode_t *parse_expr (parser_data_t *parser)
{
    parser_checkpoint (parser);
    astnode_t *expr = allocate_astnode (AST_EXPR);

    if (!(expr->as.expr.body = parse_factor (parser)) ||
        !(expr->as.expr.body = parse_binary_expr_2 (parser)))
    {
        parser_rollback (parser);
        ASTNode_free (expr);
        return NULL;
    }

    parser_commit (parser);
    return expr;
}

static bool to_int (parser_data_t *parser, token_t *tok, int *out)
{
    if (tok->type != TOKEN_LITERAL_INT)
    {
        err (parser, "Expected literal int\n");
        return false;
    }

    int val = 0;
    for (int i = 0; i < tok->length; i++)
    {
        val *= 10;
        val += tok->src[i] - '0';
    }

    *out = val;
    return true;
}

static astnode_t *parse_literal_int (parser_data_t *parser)
{
    parser_checkpoint (parser);
    astnode_t *literal_int = allocate_astnode (AST_LITERAL_INT);
    if (!(literal_int->as.literal_int.tok_int = exp_nxttok_is (parser, TOKEN_LITERAL_INT, "Expected integer literal\n")) ||
        !to_int (parser, literal_int->as.literal_int.tok_int, &literal_int->as.literal_int.value))
    {
        parser_rollback (parser);
        ASTNode_free (literal_int);
        return NULL;
    }

    parser_commit (parser);
    return literal_int;
}

static token_t *get_unary_op (parser_data_t *parser)
{
    token_t *tok = nxttok (parser);
    if (!tok)
    {
        return NULL;
    }

    if (tok->type != TOKEN_HYPEN && tok->type != TOKEN_EXCLAMATION_MARK && tok->type != TOKEN_TILDE)
    {
        tokerr (parser, tok);
        err (parser, "ERROR: invalid unary op\n");
        return NULL;
    }
    return tok;
}

static astnode_t *parse_unary_expr (parser_data_t *parser)
{
    parser_checkpoint (parser);
    astnode_t *unary_expr = allocate_astnode (AST_UNARY_EXPR);

    if (!(unary_expr->as.unary_expr.tok_op = get_unary_op (parser)) ||
        !(unary_expr->as.unary_expr.operand = parse_factor (parser)))
    {
        parser_rollback (parser);
        ASTNode_free (unary_expr);
        return NULL;
    }

    parser_commit (parser);
    return unary_expr;
}

// TODO LEFT OFF HERE

static token_t *get_binary_op_1 (parser_data_t *parser)
{
    token_t *tok = nxttok (parser);
    if (!tok )
    {
        return NULL;
    }

    if (tok->type != TOKEN_ASTERICK && tok->type != TOKEN_FORWARD_SLASH)
    {
        tokerr (parser, tok);
        err (parser, "ERROR: invalid binary_op_1\n");
        return NULL;
    }
    return tok;
}

static astnode_t *parse_binary_expr_1 (parser_data_t *parser)
{
    parser_checkpoint (parser);
    astnode_t *binary_expr_1 = allocate_astnode (AST_BINARY_EXPR_1);

    if (!(binary_expr_1->as.binary_expr_1.operand_a = parse_factor (parser)) ||
        !(binary_expr_1->as.binary_expr_1.tok_op = get_binary_op_1 (parser)) ||
        !(binary_expr_1->as.binary_expr_1.operand_b = parse_factor (parser)))
    {
        parser_rollback (parser);
        ASTNode_free (binary_expr_1);
        return NULL;
    }

    parser_commit (parser);
    return binary_expr_1;
}


static token_t *get_binary_op_2 (parser_data_t *parser)
{
    token_t *tok = nxttok (parser);
    if (!tok )
    {
        return NULL;
    }

    if (tok->type != TOKEN_PLUS && tok->type != TOKEN_HYPEN)
    {
        tokerr (parser, tok);
        err (parser, "ERROR: invalid binary_op_2\n");
        return NULL;
    }
    return tok;
}

static astnode_t *parse_binary_expr_2 (parser_data_t *parser)
{
    parser_checkpoint (parser);
    astnode_t *binary_expr_2 = allocate_astnode (AST_BINARY_EXPR_2);

    if (!(binary_expr_2->as.binary_expr_2.operand_a = parse_binary_expr_1 (parser)) ||
        !(binary_expr_2->as.binary_expr_2.tok_op = get_binary_op_2 (parser)) ||
        !(binary_expr_2->as.binary_expr_2.operand_b = parse_binary_expr_1 (parser)))
    {
        parser_rollback (parser);
        ASTNode_free (binary_expr_2);
        return NULL;
    }

    parser_commit (parser);
    return binary_expr_2;
}

static astnode_t *parse_factor (parser_data_t *parser)
{
    parser_checkpoint (parser);
    astnode_t *factor = allocate_astnode (AST_FACTOR);

    if (exp_nxttok_is (parser, TOKEN_OPEN_PARENTHESIS, "Expected open parenthesis\n") &&
        (factor->as.factor.body = parse_expr (parser)))
    {
        parser_commit (parser);
        return factor;
    }

    parser_rollback (parser);
    parser_checkpoint (parser);

    if (!(factor->as.factor.body = parse_unary_expr (parser)) ||
        !(factor->as.factor.body = parse_literal_int (parser)))
    {
        parser_rollback (parser);
        ASTNode_free (factor);
        return NULL;
    }

    parser_commit (parser);
    return factor;
}

static astnode_t *parse_ident (parser_data_t *parser)
{
    parser_checkpoint (parser);
    astnode_t *ident = allocate_astnode (AST_IDENT);

    if (!(ident->as.ident.tok_id = exp_nxttok_is (parser, TOKEN_IDENTIFIER, "Expected identifier\n")))
    {
        parser_rollback (parser);
        ASTNode_free (ident);
        return NULL;
    }

    parser_commit (parser);
    return ident;
}
