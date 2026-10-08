#pragma once

#include "assembler/tokenizer.h"
#include "util/types.h"

#include <string_view>

namespace basm
{

/// How the operands of an instruction are parsed and encoded.
enum class InstructionFormat
{
    O,         ///< op xd, xn, <xm[, shift] | imm14 | :lo12:symbol>
    O_NO_DEST, ///< op xn, <operand>, the destination is xzr (cmp, cmn, tst, teq)
    O1,        ///< shifts: op xd, xn, <xm | imm5>
    O2,        ///< long multiply: op xlo, xhi, xn, xm
    O3,        ///< mov, mvn: op xd, <xm | imm | :hi13:symbol | :lo19:symbol>
    M,         ///< loads and stores
    M1,        ///< adrp
    B1,        ///< b, bl: a label or an offset
    B2,        ///< bx, blx: a register
    SWI,       ///< swi[.cond] [number], a number of 22 bits (0 if left out)
    ATOMIC,    ///< swp and the ldadd, ldclr, ldset families
    CSEL,      ///< csel, csinc, csinv, csneg: op xd, xn, xm, cond
    CSET,      ///< cset, csetm: op xd, cond (xn and xm are xzr, the condition is inverted)
    CINC,      ///< cinc, cinv, cneg: op xd, xn, cond (xn is also xm, the condition is inverted)
    UNARY,     ///< sxtb, sxth, uxtb, uxth, clz, rev, rev16: op xd, xn
    HLT,
    NOP,
    ERET,
    WFI,
    BRK, ///< brk [number], a number of 22 bits (0 if left out)
    MSR,
    MRS,
    RET,
    UNIMPLEMENTED, ///< recognized, but fails to assemble
};

/// One row of instruction_list.h.
struct InstructionSpec
{
    TokenType token;
    std::string_view text;
    InstructionFormat format;

    /// The opcode, or for ATOMIC the width.
    byte a;

    /// For ATOMIC the operation.
    byte b;
};

/// The row of an instruction token. `is_instruction(type)` must hold.
const InstructionSpec &instruction_spec(TokenType type);

} // namespace basm
