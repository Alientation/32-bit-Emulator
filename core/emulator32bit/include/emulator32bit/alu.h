#pragma once

#include "emulator32bit/emulator32bit_util.h"
#include "util/logger.h"
#include <util/common.h>

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

///
/// @brief                  IDs for special registers
///
/// Stack grows downwards
/// <--------STACK_TOP-------->
///          Saved FP
///          Saved LR                <--- fp
///  ---STACK_FRAME_BORDER---
///      local variables
///          <...>
///          <...>
///          <...>
///      local variables             <---- sp
///
/// Link register stores the previous pc, the next instruction is what will be
/// executed.
///
/// Register Conventions
///  - x0-x17: Caller Saved
///     - x0-x7: Parameter Registers
///     - x0: Return value
///     - x8: Syscall Number
///  - x19-27: Callee Saved
///  - x28: Frame Register
///  - x29: Link Register
///

/// @brief              Number of general purpose stack registers.
static constexpr U8 kNumReg = static_cast<U8>(Register::NUM_REG);
static_assert(kNumReg == 32);

static constexpr inline U8 register_to_U8(Register reg)
{
    return static_cast<U8>(reg);
}

enum class ConditionCode : U8
{
    /// @brief          Equal                           : Z==1
    EQ = 0,

    /// @brief          Not Equal                       : Z==0
    NE = 1,

    /// @brief          Unsigned higher or same         : C==1
    CS = 2,
    HS = 2,

    /// @brief          Unsigned lower                  : C==0
    CC = 3,
    LO = 3,

    /// @brief          Negative                        : N==1
    MI = 4,

    /// @brief          Nonnegative                     : N==0
    PL = 5,

    /// @brief          Signed overflow                 : V==1
    VS = 6,

    /// @brief          No signed overflow              : V==0
    VC = 7,

    /// @brief          Unsigned higher                 : C==1 && Z==0
    HI = 8,

    /// @brief          Unsigned lower or same          : C==0 || Z==0
    LS = 9,

    /// @brief          Signed greater than or equal    : N==V
    GE = 10,

    /// @brief          Signed less than                : N!=V
    LT = 11,

    /// @brief          Signed greater than             : Z==0 && N==V
    GT = 12,

    /// @brief          Signed less than or equal       : Z==1 || N!=V
    LE = 13,

    /// @brief          Always executed                 : NONE
    AL = 14,

    /// @brief          Never executed                  : NONE
    NV = 15,
};

static inline bool check_cond(word pstate, U8 cond)
{
    const bool N = test_bit(pstate, kNFlagBit);
    const bool Z = test_bit(pstate, kZFlagBit);
    const bool C = test_bit(pstate, kCFlagBit);
    const bool V = test_bit(pstate, kVFlagBit);

    switch (static_cast<ConditionCode>(cond))
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

    AEMU_FATAL("Unknown condition code: {}", U32(cond));
    return false;
}

enum class ShiftType : U8
{
    SHIFT_LSL,
    SHIFT_LSR,
    SHIFT_ASR,
    SHIFT_ROR
};

struct NZCVFlags
{
    bool n;
    bool z;
    bool c;
    bool v;
};

struct AluResult
{
    dword result;
    NZCVFlags flags;
};

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

/**
 * Result of a logical operation or move. N and Z are derived from the result, C and V are kept
 * from the initial flags.
 */
static inline AluResult alu_logic_result(const word result, const NZCVFlags initial_flags)
{
    return {
        .result = result,
        .flags =
            {
                .n = test_bit(result, 31),
                .z = result == 0,
                .c = initial_flags.c,
                .v = initial_flags.v,
            },
    };
}

static inline AluResult alu_and(const word a, const word b, const NZCVFlags flags)
{
    return alu_logic_result(a & b, flags);
}

static inline AluResult alu_orr(const word a, const word b, const NZCVFlags flags)
{
    return alu_logic_result(a | b, flags);
}

static inline AluResult alu_eor(const word a, const word b, const NZCVFlags flags)
{
    return alu_logic_result(a ^ b, flags);
}

/// Bit clear: a & ~b.
static inline AluResult alu_bic(const word a, const word b, const NZCVFlags flags)
{
    return alu_logic_result(a & ~b, flags);
}

static inline AluResult alu_mov(const word value, const NZCVFlags flags)
{
    return alu_logic_result(value, flags);
}

static inline AluResult alu_mvn(const word value, const NZCVFlags flags)
{
    return alu_logic_result(~value, flags);
}

static inline AluResult alu_mul(const word a, const word b, const NZCVFlags flags)
{
    const word result = a * b;
    const word result32 = word(result);

    return {
        .result = result,
        .flags =
            {
                .n = test_bit(result32, 31),
                .z = result32 == 0,
                .c = flags.c,
                .v = flags.v,
            },
    };
}

static inline AluResult alu_umull(const word a, const word b, const NZCVFlags flags)
{
    const dword result = dword(a) * dword(b);

    return {
        .result = result,
        .flags =
            {
                .n = test_bit(result, 63),
                .z = result == 0,
                .c = flags.c,
                .v = flags.v,
            },
    };
}

static inline AluResult alu_smull(const word a, const word b, const NZCVFlags flags)
{
    const S64 lhs = S64(S32(a));
    const S64 rhs = S64(S32(b));
    const dword result = dword(lhs * rhs);

    return {
        .result = result,
        .flags =
            {
                .n = test_bit(result, 63),
                .z = result == 0,
                .c = flags.c,
                .v = flags.v,
            },
    };
}

/// Unsigned division. Dividing by zero gives 0 and raises nothing (the ARM A64 behavior). N and Z
/// are those of the result, C and V are kept.
static inline AluResult alu_udiv(const word a, const word b, const NZCVFlags flags)
{
    return alu_logic_result(b == 0 ? 0 : a / b, flags);
}

/// Signed division, rounded toward zero. Dividing by zero gives 0 and raises nothing, and
/// INT_MIN / -1 wraps to INT_MIN. N and Z are those of the result, C and V are kept.
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

/**
 * Perform a shift operation.
 * @param value Value to shift.
 * @param type Shift operation.
 * @param shift_amt Amount to shift by. Must be in the range [0,31].
 * @param carry_in Carry in
 */
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

    flags.n = test_bit(result, 31);
    flags.z = result == 0;
    return {.result = result, .flags = flags};
}