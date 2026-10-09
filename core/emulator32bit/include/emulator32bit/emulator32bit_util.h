#pragma once

#include "util/common.h"
#include "util/types.h"

#include <cassert>
#include <concepts>
#include <type_traits>

/*
 * Bit operations on the integer type of the argument (not on 64 bits), so the field has to lie
 * inside T. Every function has the width of T as its limit: `bit_i < width` for a single bit and
 * `len >= 1 && bit_i + len <= width` for a field. A violation is an `assert` failure (a compile
 * error in a constant expression, where the shift is undefined). When the position and length are
 * known at compile time, use the overloads with template arguments (`bitfield_unsigned<0, 14> (x)`),
 * which reject a bad field with a static_assert and cost nothing at run time.
 */

namespace bit_detail
{
/* Any integer type but bool, which has no unsigned counterpart to shift in. */
template<typename T>
concept BitInt = std::integral<T> && !std::same_as<T, bool>;

template<typename T>
inline constexpr unsigned kWidth = sizeof(T) * 8;

/* A single bit position is inside T. */
template<typename T>
constexpr bool bit_in_range(unsigned bit_i)
{
    return bit_i < kWidth<T>;
}

/* A field of `len` bits at `bit_i` is not empty and lies inside T (written so it cannot wrap). */
template<typename T>
constexpr bool field_in_range(unsigned bit_i, unsigned len)
{
    return len >= 1 && len <= kWidth<T> && bit_i <= kWidth<T> - len;
}

/* The low `len` bits set, len in [0, width] (a shift by the full width would be undefined). */
template<typename U>
constexpr U low_mask(unsigned len)
{
    return len >= kWidth<U> ? U(~U(0)) : U((U(1) << len) - 1);
}
} // namespace bit_detail

/* Tests if a bit is set. */
template<bit_detail::BitInt T>
constexpr bool test_bit(T val, unsigned bit_i)
{
    using U = std::make_unsigned_t<T>;
    assert(bit_detail::bit_in_range<T>(bit_i));
    return (U(val) >> bit_i) & 1u;
}

/* Sets a bit to 1 if `to` is true and clears it otherwise. */
template<bit_detail::BitInt T>
constexpr T set_bit(T val, unsigned bit_i, bool to)
{
    using U = std::make_unsigned_t<T>;
    assert(bit_detail::bit_in_range<T>(bit_i));
    const U bit = U(U(1) << bit_i);
    return T((U(val) & U(~bit)) | (to ? bit : U(0)));
}

/* Extract an unsigned bit vector. */
template<bit_detail::BitInt T>
constexpr T bitfield_unsigned(T val, unsigned bit_i, unsigned len)
{
    using U = std::make_unsigned_t<T>;
    assert(bit_detail::field_in_range<T>(bit_i, len));
    return T((U(val) >> bit_i) & bit_detail::low_mask<U>(len));
}

/* Extract a signed bit vector. The sign of the bit vector is kept. */
template<bit_detail::BitInt T>
constexpr T bitfield_signed(T val, unsigned bit_i, unsigned len)
{
    using U = std::make_unsigned_t<T>;
    using S = std::make_signed_t<T>;
    assert(bit_detail::field_in_range<T>(bit_i, len));
    /* Move the field to the top of T, then shift it back down arithmetically. */
    const U top = U(U(val) << (bit_detail::kWidth<T> - len - bit_i));
    return T(S(top) >> (bit_detail::kWidth<T> - len));
}

/* Zero out a section of bits. */
template<bit_detail::BitInt T>
constexpr T zero_bits(T val, unsigned bit_i, unsigned len)
{
    using U = std::make_unsigned_t<T>;
    assert(bit_detail::field_in_range<T>(bit_i, len));
    const U field = U(bit_detail::low_mask<U>(len) << bit_i);
    return T(U(val) & U(~field));
}

/* The same operations with the position and length known at compile time. */
template<unsigned BitI, bit_detail::BitInt T>
constexpr bool test_bit(T val)
{
    static_assert(bit_detail::bit_in_range<T>(BitI), "bit index is outside the type");
    return test_bit<T>(val, BitI);
}

template<unsigned BitI, bit_detail::BitInt T>
constexpr T set_bit(T val, bool to)
{
    static_assert(bit_detail::bit_in_range<T>(BitI), "bit index is outside the type");
    return set_bit<T>(val, BitI, to);
}

template<unsigned BitI, unsigned Len, bit_detail::BitInt T>
constexpr T bitfield_unsigned(T val)
{
    static_assert(bit_detail::field_in_range<T>(BitI, Len), "bit field is outside the type");
    return bitfield_unsigned<T>(val, BitI, Len);
}

template<unsigned BitI, unsigned Len, bit_detail::BitInt T>
constexpr T bitfield_signed(T val)
{
    static_assert(bit_detail::field_in_range<T>(BitI, Len), "bit field is outside the type");
    return bitfield_signed<T>(val, BitI, Len);
}

template<unsigned BitI, unsigned Len, bit_detail::BitInt T>
constexpr T zero_bits(T val)
{
    static_assert(bit_detail::field_in_range<T>(BitI, Len), "bit field is outside the type");
    return zero_bits<T>(val, BitI, Len);
}

static constexpr U8 kNumPageOffsetBits = 12;
static constexpr U32 kPageSize = 1 << kNumPageOffsetBits;

/// Physical addresses from here up belong to the memory mapped devices (devices.h).
static constexpr word kDeviceBase = 0xF0000000;

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

/// @brief              1 in user mode, 0 in kernel (privileged) mode.
static constexpr U8 kUserModeBit = 4;

/// @brief              1 while IRQs are masked.
static constexpr U8 kIrqMaskBit = 5;

/// @brief              The bits of PSTATE that exist: NZCV, the mode and the IRQ mask.
static constexpr word kPstateMask = 0b111111;

/// @brief              Which bit of the instruction determines whether flags will be updated.
static constexpr U8 kInstructionUpdateFlagBit = 25;

/// @brief              Max supported instructions. 6 bits are used to represent the opcode for easy
///                     look-up table translations.
static constexpr U8 kMaxInstructions = 64;