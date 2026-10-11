#pragma once

#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/fbl.h"

#include <bit>
#include <string>
#include <vector>

/// Which of a range of physical pages (the frames) are free, one bit for each.
///
/// What the virtual memory needs of its frames is one page at a time: the lowest
/// free one, a given one taken, one returned. The free block list does that with
/// a node to allocate and free for each page that goes back and forth, which a
/// fault paid for; this does not allocate after it is made. It hands out the
/// same pages in the same order (the lowest free page first), and fails in the
/// same cases, with the same exception.
class FrameAllocator
{
  public:
    /// @param first The number of the first frame.
    /// @param count The number of frames.
    /// @param all_free Whether every frame starts out free, otherwise every frame is taken.
    FrameAllocator(const word first, const word count, const bool all_free) :
        m_first(first),
        m_count(count),
        m_bits((std::size_t(count) + kNumDwordBits - 1) / kNumDwordBits, 0)
    {
        if (all_free)
        {
            for (std::size_t w = 0; w < m_bits.size(); w++)
            {
                const U64 left = count - w * kNumDwordBits; // The pages from this word on.
                m_bits[w] = left >= kNumDwordBits ? ~U64(0) : (U64(1) << left) - 1;
            }
            m_free = count;
        }
    }

    /// Whether a page can be allocated.
    bool any_free() const
    {
        return m_free != 0;
    }

    /// @return How many frames are free.
    word free_count() const
    {
        return m_free;
    }

    /// @param page A page number.
    /// @return Whether the page is a frame and is free.
    bool is_free(const word page) const
    {
        return in_range(page)
               && (m_bits[index(page) / kNumDwordBits] >> (index(page) % kNumDwordBits)) & 1;
    }

    /// Takes the lowest free page.
    ///
    /// @return The page, which is not free anymore.
    /// @throws FreeBlockList::Error if there is none.
    word allocate()
    {
        if (m_free == 0)
        {
            throw FreeBlockList::Error("Not enough space to allocate free block 1");
        }
        // The pages before m_hint are known not to be free.
        for (std::size_t w = m_hint; w < m_bits.size(); w++)
        {
            if (m_bits[w] != 0)
            {
                const unsigned bit = std::countr_zero(m_bits[w]);
                m_bits[w] &= ~(U64(1) << bit);
                m_hint = w;
                m_free--;
                return m_first + word(w * kNumDwordBits + bit);
            }
        }
        throw FreeBlockList::Error("Not enough space to allocate free block 1");
    }

    /// Takes a page that is free, to give it to something else.
    ///
    /// @param page The page to take.
    /// @throws FreeBlockList::Error if it is not free.
    void take(const word page)
    {
        if (!is_free(page))
        {
            throw FreeBlockList::Error("Invalid returned block " + std::to_string(page) + " - 1.");
        }
        m_bits[index(page) / kNumDwordBits] &= ~(U64(1) << (index(page) % kNumDwordBits));
        m_free--;
    }

    /// Makes a page free again.
    ///
    /// @param page The page to give back.
    /// @throws FreeBlockList::Error if it is not a frame or is free already.
    void release(const word page)
    {
        if (!in_range(page) || is_free(page))
        {
            throw FreeBlockList::Error("Invalid returned block " + std::to_string(page) + " - 1.");
        }
        m_bits[index(page) / kNumDwordBits] |= U64(1) << (index(page) % kNumDwordBits);
        m_free++;
        if (index(page) / kNumDwordBits < m_hint)
        {
            m_hint = index(page) / kNumDwordBits;
        }
    }

  private:
    /// @return Whether the page is one of the frames.
    bool in_range(const word page) const
    {
        return page >= m_first && U64(page) - m_first < m_count;
    }

    /// @return The position of the page among the frames.
    std::size_t index(const word page) const
    {
        return std::size_t(page - m_first);
    }

    word m_first;
    word m_count;
    word m_free = 0;
    /// The index of the first word of the bitmap that may have a free page.
    std::size_t m_hint = 0;
    std::vector<U64> m_bits;
};
