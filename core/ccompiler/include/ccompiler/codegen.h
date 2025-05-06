#pragma once
#ifndef CODEGEN_H
#define CODEGEN_H

#include <ccompiler/lexer.h>
#include <ccompiler/parser.h>

#include <stdio.h>


/*
example.c
int main ()
{
    return 1;
}

=> example.basm

.global main

.text
main:
        mov x0, 1
        ret
*/

static const int N_CALLER_REGS = 18;
static const int N_CALLEE_REGS = 9;

typedef struct CodegenBlock
{
    char *code;
    int length;
    int capacity;
} codegen_block_t;

typedef struct CodegenReg
{
    const char *name;
    bool alloc;
    int stack_offset;
} codegen_reg_t;

typedef struct CodegenFunc
{
    astnode_t *func;
    codegen_block_t prologue;   // code to set up stack (store LR, FP)
    codegen_block_t body;
    codegen_block_t epilogue;   // code to pop off stack (load LR, FP)

    /*
        HIGH ADDR
        |-----|
        |/ / /| <- alignment/padding
        |/ / /| <- saved registers
        |/ / /|  |
        |/ / /|  L
        |/ / /| <- local variables
        |/ / /|  |
        |/ / /|  L
        |/ / /| <- LR
        |/ / /| <- FP
        |-----| <- SP
        | . . |
        | . . |
        |-----|
        LO ADDR
    */
    int stack_used;
    int stack_capacity;     // max stack space required by the function

    // push allocated to stack before function call, pop off after
    codegen_reg_t caller_saved_regs[N_CALLER_REGS]; // x0-x17

    // push to stack when allocated, pop off when deallocated
    codegen_reg_t callee_saved_regs[N_CALLEE_REGS]; // x19-x27
} codegen_func_t;


typedef struct CodegenData
{
    parser_data_t *parser;
    FILE *output_file;

    codegen_block_t glob_sym_decl;
    codegen_block_t txt_sect;

    codegen_func_t *cur_func;
} codegen_data_t;

void codegen (parser_data_t *parser, const char *output_filepath);

#endif /* CODEGEN_H */