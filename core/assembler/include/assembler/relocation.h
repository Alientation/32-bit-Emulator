#pragma once

#include "assembler/object_file.h"

/// Patches `instr`, which is located at `instr_address`, so that it refers to `target`, and returns
/// the patched instruction. This is the one place that knows how each relocation type is encoded.
///
/// `instr_address` and `target` must be in the same address space. The assembler passes section
/// relative offsets (for branches inside one file), the linker passes final addresses.
///
/// Fatal if the value cannot be encoded (a branch target that is not a multiple of 4 or is too
/// far, an `adr` that cannot reach).
///
/// @param[in] type How the instruction refers to the target.
/// @param[in] instr The instruction. For a relocation in a data section (R_EMU32_ABS32) it is the
///     word that is there.
/// @param[in] instr_address Where the instruction is.
/// @param[in] target The address that the instruction has to refer to, plus the addend.
/// @return The patched instruction.
word apply_relocation(ObjectFile::RelocationEntry::Type type, word instr, word instr_address,
                      word target);
