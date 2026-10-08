#pragma once

#include "emulator32bit/emulator32bit_util.h"
#include "util/file.h"

#include <algorithm>
#include <string>

class BaseMemory
{
  public:
    BaseMemory(word npages, word start_page);
    virtual ~BaseMemory();

    virtual inline byte read_byte(word address) = 0;
    virtual inline hword read_hword(word address) = 0;
    virtual inline word read_word(word address) = 0;
    virtual inline void write_byte(word address, byte value) = 0;
    virtual inline void write_hword(word address, hword value) = 0;
    virtual inline void write_word(word address, word value) = 0;

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
        // is not an address. Memory without pages has no address.
        const word page = address >> kNumPageOffsetBits;
        return m_npages != 0 && page >= m_start_page && U64(page) < U64(m_start_page) + m_npages;
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

    inline hword read_hword(word address)
    {
        const byte *p = m_data + (address - m_start_addr);
        return hword(p[0] | (hword(p[1]) << 8));
    }

    inline word read_word(word address)
    {
        const byte *p = m_data + (address - m_start_addr);
        return word(p[0]) | (word(p[1]) << 8) | (word(p[2]) << 16) | (word(p[3]) << 24);
    }

    virtual inline word read_word_aligned(word address)
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

    inline void write_byte(word address, byte value)
    {
        m_data[address - m_start_addr] = value;
    }

    inline void write_hword(word address, hword value)
    {
        byte *p = m_data + (address - m_start_addr);
        p[0] = byte(value);
        p[1] = byte(value >> 8);
    }

    inline void write_word(word address, word value)
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

class RAM : public Memory
{
  public:
    RAM(word npages, word start_pages);
};

class ROM : public Memory
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
