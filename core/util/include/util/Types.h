#ifndef TYPES_H
#define TYPES_H

// SUPER BAD CHEATY WAY BUT IT WORKS
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
static std::string stringifyHex(u64 hex, int digits = 16) {
    std::stringstream stream;
    stream << HEXADECIMAL_PREFIX << std::setfill('0') << std::setw(digits) << std::hex << hex;
    std::string result(stream.str());
    return result;
}

/**
 * Converts a 32 bit value into hexadecimal string representation
 * 
 * @param hex the value to convert
 * @return the hexadecimal string representation of the value
 */
static std::string stringifyHex(u32 hex) {
    return stringifyHex(hex,8);
}

/**
 * Converts a 16 bit value into hexadecimal string representation
 * 
 * @param hex the value to convert
 * @return the hexadecimal string representation of the value
 */
static std::string stringifyHex(u16 hex) {
    return stringifyHex(hex,4);
}

/**
 * Converts an 8 bit value into hexadecimal string representation
 * 
 * @param hex the value to convert
 * @return the hexadecimal string representation of the value
 */
static std::string stringifyHex(u8 hex) {
    return stringifyHex(hex,2);
}


/**
 * Pretty stringifies a value by bolding the digits and graying out the leading zeros
 * 
 * @param string the string to pretty stringify
 * @return the pretty stringified value
 */
static std::string prettyStringifyValue(std::string string) {
    std::string result = ccolor::GRAY;
    auto iterator = string.begin();
    if ((*iterator) == '$' || (*iterator) == '%' || (*iterator) == '#') {
        iterator++;
    }

	// gray out any leading zeros except for the last zero
    while ((*iterator) == '0' && iterator != string.end() - 1) {
        result += *iterator;
        iterator++;
    }

	// bold the remaining digits
    result += ccolor::BOLD_WHITE;
    while (iterator != string.end()) {
        result += *iterator;
        iterator++;
    }

    result += ccolor::RESET;
    return result;
}


/**
 * Converts a 64 bit value into binary string representation
 * 
 * @param bin the value to convert
 * @return the binary string representation of the value
 */
static std::string stringifyBin(u64 bin) {
    std::stringstream stream;
    stream << BINARY_PREFIX << std::bitset<64>(bin);
    std::string result(stream.str());
    return result;
}

/**
 * Converts a 32 bit value into binary string representation
 * 
 * @param bin the value to convert
 * @return the binary string representation of the value
 */
static std::string stringifyBin(u32 bin) {
    std::stringstream stream;
    stream << BINARY_PREFIX << std::bitset<32>(bin);
    std::string result(stream.str());
    return result;
}

/**
 * Converts a 16 bit value into binary string representation
 * 
 * @param bin the value to convert
 * @return the binary string representation of the value
 */
static std::string stringifyBin(u16 bin) {
    std::stringstream stream;
    stream << BINARY_PREFIX << std::bitset<16>(bin);
    std::string result(stream.str());
    return result;
}

/**
 * Converts an 8 bit value into binary string representation
 * 
 * @param bin the value to convert
 * @return the binary string representation of the value
 */
static std::string stringifyBin(u8 bin) {
    std::stringstream stream;
    stream << BINARY_PREFIX << std::bitset<8>(bin);
    std::string result(stream.str());
    return result;
}

#endif // TYPES_H