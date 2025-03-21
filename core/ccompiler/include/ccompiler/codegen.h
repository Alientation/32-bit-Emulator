#pragma once
#ifndef CODEGEN_H

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

struct CodegenBlock
{
    char *code;
    int length;
    int capacity;
};

struct CodegenData
{
    struct ParserData *parser;
    FILE *output_file;

    struct CodegenBlock glob_sym_decl;
    struct CodegenBlock txt_sect;
};

void codegen (struct ParserData *parser, const char *output_filepath);

#endif /* CODEGEN_H */