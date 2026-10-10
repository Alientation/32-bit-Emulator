#pragma once

#include "util/types.h"

#include <cassert>

/// A fixed array of up to 64 booleans, kept in one word. For the flags of the entries of a table
/// that is a few entries long (struct of arrays: a `BitArray` instead of a `bool` array or an
/// optional per entry). Not a template: the capacity is always 64.
class BitArray
{
  public:
    static constexpr unsigned kCapacity = 64;

    /// @param i the index, below kCapacity
    /// @return the bit
    bool test(const unsigned i) const
    {
        assert(i < kCapacity);
        return (m_bits >> i) & 1;
    }

    /// @param i the index, below kCapacity
    /// @param value what the bit becomes
    void assign(const unsigned i, const bool value)
    {
        assert(i < kCapacity);
        m_bits = (m_bits & ~(U64(1) << i)) | (U64(value) << i);
    }

    /// Copies the bit at `from` to `to`: the move of the last entry into a removed one.
    void copy(const unsigned from, const unsigned to)
    {
        assign(to, test(from));
    }

    void clear()
    {
        m_bits = 0;
    }

    /// @return whether any bit is set
    bool any() const
    {
        return m_bits != 0;
    }

  private:
    U64 m_bits = 0;
};
