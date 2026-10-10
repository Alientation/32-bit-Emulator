#pragma once

/// The single list of the CPU's primary opcodes (the 6 bit opcode field of an instruction).
///
/// Everything that is indexed by opcode is generated from this list, so adding an instruction is
/// one new row here (plus its behavior):
///   - the handler declaration `_<name>(word)` and the constant `Emulator32bit::_op_<name>`
///     (emulator32bit.h),
///   - the dispatch switch (Emulator32bit::execute),
///   - the disassembler table (disassembler.cpp, which needs a `disassemble_<name>`).
///
/// Some instructions have no row of their own:
///   - `cmp`, `cmn`, `tst` and `teq` are `subs`, `adds`, `ands` and `eors` with `xzr` as the
///     destination (the assembler writes them that way and the disassembler names them),
///   - `lsl`, `lsr`, `asr` and `ror` are `shift`, the type is in bits 7-8,
///   - `umull` and `smull` are `mull`, bit 0 says signed,
///   - `bx` and `blx` are `bx`, bit 0 says link,
///   - the floating point instructions are `fop1` (unary and conversions), `fop2` (binary),
///     `fcmp` and `fop3` (fused multiply-add); the function is in bits 4-0 (bits 1-0 for `fop3`)
///     and the precision in bit 25 (docs/isa.md).
/// The rows are kept dense, so the unused opcodes are one range at the end:
/// 0b100011..0b111111 (29 opcodes).
///
/// Opcodes that are not listed fault with BAD_INSTR. A compile time check in emulator32bit.cpp
/// makes sure that no two rows share an opcode and that all of them fit in 6 bits.
///
/// X(name, opcode)
#define AEMU_OPCODES(X)                                                                            \
    X(special_instructions, 0b000000)                                                              \
                                                                                                   \
    X(add, 0b000001)                                                                               \
    X(sub, 0b000010)                                                                               \
    X(rsb, 0b000011)                                                                               \
    X(adc, 0b000100)                                                                               \
    X(sbc, 0b000101)                                                                               \
    X(rsc, 0b000110)                                                                               \
    X(mul, 0b000111)                                                                               \
    X(mull, 0b001000)                                                                              \
                                                                                                   \
    X(fop1, 0b001001)                                                                              \
    X(fop2, 0b001010)                                                                              \
    X(fcmp, 0b001011)                                                                              \
                                                                                                   \
    X(and, 0b001100)                                                                               \
    X(orr, 0b001101)                                                                               \
    X(eor, 0b001110)                                                                               \
    X(bic, 0b001111)                                                                               \
    X(shift, 0b010000)                                                                             \
                                                                                                   \
    X(mov, 0b010001)                                                                               \
    X(mvn, 0b010010)                                                                               \
                                                                                                   \
    X(ldr, 0b010011)                                                                               \
    X(ldrb, 0b010100)                                                                              \
    X(ldrh, 0b010101)                                                                              \
    X(str, 0b010110)                                                                               \
    X(strb, 0b010111)                                                                              \
    X(strh, 0b011000)                                                                              \
                                                                                                   \
    X(udiv, 0b011001)                                                                              \
    X(sdiv, 0b011010)                                                                              \
    X(csel, 0b011011)                                                                              \
                                                                                                   \
    X(b, 0b011100)                                                                                 \
    X(bl, 0b011101)                                                                                \
    X(bx, 0b011110)                                                                                \
    X(swi, 0b011111)                                                                               \
                                                                                                   \
    X(adrp, 0b100000)                                                                              \
    X(adr, 0b100001)                                                                               \
    X(fop3, 0b100010)
