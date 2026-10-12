#pragma once

#include "util/types.h"

#include <cassert>

/// A fixed array of `N` booleans, kept in words of 64. For the flags of the entries of a table
/// (struct of arrays: a `BitArray` instead of a `bool` array or an optional per entry).
///
/// @tparam N The number of bits, at least 1.
template<size_t N>
class BitArray
{
    static_assert(N > 0, "A BitArray has at least one bit");

  public:
    /// The number of bits.
    static constexpr size_t kCapacity = N;

    /// @param[in] i The index, below N.
    /// @return The bit.
    bool test(const size_t i) const
    {
        assert(i < N);
        return (m_words[i / kNumDwordBits] >> (i % kNumDwordBits)) & 1;
    }

    /// @param[in] i The index, below N.
    /// @param[in] value What the bit becomes.
    void assign(const size_t i, const bool value)
    {
        assert(i < N);
        U64 &word = m_words[i / kNumDwordBits];
        word = (word & ~(U64(1) << (i % kNumDwordBits))) | (U64(value) << (i % kNumDwordBits));
    }

    /// Copies the bit at `from` to `to`: the move of the last entry into a removed one.
    ///
    /// @param[in] from The index of the bit to copy.
    /// @param[in] to The index of the bit to replace.
    void copy(const size_t from, const size_t to)
    {
        assign(to, test(from));
    }

    /// Clears every bit.
    void clear()
    {
        for (U64 &word : m_words) word = 0;
    }

    /// @return Whether any bit is set.
    bool any() const
    {
        for (const U64 word : m_words)
        {
            if (word != 0) return true;
        }
        return false;
    }

  private:
    static constexpr size_t kWords = (N + 63) / 64;
    U64 m_words[kWords] = {};
};
