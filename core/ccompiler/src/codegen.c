#include "ccompiler/codegen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void codegen_ast (struct CodegenData *codegen, astnode_t *node);
static void codegen_unary_op (struct CodegenData *codegen, astnode_t *node);
static void codegen_expression (struct CodegenData *codegen, astnode_t *node);
static void codegen_statement (struct CodegenData *codegen, astnode_t *node);
static void codegen_function (struct CodegenData *codegen, astnode_t *node);
static void codegen_program (struct CodegenData *codegen, astnode_t *node);

static void codegenblock_init (struct CodegenBlock *block);
static void codegenblock_free (struct CodegenBlock *block);
static void codegenblock_add (struct CodegenBlock *block, const char *code);
static void codegenblock_ladd (struct CodegenBlock *block, const char *code, int len);
static void codegenblock_addtok (struct CodegenBlock *block, token_t *tok);


void codegen (struct ParserData *parser, const char *output_filepath)
{
    FILE *file = fopen (output_filepath, "w");
    if (file == NULL)
    {
        fprintf (stderr, "ERROR: failed to open file\n");
        exit (EXIT_FAILURE);
    }

    struct CodegenData codegen;
    codegen.parser = parser;
    codegen.output_file = file;
    codegenblock_init (&codegen.glob_sym_decl);
    codegenblock_init (&codegen.txt_sect);

    codegen_ast (&codegen, (astnode_t *) parser->ast);

    /* Write code blocks to file */
    fprintf (file, "%s\n.text\n%s\nhlt\n", codegen.glob_sym_decl.code, codegen.txt_sect.code);

    printf ("GENERATED CODE\n\"%s\":\n", output_filepath);
    printf ("%s\n.text\n%s\nhlt\n", codegen.glob_sym_decl.code, codegen.txt_sect.code);

    fclose (file);
    codegenblock_free (&codegen.glob_sym_decl);
    codegenblock_free (&codegen.txt_sect);
}


static void codegen_ast (struct CodegenData *codegen, astnode_t *node)
{
    switch (node->type)
    {
        case AST_ERROR:
            fprintf (stderr, "ERROR: encountered ERROR ASTNode\n");
            exit (EXIT_FAILURE);
        case AST_LITERAL_INT:
            fprintf (stderr, "ERROR: unexpected AST_LITERAL_INT\n");
            exit (EXIT_FAILURE);
        case AST_IDENTIFIER:
            fprintf (stderr, "ERROR: unexpected AST_IDENTIFIER\n");
            exit (EXIT_FAILURE);
        case AST_EXPRESSION:
            return codegen_expression (codegen, node);
        case AST_UNARY_OP:
            return codegen_unary_op (codegen, node);
        case AST_STATEMENT:
            return codegen_statement (codegen, node);
        case AST_FUNCTION:
            return codegen_function (codegen, node);
        case AST_PROGRAM:
            return codegen_program (codegen, node);
    }

    fprintf (stderr, "ERROR: unknown ASTNode type %d\n", node->type);
    exit (EXIT_FAILURE);
}

static void codegen_unary_op (struct CodegenData *codegen, astnode_t *node)
{
    codegen_expression (codegen, node->as.unary_op.operand);

    switch (node->as.unary_op.tok_op->type)
    {
        case TOKEN_EXCLAMATION_MARK:
            codegenblock_add (&codegen->txt_sect,
                ".scope\n"
                    "cmp x0, 0\n"
                    "b.eq __set\n"
                    "mov x0, 0\n"
                    "b __done\n"
                "__set:\n"
                    "mov x0, 1\n"
                "__done:\n"
                ".scend\n"
            );
            break;
        case TOKEN_HYPEN:
            codegenblock_add (&codegen->txt_sect,
                "sub x0, xzr, x0\n"
            );
            break;
        case TOKEN_TILDE:
            codegenblock_add (&codegen->txt_sect,
                "mvn x0, x0\n"
            );
            break;
        default:
            fprintf (stderr, "ERROR: unknown unary op \'%s\'\n", token_tostr (node->as.unary_op.tok_op));
            exit (EXIT_FAILURE);
    }
}

static void codegen_expression (struct CodegenData *codegen, astnode_t *node)
{
    switch (node->as.expression.expr->type)
    {
        case AST_LITERAL_INT:
            codegenblock_add (&codegen->txt_sect, "mov x0, ");
            codegenblock_addtok (&codegen->txt_sect, node->as.expression.expr->as.literal_int.tok_int);
            break;
        case AST_UNARY_OP:
            codegen_unary_op (codegen, node->as.expression.expr);
            break;
        default:
            fprintf (stderr, "ERROR: unexpected ASTNode type %d\n", node->type);
            exit (EXIT_FAILURE);
    }

    codegenblock_add (&codegen->txt_sect, "\n");
}

static void codegen_statement (struct CodegenData *codegen, astnode_t *node)
{
    codegen_expression (codegen, node->as.statement.body);
    codegenblock_add (&codegen->txt_sect, "ret\n");
}

static void codegen_function (struct CodegenData *codegen, astnode_t *node)
{
    astnode_t *identifier = node->as.function.name;
    codegenblock_add (&codegen->glob_sym_decl, ".global ");
    codegenblock_addtok (&codegen->glob_sym_decl, identifier->as.identifier.tok_id);
    codegenblock_add (&codegen->glob_sym_decl, "\n");

    codegenblock_addtok (&codegen->txt_sect, identifier->as.identifier.tok_id);
    codegenblock_add (&codegen->txt_sect, ":\n");

    codegen_statement (codegen, node->as.function.body);
}

static void codegen_program (struct CodegenData *codegen, astnode_t *node)
{
    codegen_function (codegen, node->as.program.function);
}

static void codegenblock_init (struct CodegenBlock *block)
{
    assert (block);

    block->capacity = 16;
    block->length = 0;
    block->code = calloc (block->capacity + 1, sizeof (char));
    if (!block->code)
    {
        fprintf (stderr, "ERROR: failed to allocate memory.\n");
        exit (EXIT_FAILURE);
    }
}

static void codegenblock_free (struct CodegenBlock *block)
{
    assert (block);

    free (block->code);
    block->code = NULL;
    block->capacity = 0;
    block->length = 0;
}

static void codegenblock_add (struct CodegenBlock *block, const char *code)
{
    int len = strlen (code);
    codegenblock_ladd (block, code, len);
}

static void codegenblock_addtok (struct CodegenBlock *block, token_t *tok)
{
    codegenblock_ladd (block, tok->src, tok->length);
}

static void codegenblock_ladd (struct CodegenBlock *block, const char *code, int len)
{
    if (block->length + len >= block->capacity)
    {
        char *old_code = block->code;
        block->capacity += block->capacity + 10;
        if (block->length + len >= block->capacity)
        {
            block->capacity = 2 * (block->length + len);
        }

        block->code = calloc (block->capacity + 1, sizeof (char));
        if (!block->code)
        {
            fprintf (stderr, "ERROR: failed to allocate memory\n");
            exit (EXIT_FAILURE);
        }
        strncpy (block->code, old_code, block->length);
        block->code[block->length] = '\0';
        free (old_code);
    }

    strncpy (block->code + block->length, code, len);
    block->code[block->length + len] = '\0';  /* Ensure a NULL terminator */
    block->length += len;
}