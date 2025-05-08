#pragma once
#ifndef CODEGEN_H
#define CODEGEN_H

#include "ccompiler/lexer.h"
#include "ccompiler/parser.h"
#include "ccompiler/stringbuffer.h"

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

#define CODEGEN_NUM_CALLER_REGS 18
#define CODEGEN_NUM_CALLEE_REGS 9

typedef int regid_t;

typedef struct CodegenReg
{
    const char *name;
    bool alloc;
    int stack_offset;
} codegen_reg_t;

typedef struct CodegenFunc
{
    astnode_t *func;
    stringbuffer_t body;

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
    codegen_reg_t caller_saved_regs[CODEGEN_NUM_CALLER_REGS]; // x0-x17

    // push to stack when allocated, pop off when deallocated
    codegen_reg_t callee_saved_regs[CODEGEN_NUM_CALLEE_REGS]; // x19-x27
} codegen_func_t;


typedef struct CodegenData
{
    parser_data_t *parser;
    FILE *output_file;

    stringbuffer_t glob_sym_decl;
    stringbuffer_t txt_sect;

    codegen_func_t *cur_func;
    stringbuffer_t *cur_txt_sect;
} codegen_data_t;

void codegen (parser_data_t *parser, const char *output_filepath);

#endif /* CODEGEN_H */