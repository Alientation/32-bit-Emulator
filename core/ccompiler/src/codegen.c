#include "ccompiler/codegen.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int codegen_astnode (struct CodegenData *codegen, astnode_t *ast_node);
static int codegen_astexpression (struct CodegenData *codegen, astexpression_t *ast_node);
static int codegen_aststatement (struct CodegenData *codegen, aststatement_t *ast_node);
static int codegen_astfunction (struct CodegenData *codegen, astfunction_t *ast_node);
static int codegen_astprogram (struct CodegenData *codegen, astprogram_t *ast_node);

static int codegenblock_init (struct CodegenBlock *block);
static void codegenblock_free (struct CodegenBlock *block);
static int codegenblock_add (struct CodegenBlock *block, const char *code);
static int codegenblock_ladd (struct CodegenBlock *block, const char *code, int len);

int codegen (struct ParserData *parser, const char *output_filepath)
{
    FILE *file = fopen (output_filepath, "w");
    if (file == NULL)
    {
        perror ("Error opening file");
        return CODEGEN_FAILURE__FILE;
    }

    struct CodegenData codegen;
    codegen.parser = parser;
    codegen.output_file = file;
    int exitcode = codegenblock_init (&codegen.glob_sym_decl);
    exitcode |= codegenblock_init (&codegen.txt_sect);
    if (exitcode)
    {
        fail:
        codegenblock_free (&codegen.glob_sym_decl);
        codegenblock_free (&codegen.txt_sect);
        fclose (codegen.output_file);
        return exitcode;
    }

    exitcode = codegen_astnode (&codegen, (astnode_t *) parser->ast);
    if (exitcode) goto fail;

    /* Write code blocks to file */
    fprintf (file, "%s\n.text\n%s\nhlt\n", codegen.glob_sym_decl.code, codegen.txt_sect.code);

    printf ("GENERATED CODE\n\"%s\":\n", output_filepath);
    printf ("%s\n.text\n%s\nhlt\n", codegen.glob_sym_decl.code, codegen.txt_sect.code);

    fclose (file);
    return 0;
}


static int codegen_astnode (struct CodegenData *codegen, astnode_t *ast_node)
{
    switch (ast_node->type)
    {
        case AST_ERROR:
            fprintf (stderr, "Encountered ERROR ASTNode\n");
            return 1;
        case AST_LITERAL_INT:
            fprintf (stderr, "Unexpected AST_LITERAL_INT\n");
            return 1;
        case AST_IDENTIFIER:
            fprintf (stderr, "Unexpected AST_IDENTIFIER\n");
            return 1;
        case AST_EXPRESSION:
            return codegen_astexpression (codegen, (astexpression_t *) ast_node);
        case AST_STATEMENT:
            return codegen_aststatement (codegen, (aststatement_t *) ast_node);
        case AST_FUNCTION:
            return codegen_astfunction (codegen, (astfunction_t *) ast_node);
        case AST_PROGRAM:
            return codegen_astprogram (codegen, (astprogram_t *) ast_node);
    }

    fprintf (stderr, "Unknown ASTNode type %d\n", ast_node->type);
    return 1;
}

static int codegen_astexpression (struct CodegenData *codegen, astexpression_t *ast_node)
{
    codegenblock_add (&codegen->txt_sect, "mov x0, ");
    codegenblock_ladd (&codegen->txt_sect, ast_node->literal_int->tok->src, ast_node->literal_int->tok->length);
    codegenblock_add (&codegen->txt_sect, "\n");

    return 0;
}

static int codegen_aststatement (struct CodegenData *codegen, aststatement_t *ast_node)
{
    int ret = codegen_astexpression (codegen, ast_node->expression);
    if (ret) return ret;

    codegenblock_add (&codegen->txt_sect, "ret\n");

    return 0;
}

static int codegen_astfunction (struct CodegenData *codegen, astfunction_t *ast_node)
{
    codegenblock_add (&codegen->glob_sym_decl, ".global ");
    codegenblock_ladd (&codegen->glob_sym_decl, ast_node->identifier->tok->src, ast_node->identifier->tok->length);
    codegenblock_add (&codegen->glob_sym_decl, "\n");

    codegenblock_ladd (&codegen->txt_sect, ast_node->identifier->tok->src, ast_node->identifier->tok->length);
    codegenblock_add (&codegen->txt_sect, ":\n");

    int ret = codegen_aststatement (codegen, ast_node->statement);
    if (ret) return ret;

    return 0;
}

static int codegen_astprogram (struct CodegenData *codegen, astprogram_t *ast_node)
{
    int ret = codegen_astfunction (codegen, ast_node->function);
    if (ret) return ret;

    return 0;
}

static int codegenblock_init (struct CodegenBlock *block)
{
    if (!block)
    {
        fprintf (stderr, "CodegenBlock is NULL\n");
        return 1;
    }

    block->capacity = 16;
    block->length = 0;
    block->code = calloc (block->capacity + 1, sizeof (char));
    if (!block->code)
    {
        fprintf (stderr, "Memory allocation failed\n");
        return 1;
    }

    return 0;
}

static void codegenblock_free (struct CodegenBlock *block)
{
    free (block->code);
    block->code = NULL;
    block->capacity = 0;
    block->length = 0;
}

static int codegenblock_add (struct CodegenBlock *block, const char *code)
{
    int len = strlen (code);
    return codegenblock_ladd (block, code, len);
}

static int codegenblock_ladd (struct CodegenBlock *block, const char *code, int len)
{
    if (block->length + len > block->capacity)
    {
        char *old_code = block->code;
        block->capacity += block->capacity + 10;

        block->code = calloc (block->capacity + 1, sizeof (char));
        if (!block->code)
        {
            perror ("Memory allocation failed\n");
            return 1;
        }
        strncpy (block->code, old_code, block->length);
        block->code[block->length] = '\0';
        free (old_code);
        old_code = NULL;
    }

    strncpy (block->code + block->length, code, len);
    block->code[block->length + len] = '\0';  /* Ensure a NULL terminator */
    block->length += len;
    return 0;
}