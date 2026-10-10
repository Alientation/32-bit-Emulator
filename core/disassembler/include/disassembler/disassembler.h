#pragma once

#include "util/types.h"

#include <string>

/// The disassembler of the instruction set (docs/isa.md). It reads an instruction word and needs
/// nothing of the machine but the numbers of the encoding (emulator32bit/encoding.h), so the
/// emulator (the trace, the debugger) and the assembler (the listing of `basm -dump`) use it
/// without it knowing either.
namespace disassembler
{

/// @param instr an instruction word
/// @return the instruction as assembly text, as the assembler would read it (a pseudo operation
///         like `cmp` is named as such). A word that is no instruction is shown as a `nop`.
std::string disassemble(word instr);

} // namespace disassembler
