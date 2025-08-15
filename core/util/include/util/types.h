#pragma once
#ifndef TYPES_H
#define TYPES_H

#include <bitset>
#include <iostream>
#include <iomanip>
#include <util/console_color.h>
#include <sstream>
#include <stdint.h>

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

static constexpr U8 kNumDwordBits = sizeof(dword) << 3;
static constexpr U8 kNumWordBits = sizeof(word) << 3;
static constexpr U8 kNumHwordBits = sizeof(hword) << 3;
static constexpr U8 kNumByteBits = sizeof(byte) << 3;

static constexpr const char *kBinaryPrefix = "0b";
static constexpr const char *kHexPrefix = "0x";

/**
 * Converts a value into hexadecimal string representation
 *
 * @param hex the value to convert
 * @param digits the number of hexadecimal digits to output
 * @return the hexadecimal string representation of the value
 */
std::string to_hex_str(U64 hex, int digits = 16);

/**
 * Converts a 32 bit value into hexadecimal string representation
 *
 * @param hex the value to convert
 * @return the hexadecimal string representation of the value
 */
std::string to_hex_str(U32 hex);

/**
 * Converts a 16 bit value into hexadecimal string representation
 *
 * @param hex the value to convert
 * @return the hexadecimal string representation of the value
 */
std::string to_hex_str(U16 hex);

/**
 * Converts an 8 bit value into hexadecimal string representation
 *
 * @param hex the value to convert
 * @return the hexadecimal string representation of the value
 */
std::string to_hex_str(U8 hex);


/**
 * Pretty stringifies a value by bolding the digits and graying out the leading zeros
 *
 * @param string the string to pretty stringify
 * @return the pretty stringified value
 */
std::string color_val_str(std::string string);

std::string to_color_hex_str(U64 hex, int digits = 16);
std::string to_color_hex_str(U32 hex);
std::string to_color_hex_str(U16 hex);
std::string to_color_hex_str(U8 hex);

/**
 * Converts a 64 bit value into binary string representation
 *
 * @param bin the value to convert
 * @return the binary string representation of the value
 */
std::string to_bin_str(U64 bin);

/**
 * Converts a 32 bit value into binary string representation
 *
 * @param bin the value to convert
 * @return the binary string representation of the value
 */
std::string to_bin_str(U32 bin);

/**
 * Converts a 16 bit value into binary string representation
 *
 * @param bin the value to convert
 * @return the binary string representation of the value
 */
std::string to_bin_str(U16 bin);

/**
 * Converts an 8 bit value into binary string representation
 *
 * @param bin the value to convert
 * @return the binary string representation of the value
 */
std::string to_bin_str(U8 bin);

#endif // TYPES_H