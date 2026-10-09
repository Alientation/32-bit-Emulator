#pragma once

#include "emulator32bit/emulator32bit_util.h"
#include "util/file.h"

#include <algorithm>
#include <string>
#include <type_traits>

/// A range of pages of the physical address space that can be read and written by address: the
/// interface of the RAM, the ROM, the disk and the devices. Values are little endian.
class BaseMemory
{
  public:
    /// @param npages the number of pages in the memory
    /// @param start_page the page number of the first page, so the first address is
    ///     `start_page << kNumPageOffsetBits`
    BaseMemory(word npages, word start_page);
    virtual ~BaseMemory();

    /// Each read returns the value at the address, which has to be in this memory.
    ///
    /// @param address the full (physical) address
    virtual byte read_byte(word address) = 0;
    virtual hword read_hword(word address) = 0;
    virtual word read_word(word address) = 0;

    /// Each write stores the value at the address, which has to be in this memory.
    ///
    /// @param address the full (physical) address
    /// @param value the value to store
    virtual void write_byte(word address, byte value) = 0;
    virtual void write_hword(word address, hword value) = 0;
    virtual void write_word(word address, word value) = 0;

    /// Copies `size` bytes starting at the address to `out`. The bytes are all in this memory.
    ///
    /// @param address the address of the first byte
    /// @param out where to copy the bytes to, with room for `size` bytes
    /// @param size the number of bytes
    virtual void read_block(word address, byte *out, word size)
    {
        for (word i = 0; i < size; i++)
        {
            out[i] = read_byte(address + i);
        }
    }

    /// Copies `size` bytes to the memory starting at the address. They all fit in this memory.
    ///
    /// @param address the address of the first byte to write
    /// @param data the bytes to copy
    /// @param size the number of bytes
    virtual void write_block(word address, const byte *data, word size)
    {
        for (word i = 0; i < size; i++)
        {
            write_byte(address + i, data[i]);
        }
    }

    /// @return the number of pages in the memory
    inline word get_mem_pages() const
    {
        return m_npages;
    }

    /// @return the number of the first page of the memory
    inline word get_lo_page() const
    {
        return m_start_page;
    }

    /// @return the number of the last page of the memory
    inline word get_hi_page() const
    {
        return m_start_page + m_npages - 1;
    }

    /// @param address a physical address
    /// @return whether the address is in this memory
    inline bool in_bounds(word address) const
    {
        // Compared as pages: the end of memory at the top of the address space is 2^32, which
        // is not an address. A page below the start wraps around to a number that is larger than
        // any page count, and memory without pages has no address, so this is the whole test.
        return word((address >> kNumPageOffsetBits) - m_start_page) < m_npages;
    }

    /// @param other the memory to compare with
    /// @return whether the two memories have a page in common (memory without pages has none)
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

/// Reads a byte, half word or word (T is `byte`, `hword` or `word`) from a memory. Called with a
/// RAM or ROM the access is not virtual, called with a BaseMemory it is.
///
/// @param memory the memory to read from
/// @param address the address to read at
/// @return the value
template <class T, class Mem> inline T mem_read(Mem &memory, const word address)
{
    static_assert(std::is_unsigned_v<T> && sizeof(T) <= sizeof(word), "byte, hword or word");
    if constexpr (sizeof(T) == sizeof(byte)) return memory.read_byte(address);
    else if constexpr (sizeof(T) == sizeof(hword)) return memory.read_hword(address);
    else return memory.read_word(address);
}

/// The counterpart of mem_read.
///
/// @param memory the memory to write to
/// @param address the address to write at
/// @param value the value to store
template <class T, class Mem> inline void mem_write(Mem &memory, const word address, const T value)
{
    static_assert(std::is_unsigned_v<T> && sizeof(T) <= sizeof(word), "byte, hword or word");
    if constexpr (sizeof(T) == sizeof(byte)) memory.write_byte(address, value);
    else if constexpr (sizeof(T) == sizeof(hword)) memory.write_hword(address, value);
    else memory.write_word(address, value);
}

/// Memory that is an array of bytes in the host: the common part of RAM and ROM.
class Memory : public BaseMemory
{
  public:
    /// Makes zeroed memory.
    ///
    /// @param npages the number of pages in the memory
    /// @param start_page the page number of the first page
    Memory(word npages, word start_page);

    // The memory owns its bytes, so a copy would free them twice.
    Memory(const Memory &) = delete;
    Memory &operator=(const Memory &) = delete;
    virtual ~Memory();

    // TODO: faults could be caught with signal handlers instead of a conditional check on every
    // access to the memory.

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

    /// Reads a word at an address that is a multiple of 4 (instruction fetch).
    ///
    /// @param address the address, aligned to 4 bytes
    /// @return the word
    inline word read_word_aligned(word address)
    {
        return read_word(address);
    }

    /// @param address an address in this memory
    /// @return a pointer to the bytes from the address on
    byte *bytes_at(word address)
    {
        return m_data + (address - m_start_addr);
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

    /// Sets every byte to 0.
    void reset();

  protected:
    byte *m_data;
};

/// RAM and ROM are final so that a call on one of them is not virtual and can be inlined.
class RAM final : public Memory
{
  public:
    /// @param npages the number of pages in the RAM
    /// @param start_pages the page number of the first page
    RAM(word npages, word start_pages);
};

/// Memory that a program cannot write. The ROM object itself is writable, which is how an image
/// gets there.
class ROM final : public Memory
{
  public:
    /// Makes a ROM from an image in memory.
    ///
    /// @param data the image, of `npages` pages
    /// @param npages the number of pages in the ROM
    /// @param start_page the page number of the first page
    ROM(const byte *data, word npages, word start_page);

    /// Makes a ROM of zeros.
    ///
    /// @param npages the number of pages in the ROM
    /// @param start_page the page number of the first page
    ROM(word npages, word start_page);

    /// Loads the image in the file. The file is only read, never written back.
    ///
    /// @param file the image
    /// @param npages the number of pages in the ROM
    /// @param start_page the page number of the first page
    /// @throws ROM_Exception if the file is larger than the ROM
    ROM(File file, word npages, word start_page);
    ~ROM() override;

    /// Thrown when the image does not fit in the ROM.
    class ROM_Exception : public std::exception
    {
      private:
        std::string message;

      public:
        ROM_Exception(std::string msg);
        const char *what() const noexcept override;
    };

    // A program cannot write it: the stores of the CPU go through SystemBus::route_store, which
    // refuses the ROM.
};
