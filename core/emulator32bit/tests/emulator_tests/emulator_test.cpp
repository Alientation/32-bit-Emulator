#include <emulator32bit_test/emulator32bit_test.h>

#include "emulator32bit/emulator32bit_util.h"

TEST (emulator_test, emulator32bit_util_test)
{
    /* Test test_bit and set_bit. */
    {
        constexpr dword kOnes = ~0;
        constexpr dword kZeros = 0;
        constexpr dword kAlternating10 = 0xAAAAAAAAAAAAAAAA;
        constexpr dword kAlternating01 = 0x5555555555555555;

        for (U8 bit = 0; bit < kNumByteBits; bit++)
        {
            EXPECT_EQ(test_bit(byte(kOnes), bit), 1);
            EXPECT_EQ(test_bit(byte(kZeros), bit), 0);
            EXPECT_EQ(test_bit(byte(kAlternating10), bit), bit & 1);
            EXPECT_EQ(test_bit(byte(kAlternating01), bit), 1 - (bit & 1));

            EXPECT_EQ(test_bit(set_bit(byte(kOnes), bit, 1), bit), 1);
            EXPECT_EQ(test_bit(set_bit(byte(kOnes), bit, 0), bit), 0);
            EXPECT_EQ(test_bit(set_bit(byte(kZeros), bit, 1), bit), 1);
            EXPECT_EQ(test_bit(set_bit(byte(kZeros), bit, 0), bit), 0);
        }

        for (U8 bit = 0; bit < kNumHwordBits; bit++)
        {
            EXPECT_EQ(test_bit(hword(kOnes), bit), 1);
            EXPECT_EQ(test_bit(hword(kZeros), bit), 0);
            EXPECT_EQ(test_bit(hword(kAlternating10), bit), bit & 1);
            EXPECT_EQ(test_bit(hword(kAlternating01), bit), 1 - (bit & 1));

            EXPECT_EQ(test_bit(set_bit(hword(kOnes), bit, 1), bit), 1);
            EXPECT_EQ(test_bit(set_bit(hword(kOnes), bit, 0), bit), 0);
            EXPECT_EQ(test_bit(set_bit(hword(kZeros), bit, 1), bit), 1);
            EXPECT_EQ(test_bit(set_bit(hword(kZeros), bit, 0), bit), 0);
        }

        for (U8 bit = 0; bit < kNumWordBits; bit++)
        {
            EXPECT_EQ(test_bit(word(kOnes), bit), 1);
            EXPECT_EQ(test_bit(word(kZeros), bit), 0);
            EXPECT_EQ(test_bit(word(kAlternating10), bit), bit & 1);
            EXPECT_EQ(test_bit(word(kAlternating01), bit), 1 - (bit & 1));

            EXPECT_EQ(test_bit(set_bit(word(kOnes), bit, 1), bit), 1);
            EXPECT_EQ(test_bit(set_bit(word(kOnes), bit, 0), bit), 0);
            EXPECT_EQ(test_bit(set_bit(word(kZeros), bit, 1), bit), 1);
            EXPECT_EQ(test_bit(set_bit(word(kZeros), bit, 0), bit), 0);
        }


        for (U8 bit = 0; bit < kNumDwordBits; bit++)
        {
            EXPECT_EQ(test_bit(kOnes, bit), 1);
            EXPECT_EQ(test_bit(kZeros, bit), 0);
            EXPECT_EQ(test_bit(kAlternating10, bit), bit & 1);
            EXPECT_EQ(test_bit(kAlternating01, bit), 1 - (bit & 1));

            EXPECT_EQ(test_bit(set_bit(kOnes, bit, 1), bit), 1);
            EXPECT_EQ(test_bit(set_bit(kOnes, bit, 0), bit), 0);
            EXPECT_EQ(test_bit(set_bit(kZeros, bit, 1), bit), 1);
            EXPECT_EQ(test_bit(set_bit(kZeros, bit, 0), bit), 0);
        }
    }

    /* Test bitfield_u32 and bitfield_s32. */
    {
        constexpr dword value = 0b111100001010010111000011111100001010010111000011;
        for (U8 bit = 0; bit < kNumByteBits; bit++)
        {
            for (U8 len = 1; len < kNumByteBits - bit; len++)
            {
                EXPECT_EQ(bitfield_unsigned(byte(value), bit, len), (byte(value) >> bit) & ((1ULL << len) - 1));
            }
        }

        for (U8 bit = 0; bit < kNumHwordBits; bit++)
        {
            for (U8 len = 1; len < kNumHwordBits - bit; len++)
            {
                EXPECT_EQ(bitfield_unsigned(hword(value), bit, len), (hword(value) >> bit) & ((1ULL << len) - 1));
            }
        }

        for (U8 bit = 0; bit < kNumWordBits; bit++)
        {
            for (U8 len = 1; len < kNumWordBits - bit; len++)
            {
                EXPECT_EQ(bitfield_unsigned(word(value), bit, len), (word(value) >> bit) & ((1ULL << len) - 1));
            }
        }

        for (U8 bit = 0; bit < kNumDwordBits; bit++)
        {
            for (U8 len = 1; len < kNumDwordBits - bit; len++)
            {
                EXPECT_EQ(bitfield_unsigned(value, bit, len), (value >> bit) & ((1ULL << len) - 1));
            }
        }

        EXPECT_EQ(bitfield_unsigned(sbyte(-3), 0, 8), sbyte(0b11111101));
        EXPECT_EQ(bitfield_unsigned(hword(-3), 0, 12), hword(0b111111111101));
        EXPECT_EQ(bitfield_unsigned(sword(-3), 0, 12), sword(0b111111111101));
        EXPECT_EQ(bitfield_unsigned(dword(-3), 0, 12), dword(0b111111111101));

        EXPECT_EQ(bitfield_signed(byte(value), 0, 4), 0b0011);
        EXPECT_EQ(bitfield_signed(byte(value), 0, 6), 0b000011);
        EXPECT_EQ(bitfield_signed(byte(value), 0, 7), byte(~0b0111100));

        EXPECT_EQ(bitfield_signed(hword(value), 0, 4), 0b0011);
        EXPECT_EQ(bitfield_signed(hword(value), 0, 6), 0b000011);
        EXPECT_EQ(bitfield_signed(hword(value), 0, 7), hword(~0b0111100));

        EXPECT_EQ(bitfield_signed(word(value), 0, 4), 0b0011);
        EXPECT_EQ(bitfield_signed(word(value), 0, 6), 0b000011);
        EXPECT_EQ(bitfield_signed(word(value), 0, 7), word(~0b0111100));

        EXPECT_EQ(bitfield_signed(value, 0, 4), 0b0011);
        EXPECT_EQ(bitfield_signed(value, 0, 6), 0b000011);
        EXPECT_EQ(bitfield_signed(value, 0, 7), dword(~0b0111100));
    }

    /* Test mask_0. */
    {
        EXPECT_EQ(mask_0(byte(0x12), 0, 4), 0x10);
        EXPECT_EQ(mask_0(byte(0x12), 4, 4), 0x02);
        EXPECT_EQ(mask_0(byte(0x12), 0, 8), 0);

        EXPECT_EQ(mask_0(hword(0x1234), 0, 4), 0x1230);
        EXPECT_EQ(mask_0(hword(0x1234), 12, 4), 0x0234);
        EXPECT_EQ(mask_0(hword(0x1234), 8, 8), 0x0034);
        EXPECT_EQ(mask_0(hword(0x1234), 0, 8), 0x1200);
        EXPECT_EQ(mask_0(hword(0x1234), 0, 16), 0);

        EXPECT_EQ(mask_0(0x12345678, 0, 4), 0x12345670);
        EXPECT_EQ(mask_0(0x12345678, 4, 4), 0x12345608);
        EXPECT_EQ(mask_0(0x12345678, 28, 4), 0x02345678);
        EXPECT_EQ(mask_0(0x12345678, 24, 8), 0x00345678);
        EXPECT_EQ(mask_0(0x12345678, 16, 16), 0x00005678);
        EXPECT_EQ(mask_0(0x12345678, 0, 16), 0x12340000);
        EXPECT_EQ(mask_0(0x12345678, 0, 32), 0);

        EXPECT_EQ(mask_0(dword(0xF123456789ABCDEFULL), 0, 4), dword(0xF123456789ABCDE0ULL));
        EXPECT_EQ(mask_0(dword(0xF123456789ABCDEFULL), 60, 4), dword(0x0123456789ABCDEFULL));
        EXPECT_EQ(mask_0(dword(0xF123456789ABCDEFULL), 0, 64), 0);
    }
}