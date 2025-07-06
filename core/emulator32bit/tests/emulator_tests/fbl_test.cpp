#include "emulator32bit_test/emulator32bit_test.h"

#include "emulator32bit/fbl.h"

#include "algorithm"

TEST (fbl, in_order)
{
    const int MEM_SIZE = 4;
    FreeBlockList fbl (0, MEM_SIZE);

    ASSERT_FALSE (fbl.empty ());

    ASSERT_EQ (fbl.size (), 4);
    word b1 = fbl.get_free_block (1);
    ASSERT_EQ (fbl.size (), 3);
    word b2 = fbl.get_free_block (1);
    ASSERT_EQ (fbl.size (), 2);
    word b3 = fbl.get_free_block (1);
    ASSERT_EQ (fbl.size (), 1);
    word b4 = fbl.get_free_block (1);
    ASSERT_EQ (fbl.size (), 0);

    ASSERT_NE (b1, b2);
    ASSERT_NE (b1, b3);
    ASSERT_NE (b1, b4);
    ASSERT_NE (b2, b3);
    ASSERT_NE (b2, b4);
    ASSERT_NE (b3, b4);

    ASSERT_TRUE (b1 < MEM_SIZE);
    ASSERT_TRUE (b2 < MEM_SIZE);
    ASSERT_TRUE (b3 < MEM_SIZE);
    ASSERT_TRUE (b4 < MEM_SIZE);

    ASSERT_TRUE (fbl.empty ());

    fbl.return_block (b1, 1);
    ASSERT_FALSE (fbl.empty ());
    ASSERT_EQ (b1, fbl.get_free_block (1));

    fbl.return_block (b1, 1);
    ASSERT_EQ (fbl.size (), 1);
    fbl.return_block (b2, 1);
    ASSERT_EQ (fbl.size (), 2);
    fbl.return_block (b3, 1);
    ASSERT_EQ (fbl.size (), 3);
    fbl.return_block (b4, 1);
    ASSERT_EQ (fbl.size (), 4);
}

TEST(fbl, out_of_order)
{
    const int MEM_SIZE = 4;
    FreeBlockList fbl(0, MEM_SIZE);

    ASSERT_FALSE (fbl.empty ());

    word b1 = fbl.get_free_block (1);
    word b2 = fbl.get_free_block (1);
    word b3 = fbl.get_free_block (1);
    word b4 = fbl.get_free_block (1);

    /* Cannot assume order, that is implementation dependent */
    std::vector <word> sort;
    sort.push_back (b1);
    sort.push_back (b2);
    sort.push_back (b3);
    sort.push_back (b4);

    std::sort (sort.begin (), sort.end ());
    b1 = sort.at (0);
    b2 = sort.at (1);
    b3 = sort.at (2);
    b4 = sort.at (3);

    ASSERT_TRUE (fbl.empty ());

    fbl.return_block (b2, 1);
    ASSERT_EQ (fbl.size (), 1);
    ASSERT_EQ (fbl.get_blocks ().size (), 1);
    fbl.return_block (b1, 1);
    ASSERT_EQ (fbl.size (), 2);
    ASSERT_EQ (fbl.get_blocks ().size (), 1);

    fbl.return_block (b4, 1);
    ASSERT_EQ (fbl.size (), 3);
    ASSERT_EQ (fbl.get_blocks ().size (), 2);

    fbl.return_block (b3, 1);
    ASSERT_EQ (fbl.size (), 4);
    ASSERT_EQ (fbl.get_blocks ().size (), 1);
}