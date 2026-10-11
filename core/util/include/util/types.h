#pragma once

#include <stdint.h>
#include <string>

// Machine integer types.
using dword = uint64_t;
using word = uint32_t;
using hword = uint16_t;
using byte = uint8_t;

using sdword = int64_t;
using sword = int32_t;
using shword = int16_t;
using sbyte = int8_t;

// Explicit integer types.
using U64 = uint64_t;
using U32 = uint32_t;
using U16 = uint16_t;
using U8 = uint8_t;

using S64 = int64_t;
using S32 = int32_t;
using S16 = int16_t;
using S8 = int8_t;

/// The number of bits in each of the machine types.
inline constexpr U8 kNumDwordBits = sizeof(dword) << 3;
inline constexpr U8 kNumWordBits = sizeof(word) << 3;
inline constexpr U8 kNumHwordBits = sizeof(hword) << 3;
inline constexpr U8 kNumByteBits = sizeof(byte) << 3;

/// The hexadecimal digits of a 32 bit number and of a 64 bit one.
inline constexpr int kWordHexDigits = 8;
inline constexpr int kDwordHexDigits = 16;

/// The start of a number that is written in hexadecimal.
inline constexpr const char *kHexPrefix = "0x";

/// Writes a value in hexadecimal.
///
/// @param[in] hex The value to convert.
/// @param[in] digits The least number of hexadecimal digits: shorter values are padded with zeros,
///     a longer one has more digits.
/// @return `0x` and the digits, in lower case.
std::string to_hex_str(U64 hex, int digits = kDwordHexDigits);

/// Writes a 32 bit value in hexadecimal, with 8 digits.
///
/// @param[in] hex The value to convert.
/// @return `0x` and the digits, in lower case.
std::string to_hex_str(U32 hex);

/// Colors a number that was written by to_hex_str (): the prefix and the leading zeros are gray
/// and the other digits bold white. The last digit is never gray, even when it is a zero.
///
/// @param[in] string A number, with the prefix `0x` or without one.
/// @return The string with the escape sequences of the colors, and one that resets them at the end.
std::string color_val_str(const std::string &string);

/// Same as to_hex_str (), colored with color_val_str ().
///
/// @param[in] hex The value to convert.
/// @param[in] digits The least number of hexadecimal digits.
/// @return The colored string.
std::string to_color_hex_str(U64 hex, int digits = kDwordHexDigits);

/// Same as to_hex_str (U32), colored with color_val_str ().
///
/// @param[in] hex The value to convert.
/// @return The colored string, with 8 digits.
std::string to_color_hex_str(U32 hex);
