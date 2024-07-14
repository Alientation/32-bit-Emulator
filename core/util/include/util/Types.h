#ifndef TYPES_H
#define TYPES_H

// SUPER BAD CHEATY WAY BUT IT WORKS  //todo help now, i forgot what this does, to scared to delete it, guess i will keep this
#ifdef private
#undef private
#include <iostream>
#include <iomanip>
#include <bitset>
#include <util/ConsoleColor.h>
#define private public
#else
#include <iostream>
#include <iomanip>
#include <bitset>
#include <util/ConsoleColor.h>
#include <sstream>
#endif

using Byte = unsigned char;

// Standard size of a unit of data
using Word = unsigned int; // 4 bytes

// int sizes
using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long;

// static constants
static const u8 BITS_IN_BYTE = 8;
static const u8 BITS_IN_WORD = 32;

static const char VALUE_PREFIX = '#';
static const char BINARY_PREFIX = '%'; // could also be 0b
static const char HEXADECIMAL_PREFIX = '$'; // could also be 0x


/**
 * Converts a value into hexadecimal string representation
 *
 * @param hex the value to convert
 * @param digits the number of hexadecimal digits to output
 * @return the hexadecimal string representation of the value
 */
std::string to_hex_str(u64 hex, int digits = 16);

/**
 * Converts a 32 bit value into hexadecimal string representation
 *
 * @param hex the value to convert
 * @return the hexadecimal string representation of the value
 */
std::string to_hex_str(u32 hex);

/**
 * Converts a 16 bit value into hexadecimal string representation
 *
 * @param hex the value to convert
 * @return the hexadecimal string representation of the value
 */
std::string to_hex_str(u16 hex);

/**
 * Converts an 8 bit value into hexadecimal string representation
 *
 * @param hex the value to convert
 * @return the hexadecimal string representation of the value
 */
std::string to_hex_str(u8 hex);


/**
 * Pretty stringifies a value by bolding the digits and graying out the leading zeros
 *
 * @param string the string to pretty stringify
 * @return the pretty stringified value
 */
std::string color_val_str(std::string string);

std::string to_color_hex_str(u64 hex, int digits = 16);
std::string to_color_hex_str(u32 hex);
std::string to_color_hex_str(u16 hex);
std::string to_color_hex_str(u8 hex);

/**
 * Converts a 64 bit value into binary string representation
 *
 * @param bin the value to convert
 * @return the binary string representation of the value
 */
std::string to_bin_str(u64 bin);

/**
 * Converts a 32 bit value into binary string representation
 *
 * @param bin the value to convert
 * @return the binary string representation of the value
 */
std::string to_bin_str(u32 bin);

/**
 * Converts a 16 bit value into binary string representation
 *
 * @param bin the value to convert
 * @return the binary string representation of the value
 */
std::string to_bin_str(u16 bin);

/**
 * Converts an 8 bit value into binary string representation
 *
 * @param bin the value to convert
 * @return the binary string representation of the value
 */
std::string to_bin_str(u8 bin);

#endif // TYPES_H