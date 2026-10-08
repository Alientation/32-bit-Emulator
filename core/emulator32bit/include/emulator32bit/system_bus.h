#pragma once

#include "emulator32bit/disk.h"
#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/memory.h"
#include "emulator32bit/virtual_memory.h"

#include <vector>

class SystemBus
{
  public:
    SystemBus(RAM *ram, ROM *rom);
    SystemBus(RAM *ram, ROM *rom, Disk *disk, VirtualMemory *mmu);
    ~SystemBus();

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
        Exception(const std::string &msg);

        const char *what() const noexcept override;
    };

    inline void ensure_unmapped_mapping(word address)
    {
        VirtualMemory::Exception exception;
        word ppage = address >> kNumPageOffsetBits;
        mmu->ensure_physical_page_mapping(mmu->current_process(), ppage, ppage, exception);

        if (UNLIKELY(exception.type != VirtualMemory::Exception::Type::AOK))
        {
            handle_mmu_exception(exception);
        }
    }

    /**
     * Check if data at an address is in one page. This allows using a single virtual address
     * translation.
     *
     * @param address The start address (lowest address of the block of data).
     * @param n_bytes The size of the data. Data is located at [address, address + n_bytes - 1].
     * @return True of the data lies within one page, False otherwise.
     */
    static constexpr bool is_within_page(const word address, const U8 n_bytes)
    {
        return (address >> kNumPageOffsetBits) == ((address + n_bytes - 1) >> kNumPageOffsetBits);
    }

    /**
     * Read a 1-8 byte value from system bus. The data is assumed to be in little endian format.
     *
     * @param address The start address (lowest address of the block of data).
     * @param n_bytes The size of the data. Data is located at [address, address + n_bytes - 1].
     * @return The value read from memory. If less than 8 bytes are read, the read value is stored
     *         in the lower bytes of the return value.
     */
    inline dword read_val(const word address, const U8 n_bytes)
    {
        dword val = 0;
        for (U8 i = 0; i < n_bytes; i++)
        {
            const word real_addr = translate_address(address + n_bytes - i - 1);
            val = (val << 8) + route_memory(real_addr)->read_byte(real_addr);
        }
        return val;
    }

    /**
     * Write a 1-8 byte value to system bus. The data will be written in little endian format.
     *
     * @param address The start address (lowest address of the block of data).
     * @param val The value to write. If less than 8 bytes are to be written, the write value is
     *            stored in the lower bytes of this value.
     * @param n_bytes The size of the data. Data will be written to [address, address + n_bytes - 1].
     */
    inline void write_val(const word address, dword val, const U8 n_bytes)
    {
        for (U8 i = 0; i < n_bytes; i++)
        {
            const word real_adr = translate_address(address + i);
            route_memory(real_adr)->write_byte(real_adr, val & 0xFF);
            val >>= 8;
        }
    }

    /**
     * Read a byte from the system bus.
     *
     * @param address The address of the byte to read.
     * @return The byte read.
     */
    inline byte read_byte(const word address)
    {
        const word real_addr = translate_address(address);
        return route_memory(real_addr)->read_byte(real_addr);
    }

    /**
     * Read an unmapped byte from the system bus.
     *
     * @param address The unmapped address of the byte to read.
     * @return The byte read.
     */
    inline byte read_unmapped_byte(const word address)
    {
        ensure_unmapped_mapping(address);
        return route_memory(address)->read_byte(address);
    }

    inline hword read_hword(const word address)
    {
        if (LIKELY(is_within_page(address, sizeof(hword))))
        {
            const word real_addr = translate_address(address);
            return route_memory(real_addr)->read_hword(real_addr);
        }

        return read_val(address, sizeof(hword));
    }

    inline hword read_unmapped_hword(const word address)
    {
        ensure_unmapped_mapping(address);
        return route_memory(address)->read_word(address);
    }

    inline word read_word(const word address)
    {
        if (LIKELY(is_within_page(address, sizeof(word))))
        {
            const word real_addr = translate_address(address);
            return route_memory(real_addr)->read_word(real_addr);
        }

        return read_val(address, sizeof(word));
    }

    inline word read_unmapped_word(const word address)
    {
        ensure_unmapped_mapping(address);
        return route_memory(address)->read_word(address);
    }

    inline word read_word_aligned_ram(const word address)
    {
        return ram->read_word_aligned(translate_address(address));
    }

    inline word read_unmapped_word_aligned_ram(const word address)
    {
        return ram->read_word_aligned(address);
    }

    /**
     * Write a byte to the system bus
     *
     * @param address The address to write to
     * @param exception The exception raised by the write operation
     * @param data The byte to write
     */
    inline void write_byte(const word address, const byte data)
    {
        const word real_addr = translate_address(address);
        route_memory(real_addr)->write_byte(real_addr, data);
    }

    inline void write_unmapped_byte(const word address, const byte data)
    {
        ensure_unmapped_mapping(address);
        route_memory(address)->write_byte(address, data);
    }

    inline void write_hword(const word address, const hword data)
    {
        if (LIKELY(is_within_page(address, sizeof(data))))
        {
            const word real_addr = translate_address(address);
            route_memory(real_addr)->write_hword(real_addr, data);
        }
        else
        {
            write_val(address, data, sizeof(data));
        }
    }

    inline void write_unmapped_hword(const word address, const hword data)
    {
        ensure_unmapped_mapping(address);
        route_memory(address)->write_hword(address, data);
    }

    inline void write_word(const word address, const word data)
    {
        if (LIKELY(is_within_page(address, sizeof(data))))
        {
            const word real_addr = translate_address(address);
            route_memory(real_addr)->write_word(real_addr, data);
        }
        else
        {
            write_val(address, data, sizeof(data));
        }
    }

    inline void write_unmapped_word(const word address, const word data)
    {
        ensure_unmapped_mapping(address);
        route_memory(address)->write_word(address, data);
    }

    void reset();

  private:
    void validate_memory();

    inline void handle_mmu_exception(VirtualMemory::Exception &exception)
    {
        if (exception.type == VirtualMemory::Exception::Type::DISK_RETURN_AND_FETCH_SUCCESS)
        {
            exception.type =
                VirtualMemory::Exception::Type::DISK_FETCH_SUCCESS; /* so the next conditional can
                                                                       handle */

            std::vector<byte> bytes(kPageSize);

            // EXPECTS page to be part of single memory target
            word p_addr = exception.ppage_return << kNumPageOffsetBits;
            BaseMemory *target = route_memory(p_addr);

            for (word i = 0; i < kPageSize; i++)
            {
                bytes.at(i) = target->read_byte(p_addr + i);
            }

            mmu->m_disk->write_page(exception.disk_page_return, bytes);
        }

        if (exception.type == VirtualMemory::Exception::Type::DISK_FETCH_SUCCESS)
        {
            /* handle exception by writing page fetched from disk to memory */
            word paddr = exception.ppage_fetch << kNumPageOffsetBits;

            // EXPECTS page to be part of single memory target
            BaseMemory *target = route_memory(paddr);

            for (word i = 0; i < kPageSize; i++)
            {
                target->write_byte(paddr + i, exception.disk_fetch.at(i));
            }
        }
    }

    inline word translate_address(word address)
    {
        VirtualMemory::Exception exception;
        word addr = mmu->translate_address(address, exception);

        if (exception.type != VirtualMemory::Exception::Type::AOK)
        {
            handle_mmu_exception(exception);
        }

        return addr;
    }

    inline BaseMemory *route_memory(const word address)
    {
        // TODO: 'Likely' specifiers would help
        if (ram->in_bounds(address))
        {
            return ram;
        }
        else if (rom->in_bounds(address))
        {
            return rom;
        }
        else if (disk->in_bounds(address))
        {
            return disk;
        }
        else
        {
            throw Exception("Could not route address " + std::to_string(address) + " to memory.");
        }
    }
};
