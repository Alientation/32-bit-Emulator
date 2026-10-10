#pragma once

#include "util/types.h"

#include <cassert>

/// A fixed array of `N` booleans, kept in words of 64. For the flags of the entries of a table
/// (struct of arrays: a `BitArray` instead of a `bool` array or an optional per entry).
///
/// @tparam N the number of bits, at least 1
template<unsigned N>
class BitArray
{
    static_assert(N > 0, "A BitArray has at least one bit");

  public:
    static constexpr unsigned kCapacity = N;

    /// @param i the index, below N
    /// @return the bit
    bool test(const unsigned i) const
    {
        assert(i < N);
        return (m_words[i / 64] >> (i % 64)) & 1;
    }

    /// @param i the index, below N
    /// @param value what the bit becomes
    void assign(const unsigned i, const bool value)
    {
        assert(i < N);
        U64 &word = m_words[i / 64];
        word = (word & ~(U64(1) << (i % 64))) | (U64(value) << (i % 64));
    }

    /// Copies the bit at `from` to `to`: the move of the last entry into a removed one.
    void copy(const unsigned from, const unsigned to)
    {
        assign(to, test(from));
    }

    void clear()
    {
        for (U64 &word : m_words) word = 0;
    }

    /// @return whether any bit is set
    bool any() const
    {
        for (const U64 word : m_words)
        {
            if (word != 0) return true;
        }
        return false;
    }

  private:
    static constexpr unsigned kWords = (N + 63) / 64;
    U64 m_words[kWords] = {};
};
