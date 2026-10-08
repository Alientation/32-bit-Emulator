#pragma once

#include "util/common.h"
#include "util/types.h"

/* Tests if a bit is set. */
template<typename T>
static inline constexpr bool test_bit(T val, U8 bit_i)
{
    return ((val & (T(1) << bit_i)) >> bit_i) & 1;
}

/* Sets a bit. */
template<typename T>
static inline constexpr T set_bit(T val, U8 bit_i, U8 to)
{
    return (val & ~(T(1) << bit_i)) | (T(to) << bit_i);
}

/* Extract an unsigned bit vector. Undefined if len == 0. */
template<typename T>
static inline constexpr T bitfield_unsigned(T val, U8 bit_i, U8 len)
{
    return (val >> bit_i) & ((~U64(0)) >> (sizeof(U64) * 8 - len));
}

/* Extract a signed bit vector. The sign of the bit vector is kept. Undefined if len == 0. */
template<typename T>
static inline constexpr T bitfield_signed(T val, U8 bit_i, U8 len)
{
    return T(S64(U64(val) << (sizeof(U64) * 8 - len - bit_i)) >> (sizeof(U64) * 8 - len));
}

/* Zero out a section of bits. */
template<typename T>
static inline constexpr T mask_0(T val, U8 bit_i, U8 len)
{
    return val & ~(((~U64(0)) >> (sizeof(U64) * 8 - len)) << bit_i);
}

static constexpr U8 kNumPageOffsetBits = 12;
static constexpr U32 kPageSize = 1 << kNumPageOffsetBits;

///
/// @brief              Flag bit locations in the _pstate register.
///

/// @brief              Negative Flag.
static constexpr U8 kNFlagBit = 0;

/// @brief              Zero Flag.
static constexpr U8 kZFlagBit = 1;

/// @brief              Carry Flag.
static constexpr U8 kCFlagBit = 2;

/// @brief              Overflow Flag.
static constexpr U8 kVFlagBit = 3;

/// @brief              User mode flag.
static constexpr U8 kUserModeFlagBit = 8;

/// @brief              Real memory mode flag.
static constexpr U8 kRealModeFlagBit = 9;

/// @brief              Which bit of the instruction determines whether flags will be updated.
static constexpr U8 kInstructionUpdateFlagBit = 25;

/// @brief              Max supported instructions. 6 bits are used to represent the opcode for easy
///                     look-up table translations.
static constexpr U8 kMaxInstructions = 64;