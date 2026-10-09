#include "emulator32bit/emulator32bit_util.h"

#include <gtest/gtest.h>
#include <array>
#include <type_traits>

namespace
{
// References that build the result one bit at a time, so they share no shift or mask code with the
// functions under test.
template<typename T>
bool ref_bit(const T val, const unsigned i)
{
    return (U64(std::make_unsigned_t<T>(val)) / (U64(1) << i)) % 2 == 1;
}

using Bits = std::array<bool, 64>;

template<typename T>
T pack(const Bits &bits)
{
    U64 value = 0;
    for (unsigned k = 0; k < 64; k++)
    {
        if (bits[k])
        {
            value += U64(1) << k;
        }
    }
    return T(value);
}

template<typename T>
T ref_set_bit(const T val, const unsigned bit_i, const bool to)
{
    Bits bits{};
    for (unsigned k = 0; k < sizeof(T) * 8; k++)
    {
        bits[k] = k == bit_i ? to : ref_bit(val, k);
    }
    return pack<T>(bits);
}

template<typename T>
T ref_unsigned(const T val, const unsigned bit_i, const unsigned len)
{
    Bits bits{};
    for (unsigned k = 0; k < len; k++)
    {
        bits[k] = ref_bit(val, bit_i + k);
    }
    return pack<T>(bits);
}

template<typename T>
T ref_signed(const T val, const unsigned bit_i, const unsigned len)
{
    Bits bits{};
    for (unsigned k = 0; k < 64; k++)
    {
        bits[k] = ref_bit(val, bit_i + (k < len ? k : len - 1));
    }
    return pack<T>(bits);
}

template<typename T>
T ref_mask_0(const T val, const unsigned bit_i, const unsigned len)
{
    Bits bits{};
    for (unsigned k = 0; k < sizeof(T) * 8; k++)
    {
        bits[k] = (k >= bit_i && k < bit_i + len) ? false : ref_bit(val, k);
    }
    return pack<T>(bits);
}

template<typename T>
std::array<T, 7> sample_values()
{
    return {T(0),
            T(1),
            T(~U64(0)),
            T(U64(1) << (sizeof(T) * 8 - 1)),
            T(0x5555555555555555ULL),
            T(0xAAAAAAAAAAAAAAAAULL),
            T(0x123456789ABCDEF0ULL)};
}
} // namespace

template<typename T>
class BitUtilTest : public ::testing::Test
{
};

using BitUtilTypes = ::testing::Types<U8, U16, U32, U64, S8, S16, S32, S64>;
TYPED_TEST_SUITE(BitUtilTest, BitUtilTypes);

TYPED_TEST(BitUtilTest, test_bit_matches_reference)
{
    constexpr unsigned width = sizeof(TypeParam) * 8;
    for (const TypeParam val : sample_values<TypeParam>())
    {
        for (unsigned i = 0; i < width; i++)
        {
            ASSERT_EQ(test_bit(val, i), ref_bit(val, i)) << "val=" << +val << " bit=" << i;
        }
    }
}

TYPED_TEST(BitUtilTest, set_bit_matches_reference)
{
    constexpr unsigned width = sizeof(TypeParam) * 8;
    for (const TypeParam val : sample_values<TypeParam>())
    {
        for (unsigned i = 0; i < width; i++)
        {
            for (const bool to : {false, true})
            {
                ASSERT_EQ(set_bit(val, i, to), ref_set_bit(val, i, to))
                    << "val=" << +val << " bit=" << i << " to=" << to;
            }
        }
    }
}

TYPED_TEST(BitUtilTest, fields_match_reference)
{
    constexpr unsigned width = sizeof(TypeParam) * 8;
    for (const TypeParam val : sample_values<TypeParam>())
    {
        for (unsigned bit_i = 0; bit_i < width; bit_i++)
        {
            for (unsigned len = 1; len <= width - bit_i; len++)
            {
                const std::string ctx =
                    "val=" + std::to_string(+val) + " bit_i=" + std::to_string(bit_i) +
                    " len=" + std::to_string(len);
                ASSERT_EQ(bitfield_unsigned(val, bit_i, len), ref_unsigned(val, bit_i, len)) << ctx;
                ASSERT_EQ(bitfield_signed(val, bit_i, len), ref_signed(val, bit_i, len)) << ctx;
                ASSERT_EQ(zero_bits(val, bit_i, len), ref_mask_0(val, bit_i, len)) << ctx;
            }
        }
    }
}

TEST(BitUtil, set_bit_takes_a_bool)
{
    // A flag that is not 0 or 1 used to be shifted in as it was and corrupted the next bit.
    EXPECT_EQ(set_bit(word(0), 3, true), word(0x8));
    EXPECT_EQ(set_bit(word(0xFF), 3, false), word(0xF7));
    EXPECT_EQ(set_bit(U8(0), 2, bool(4)), U8(0x4));
}

TEST(BitUtil, small_types_do_not_widen_the_field)
{
    EXPECT_EQ(bitfield_signed(U8(0x80), 7, 1), U8(0xFF));
    EXPECT_EQ(bitfield_unsigned(U8(0xFF), 0, 8), U8(0xFF));
    EXPECT_EQ(bitfield_signed(S16(-1), 0, 16), S16(-1));
    EXPECT_EQ(zero_bits(U8(0xFF), 0, 8), U8(0));
}

// The compile time forms.
static_assert(bitfield_unsigned<4, 8>(word(0xABCD1234)) == 0x23);
static_assert(bitfield_unsigned<0, 32>(word(0xABCD1234)) == 0xABCD1234);
static_assert(bitfield_signed<0, 4>(word(0xF)) == word(0xFFFFFFFF));
static_assert(bitfield_signed<28, 4>(word(0x70000000)) == word(7));
static_assert(zero_bits<4, 8>(word(0xFFFFFFFF)) == 0xFFFFF00F);
static_assert(set_bit<31>(word(0), true) == 0x80000000);
static_assert(!test_bit<7>(U8(0x7F)));
static_assert(test_bit<63>(U64(1) << 63));

#ifndef NDEBUG
// The run time forms assert on a bit or field outside the type; asserts are compiled out of the
// release build.
TEST(BitUtilDeathTest, bit_outside_the_type)
{
    volatile unsigned past_the_end = 32;
    EXPECT_DEATH(test_bit(word(1), past_the_end), "");
    EXPECT_DEATH(set_bit(word(1), past_the_end, true), "");
}

TEST(BitUtilDeathTest, empty_field)
{
    volatile unsigned zero = 0;
    EXPECT_DEATH(bitfield_unsigned(word(1), 0, zero), "");
    EXPECT_DEATH(bitfield_signed(word(1), 0, zero), "");
    EXPECT_DEATH(zero_bits(word(1), 0, zero), "");
}

TEST(BitUtilDeathTest, field_runs_past_the_type)
{
    volatile unsigned bit_i = 20;
    volatile unsigned len = 13;
    volatile unsigned too_long = 65;
    EXPECT_DEATH(bitfield_unsigned(word(1), bit_i, len), "");
    EXPECT_DEATH(bitfield_signed(word(1), bit_i, len), "");
    EXPECT_DEATH(zero_bits(word(1), bit_i, len), "");
    EXPECT_DEATH(bitfield_unsigned(U64(1), 0, too_long), "");
}
#endif
