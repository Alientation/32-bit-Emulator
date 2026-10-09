#include "emulator32bit_test/emulator32bit_test.h"
#include "emulator32bit/fbl.h"
#include "emulator32bit/frame_allocator.h"

#include <random>

using Error = FreeBlockList::FreeBlockListException;

TEST(frame_allocator, pages_are_handed_out_lowest_first_until_there_are_none)
{
    FrameAllocator frames(10, 5, true);
    EXPECT_TRUE(frames.any_free());
    EXPECT_EQ(frames.free_count(), 5u);
    for (word page = 10; page < 15; page++)
    {
        EXPECT_EQ(frames.allocate(), page);
    }
    EXPECT_FALSE(frames.any_free());
    EXPECT_THROW(frames.allocate(), Error);

    frames.release(12);
    frames.release(11);
    EXPECT_EQ(frames.allocate(), 11u) << "the lowest of the pages that came back";
    EXPECT_EQ(frames.allocate(), 12u);
}

TEST(frame_allocator, a_page_can_be_taken_and_only_if_it_is_free)
{
    FrameAllocator frames(0, 8, true);
    frames.take(3);
    EXPECT_FALSE(frames.is_free(3));
    EXPECT_EQ(frames.free_count(), 7u);
    EXPECT_THROW(frames.take(3), Error) << "already taken";
    EXPECT_THROW(frames.take(8), Error) << "not a frame";
    EXPECT_EQ(frames.allocate(), 0u);
    EXPECT_EQ(frames.allocate(), 1u);
    EXPECT_EQ(frames.allocate(), 2u);
    EXPECT_EQ(frames.allocate(), 4u) << "3 is not free";
}

TEST(frame_allocator, a_page_that_is_free_or_not_a_frame_cannot_be_released)
{
    FrameAllocator frames(100, 4, true);
    EXPECT_THROW(frames.release(100), Error) << "free already";
    EXPECT_THROW(frames.release(99), Error) << "below the frames";
    EXPECT_THROW(frames.release(104), Error) << "above the frames";
    const word page = frames.allocate();
    frames.release(page);
    EXPECT_THROW(frames.release(page), Error);
}

TEST(frame_allocator, a_range_that_starts_with_no_free_pages_has_none)
{
    FrameAllocator frames(0, 3, false);
    EXPECT_FALSE(frames.any_free());
    EXPECT_THROW(frames.allocate(), Error);
    frames.release(2);
    EXPECT_EQ(frames.allocate(), 2u);

    FrameAllocator none(0, 0, true);
    EXPECT_FALSE(none.any_free());
    EXPECT_THROW(none.allocate(), Error);
}

// The bitmap is words of 64 pages, and the range ends in the middle of one.
TEST(frame_allocator, the_edges_of_the_words_of_the_bitmap)
{
    for (const word count : {1u, 63u, 64u, 65u, 127u, 128u, 129u, 1000u})
    {
        FrameAllocator frames(7, count, true);
        EXPECT_EQ(frames.free_count(), count);
        for (word i = 0; i < count; i++)
        {
            ASSERT_EQ(frames.allocate(), 7 + i) << "count " << count;
        }
        EXPECT_THROW(frames.allocate(), Error) << "count " << count << ": nothing past the end";
        EXPECT_THROW(frames.release(7 + count), Error);
        frames.release(7 + count - 1);
        EXPECT_EQ(frames.allocate(), 7 + count - 1);
    }
}

// What the virtual memory used before: the free block list, with one page at a time. Both are given
// the same random operations and have to agree on what each one does, errors included.
TEST(frame_allocator, it_does_what_the_free_block_list_does_for_single_pages)
{
    for (const word count : {5u, 64u, 150u})
    {
        constexpr word kFirst = 40;
        FrameAllocator frames(kFirst, count, true);
        FreeBlockList list(kFirst, count);
        std::mt19937 random(count);

        for (int step = 0; step < 4000; step++)
        {
            const word page = kFirst - 2 + random() % (count + 4); // some are outside the range
            switch (random() % 3)
            {
            case 0: // allocate
            {
                const bool can = list.can_fit(1);
                ASSERT_EQ(frames.any_free(), can);
                if (can)
                {
                    ASSERT_EQ(frames.allocate(), list.get_free_block(1));
                }
                else
                {
                    ASSERT_THROW(frames.allocate(), Error);
                    ASSERT_THROW(list.get_free_block(1), Error);
                }
                break;
            }
            case 1: // take
            {
                bool list_threw = false;
                bool frames_threw = false;
                try
                {
                    list.remove_block(page, 1);
                }
                catch (const Error &)
                {
                    list_threw = true;
                }
                try
                {
                    frames.take(page);
                }
                catch (const Error &)
                {
                    frames_threw = true;
                }
                ASSERT_EQ(frames_threw, list_threw) << "take " << page;
                break;
            }
            default: // release
            {
                bool list_threw = false;
                bool frames_threw = false;
                try
                {
                    list.return_block(page, 1);
                }
                catch (const Error &)
                {
                    list_threw = true;
                }
                try
                {
                    frames.release(page);
                }
                catch (const Error &)
                {
                    frames_threw = true;
                }
                ASSERT_EQ(frames_threw, list_threw) << "release " << page;
                break;
            }
            }
            ASSERT_EQ(frames.free_count(), list.size());
        }
    }
}
