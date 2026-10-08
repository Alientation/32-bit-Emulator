#pragma once

#include "assembler/object_file.h"

/// Patches `instr`, which is located at `instr_address`, so that it refers to `target`, and returns
/// the patched instruction. This is the one place that knows how each relocation type is encoded.
/// For a relocation in .data (R_EMU32_ABS32) `instr` is the word that is there.
///
/// `instr_address` and `target` must be in the same address space. The assembler passes section
/// relative offsets (for branches inside one file), the linker passes final addresses.
///
/// Fatal if the value cannot be encoded (a misaligned or too far branch target).
word apply_relocation(ObjectFile::RelocationEntry::Type type, word instr, word instr_address,
                      word target);
