#pragma once

#include "util/common.h"
#include "util/types.h"

/* Tests if a bit is set. */
template<typename T>
static inline constexpr T test_bit(T val, U8 bit_i)
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

/* TODO: This should be moved to some header file specific to the kernel. */
static constexpr U8 kNumPageOffsetBits = 12;
static constexpr U32 kPageSize = 1 << kNumPageOffsetBits;