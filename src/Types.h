#ifndef TYPES_H
#define TYPES_H

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

#endif // TYPES_H