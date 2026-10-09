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
///   - `bx` and `blx` are `bx`, bit 0 says link.
/// The rows are kept dense, so the unused opcodes are one range at the end:
/// 0b101011..0b111111 (21 opcodes).
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
    X(vabs, 0b001001)                                                                              \
    X(vneg, 0b001010)                                                                              \
    X(vsqrt, 0b001011)                                                                             \
    X(vadd, 0b001100)                                                                              \
    X(vsub, 0b001101)                                                                              \
    X(vdiv, 0b001110)                                                                              \
    X(vmul, 0b001111)                                                                              \
    X(vcmp, 0b010000)                                                                              \
    X(vsel, 0b010001)                                                                              \
    X(vcint, 0b010010)                                                                             \
    X(vcflo, 0b010011)                                                                             \
    X(vmov, 0b010100)                                                                              \
                                                                                                   \
    X(and, 0b010101)                                                                               \
    X(orr, 0b010110)                                                                               \
    X(eor, 0b010111)                                                                               \
    X(bic, 0b011000)                                                                               \
    X(shift, 0b011001)                                                                             \
                                                                                                   \
    X(mov, 0b011010)                                                                               \
    X(mvn, 0b011011)                                                                               \
                                                                                                   \
    X(ldr, 0b011100)                                                                               \
    X(ldrb, 0b011101)                                                                              \
    X(ldrh, 0b011110)                                                                              \
    X(str, 0b011111)                                                                               \
    X(strb, 0b100000)                                                                              \
    X(strh, 0b100001)                                                                              \
                                                                                                   \
    X(udiv, 0b100010)                                                                              \
    X(sdiv, 0b100011)                                                                              \
    X(csel, 0b100100)                                                                              \
                                                                                                   \
    X(b, 0b100101)                                                                                 \
    X(bl, 0b100110)                                                                                \
    X(bx, 0b100111)                                                                                \
    X(swi, 0b101000)                                                                               \
                                                                                                   \
    X(adrp, 0b101001)                                                                              \
    X(adr, 0b101010)
