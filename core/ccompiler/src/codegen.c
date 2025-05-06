#include "ccompiler/codegen.h"

#include "ccompiler/massert.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void codegen_ast (codegen_data_t *codegen, astnode_t *node);
static void codegen_prog (codegen_data_t *codegen, astnode_t *node);
static void codegen_func (codegen_data_t *codegen, astnode_t *node);
static void codegen_statement (codegen_data_t *codegen, astnode_t *node);
static void codegen_expr (codegen_data_t *codegen, astnode_t *node);
static void codegen_factor (codegen_data_t *codegen, astnode_t *node);
static void codegen_binary_expr_2 (codegen_data_t *codegen, astnode_t *node);
static void codegen_binary_expr_1 (codegen_data_t *codegen, astnode_t *node);
static void codegen_unary_expr (codegen_data_t *codegen, astnode_t *node);

static void codegenblock_init (codegen_block_t *block);
static void codegenblock_free (codegen_block_t *block);
static void codegenblock_add (codegen_block_t *block, const char *code);
static void codegenblock_ladd (codegen_block_t *block, const char *code, int len);
static void codegenblock_addtok (codegen_block_t *block, token_t *tok);

static void register_init (codegen_data_t *codegen);
static int register_alloc (codegen_data_t *codegen);
static void register_free (codegen_data_t *codegen, int reg);
static codegen_reg_t register_get (codegen_data_t *codegen, int reg);
static bool register_is_caller_saved (codegen_data_t *codegen, int reg);
static bool register_is_callee_saved (codegen_data_t *codegen, int reg);


static void register_init (codegen_data_t *codegen)
{
    for (int i = 0; i < N_CALLER_REGS; i++)
    {
        codegen->caller_saved_regs[i].alloc = false;
    }

    for (int i = 0; i < N_CALLEE_REGS; i++)
    {
        codegen->callee_saved_regs[i].alloc = false;
    }

    codegen->caller_saved_regs[0].name = "x0";
    codegen->caller_saved_regs[1].name = "x1";
    codegen->caller_saved_regs[2].name = "x2";
    codegen->caller_saved_regs[3].name = "x3";
    codegen->caller_saved_regs[4].name = "x4";
    codegen->caller_saved_regs[5].name = "x5";
    codegen->caller_saved_regs[6].name = "x6";
    codegen->caller_saved_regs[7].name = "x7";
    codegen->caller_saved_regs[8].name = "x8";
    codegen->caller_saved_regs[9].name = "x9";
    codegen->caller_saved_regs[10].name = "x10";
    codegen->caller_saved_regs[11].name = "x11";
    codegen->caller_saved_regs[12].name = "x12";
    codegen->caller_saved_regs[13].name = "x13";
    codegen->caller_saved_regs[14].name = "x14";
    codegen->caller_saved_regs[15].name = "x15";
    codegen->caller_saved_regs[16].name = "x16";
    codegen->caller_saved_regs[17].name = "x17";

    codegen->callee_saved_regs[0].name = "x19";
    codegen->callee_saved_regs[1].name = "x20";
    codegen->callee_saved_regs[2].name = "x21";
    codegen->callee_saved_regs[3].name = "x22";
    codegen->callee_saved_regs[4].name = "x23";
    codegen->callee_saved_regs[5].name = "x24";
    codegen->callee_saved_regs[6].name = "x25";
    codegen->callee_saved_regs[7].name = "x26";
    codegen->callee_saved_regs[8].name = "x27";
}

static int register_alloc (codegen_data_t *codegen)
{
    // prioritize allocating caller saved registers
    


    return -1;
}

static void register_free (codegen_data_t *codegen, int reg)
{

}

static codegen_reg_t register_get (codegen_data_t *codegen, int reg)
{
    massert (reg >= 0 && reg < N_CALLER_REGS + N_CALLEE_REGS, "Invalid register identifier: r%d", reg);

    if (reg < N_CALLER_REGS)
    {
        return codegen->caller_saved_regs[reg];
    }
    else if (reg < N_CALLER_REGS + N_CALLEE_REGS)
    {
        return codegen->callee_saved_regs[reg - N_CALLER_REGS];
    }

    UNREACHABLE ();
}

static bool register_is_caller_saved (codegen_data_t *codegen, int reg)
{
    return reg >= 0 && reg < N_CALLER_REGS;
}

static bool register_is_callee_saved (codegen_data_t *codegen, int reg)
{
    return reg >= N_CALLER_REGS && reg < N_CALLER_REGS + N_CALLEE_REGS;
}



void codegen (parser_data_t *parser, const char *output_filepath)
{
    FILE *file = fopen (output_filepath, "w");
    if (file == NULL)
    {
        fprintf (stderr, "ERROR: failed to open file\n");
        exit (EXIT_FAILURE);
    }

    codegen_data_t codegen = {0};
    codegen.parser = parser;
    codegen.output_file = file;
    register_init (&codegen);
    codegenblock_init (&codegen.glob_sym_decl);
    codegenblock_init (&codegen.txt_sect);

    codegen_ast (&codegen, (astnode_t *) parser->ast);

    /* Write code blocks to file */
    fprintf (file, "%s\n.text\n%s\n\thlt\n", codegen.glob_sym_decl.code, codegen.txt_sect.code);

    printf ("GENERATED CODE\n\"%s\":\n", output_filepath);
    printf ("%s\n.text\n%s\n\thlt\n", codegen.glob_sym_decl.code, codegen.txt_sect.code);

    fclose (file);
    codegenblock_free (&codegen.glob_sym_decl);
    codegenblock_free (&codegen.txt_sect);
}


static void codegen_ast (codegen_data_t *codegen, astnode_t *node)
{
    switch (node->type)
    {
        case AST_ERR:
            M_UNREACHABLE ("Encountered AST_ERR in codegen");
            break;
        case AST_LITERAL_INT:
            massert (false, "Encountered AST_LITERAL_INT in codegen");
            break;
        case AST_IDENT:
            massert (false, "Encountered AST_IDENT in codegen");
            M_UNREACHABLE ("Unexpected ASTNODE_TYPE in codegen: %s", node->type);
            break;
        case AST_EXPR:
            return codegen_expr (codegen, node);
        case AST_UNARY_EXPR:
            return codegen_unary_expr (codegen, node);
        case AST_STATEMENT:
            return codegen_statement (codegen, node);
        case AST_FUNC:
            return codegen_func (codegen, node);
        case AST_PROG:
            return codegen_prog (codegen, node);
        default:
            UNREACHABLE ();
    }
}


static void codegen_prog (codegen_data_t *codegen, astnode_t *node)
{
    codegen_func (codegen, node->as.prog.func);
}

static void codegen_func (codegen_data_t *codegen, astnode_t *node)
{
    // todo register allocation, stack space management

    astnode_t *ident = node->as.func.name;
    codegenblock_add (&codegen->glob_sym_decl, ".global ");
    codegenblock_addtok (&codegen->glob_sym_decl, ident->as.ident.tok_id);
    codegenblock_add (&codegen->glob_sym_decl, "\n");

    codegenblock_addtok (&codegen->txt_sect, ident->as.ident.tok_id);
    codegenblock_add (&codegen->txt_sect, ":\n");

    codegen_statement (codegen, node->as.func.body);
}

static void codegen_statement (codegen_data_t *codegen, astnode_t *node)
{
    codegen_expr (codegen, node->as.statement.body);
    codegenblock_add (&codegen->txt_sect, "\tret\n");
}

static void codegen_expr (codegen_data_t *codegen, astnode_t *node)
{
    switch (node->as.expr.body->type)
    {
        case AST_FACTOR:
            codegen_factor (codegen, node->as.expr.body);
            break;
        case AST_BINARY_EXPR_2:
            codegen_binary_expr_2 (codegen, node->as.expr.body);
            break;
        default:
            fprintf (stderr, "ERROR: unexpected ASTNode type %d\n", node->as.expr.body->type);
    }

    codegenblock_add (&codegen->txt_sect, "\n");
}

static void codegen_factor (codegen_data_t *codegen, astnode_t *node)
{
    switch (node->as.factor.body->type)
    {
        case AST_EXPR:
            codegen_expr (codegen, node->as.factor.body);
            break;
        case AST_UNARY_EXPR:
            codegen_unary_expr (codegen, node->as.factor.body);
            break;
        case AST_LITERAL_INT:
            codegenblock_add (&codegen->txt_sect, "\tmov x0, ");
            codegenblock_addtok (&codegen->txt_sect, node->as.factor.body->as.literal_int.tok_int);
            break;
        default:
            fprintf (stderr, "ERROR: unexpected ASTNode type %d\n", node->as.factor.body->type);
    }
}

static void codegen_binary_expr_2 (codegen_data_t *codegen, astnode_t *node)
{
    switch (node->as.binary_expr_2.operand_a->type)
    {
        case AST_BINARY_EXPR_1:
            codegen_factor (codegen, node->as.binary_expr_2.operand_a);
            break;
        default:
            fprintf (stderr, "ERROR: unexpected ASTNode type %d\n", node->as.binary_expr_2.operand_a->type);
    }

    switch (node->as.binary_expr_2.operand_b->type)
    {
        case AST_BINARY_EXPR_1:
            codegen_factor (codegen, node->as.binary_expr_2.operand_b);
            break;
        default:
            fprintf (stderr, "ERROR: unexpected ASTNode type %d\n", node->as.binary_expr_2.operand_b->type);
    }
}

static void codegen_binary_expr_1 (codegen_data_t *codegen, astnode_t *node)
{

}

static void codegen_unary_expr (codegen_data_t *codegen, astnode_t *node)
{
    codegen_expr (codegen, node->as.unary_expr.operand);

    switch (node->as.unary_expr.tok_op->type)
    {
        case TOKEN_EXCLAMATION_MARK:
            codegenblock_add (&codegen->txt_sect,
                ".scope\n"
                    "\tcmp x0, 0\n"
                    "\tb.eq __set\n"
                    "\tmov x0, 0\n"
                    "\tb __done\n"
                "__set:\n"
                    "\tmov x0, 1\n"
                "__done:\n"
                ".scend\n"
            );
            break;
        case TOKEN_HYPEN:
            codegenblock_add (&codegen->txt_sect,
                "\tsub x0, xzr, x0\n"
            );
            break;
        case TOKEN_TILDE:
            codegenblock_add (&codegen->txt_sect,
                "\tmvn x0, x0\n"
            );
            break;
        default:
            fprintf (stderr, "ERROR: unknown unary op \'%s\'\n", token_tostr (node->as.unary_expr.tok_op));
            exit (EXIT_FAILURE);
    }
}

static void codegenblock_init (codegen_block_t *block)
{
    block->capacity = 16;
    block->length = 0;
    block->code = calloc (block->capacity + 1, sizeof (char));
    if (!block->code)
    {
        fprintf (stderr, "ERROR: failed to allocate memory.\n");
        exit (EXIT_FAILURE);
    }
}

static void codegenblock_free (codegen_block_t *block)
{
    free (block->code);
    block->code = NULL;
    block->capacity = 0;
    block->length = 0;
}

static void codegenblock_add (codegen_block_t *block, const char *code)
{
    int len = strlen (code);
    codegenblock_ladd (block, code, len);
}

static void codegenblock_addtok (codegen_block_t *block, token_t *tok)
{
    codegenblock_ladd (block, tok->src, tok->length);
}

static void codegenblock_ladd (codegen_block_t *block, const char *code, int len)
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

    memcpy (block->code + block->length, code, len);
    block->code[block->length + len] = '\0';  /* Ensure a NULL terminator */
    block->length += len;
}