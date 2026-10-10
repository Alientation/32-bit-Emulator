#pragma once

/// The arithmetic of the floating point instructions (docs/isa.md, "Floating point").
///
/// The operations are done by the FPU of the host, one instruction at a time, with the rounding
/// mode of the emulated FPCR and the exception flags it raises collected into an FPSR value.
/// A float or a double travels as its bit pattern (`U64`; a float is in the low 32 bits).
///
/// Two things are fixed by the emulator instead of being left to the host, so that a program
/// gives the same result on every machine:
///   - a NaN that an operation produces is always the default NaN (quiet, positive, empty
///     payload): 0x7FC00000 or 0x7FF8000000000000. `abs`, `neg` and the moves are bit operations
///     and leave a NaN as it is,
///   - converting a float to an integer saturates (a NaN gives 0) and raises Invalid.
/// What stays with the host is the detection of tininess for Underflow (before or after
/// rounding), which IEEE 754 leaves open.

#include "util/types.h"

namespace fpu
{

/// FPCR: the rounding mode, bits 1-0.
constexpr word kRoundNearest = 0; ///< to nearest, ties to even
constexpr word kRoundUp = 1;      ///< toward +infinity
constexpr word kRoundDown = 2;    ///< toward -infinity
constexpr word kRoundZero = 3;    ///< toward zero
constexpr word kFpcrMask = 0b11;

/// FPSR: the cumulative exception flags, bits 4-0. They are set by an operation and only cleared
/// by writing the register.
constexpr word kInvalid = 1u << 0;
constexpr word kDivByZero = 1u << 1;
constexpr word kOverflow = 1u << 2;
constexpr word kUnderflow = 1u << 3;
constexpr word kInexact = 1u << 4;
constexpr word kFpsrMask = 0b11111;

/// The function field (bits 4-0) of `fop2`.
constexpr U8 kBinaryFn_add = 0;
constexpr U8 kBinaryFn_sub = 1;
constexpr U8 kBinaryFn_mul = 2;
constexpr U8 kBinaryFn_div = 3;
constexpr U8 kBinaryFn_min = 4;
constexpr U8 kBinaryFn_max = 5;
constexpr U8 kBinaryFn_count = 6;

/// The function field (bits 4-0) of `fop1`.
constexpr U8 kUnaryFn_abs = 0;
constexpr U8 kUnaryFn_neg = 1;
constexpr U8 kUnaryFn_sqrt = 2;
constexpr U8 kUnaryFn_rint = 3;  ///< round to an integral value, in the mode of the FPCR
constexpr U8 kUnaryFn_rintz = 4; ///< toward zero
constexpr U8 kUnaryFn_rintm = 5; ///< toward -infinity (floor)
constexpr U8 kUnaryFn_rintp = 6; ///< toward +infinity (ceil)
constexpr U8 kUnaryFn_rinta = 7; ///< to nearest, ties away from zero
constexpr U8 kUnaryFn_tos32 = 8;   ///< float to int32, toward zero (the C cast)
constexpr U8 kUnaryFn_tou32 = 9;   ///< float to uint32, toward zero
constexpr U8 kUnaryFn_tos32r = 10; ///< float to int32, in the mode of the FPCR
constexpr U8 kUnaryFn_tou32r = 11; ///< float to uint32, in the mode of the FPCR
constexpr U8 kUnaryFn_froms32 = 12; ///< int32 to float
constexpr U8 kUnaryFn_fromu32 = 13; ///< uint32 to float
constexpr U8 kUnaryFn_fcvt = 14;    ///< float to the other precision
constexpr U8 kUnaryFn_count = 15;

/// @param fn a kUnaryFn_*
/// @param dbl the precision bit of the instruction: the float operand is a double. For
///        kUnaryFn_fcvt it is the precision of the source.
/// @return whether the destination is a register pair (a double)
constexpr bool unary_dest_is_pair(U8 fn, bool dbl)
{
    if (fn >= kUnaryFn_tos32 && fn <= kUnaryFn_tou32r)
    {
        return false;
    }
    return fn == kUnaryFn_fcvt ? !dbl : dbl;
}

/// @return whether the source is a register pair (a double), see unary_dest_is_pair
constexpr bool unary_source_is_pair(U8 fn, bool dbl)
{
    if (fn == kUnaryFn_froms32 || fn == kUnaryFn_fromu32)
    {
        return false;
    }
    return dbl;
}

/// The result of an operation: the value (a bit pattern, an integer or NZCV) and the exception
/// flags (kInvalid...) it raised.
struct Result
{
    U64 value;
    word flags;
};

/// `fop2`.
///
/// @param fn a kBinaryFn_* below kBinaryFn_count
/// @param dbl whether the operands and the result are doubles
/// @param rounding the mode of the FPCR (kRound*)
Result binary(U8 fn, bool dbl, U64 a, U64 b, word rounding);

/// `fop1`: @p a is the source in the representation of its type, the result the destination in
/// the representation of its type (a float, a double, or the 32 bits of an integer).
///
/// @param fn a kUnaryFn_* below kUnaryFn_count
/// @param dbl see unary_dest_is_pair
/// @param rounding the mode of the FPCR (kRound*)
Result unary(U8 fn, bool dbl, U64 a, word rounding);

/// `fcmp`: the value of the result is NZCV in bits 3-0 (N is bit 3), as the flags are set by a
/// comparison of integers. Equal is Z and C, less is N, greater is C and unordered is C and V.
///
/// @param signaling whether any NaN raises Invalid (`fcmpe`); otherwise only a signaling one does
Result compare(bool dbl, bool signaling, U64 a, U64 b);

} // namespace fpu
