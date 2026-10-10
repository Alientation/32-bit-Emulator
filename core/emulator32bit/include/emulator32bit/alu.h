#pragma once

#include "emulator32bit/emulator32bit_util.h"
#include "util/common.h"
#include "util/logger.h"

#include <array>

/// The registers by number. Some have a second name for what the calling convention uses them
/// for: SYSCALL (x8), FP (x28) and LR (x29).
enum class Register : U8
{
    X0 = 0,
    X1 = 1,
    X2 = 2,
    X3 = 3,
    X4 = 4,
    X5 = 5,
    X6 = 6,
    X7 = 7,
    X8 = 8,
    SYSCALL = 8,
    X9 = 9,
    X10 = 10,
    X11 = 11,
    X12 = 12,
    X13 = 13,
    X14 = 14,
    X15 = 15,
    X16 = 16,
    X17 = 17,
    X18 = 18,
    X19 = 19,
    X20 = 20,
    X21 = 21,
    X22 = 22,
    X23 = 23,
    X24 = 24,
    X25 = 25,
    X26 = 26,
    X27 = 27,
    X28 = 28,
    FP = 28,
    X29 = 29,
    LR = 29,
    SP = 30,
    XZR = 31,
    NUM_REG,
};

/// Number of registers: x0-x29, sp and xzr.
inline constexpr U8 kNumReg = static_cast<U8>(Register::NUM_REG);
static_assert(kNumReg == 32);

/// @param reg the register
/// @return the number of the register
constexpr inline U8 register_to_U8(Register reg)
{
    return static_cast<U8>(reg);
}

/// The condition of a conditional instruction, the 4 bits that are compared with the flags.
enum class ConditionCode : U8
{
    /// Equal                           : Z==1
    EQ = 0,

    /// Not Equal                       : Z==0
    NE = 1,

    /// Unsigned higher or same         : C==1
    CS = 2,
    HS = 2,

    /// Unsigned lower                  : C==0
    CC = 3,
    LO = 3,

    /// Negative                        : N==1
    MI = 4,

    /// Nonnegative                     : N==0
    PL = 5,

    /// Signed overflow                 : V==1
    VS = 6,

    /// No signed overflow              : V==0
    VC = 7,

    /// Unsigned higher                 : C==1 && Z==0
    HI = 8,

    /// Unsigned lower or same          : C==0 || Z==0
    LS = 9,

    /// Signed greater than or equal    : N==V
    GE = 10,

    /// Signed less than                : N!=V
    LT = 11,

    /// Signed greater than             : Z==0 && N==V
    GT = 12,

    /// Signed less than or equal       : Z==1 || N!=V
    LE = 13,

    /// Always executed                 : NONE
    AL = 14,

    /// Never executed                  : NONE
    NV = 15,
};

/// Whether the condition `cond` (4 bits, so every value is a condition) holds for the flags.
///
/// @param N the negative flag
/// @param Z the zero flag
/// @param C the carry flag
/// @param V the overflow flag
/// @param cond the condition to test
/// @return whether the condition holds
constexpr bool condition_holds(const bool N, const bool Z, const bool C, const bool V,
                               const ConditionCode cond)
{
    switch (cond)
    {
    case ConditionCode::EQ:
        return Z == 1;
    case ConditionCode::NE:
        return Z == 0;
    case ConditionCode::CS:
        return C == 1;
    case ConditionCode::CC:
        return C == 0;
    case ConditionCode::MI:
        return N == 1;
    case ConditionCode::PL:
        return N == 0;
    case ConditionCode::VS:
        return V == 1;
    case ConditionCode::VC:
        return V == 0;
    case ConditionCode::HI:
        return C == 1 && Z == 0;
    case ConditionCode::LS:
        return C == 0 || Z == 1;
    case ConditionCode::GE:
        return N == V;
    case ConditionCode::LT:
        return N != V;
    case ConditionCode::GT:
        return Z == 0 && (N == V);
    case ConditionCode::LE:
        return Z == 1 || (N != V);
    case ConditionCode::AL:
        return true;
    case ConditionCode::NV:
        return false;
    }
    return false;
}

// The flags are the low four bits of PSTATE, so they index a table with the conditions that hold
// for them as a bit mask (bit `cond`). That is the whole of check_cond: no switch, no call.
static_assert(kNFlagBit == 0 && kZFlagBit == 1 && kCFlagBit == 2 && kVFlagBit == 3,
              "check_cond indexes its table with the low four bits of PSTATE");

/// Builds kConditionTable: for each combination of the four flags, a mask with bit `cond` set when
/// the condition holds.
constexpr std::array<U16, 16> make_condition_table()
{
    std::array<U16, 16> table{};
    for (unsigned flags = 0; flags < 16; flags++)
    {
        for (unsigned cond = 0; cond < 16; cond++)
        {
            if (condition_holds(test_bit<kNFlagBit>(flags), test_bit<kZFlagBit>(flags),
                                test_bit<kCFlagBit>(flags), test_bit<kVFlagBit>(flags),
                                static_cast<ConditionCode>(cond)))
            {
                table[flags] |= U16(1) << cond;
            }
        }
    }
    return table;
}

/// Indexed by the low four bits of PSTATE (NZCV).
inline constexpr std::array<U16, 16> kConditionTable = make_condition_table();

/// Whether the condition holds for the flags in PSTATE, with one table lookup.
///
/// @param pstate the PSTATE value, of which the low four bits are the flags
/// @param cond the condition, of which the low four bits are used
/// @return whether the condition holds
static inline bool check_cond(const word pstate, const U8 cond)
{
    return (kConditionTable[pstate & 0xF] >> (cond & 0xF)) & 1;
}

/// The kinds of shift: logical left, logical right, arithmetic right and rotate right.
enum class ShiftType : U8
{
    SHIFT_LSL,
    SHIFT_LSR,
    SHIFT_ASR,
    SHIFT_ROR
};

/// The condition flags: negative, zero, carry and overflow.
struct NZCVFlags
{
    bool n;
    bool z;
    bool c;
    bool v;
};

/// What an ALU operation produces: the result (64 bits wide for the long multiplies) and the
/// flags that it sets.
struct AluResult
{
    dword result;
    NZCVFlags flags;
};

/// Addition with carry. C is the carry out, V the signed overflow.
///
/// @param a the first operand
/// @param b the second operand
/// @param carry_in 1 to add one more (the carry flag for `adc`, 0 for `add`)
/// @return the sum and its flags
static inline AluResult alu_add(const word a, const word b, const bool carry_in)
{
    const U64 extended = U64(a) + U64(b) + U64(carry_in);
    const word result = word(extended);

    constexpr word kSignBit = 0x80000000U;

    const bool c = extended > 0xFFFFFFFFULL;
    const bool v = ((~(a ^ b) & (a ^ result)) & kSignBit) != 0;

    return {.result = result,
            .flags = {
                .n = (result & kSignBit) != 0,
                .z = result == 0,
                .c = c,
                .v = v,
            }};
}

/// Subtraction with borrow, `a - b - !carry_in`. C is "no borrow", the ARM convention.
///
/// @param a the first operand
/// @param b the value to subtract
/// @param carry_in 1 for `sub`, the carry flag for `sbc`
/// @return the difference and its flags
static inline AluResult alu_sub(const word a, const word b, const bool carry_in)
{
    // ARM convention:
    //   SUB: carry_in = true
    //   SBC: carry_in = previous C flag
    //
    // a - b - !carry_in == a + ~b + carry_in, so C is "no borrow" and V is the signed overflow of
    // that sum. Computing the flags on the raw ~b avoids wrapping b + 1 into the sign bit.
    return alu_add(a, ~b, carry_in);
}

/// Result of a logical operation or move. N and Z are derived from the result, C and V are kept
/// from the initial flags.
///
/// @param result the value that the operation computed
/// @param initial_flags the flags before the operation
/// @return the result with the flags
static inline AluResult alu_logic_result(const word result, const NZCVFlags initial_flags)
{
    return {
        .result = result,
        .flags =
            {
                .n = test_bit<31>(result),
                .z = result == 0,
                .c = initial_flags.c,
                .v = initial_flags.v,
            },
    };
}

/// Bitwise and: `a & b`. N and Z are those of the result, C and V are kept.
///
/// @param a the first operand
/// @param b the second operand
/// @param flags the flags before the operation
/// @return the result and its flags
static inline AluResult alu_and(const word a, const word b, const NZCVFlags flags)
{
    return alu_logic_result(a & b, flags);
}

/// Bitwise or: `a | b`. N and Z are those of the result, C and V are kept.
///
/// @param a the first operand
/// @param b the second operand
/// @param flags the flags before the operation
/// @return the result and its flags
static inline AluResult alu_orr(const word a, const word b, const NZCVFlags flags)
{
    return alu_logic_result(a | b, flags);
}

/// Bitwise exclusive or: `a ^ b`. N and Z are those of the result, C and V are kept.
///
/// @param a the first operand
/// @param b the second operand
/// @param flags the flags before the operation
/// @return the result and its flags
static inline AluResult alu_eor(const word a, const word b, const NZCVFlags flags)
{
    return alu_logic_result(a ^ b, flags);
}

/// Bit clear: `a & ~b`. N and Z are those of the result, C and V are kept.
///
/// @param a the value to clear bits of
/// @param b the bits to clear
/// @param flags the flags before the operation
/// @return the result and its flags
static inline AluResult alu_bic(const word a, const word b, const NZCVFlags flags)
{
    return alu_logic_result(a & ~b, flags);
}

/// Move: the result is `value`. N and Z are those of the result, C and V are kept.
///
/// @param value the value to move
/// @param flags the flags before the operation
/// @return the result and its flags
static inline AluResult alu_mov(const word value, const NZCVFlags flags)
{
    return alu_logic_result(value, flags);
}

/// Move not: the result is `~value`. N and Z are those of the result, C and V are kept.
///
/// @param value the value to invert
/// @param flags the flags before the operation
/// @return the result and its flags
static inline AluResult alu_mvn(const word value, const NZCVFlags flags)
{
    return alu_logic_result(~value, flags);
}

/// Multiplication, keeping the low 32 bits. N and Z are those of the result, C and V are kept.
///
/// @param a the first factor
/// @param b the second factor
/// @param flags the flags before the operation
/// @return the product and its flags
static inline AluResult alu_mul(const word a, const word b, const NZCVFlags flags)
{
    const word result = a * b;
    const word result32 = word(result);

    return {
        .result = result,
        .flags =
            {
                .n = test_bit<31>(result32),
                .z = result32 == 0,
                .c = flags.c,
                .v = flags.v,
            },
    };
}

/// Unsigned long multiplication, with the full 64 bit product. N and Z are those of the 64 bit
/// result, C and V are kept.
///
/// @param a the first factor
/// @param b the second factor
/// @param flags the flags before the operation
/// @return the product and its flags
static inline AluResult alu_umull(const word a, const word b, const NZCVFlags flags)
{
    const dword result = dword(a) * dword(b);

    return {
        .result = result,
        .flags =
            {
                .n = test_bit<63>(result),
                .z = result == 0,
                .c = flags.c,
                .v = flags.v,
            },
    };
}

/// Signed long multiplication, with the full 64 bit product. N and Z are those of the 64 bit
/// result, C and V are kept.
///
/// @param a the first factor, as a signed number
/// @param b the second factor, as a signed number
/// @param flags the flags before the operation
/// @return the product and its flags
static inline AluResult alu_smull(const word a, const word b, const NZCVFlags flags)
{
    const S64 lhs = S64(S32(a));
    const S64 rhs = S64(S32(b));
    const dword result = dword(lhs * rhs);

    return {
        .result = result,
        .flags =
            {
                .n = test_bit<63>(result),
                .z = result == 0,
                .c = flags.c,
                .v = flags.v,
            },
    };
}

/// Unsigned division. Dividing by zero gives 0 and raises nothing (the ARM A64 behavior). N and Z
/// are those of the result, C and V are kept.
///
/// @param a the dividend
/// @param b the divisor
/// @param flags the flags before the operation
/// @return the quotient and its flags
static inline AluResult alu_udiv(const word a, const word b, const NZCVFlags flags)
{
    return alu_logic_result(b == 0 ? 0 : a / b, flags);
}

/// Signed division, rounded toward zero. Dividing by zero gives 0 and raises nothing, and
/// INT_MIN / -1 wraps to INT_MIN. N and Z are those of the result, C and V are kept.
///
/// @param a the dividend, as a signed number
/// @param b the divisor, as a signed number
/// @param flags the flags before the operation
/// @return the quotient and its flags
static inline AluResult alu_sdiv(const word a, const word b, const NZCVFlags flags)
{
    const S32 numerator = S32(a);
    const S32 denominator = S32(b);
    if (denominator == 0)
    {
        return alu_logic_result(0, flags);
    }
    if (numerator == INT32_MIN && denominator == -1)
    {
        return alu_logic_result(a, flags);
    }
    return alu_logic_result(word(numerator / denominator), flags);
}

/// Performs a shift operation. N and Z are those of the result. C is the last bit shifted out, V
/// is kept. A shift by 0 leaves the value and C as they were.
///
/// @param value the value to shift
/// @param type the shift operation
/// @param shift_amt the amount to shift by, in the range [0,31]
/// @param initial_flags the flags before the operation
/// @return the shifted value and its flags
static inline AluResult alu_shift(const word value, const ShiftType type, const U8 shift_amt,
                                  const NZCVFlags initial_flags)
{
    AEMU_DCHECK(shift_amt < 32, "Expected shift amount to be [0,31], got {}.", shift_amt);

    word result = value;
    NZCVFlags flags = initial_flags;
    if (shift_amt != 0)
    {
        switch (type)
        {
        case ShiftType::SHIFT_LSL:
            result = value << shift_amt;
            flags.c = bool((value >> (32 - shift_amt)) & 1);
            break;
        case ShiftType::SHIFT_LSR:
            result = value >> shift_amt;
            flags.c = bool((value >> (shift_amt - 1)) & 1);
            break;
        case ShiftType::SHIFT_ASR:
            result = word(sword(value) >> shift_amt);
            flags.c = bool((value >> (shift_amt - 1)) & 1);
            break;
        case ShiftType::SHIFT_ROR:
            result = (value >> shift_amt) | (value << (32 - shift_amt));
            flags.c = bool((value >> (shift_amt - 1)) & 1);
            break;
        default:
            AEMU_FATAL("Invalid shift type: {}", U32(type));
        }
    }

    flags.n = test_bit<31>(result);
    flags.z = result == 0;
    return {.result = result, .flags = flags};
}