#include "ccompiler/codegen.h"

#include "ccompiler/massert.h"

#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define UNUSED(x) (void)(x)

static void codegen_ast (codegen_data_t *codegen, astnode_t *node);
static void codegen_prog (codegen_data_t *codegen, astnode_t *node);
static void codegen_func (codegen_data_t *codegen, astnode_t *node);
static void codegen_statement (codegen_data_t *codegen, astnode_t *node);
static regid_t codegen_expr (codegen_data_t *codegen, astnode_t *node);
static regid_t codegen_factor (codegen_data_t *codegen, astnode_t *node);
static regid_t codegen_binary_expr_2 (codegen_data_t *codegen, astnode_t *node);
static regid_t codegen_binary_expr_1 (codegen_data_t *codegen, astnode_t *node);
static regid_t codegen_unary_expr (codegen_data_t *codegen, astnode_t *node);

static void codegenblock_addtok (stringbuffer_t *block, token_t *tok);

static void func_init (codegen_func_t *func, astnode_t *node);
static void func_push_reg (codegen_func_t *func, regid_t rid);
static void func_pop_reg (codegen_func_t *func, regid_t rid);

static void reg_init (codegen_func_t *func);
static regid_t reg_alloc (codegen_func_t *func);
static regid_t reg_evict (codegen_func_t *func);
static void reg_free (codegen_func_t *func, regid_t rid);
static codegen_reg_t *reg_get (codegen_func_t *func, regid_t rid);
static bool reg_is_caller_saved (regid_t rid);
static bool reg_is_callee_saved (regid_t rid);
static regid_t reg_get_caller_saved_id (int i);
static regid_t reg_get_callee_saved_id (int i);


static void func_init (codegen_func_t *func, astnode_t *node)
{
    func->func = node;

    // by default, stack needs space for LR and FP
    // can be optimized out, but at the end when funcion code has been generated
    func->stack_used = 16;
    func->stack_capacity = 16;

    stringbuffer_init (&func->body);
    reg_init (func);
}

static void func_push_reg (codegen_func_t *func, regid_t rid)
{
    codegen_reg_t *reg = reg_get (func, rid);
    reg->stack_offset = func->stack_used;
    func->stack_used += 8;
    if (func->stack_capacity < func->stack_used)
    {
        // align to 16
        func->stack_capacity = ((func->stack_capacity + 15) / 16) * 16;
    }

    // save register to stack 'str <r>, [sp, <offset>]'
    stringbuffer_appendf (&func->body, "str %s, [sp, %d]\n", reg->name, reg->stack_offset);
}

static void func_pop_reg (codegen_func_t *func, regid_t rid)
{
    codegen_reg_t *reg = reg_get (func, rid);

    massert (func->stack_used >= 8, "expected stack size to be greater than the size of a register");
    massert (reg->stack_offset >= 0, "expected stack offset to be positive since SP points to the bottom of the stack");

    // restore reg from stack 'ldr <r>, [sp, <offset>]'
    stringbuffer_appendf (&func->body, "ldr %s, [sp, %d]\n", reg->name, reg->stack_offset);

    reg->stack_offset = -1;
    func->stack_used -= 8;
}

static void reg_init (codegen_func_t *func)
{
    for (int i = 0; i < CODEGEN_NUM_CALLER_REGS; i++)
    {
        func->caller_saved_regs[i].name = NULL;
        func->caller_saved_regs[i].alloc = false;
        func->caller_saved_regs[i].stack_offset = -1;
    }

    for (int i = 0; i < CODEGEN_NUM_CALLEE_REGS; i++)
    {
        func->callee_saved_regs[i].name = NULL;
        func->callee_saved_regs[i].alloc = false;
        func->callee_saved_regs[i].stack_offset = -1;
    }

    func->caller_saved_regs[0].name = "x0";
    func->caller_saved_regs[1].name = "x1";
    func->caller_saved_regs[2].name = "x2";
    func->caller_saved_regs[3].name = "x3";
    func->caller_saved_regs[4].name = "x4";
    func->caller_saved_regs[5].name = "x5";
    func->caller_saved_regs[6].name = "x6";
    func->caller_saved_regs[7].name = "x7";
    func->caller_saved_regs[8].name = "x8";
    func->caller_saved_regs[9].name = "x9";
    func->caller_saved_regs[10].name = "x10";
    func->caller_saved_regs[11].name = "x11";
    func->caller_saved_regs[12].name = "x12";
    func->caller_saved_regs[13].name = "x13";
    func->caller_saved_regs[14].name = "x14";
    func->caller_saved_regs[15].name = "x15";
    func->caller_saved_regs[16].name = "x16";
    func->caller_saved_regs[17].name = "x17";

    func->callee_saved_regs[0].name = "x19";
    func->callee_saved_regs[1].name = "x20";
    func->callee_saved_regs[2].name = "x21";
    func->callee_saved_regs[3].name = "x22";
    func->callee_saved_regs[4].name = "x23";
    func->callee_saved_regs[5].name = "x24";
    func->callee_saved_regs[6].name = "x25";
    func->callee_saved_regs[7].name = "x26";
    func->callee_saved_regs[8].name = "x27";
}

static regid_t reg_alloc (codegen_func_t *func)
{
    // prioritize allocating caller saved registers
    for (int i = 0; i < CODEGEN_NUM_CALLER_REGS; i++)
    {
        if (!func->caller_saved_regs[i].alloc)
        {
            func->caller_saved_regs[i].alloc = true;
            return reg_get_caller_saved_id (i);
        }
    }

    // otherwise try to allocate a callee saved register
    // need to push current value in the reg to stack
    for (int i = 0; i < CODEGEN_NUM_CALLEE_REGS; i++)
    {
        if (!func->callee_saved_regs[i].alloc)
        {
            func->callee_saved_regs[i].alloc = true;
            func_push_reg (func, reg_get_callee_saved_id (i));
            return reg_get_callee_saved_id (i);
        }
    }

    // otherwise, we need to evict
    return reg_evict (func);
}

static regid_t reg_evict (codegen_func_t *func)
{
    UNUSED(func);
    M_UNREACHABLE ("unimplemented");
    return -1;
}

static void reg_free (codegen_func_t *func, regid_t rid)
{
    codegen_reg_t *reg = reg_get (func, rid);

    massert (reg->alloc, "attempted to free unallocated register %s", reg->name);

    reg->alloc = false;

    // need to restore old value
    if (reg_is_callee_saved (rid))
    {
        func_pop_reg (func, rid);
    }
}

static codegen_reg_t *reg_get (codegen_func_t *func, regid_t rid)
{
    massert (rid >= 0 && rid < CODEGEN_NUM_CALLER_REGS + CODEGEN_NUM_CALLEE_REGS, "Invalid register identifier: r%d", rid);

    if (rid < CODEGEN_NUM_CALLER_REGS)
    {
        return &func->caller_saved_regs[rid];
    }
    else if (rid < CODEGEN_NUM_CALLER_REGS + CODEGEN_NUM_CALLEE_REGS)
    {
        return &func->callee_saved_regs[rid - CODEGEN_NUM_CALLER_REGS];
    }

    UNREACHABLE ();
    return NULL;
}

static bool reg_is_caller_saved (regid_t rid)
{
    return rid >= 0 && rid < CODEGEN_NUM_CALLER_REGS;
}

static bool reg_is_callee_saved (regid_t rid)
{
    return rid >= CODEGEN_NUM_CALLER_REGS && rid < CODEGEN_NUM_CALLER_REGS + CODEGEN_NUM_CALLEE_REGS;
}

static regid_t reg_get_caller_saved_id (int i)
{
    return i;
}

static regid_t reg_get_callee_saved_id (int i)
{
    return i + CODEGEN_NUM_CALLER_REGS;
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
    codegen.cur_func = NULL;
    codegen.cur_txt_sect = &codegen.txt_sect;
    stringbuffer_init (&codegen.glob_sym_decl);
    stringbuffer_init (&codegen.txt_sect);

    codegen_ast (&codegen, (astnode_t *) parser->ast);

    // Write code blocks to file
    fprintf (file, "%s\n.text\n%s\n\thlt\n", codegen.glob_sym_decl.buf, codegen.txt_sect.buf);

    printf ("GENERATED CODE\n\"%s\":\n", output_filepath);
    printf ("%s\n.text\n%s\n\thlt\n", codegen.glob_sym_decl.buf, codegen.txt_sect.buf);

    fclose (file);
    stringbuffer_free (&codegen.glob_sym_decl);
    stringbuffer_free (&codegen.txt_sect);
}


static void codegen_ast (codegen_data_t *codegen, astnode_t *node)
{
    switch (node->type)
    {
        case AST_ERR:
        case AST_LITERAL_INT:
        case AST_IDENT:
        case AST_EXPR:
        case AST_UNARY_EXPR:
        case AST_BINARY_EXPR_1:
        case AST_BINARY_EXPR_2:
        case AST_FACTOR:
        case AST_STATEMENT:
        case AST_FUNC:
            M_UNREACHABLE ("Unexpected ASTNODE_TYPE in codegen: %s", parser_astnode_type_to_str (node->type));
            break;
        case AST_PROG:
            codegen_prog (codegen, node);
            break;
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
    astnode_t *ident = node->as.func.name;

    // mark symbol as global '.global <func.name>'
    stringbuffer_appendf (&codegen->glob_sym_decl, ".global %.*s\n",
            ident->as.ident.tok_id->length, ident->as.ident.tok_id->length);

    // label in text section '<func.name>:'
    stringbuffer_appendf (&codegen->txt_sect, "%.*s:\n",
            ident->as.ident.tok_id->length, ident->as.ident.tok_id->length);

    codegen_func_t func;
    func_init (&func, node);
    codegen->cur_func = &func;
    codegen->cur_txt_sect = &func.body;

    codegen_statement (codegen, node->as.func.body);

    // prologue, set up stack frame
    stringbuffer_appendf (&codegen->txt_sect, "sub sp, sp, %d\n", func.stack_capacity);
    stringbuffer_appendf (&codegen->txt_sect, "str x28, [sp]\n");
    stringbuffer_appendf (&codegen->txt_sect, "str x29, [sp, 8]\n");

    // body
    stringbuffer_appendf (&codegen->txt_sect, "\n%s\n", func.body.buf);

    // epliogue
    stringbuffer_appendf (&codegen->txt_sect, "ldr x28, [sp]\n");
    stringbuffer_appendf (&codegen->txt_sect, "ldr x29, [sp, 8]\n");
    stringbuffer_appendf (&codegen->txt_sect, "add sp, sp, %d\n", func.stack_capacity);
    codegen->cur_func = NULL;
    codegen->cur_txt_sect = &codegen->txt_sect;
}

static void codegen_statement (codegen_data_t *codegen, astnode_t *node)
{
    massert (codegen->cur_func, "Expected AST_STATEMENT to be under an AST_FUNC");

    codegen_expr (codegen, node->as.statement.body);
    stringbuffer_append (codegen->cur_txt_sect, "\tret\n");
}

static regid_t codegen_expr (codegen_data_t *codegen, astnode_t *node)
{
    switch (node->as.expr.body->type)
    {
        case AST_FACTOR:
            return codegen_factor (codegen, node->as.expr.body);
            break;
        case AST_BINARY_EXPR_2:
            return codegen_binary_expr_2 (codegen, node->as.expr.body);
            break;
        default:
            fprintf (stderr, "ERROR: unexpected ASTNode type %d\n", node->as.expr.body->type);
    }
    return -1;
}

static regid_t codegen_factor (codegen_data_t *codegen, astnode_t *node)
{
    switch (node->as.factor.body->type)
    {
        case AST_EXPR:
            return codegen_expr (codegen, node->as.factor.body);
            break;
        case AST_UNARY_EXPR:
            return codegen_unary_expr (codegen, node->as.factor.body);
            break;
        case AST_LITERAL_INT:
            // todo, problem
            //  generating code this way would lead to many inefficiencies
            // since any time we want to use a literal, we have to first load the literal into a reg
            // instead of taking advantage of instructio immediates
            // this would be more easily solved with constant folding on IR
            // but we don't have IR, so... should we implement IR to allow easier compiler otpimizations?

            // TODO last left off here, for now, just allocate a register for the immediate constant...
            stringbuffer_append (codegen->cur_txt_sect, "\tmov x0, ");
            codegenblock_addtok (codegen->cur_txt_sect, node->as.factor.body->as.literal_int.tok_int);
            stringbuffer_append (codegen->cur_txt_sect, "\n");
            break;
        default:
            fprintf (stderr, "ERROR: unexpected ASTNode type %d\n", node->as.factor.body->type);
    }

    return -1;
}

static regid_t codegen_binary_expr_2 (codegen_data_t *codegen, astnode_t *node)
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

    UNUSED (codegen);
    UNUSED (node);
    return -1;
}

static regid_t codegen_binary_expr_1 (codegen_data_t *codegen, astnode_t *node)
{
    UNUSED (codegen);
    UNUSED (node);
    return -1;
}

static regid_t codegen_unary_expr (codegen_data_t *codegen, astnode_t *node)
{
    codegen_expr (codegen, node->as.unary_expr.operand);

    switch (node->as.unary_expr.tok_op->type)
    {
        case TOKEN_EXCLAMATION_MARK:
            stringbuffer_append (codegen->cur_txt_sect,
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
            stringbuffer_append (codegen->cur_txt_sect,
                "\tsub x0, xzr, x0\n"
            );
            break;
        case TOKEN_TILDE:
            stringbuffer_append (codegen->cur_txt_sect,
                "\tmvn x0, x0\n"
            );
            break;
        default:
            fprintf (stderr, "ERROR: unknown unary op \'%s\'\n", token_tostr (node->as.unary_expr.tok_op));
            exit (EXIT_FAILURE);
    }

    return -1;
}


static void codegenblock_addtok (stringbuffer_t *block, token_t *tok)
{
    stringbuffer_appendl (block, tok->src, tok->length);
}