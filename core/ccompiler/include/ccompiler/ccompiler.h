#pragma once
#ifndef CCOMPILER_H
#define CCOMPILER_H

static const int CCOMPILER_FAILURE__BAD_SOURCE_FILE = 16;

/**
 * @brief Compiles a .c file into a .basm assembly file
 *
 * TODO add compile options, perhaps in some struct CompilerOptions
 * TODO add preprocessor stage
 *
 * @param filepath
 */
int ccompile (const char *sourcefile);


#endif /* CCOMPILER_H */