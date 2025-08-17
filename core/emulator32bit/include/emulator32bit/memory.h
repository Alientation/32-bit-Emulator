#pragma once

#include "emulator32bit/emulator32bit_util.h"
#include "util/file.h"

#include <string>

class BaseMemory
{
  public:
    BaseMemory (word npages, word start_page);
    virtual ~BaseMemory ();

    virtual inline byte read_byte (word address) = 0;
    virtual inline hword read_hword (word address) = 0;
    virtual inline word read_word (word address) = 0;
    virtual inline void write_byte (word address, byte value) = 0;
    virtual inline void write_hword (word address, hword value) = 0;
    virtual inline void write_word (word address, word value) = 0;

    inline word get_mem_pages ()
    {
        return m_npages;
    }

    inline word get_lo_page ()
    {
        return m_start_page;
    }

    inline word get_hi_page ()
    {
        return m_start_page + m_npages - 1;
    }

    inline bool in_bounds (word address)
    {
        return address >= (m_start_page << kNumPageOffsetBits)
               && address < ((get_hi_page () + 1) << kNumPageOffsetBits);
    }

  protected:
    word m_npages;
    word m_start_page;
    word m_start_addr;
};

class Memory : public BaseMemory
{
  public:
    Memory (word npages, word start_page);
    Memory (Memory &other);
    virtual ~Memory ();

    // FOR SEG FAULTS, WE CAN USE SIGNAL HANDLERS TO CATCH AND HANDLE THEM IN THE KERNEL, WITHOUT
    // HAVING AN EXPENSIVE CONDITIONAL CHECK EVERYTIME MEMORY IS ACCESSED
    inline byte read_byte (word address) override
    {
        return m_data[address - (m_start_page << kNumPageOffsetBits)];
    }

    inline hword read_hword (word address)
    {
        address -= m_start_addr;
        return ((hword *) (m_data + (address & 1)))[address >> 1];
    }

    inline word read_word (word address)
    {
        address -= m_start_addr;
        return ((word *) (m_data + (address & 0b11)))[address >> 2];
    }

    virtual inline word read_word_aligned (word address)
    {
        return ((word *) m_data)[(address - m_start_addr) >> 2];
    }

    inline void write_byte (word address, byte value)
    {
        m_data[address - (m_start_page << kNumPageOffsetBits)] = value;
    }

    inline void write_hword (word address, hword value)
    {
        address -= m_start_addr;
        ((hword *) (m_data + (address & 1)))[address >> 1] = value;
    }

    inline void write_word (word address, word value)
    {
        address -= m_start_addr;
        ((word *) (m_data + (address & 0b11)))[address >> 2] = value;
    }

    void reset ();

  protected:
    byte *m_data;

    friend class MMU;
};

class RAM : public Memory
{
  public:
    RAM (word npages, word start_pages);
};

class ROM : public Memory
{
  public:
    ROM (const byte *data, word npages, word start_page);
    ROM (word npages, word start_page);
    ROM (File file, word npages, word start_page);
    ~ROM () override;

    class ROM_Exception : public std::exception
    {
      private:
        std::string message;

      public:
        ROM_Exception (std::string msg);
        const char *what () const noexcept override;
    };

    // TODO: prevent writes, have special way to flash memory

  private:
    bool m_save_file = false;
    File m_file;
};
