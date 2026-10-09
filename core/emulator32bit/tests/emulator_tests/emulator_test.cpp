#include "emulator32bit_test/emulator32bit_test.h"
#include "emulator32bit/emulator32bit_util.h"

TEST(emulator_test, emulator32bit_util_test)
{
    // Test test_bit and set_bit.
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

    // Test bitfield_u32 and bitfield_s32.
    {
        constexpr dword value = 0b111100001010010111000011111100001010010111000011;
        for (U8 bit = 0; bit < kNumByteBits; bit++)
        {
            for (U8 len = 1; len < kNumByteBits - bit; len++)
            {
                EXPECT_EQ(bitfield_unsigned(byte(value), bit, len),
                          (byte(value) >> bit) & ((1ULL << len) - 1));
            }
        }

        for (U8 bit = 0; bit < kNumHwordBits; bit++)
        {
            for (U8 len = 1; len < kNumHwordBits - bit; len++)
            {
                EXPECT_EQ(bitfield_unsigned(hword(value), bit, len),
                          (hword(value) >> bit) & ((1ULL << len) - 1));
            }
        }

        for (U8 bit = 0; bit < kNumWordBits; bit++)
        {
            for (U8 len = 1; len < kNumWordBits - bit; len++)
            {
                EXPECT_EQ(bitfield_unsigned(word(value), bit, len),
                          (word(value) >> bit) & ((1ULL << len) - 1));
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

    // Test zero_bits.
    {
        EXPECT_EQ(zero_bits(byte(0x12), 0, 4), 0x10);
        EXPECT_EQ(zero_bits(byte(0x12), 4, 4), 0x02);
        EXPECT_EQ(zero_bits(byte(0x12), 0, 8), 0);

        EXPECT_EQ(zero_bits(hword(0x1234), 0, 4), 0x1230);
        EXPECT_EQ(zero_bits(hword(0x1234), 12, 4), 0x0234);
        EXPECT_EQ(zero_bits(hword(0x1234), 8, 8), 0x0034);
        EXPECT_EQ(zero_bits(hword(0x1234), 0, 8), 0x1200);
        EXPECT_EQ(zero_bits(hword(0x1234), 0, 16), 0);

        EXPECT_EQ(zero_bits(0x12345678, 0, 4), 0x12345670);
        EXPECT_EQ(zero_bits(0x12345678, 4, 4), 0x12345608);
        EXPECT_EQ(zero_bits(0x12345678, 28, 4), 0x02345678);
        EXPECT_EQ(zero_bits(0x12345678, 24, 8), 0x00345678);
        EXPECT_EQ(zero_bits(0x12345678, 16, 16), 0x00005678);
        EXPECT_EQ(zero_bits(0x12345678, 0, 16), 0x12340000);
        EXPECT_EQ(zero_bits(0x12345678, 0, 32), 0);

        EXPECT_EQ(zero_bits(dword(0xF123456789ABCDEFULL), 0, 4), dword(0xF123456789ABCDE0ULL));
        EXPECT_EQ(zero_bits(dword(0xF123456789ABCDEFULL), 60, 4), dword(0x0123456789ABCDEFULL));
        EXPECT_EQ(zero_bits(dword(0xF123456789ABCDEFULL), 0, 64), 0);
    }
}

/// Independent model of the 16 ARM condition codes (see docs/isa.md, "Condition codes").
static bool cond_holds(int cond, bool n, bool z, bool c, bool v)
{
    switch (cond)
    {
    case 0:
        return z;            // EQ
    case 1:
        return !z;           // NE
    case 2:
        return c;            // CS/HS
    case 3:
        return !c;           // CC/LO
    case 4:
        return n;            // MI
    case 5:
        return !n;           // PL
    case 6:
        return v;            // VS
    case 7:
        return !v;           // VC
    case 8:
        return c && !z;      // HI
    case 9:
        return !c || z;      // LS
    case 10:
        return n == v;       // GE
    case 11:
        return n != v;       // LT
    case 12:
        return !z && n == v; // GT
    case 13:
        return z || n != v;  // LE
    case 14:
        return true;         // AL
    default:
        return false;        // NV
    }
}

TEST_F(EmulatorFixture, branch_b_forward)
{
    // b +3 instructions: target is the branch address + 12
    step(0, Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, 3));
    EXPECT_EQ(cpu.get_pc(), 12u);
}

TEST_F(EmulatorFixture, branch_b_backward)
{
    // b -2 instructions from address 16 lands on address 8
    step(16, Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, -2));
    EXPECT_EQ(cpu.get_pc(), 8u);
}

TEST_F(EmulatorFixture, branch_b_does_not_touch_link_register)
{
    cpu.write_reg(29, 0x1234);
    step(0, Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, 3));
    EXPECT_EQ(cpu.read_reg(29), 0x1234u);
}

TEST_F(EmulatorFixture, branch_b_condition_codes_match_flags)
{
    for (int cond = 0; cond < 16; cond++)
    {
        for (int flags = 0; flags < 16; flags++)
        {
            const bool n = flags & 8;
            const bool z = flags & 4;
            const bool c = flags & 2;
            const bool v = flags & 1;

            cpu.memory.write_word(
                0, Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode(cond), 3));
            cpu.set_pc(0);
            cpu.set_NZCV(n, z, c, v);
            cpu.run(1);

            // taken: 0 + 12, not taken: falls through to the next instruction
            const word expected = cond_holds(cond, n, z, c, v) ? 12u : 4u;
            EXPECT_EQ(cpu.get_pc(), expected)
                << "cond=" << cond << " N=" << n << " Z=" << z << " C=" << c << " V=" << v;
        }
    }
}