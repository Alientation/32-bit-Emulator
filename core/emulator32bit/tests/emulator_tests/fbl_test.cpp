#include "emulator32bit_test/emulator32bit_test.h"

#include "emulator32bit/fbl.h"

#include "algorithm"

TEST(fbl, in_order)
{
    const int MEM_SIZE = 4;
    FreeBlockList fbl(0, MEM_SIZE);

    ASSERT_FALSE(fbl.empty());

    ASSERT_EQ(fbl.size(), 4);
    word b1 = fbl.get_free_block(1);
    ASSERT_EQ(fbl.size(), 3);
    word b2 = fbl.get_free_block(1);
    ASSERT_EQ(fbl.size(), 2);
    word b3 = fbl.get_free_block(1);
    ASSERT_EQ(fbl.size(), 1);
    word b4 = fbl.get_free_block(1);
    ASSERT_EQ(fbl.size(), 0);

    ASSERT_NE(b1, b2);
    ASSERT_NE(b1, b3);
    ASSERT_NE(b1, b4);
    ASSERT_NE(b2, b3);
    ASSERT_NE(b2, b4);
    ASSERT_NE(b3, b4);

    ASSERT_TRUE(b1 < MEM_SIZE);
    ASSERT_TRUE(b2 < MEM_SIZE);
    ASSERT_TRUE(b3 < MEM_SIZE);
    ASSERT_TRUE(b4 < MEM_SIZE);

    ASSERT_TRUE(fbl.empty());

    fbl.return_block(b1, 1);
    ASSERT_FALSE(fbl.empty());
    ASSERT_EQ(b1, fbl.get_free_block(1));

    fbl.return_block(b1, 1);
    ASSERT_EQ(fbl.size(), 1);
    fbl.return_block(b2, 1);
    ASSERT_EQ(fbl.size(), 2);
    fbl.return_block(b3, 1);
    ASSERT_EQ(fbl.size(), 3);
    fbl.return_block(b4, 1);
    ASSERT_EQ(fbl.size(), 4);
}

TEST(fbl, multi_page_return_not_at_head)
{
    FreeBlockList fbl(0, 100, false);
    fbl.return_block(0, 1);
    fbl.return_block(10, 4);
    ASSERT_EQ(fbl.size(), 5);

    const auto blocks = fbl.get_blocks();
    ASSERT_EQ(blocks.size(), 2);
    ASSERT_EQ(blocks[1].first, 10);
    ASSERT_EQ(blocks[1].second, 4);
}

TEST(fbl, overlapping_return_is_rejected_and_undone)
{
    FreeBlockList fbl(0, 100, false);
    fbl.return_block(10, 4);
    ASSERT_THROW(fbl.return_block(12, 4), FreeBlockList::FreeBlockListException);
    ASSERT_EQ(fbl.size(), 4);
    ASSERT_EQ(fbl.get_blocks().size(), 1);
}

using Blocks = std::vector<std::pair<word, word>>;

TEST(fbl, force_return_on_an_empty_list)
{
    FreeBlockList fbl(0, 100, false);
    fbl.force_return_block(10, 5);
    ASSERT_EQ(fbl.get_blocks(), (Blocks{{10, 5}}));
}

TEST(fbl, force_return_swallows_a_free_block_inside_it)
{
    FreeBlockList fbl(0, 100, false);
    fbl.return_block(2, 2);
    fbl.force_return_block(0, 10);
    ASSERT_EQ(fbl.get_blocks(), (Blocks{{0, 10}}));
    ASSERT_EQ(fbl.size(), 10);
}

TEST(fbl, force_return_swallows_several_free_blocks)
{
    FreeBlockList fbl(0, 100, false);
    fbl.return_block(2, 1);
    fbl.return_block(4, 1);
    fbl.return_block(6, 1);
    fbl.return_block(50, 5);
    fbl.force_return_block(1, 8);
    ASSERT_EQ(fbl.get_blocks(), (Blocks{{1, 8}, {50, 5}}));
}

TEST(fbl, force_return_joins_a_block_that_it_touches)
{
    FreeBlockList fbl(0, 100, false);
    fbl.return_block(0, 5);
    fbl.return_block(20, 5);
    fbl.force_return_block(5, 3);
    ASSERT_EQ(fbl.get_blocks(), (Blocks{{0, 8}, {20, 5}}));

    fbl.force_return_block(8, 12);
    ASSERT_EQ(fbl.get_blocks(), (Blocks{{0, 25}}));
}

TEST(fbl, force_return_extends_over_the_ends_of_overlapping_blocks)
{
    FreeBlockList fbl(0, 100, false);
    fbl.return_block(0, 6);
    fbl.return_block(10, 6);
    fbl.force_return_block(4, 8);
    ASSERT_EQ(fbl.get_blocks(), (Blocks{{0, 16}}));
}

TEST(fbl, force_return_of_a_free_range_changes_nothing)
{
    FreeBlockList fbl(0, 100, false);
    fbl.return_block(10, 10);
    fbl.force_return_block(12, 3);
    ASSERT_EQ(fbl.get_blocks(), (Blocks{{10, 10}}));
    fbl.force_return_block(10, 10);
    ASSERT_EQ(fbl.get_blocks(), (Blocks{{10, 10}}));
}

TEST(fbl, force_return_out_of_range_throws)
{
    FreeBlockList fbl(10, 10, false);
    ASSERT_THROW(fbl.force_return_block(5, 2), FreeBlockList::FreeBlockListException);
    ASSERT_THROW(fbl.force_return_block(18, 5), FreeBlockList::FreeBlockListException);
    ASSERT_THROW(fbl.force_return_block(0xFFFFFFFF, 2), FreeBlockList::FreeBlockListException);
    ASSERT_TRUE(fbl.empty());
}

TEST(fbl, a_block_can_end_at_the_top_of_the_address_space)
{
    FreeBlockList fbl(0xFFFFFFF0, 16, false);
    fbl.force_return_block(0xFFFFFFF8, 8);
    fbl.force_return_block(0xFFFFFFF0, 8);
    ASSERT_EQ(fbl.get_blocks(), (Blocks{{0xFFFFFFF0, 16}}));
}

TEST(fbl, remove_block_from_the_middle_splits_the_block)
{
    FreeBlockList fbl(0, 10);
    fbl.remove_block(3, 2);

    const auto blocks = fbl.get_blocks();
    ASSERT_EQ(blocks.size(), 2);
    ASSERT_EQ(blocks[0], std::make_pair(word(0), word(3)));
    ASSERT_EQ(blocks[1], std::make_pair(word(5), word(5)));
}

TEST(fbl, remove_block_not_free_throws)
{
    FreeBlockList fbl(0, 100, false);
    ASSERT_THROW(fbl.remove_block(5, 1), FreeBlockList::FreeBlockListException);
}

TEST(fbl, out_of_order)
{
    const int MEM_SIZE = 4;
    FreeBlockList fbl(0, MEM_SIZE);

    ASSERT_FALSE(fbl.empty());

    word b1 = fbl.get_free_block(1);
    word b2 = fbl.get_free_block(1);
    word b3 = fbl.get_free_block(1);
    word b4 = fbl.get_free_block(1);

    /* Cannot assume order, that is implementation dependent */
    std::vector<word> sort;
    sort.push_back(b1);
    sort.push_back(b2);
    sort.push_back(b3);
    sort.push_back(b4);

    std::sort(sort.begin(), sort.end());
    b1 = sort.at(0);
    b2 = sort.at(1);
    b3 = sort.at(2);
    b4 = sort.at(3);

    ASSERT_TRUE(fbl.empty());

    fbl.return_block(b2, 1);
    ASSERT_EQ(fbl.size(), 1);
    ASSERT_EQ(fbl.get_blocks().size(), 1);
    fbl.return_block(b1, 1);
    ASSERT_EQ(fbl.size(), 2);
    ASSERT_EQ(fbl.get_blocks().size(), 1);

    fbl.return_block(b4, 1);
    ASSERT_EQ(fbl.size(), 3);
    ASSERT_EQ(fbl.get_blocks().size(), 2);

    fbl.return_block(b3, 1);
    ASSERT_EQ(fbl.size(), 4);
    ASSERT_EQ(fbl.get_blocks().size(), 1);
}