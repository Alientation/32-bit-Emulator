#pragma once

#include <stddef.h>

// TODO:
// - Lexer
//      - Better error message system
//          - Print whole line with arrow to the offending character
//          - Have a similar system for warnings/logs
//      - Preprocessor
//          - Directives
//          - Macros


/**
 * @brief Compiles a .c file into a .basm assembly file
 *
 * @warning THIS IS W.I.P.
 *
 * TODO add compile options, perhaps in some struct CompilerOptions
 * TODO add preprocessor stage
 *
 * @param filepath
 */
void ccompile (const char *sourcefile);