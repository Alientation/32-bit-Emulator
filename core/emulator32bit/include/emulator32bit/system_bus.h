#pragma once

#include "emulator32bit/disk.h"
#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/memory.h"
#include "emulator32bit/virtual_memory.h"

#include <vector>

class SystemBus
{
  public:
    SystemBus (RAM *ram, ROM *rom);
    SystemBus (RAM *ram, ROM *rom, Disk *disk, VirtualMemory *mmu);
    ~SystemBus ();

    /* expose for now */
    RAM *ram;
    ROM *rom;
    Disk *disk;
    VirtualMemory *mmu;

    class Exception : public std::exception
    {
      private:
        std::string message;

      public:
        Exception (const std::string &msg);

        const char *what () const noexcept override;
    };

    inline dword read_val (word address, int n_bytes)
    {
        dword val = 0;
        for (int i = 0; i < n_bytes; i++)
        {
            val <<= 8;
            word real_adr = address;
            real_adr = translate_address (address + n_bytes - i - 1);
            BaseMemory *target = route_memory (real_adr);
            val += target->read_byte (real_adr);
        }
        return val;
    }

    inline void ensure_unmapped_mapping (word address)
    {
        VirtualMemory::Exception exception;
        word ppage = address >> kNumPageOffsetBits;
        mmu->ensure_physical_page_mapping (mmu->current_process (), ppage, ppage, exception);

        if (exception.type != VirtualMemory::Exception::Type::AOK)
        {
            handle_mmu_exception (exception);
        }
    }

    /**
     * Read a byte from the system bus
     *
     * @param address The address to read from
     * @return The byte read from the address
     */
    inline byte read_byte (word address)
    {
        address = translate_address (address);
        return route_memory (address)->read_byte (address);
    }

    inline byte read_unmapped_byte (word address)
    {
        ensure_unmapped_mapping (address);
        return route_memory (address)->read_byte (address);
    }

    inline hword read_hword (word address)
    {
        if ((address >> kNumPageOffsetBits) == ((address + 1) >> kNumPageOffsetBits))
        {
            address = translate_address (address);
            return route_memory (address)->read_hword (address);
        }

        return read_val (address, 2);
    }

    inline hword read_unmapped_hword (word address)
    {
        ensure_unmapped_mapping (address);
        return route_memory (address)->read_word (address);
    }

    inline word read_word (word address)
    {
        if ((address >> kNumPageOffsetBits) == ((address + 3) >> kNumPageOffsetBits))
        {
            address = translate_address (address);
            return route_memory (address)->read_word (address);
        }

        return read_val (address, 4);
    }

    inline word read_unmapped_word (word address)
    {
        ensure_unmapped_mapping (address);
        return route_memory (address)->read_word (address);
    }

    inline word read_word_aligned_ram (word address)
    {
        return ram->read_word_aligned (translate_address (address));
    }

    inline word read_unmapped_word_aligned_ram (word address)
    {
        return ram->read_word_aligned (address);
    }

    /**
     * Write a byte to the system bus
     *
     * @param address The address to write to
     * @param exception The exception raised by the write operation
     * @param data The byte to write
     */
    inline void write_byte (word address, byte data)
    {
        address = translate_address (address);
        route_memory (address)->write_byte (address, data);
    }

    inline void write_unmapped_byte (word address, byte data)
    {
        ensure_unmapped_mapping (address);
        route_memory (address)->write_byte (address, data);
    }

    inline void write_hword (word address, hword data)
    {
        if ((address >> kNumPageOffsetBits) == ((address + 1) >> kNumPageOffsetBits))
        {
            address = translate_address (address);
            route_memory (address)->write_hword (address, data);
        }
        else
        {
            write_val (address, data, 2);
        }
    }

    inline void write_unmapped_hword (word address, hword data)
    {
        ensure_unmapped_mapping (address);
        route_memory (address)->write_hword (address, data);
    }

    inline void write_word (word address, word data)
    {
        if ((address >> kNumPageOffsetBits) == ((address + 3) >> kNumPageOffsetBits))
        {
            address = translate_address (address);
            route_memory (address)->write_word (address, data);
        }
        else
        {
            write_val (address, data, 4);
        }
    }

    inline void write_unmapped_word (word address, word data)
    {
        ensure_unmapped_mapping (address);
        route_memory (address)->write_word (address, data);
    }

    inline void write_val (word address, dword val, int n_bytes)
    {
        for (int i = 0; i < n_bytes; i++)
        {
            word real_adr = address;
            real_adr = translate_address (address + i);
            BaseMemory *target = route_memory (real_adr);
            target->write_byte (real_adr, val & 0xFF);
            val >>= 8;
        }
    }

    void reset ();

  private:
    inline void handle_mmu_exception (VirtualMemory::Exception &exception)
    {
        if (exception.type == VirtualMemory::Exception::Type::DISK_RETURN_AND_FETCH_SUCCESS)
        {
            exception.type =
                VirtualMemory::Exception::Type::DISK_FETCH_SUCCESS; /* so the next conditional can
                                                                       handle */

            std::vector<byte> bytes (kPageSize);

            // EXPECTS page to be part of single memory target
            word p_addr = exception.ppage_return << kNumPageOffsetBits;
            BaseMemory *target = route_memory (p_addr);

            for (word i = 0; i < kPageSize; i++)
            {
                bytes.at (i) = target->read_byte (p_addr + i);
            }

            mmu->m_disk->write_page (exception.disk_page_return, bytes);
        }

        if (exception.type == VirtualMemory::Exception::Type::DISK_FETCH_SUCCESS)
        {
            /* handle exception by writing page fetched from disk to memory */
            word paddr = exception.ppage_fetch << kNumPageOffsetBits;

            // EXPECTS page to be part of single memory target
            BaseMemory *target = route_memory (paddr);

            for (word i = 0; i < kPageSize; i++)
            {
                target->write_byte (paddr + i, exception.disk_fetch.at (i));
            }
        }
    }

    inline word translate_address (word address)
    {
        VirtualMemory::Exception exception;
        word addr = mmu->translate_address (address, exception);

        if (exception.type != VirtualMemory::Exception::Type::AOK)
        {
            handle_mmu_exception (exception);
        }

        return addr;
    }

    inline BaseMemory *route_memory (const word address)
    {
        if (ram->in_bounds (address))
        {
            return ram;
        }
        else if (rom->in_bounds (address))
        {
            return rom;
        }
        else if (disk->in_bounds (address))
        {
            return disk;
        }
        else
        {
            throw Exception ("Could not route address " + std::to_string (address) + " to memory.");
        }
    }
};
