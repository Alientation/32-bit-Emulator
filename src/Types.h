#ifndef TYPES_H
#define TYPES_H

#include <iostream>
#include <iomanip>

using Byte = unsigned char;

// Standard size of a unit of data
using Word = unsigned int; // 4 bytes

// int sizes
using u8 = unsigned char;
using u16 = unsigned short;
using u32 = unsigned int;
using u64 = unsigned long long; 

// static constants
static const u8 NUM_BITS_IN_BYTE = 8;
static const u8 NUM_BITS_IN_WORD = 32;

static std::string stringifyHex(Word hex) {
    std::stringstream stream;
    stream << "0x" << std::setfill('0') << std::setw(8) << std::hex << hex;
    std::string result(stream.str());
    return result;
}

static std::string stringifyHex(u16 hex) {
    std::stringstream stream;
    stream << "0x" << std::setfill('0') << std::setw(4) << std::hex << hex;
    std::string result(stream.str());
    return result;
}

static std::string stringifyHex(u8 hex) {
    std::stringstream stream;
    stream << "0x" << std::setfill('0') << std::setw(2) << std::hex << hex;
    std::string result(stream.str());
    return result;
}

#endif // TYPES_H