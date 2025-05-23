#pragma once
#ifndef CCOMPILER_H
#define CCOMPILER_H

struct CompilerOptions
{
    int n_src_files;
    const char **src_files;


};

/**
 * @brief Compiles a .c file into a .basm assembly file
 *
 * TODO add compile options, perhaps in some struct CompilerOptions
 * TODO add preprocessor stage
 *
 * @param filepath
 */
void ccompile (const char *sourcefile);


#endif /* CCOMPILER_H */