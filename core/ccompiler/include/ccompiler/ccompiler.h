#pragma once
#ifndef CCOMPILER_H
#define CCOMPILER_H


/**
 * @brief Compiles a .c file into a .basm assembly file
 *
 * TODO add compile options, perhaps in some struct CompilerOptions
 * TODO add preprocessor stage
 *
 * @param filepath
 */
char* ccompile (const char *sourcefile);


#endif /* CCOMPILER_H */