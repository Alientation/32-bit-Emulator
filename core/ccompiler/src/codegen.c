#include "ccompiler/codegen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>

static void codegen_astnode (struct CodegenData *codegen, astnode_t *ast_node);
static void codegen_astexpression (struct CodegenData *codegen, astnode_t *ast_node);
static void codegen_aststatement (struct CodegenData *codegen, astnode_t *ast_node);
static void codegen_astfunction (struct CodegenData *codegen, astnode_t *ast_node);
static void codegen_astprogram (struct CodegenData *codegen, astnode_t *ast_node);

static void codegenblock_init (struct CodegenBlock *block);
static void codegenblock_free (struct CodegenBlock *block);
static void codegenblock_add (struct CodegenBlock *block, const char *code);
static void codegenblock_ladd (struct CodegenBlock *block, const char *code, int len);

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

    codegen_astnode (&codegen, (astnode_t *) parser->ast);

    /* Write code blocks to file */
    fprintf (file, "%s\n.text\n%s\nhlt\n", codegen.glob_sym_decl.code, codegen.txt_sect.code);

    printf ("GENERATED CODE\n\"%s\":\n", output_filepath);
    printf ("%s\n.text\n%s\nhlt\n", codegen.glob_sym_decl.code, codegen.txt_sect.code);

    fclose (file);
    codegenblock_free (&codegen.glob_sym_decl);
    codegenblock_free (&codegen.txt_sect);
}


static void codegen_astnode (struct CodegenData *codegen, astnode_t *ast_node)
{
    switch (ast_node->type)
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
            return codegen_astexpression (codegen, ast_node);
        case AST_STATEMENT:
            return codegen_aststatement (codegen, ast_node);
        case AST_FUNCTION:
            return codegen_astfunction (codegen, ast_node);
        case AST_PROGRAM:
            return codegen_astprogram (codegen, ast_node);
    }

    fprintf (stderr, "ERROR: unknown ASTNode type %d\n", ast_node->type);
    exit (EXIT_FAILURE);
}

static void codegen_astexpression (struct CodegenData *codegen, astnode_t *node)
{
    codegenblock_add (&codegen->txt_sect, "mov x0, ");

    // todo add codegenblock_add_token for these things
    codegenblock_ladd (&codegen->txt_sect, node->as.literal_int.tok_int->src,
                       node->as.literal_int.tok_int->length);
    codegenblock_add (&codegen->txt_sect, "\n");
}

static void codegen_aststatement (struct CodegenData *codegen, astnode_t *node)
{
    codegen_astexpression (codegen, node->as.statement.expression);
    codegenblock_add (&codegen->txt_sect, "ret\n");
}

static void codegen_astfunction (struct CodegenData *codegen, astnode_t *node)
{
    astnode_t *identifier = node->as.function.identifier;
    codegenblock_add (&codegen->glob_sym_decl, ".global ");
    codegenblock_ladd (&codegen->glob_sym_decl, identifier->as.identifier.tok_id->src,
                       identifier->as.identifier.tok_id->length);
    codegenblock_add (&codegen->glob_sym_decl, "\n");

    codegenblock_ladd (&codegen->txt_sect, identifier->as.identifier.tok_id->src,
                       identifier->as.identifier.tok_id->length);
    codegenblock_add (&codegen->txt_sect, ":\n");

    codegen_aststatement (codegen, node->as.function.statement);
}

static void codegen_astprogram (struct CodegenData *codegen, astnode_t *node)
{
    codegen_astfunction (codegen, node->as.program.function);
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

static void codegenblock_ladd (struct CodegenBlock *block, const char *code, int len)
{
    if (block->length + len > block->capacity)
    {
        char *old_code = block->code;
        block->capacity += block->capacity + 10;

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