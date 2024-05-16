#pragma once
#ifndef Emulator32bitUtil_H

#define test_bit(val, bit_i) (((val & (1ULL << bit_i)) >> bit_i) & 1)
#define set_bit(val, bit_i, to) ((val & ~(1ULL << bit_i)) | (to << bit_i))

#define bitfield_u32(val, i, len) ((((unsigned int) val) >> i) & ((1ULL << len) - 1))
#define bitfield_s32(val, i, len) ((signed int)(((((unsigned int) val) >> i) & ((1ULL << len) - 1)) << (32 - len)))

#define bytes_to_word(byte1, byte2, byte3, byte4) (((word) byte1) + ((word) byte2) << 8 + ((word) byte3) << 16 + ((word) byte4) << 24)
#define bytes_to_hword(byte1, byte2) (((word) byte1) + ((word) byte2) << 8)

#define byte_from_word(val, byte_i) ((byte) (val >> (byte_i << 3)))
#define hword_from_word(val, hword_i) ((hword) (val >> (hword_i << 4)))

typedef unsigned long long dword;
typedef unsigned int word;
typedef unsigned short hword;
typedef unsigned char byte;

const int DWORD_BITS = sizeof(dword) << 3;
const int WORD_BITS = sizeof(word) << 3;
const int HWORD_BITS = sizeof(hword) << 3;
const int BYTE_BITS = sizeof(byte) << 3;

#endif