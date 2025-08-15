#pragma once

#include "util/types.h"

#define test_bit(val, bit_i) (((val & (1ULL << bit_i)) >> bit_i) & 1)
#define set_bit(val, bit_i, to) ((val & ~(1ULL << bit_i)) | (to << bit_i))

#define bitfield_u32(val, i, len) (((U32(val)) >> i) & ((1ULL << len) - 1))
#define bitfield_s32(val, i, len) ((S32(((U32(val) >> i) & ((1ULL << len) - 1)) << (32 - len))) >> (32 - len))
#define mask_0(val, i, len) (val & (~(((1 << len)-1) << i)))

#define bytes_to_word(byte1, byte2, byte3, byte4) (word(byte1) + (word(byte2) << 8) + (word(byte3) << 16) + (word(byte4) << 24))
#define bytes_to_hword(byte1, byte2) (word(byte1) + (word(byte2) << 8))

#define byte_from_word(val, byte_i) (byte(val >> (byte_i << 3)))
#define hword_from_word(val, hword_i) (hword(val >> (hword_i << 4)))

#define UNLIKELY(cond) __builtin_expect(cond, 0)
#define LIKELY(cond) __builtin_expect(cond, 1)

static constexpr U8 kNumPageOffsetBits = 12;
static constexpr U32 kPageSize = 1 << kNumPageOffsetBits;