#include "util/bitarray.h"

#include <gtest/gtest.h>

TEST(BitArrayTest, a_bit_is_set_and_cleared_alone)
{
    BitArray<64> bits;
    EXPECT_FALSE(bits.any());

    bits.assign(0, true);
    bits.assign(63, true);
    bits.assign(5, true);
    EXPECT_TRUE(bits.test(0));
    EXPECT_TRUE(bits.test(5));
    EXPECT_TRUE(bits.test(63));
    EXPECT_FALSE(bits.test(1));
    EXPECT_FALSE(bits.test(62));

    bits.assign(5, false);
    bits.assign(5, false); // clearing a clear bit changes nothing
    bits.assign(0, true);  // nor does setting a set one
    EXPECT_FALSE(bits.test(5));
    EXPECT_TRUE(bits.test(0));
    EXPECT_TRUE(bits.test(63));

    bits.clear();
    EXPECT_FALSE(bits.any());
}

TEST(BitArrayTest, more_than_one_word)
{
    BitArray<130> bits;
    bits.assign(63, true);
    bits.assign(64, true);
    bits.assign(129, true);
    EXPECT_TRUE(bits.test(63));
    EXPECT_TRUE(bits.test(64));
    EXPECT_TRUE(bits.test(129));
    EXPECT_FALSE(bits.test(65));
    EXPECT_FALSE(bits.test(128));

    bits.assign(64, false);
    EXPECT_TRUE(bits.test(63)) << "the neighbour in the other word";
    EXPECT_FALSE(bits.test(64));

    bits.copy(129, 1);
    EXPECT_TRUE(bits.test(1));

    bits.clear();
    EXPECT_FALSE(bits.any());
    bits.assign(129, true);
    EXPECT_TRUE(bits.any()) << "a bit in the last word";
}

TEST(BitArrayTest, copy_moves_the_bit_to_the_other_index)
{
    BitArray<64> bits;
    bits.assign(3, true);
    bits.copy(3, 7);
    EXPECT_TRUE(bits.test(7));

    bits.copy(1, 3); // a clear bit clears the target
    EXPECT_FALSE(bits.test(3));
    EXPECT_TRUE(bits.test(7));
}
