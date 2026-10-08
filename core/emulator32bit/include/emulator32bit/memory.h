#pragma once

#include "emulator32bit/emulator32bit_util.h"
#include "util/file.h"

#include <algorithm>
#include <string>
#include <type_traits>

class BaseMemory
{
  public:
    BaseMemory(word npages, word start_page);
    virtual ~BaseMemory();

    virtual byte read_byte(word address) = 0;
    virtual hword read_hword(word address) = 0;
    virtual word read_word(word address) = 0;
    virtual void write_byte(word address, byte value) = 0;
    virtual void write_hword(word address, hword value) = 0;
    virtual void write_word(word address, word value) = 0;

    /// Copies `size` bytes starting at the address to `out`. The bytes are all in this memory.
    virtual void read_block(word address, byte *out, word size)
    {
        for (word i = 0; i < size; i++)
        {
            out[i] = read_byte(address + i);
        }
    }

    /// Copies `size` bytes to the memory starting at the address. They all fit in this memory.
    virtual void write_block(word address, const byte *data, word size)
    {
        for (word i = 0; i < size; i++)
        {
            write_byte(address + i, data[i]);
        }
    }

    inline word get_mem_pages() const
    {
        return m_npages;
    }

    inline word get_lo_page() const
    {
        return m_start_page;
    }

    inline word get_hi_page() const
    {
        return m_start_page + m_npages - 1;
    }

    inline bool in_bounds(word address) const
    {
        // Compared as pages: the end of memory at the top of the address space is 2^32, which
        // is not an address. A page below the start wraps around to a number that is larger than
        // any page count, and memory without pages has no address, so this is the whole test.
        return word((address >> kNumPageOffsetBits) - m_start_page) < m_npages;
    }

    inline bool overlap(const BaseMemory &other) const
    {
        return m_start_page <= other.get_hi_page() && other.m_start_page <= get_hi_page()
               && m_npages != 0 && other.m_npages != 0;
    }

  protected:
    word m_npages;
    word m_start_page;
    word m_start_addr;
};

/**
 * Reads a byte, half word or word (T is `byte`, `hword` or `word`) from a memory. Called with a
 * RAM or ROM the access is not virtual, called with a BaseMemory it is.
 */
template <class T, class Mem> inline T mem_read(Mem &memory, const word address)
{
    static_assert(std::is_unsigned_v<T> && sizeof(T) <= sizeof(word), "byte, hword or word");
    if constexpr (sizeof(T) == sizeof(byte)) return memory.read_byte(address);
    else if constexpr (sizeof(T) == sizeof(hword)) return memory.read_hword(address);
    else return memory.read_word(address);
}

/// The counterpart of mem_read.
template <class T, class Mem> inline void mem_write(Mem &memory, const word address, const T value)
{
    static_assert(std::is_unsigned_v<T> && sizeof(T) <= sizeof(word), "byte, hword or word");
    if constexpr (sizeof(T) == sizeof(byte)) memory.write_byte(address, value);
    else if constexpr (sizeof(T) == sizeof(hword)) memory.write_hword(address, value);
    else memory.write_word(address, value);
}

class Memory : public BaseMemory
{
  public:
    Memory(word npages, word start_page);
    /* The memory owns its bytes, and a ROM that is backed by a file saves them when it is gone. */
    Memory(const Memory &) = delete;
    Memory &operator=(const Memory &) = delete;
    virtual ~Memory();

    // FOR SEG FAULTS, WE CAN USE SIGNAL HANDLERS TO CATCH AND HANDLE THEM IN THE KERNEL, WITHOUT
    // HAVING AN EXPENSIVE CONDITIONAL CHECK EVERYTIME MEMORY IS ACCESSED
    // Values are little endian in memory whatever the host is. The bytes are put together one by
    // one, a pointer to the memory as a word would not be aligned for an odd address, and the
    // compiler turns this into one load.
    inline byte read_byte(word address) override
    {
        return m_data[address - m_start_addr];
    }

    inline hword read_hword(word address) override
    {
        const byte *p = m_data + (address - m_start_addr);
        return hword(p[0] | (hword(p[1]) << 8));
    }

    inline word read_word(word address) override
    {
        const byte *p = m_data + (address - m_start_addr);
        return word(p[0]) | (word(p[1]) << 8) | (word(p[2]) << 16) | (word(p[3]) << 24);
    }

    /// For an address that is a multiple of 4 (instruction fetch).
    inline word read_word_aligned(word address)
    {
        return read_word(address);
    }

    void read_block(word address, byte *out, word size) override
    {
        std::copy_n(m_data + (address - m_start_addr), size, out);
    }

    void write_block(word address, const byte *data, word size) override
    {
        std::copy_n(data, size, m_data + (address - m_start_addr));
    }

    inline void write_byte(word address, byte value) override
    {
        m_data[address - m_start_addr] = value;
    }

    inline void write_hword(word address, hword value) override
    {
        byte *p = m_data + (address - m_start_addr);
        p[0] = byte(value);
        p[1] = byte(value >> 8);
    }

    inline void write_word(word address, word value) override
    {
        byte *p = m_data + (address - m_start_addr);
        p[0] = byte(value);
        p[1] = byte(value >> 8);
        p[2] = byte(value >> 16);
        p[3] = byte(value >> 24);
    }

    void reset();

  protected:
    byte *m_data;
};

/// RAM and ROM are final so that a call on one of them is not virtual and can be inlined.
class RAM final : public Memory
{
  public:
    RAM(word npages, word start_pages);
};

class ROM final : public Memory
{
  public:
    ROM(const byte *data, word npages, word start_page);
    ROM(word npages, word start_page);
    /// @brief Loads the image in the file. The file is only read, never written back.
    ROM(File file, word npages, word start_page);
    ~ROM() override;

    class ROM_Exception : public std::exception
    {
      private:
        std::string message;

      public:
        ROM_Exception(std::string msg);
        const char *what() const noexcept override;
    };

    // TODO: prevent writes, have special way to flash memory
};
