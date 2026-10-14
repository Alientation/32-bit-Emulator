#ifndef CCOMPILER_CODEGEN_H
#define CCOMPILER_CODEGEN_H

#include "ccompiler/parse.h"

#include <stdio.h>

/// Writes the functions of a file as basm assembly.
///
/// The code follows docs/abi.md: a frame record, results in `x0`, and every expression is
/// evaluated into `x0` with the operands that wait pushed on the stack. A function `name` is the
/// global label `_name`: a C name can be an instruction, a register or a condition in basm
/// (`add`, `sp`, `eq`), which a label cannot be, and no keyword starts with `_`. The labels the
/// compiler makes itself are `L<number>`, which no C name can collide with.
///
/// @param[in] prog The functions, see parse().
/// @param[out] out The stream the assembly is written to.
void codegen(const Function *prog, FILE *out);

#endif
