#pragma once

#include "util/common.h"
#include "util/types.h"

#include <cassert>
#include <concepts>
#include <type_traits>

// Bit operations on the integer type of the argument (not on 64 bits), so the field has to lie
// inside T. Every function has the width of T as its limit: `bit_i < width` for a single bit and
// `len >= 1 && bit_i + len <= width` for a field. A violation is an `assert` failure (a compile
// error in a constant expression, where the shift is undefined). When the position and length are
// known at compile time, use the overloads with template arguments (`bitfield_unsigned<0, 14>
// (x)`), which reject a bad field with a static_assert and cost nothing at run time.

namespace bit_detail
{
/// Any integer type but bool, which has no unsigned counterpart to shift in.
template<typename T>
concept BitInt = std::integral<T> && !std::same_as<T, bool>;

/// The width of T in bits.
template<typename T>
inline constexpr unsigned kWidth = sizeof(T) * 8;

/// Whether a single bit position is inside T.
template<typename T>
constexpr bool bit_in_range(unsigned bit_i)
{
    return bit_i < kWidth<T>;
}

/// Whether a field of `len` bits at `bit_i` is not empty and lies inside T (written so it cannot
/// wrap).
template<typename T>
constexpr bool field_in_range(unsigned bit_i, unsigned len)
{
    return len >= 1 && len <= kWidth<T> && bit_i <= kWidth<T> - len;
}

/// The low `len` bits set, `len` in [0, width] (a shift by the full width would be undefined).
template<typename U>
constexpr U low_mask(unsigned len)
{
    return len >= kWidth<U> ? U(~U(0)) : U((U(1) << len) - 1);
}
} // namespace bit_detail

/// Tests if a bit is set.
///
/// @param val the value to look at
/// @param bit_i the position of the bit, 0 being the least significant
/// @return whether the bit is 1
template<bit_detail::BitInt T>
constexpr bool test_bit(T val, unsigned bit_i)
{
    using U = std::make_unsigned_t<T>;
    assert(bit_detail::bit_in_range<T>(bit_i));
    return (U(val) >> bit_i) & 1u;
}

/// Sets a bit to 1 if `to` is true and clears it otherwise.
///
/// @param val the value to change
/// @param bit_i the position of the bit
/// @param to the new value of the bit
/// @return the value with the bit changed
template<bit_detail::BitInt T>
constexpr T set_bit(T val, unsigned bit_i, bool to)
{
    using U = std::make_unsigned_t<T>;
    assert(bit_detail::bit_in_range<T>(bit_i));
    const U bit = U(U(1) << bit_i);
    return T((U(val) & U(~bit)) | (to ? bit : U(0)));
}

/// Extracts a field of bits as an unsigned number.
///
/// @param val the value to take the field from
/// @param bit_i the position of the lowest bit of the field
/// @param len the number of bits in the field
/// @return the field, in the low bits
template<bit_detail::BitInt T>
constexpr T bitfield_unsigned(T val, unsigned bit_i, unsigned len)
{
    using U = std::make_unsigned_t<T>;
    assert(bit_detail::field_in_range<T>(bit_i, len));
    return T((U(val) >> bit_i) & bit_detail::low_mask<U>(len));
}

/// Extracts a field of bits as a signed number: the top bit of the field is extended.
///
/// @param val the value to take the field from
/// @param bit_i the position of the lowest bit of the field
/// @param len the number of bits in the field
/// @return the sign extended field
template<bit_detail::BitInt T>
constexpr T bitfield_signed(T val, unsigned bit_i, unsigned len)
{
    using U = std::make_unsigned_t<T>;
    using S = std::make_signed_t<T>;
    assert(bit_detail::field_in_range<T>(bit_i, len));
    // Move the field to the top of T, then shift it back down arithmetically.
    const U top = U(U(val) << (bit_detail::kWidth<T> - len - bit_i));
    return T(S(top) >> (bit_detail::kWidth<T> - len));
}

/// Clears a field of bits.
///
/// @param val the value to change
/// @param bit_i the position of the lowest bit of the field
/// @param len the number of bits in the field
/// @return the value with the field set to 0
template<bit_detail::BitInt T>
constexpr T zero_bits(T val, unsigned bit_i, unsigned len)
{
    using U = std::make_unsigned_t<T>;
    assert(bit_detail::field_in_range<T>(bit_i, len));
    const U field = U(bit_detail::low_mask<U>(len) << bit_i);
    return T(U(val) & U(~field));
}

// The same operations with the position and length known at compile time: the position and
// length are template arguments (`bitfield_unsigned<0, 14>(x)`) and a field that is outside T
// fails to compile.

/// Same as test_bit(val, BitI).
template<unsigned BitI, bit_detail::BitInt T>
constexpr bool test_bit(T val)
{
    static_assert(bit_detail::bit_in_range<T>(BitI), "bit index is outside the type");
    return test_bit<T>(val, BitI);
}

/// Same as set_bit(val, BitI, to).
template<unsigned BitI, bit_detail::BitInt T>
constexpr T set_bit(T val, bool to)
{
    static_assert(bit_detail::bit_in_range<T>(BitI), "bit index is outside the type");
    return set_bit<T>(val, BitI, to);
}

/// Same as bitfield_unsigned(val, BitI, Len).
template<unsigned BitI, unsigned Len, bit_detail::BitInt T>
constexpr T bitfield_unsigned(T val)
{
    static_assert(bit_detail::field_in_range<T>(BitI, Len), "bit field is outside the type");
    return bitfield_unsigned<T>(val, BitI, Len);
}

/// Same as bitfield_signed(val, BitI, Len).
template<unsigned BitI, unsigned Len, bit_detail::BitInt T>
constexpr T bitfield_signed(T val)
{
    static_assert(bit_detail::field_in_range<T>(BitI, Len), "bit field is outside the type");
    return bitfield_signed<T>(val, BitI, Len);
}

/// Same as zero_bits(val, BitI, Len).
template<unsigned BitI, unsigned Len, bit_detail::BitInt T>
constexpr T zero_bits(T val)
{
    static_assert(bit_detail::field_in_range<T>(BitI, Len), "bit field is outside the type");
    return zero_bits<T>(val, BitI, Len);
}

/// The number of bits of an address that are the offset within a page.
static constexpr U8 kNumPageOffsetBits = 12;
/// The size of a page in bytes (4 KiB).
static constexpr U32 kPageSize = 1 << kNumPageOffsetBits;

/// Physical addresses from here up belong to the memory mapped devices (devices.h).
static constexpr word kDeviceBase = 0xF0000000;

// Bit locations in the PSTATE register.

/// Negative Flag.
static constexpr U8 kNFlagBit = 0;

/// Zero Flag.
static constexpr U8 kZFlagBit = 1;

/// Carry Flag.
static constexpr U8 kCFlagBit = 2;

/// Overflow Flag.
static constexpr U8 kVFlagBit = 3;

/// 1 in user mode, 0 in kernel (privileged) mode.
static constexpr U8 kUserModeBit = 4;

/// 1 while IRQs are masked.
static constexpr U8 kIrqMaskBit = 5;

/// The bits of PSTATE that exist: NZCV, the mode and the IRQ mask.
static constexpr word kPstateMask = 0b111111;

/// Which bit of the instruction determines whether flags will be updated.
static constexpr U8 kInstructionUpdateFlagBit = 25;

/// The bit of `mull` (format O2) that makes it the signed multiply (`smull`).
static constexpr U8 kLongMulSignedBit = 0;

/// The bit of `bx` (format B2) that makes it a call (`blx`): x29 gets the return address.
static constexpr U8 kBranchLinkBit = 0;

/// Max supported instructions. 6 bits are used to represent the opcode for easy
/// look-up table translations.
static constexpr U8 kMaxInstructions = 64;