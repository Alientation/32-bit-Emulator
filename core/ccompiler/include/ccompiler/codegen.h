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

typedef struct CodegenBlock
{
    char *code;
    int length;
    int capacity;
} codegen_block_t;

typedef struct CodegenData
{
    parser_data_t *parser;
    FILE *output_file;

    codegen_block_t glob_sym_decl;
    codegen_block_t txt_sect;
} codegen_data_t;

void codegen (parser_data_t *parser, const char *output_filepath);

#endif /* CODEGEN_H */