#pragma once

#include "assembler/tokenizer.h"
#include "util/types.h"

#include <string_view>

namespace basm
{

/// How the operands of an instruction are parsed and encoded.
enum class InstructionFormat
{
    O,         ///< op xd, xn, <xm[, shift] | imm14 | :lo12:symbol>.
    O_NO_DEST, ///< op xn, <operand>, the destination is xzr (cmp, cmn, tst, teq).
    O1,        ///< Shifts: op xd, xn, <xm | imm5>.
    O2,        ///< Long multiply: op xlo, xhi, xn, xm.
    O3,        ///< mov, mvn: op xd, <xm | imm | :hi13:symbol | :lo19:symbol>.
    M,         ///< Loads and stores.
    M1,        ///< adrp, adr.
    B1,        ///< b, bl: a label or an offset.
    B2,        ///< bx, blx: a register.
    SWI,       ///< swi[.cond] [number], a number of 22 bits (0 if left out).
    ATOMIC,    ///< swp and the ldadd, ldclr, ldset families.
    CSEL,      ///< csel, csinc, csinv, csneg: op xd, xn, xm, cond.
    CSET,      ///< cset, csetm: op xd, cond (xn and xm are xzr, the condition is inverted).
    CINC,      ///< cinc, cinv, cneg: op xd, xn, cond (xn is also xm, the condition is inverted).
    UNARY,     ///< sxtb, sxth, uxtb, uxth, clz, rev, rev16: op xd, xn.
    F1,        ///< Unary floating point and conversions: op.f32 xd, xn.
    F2,        ///< Binary floating point: op.f32 xd, xn, xm.
    F3,        ///< fcmp, fcmpe: op.f32 xn, xm.
    F4,        ///< Fused multiply-add: op.f32 xd, xn, xm, xa.
    FMOV,      ///< fmov.f32 xd, <xm | float literal>, a pseudo instruction.
    HLT,       ///< hlt
    NOP,       ///< nop
    TLBI,      ///< tlbi [xn]: forget the translation of the page of xn, or all of them.
    ERET,      ///< eret
    WFI,       ///< wfi
    BRK,       ///< brk [number], a number of 22 bits (0 if left out).
    MSR,       ///< msr sysreg, <xn | imm16>.
    MRS,       ///< mrs xd, sysreg
    RET,       ///< ret
};

/// One row of instruction_list.h.
struct InstructionSpec
{
    TokenType token;

    /// What is written in assembly.
    std::string_view text;
    InstructionFormat format;

    /// The opcode, or for ATOMIC the width.
    byte a;

    /// For ATOMIC the operation.
    byte b;
};

/// @param type The type of an instruction token: `is_instruction (type)` must hold.
/// @return The row of the instruction.
const InstructionSpec &instruction_spec(TokenType type);

} // namespace basm
