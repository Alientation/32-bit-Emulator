#pragma once

#include "emulator32bit/devices.h"
#include "emulator32bit/disk.h"
#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/memory.h"
#include "emulator32bit/virtual_memory.h"

#include <memory>
#include <vector>

class SystemBus : private VirtualMemory::PhysicalPages
{
  public:
    /// The bus owns what it is given. Uses a MockDisk, and a virtual memory over the RAM.
    SystemBus(RAM *ram, ROM *rom);
    SystemBus(RAM *ram, ROM *rom, Disk *disk, VirtualMemory *mmu);

    /// Saves the disk.
    ~SystemBus();

    SystemBus(const SystemBus &) = delete;
    SystemBus &operator=(const SystemBus &) = delete;

    /* expose for now */
    std::unique_ptr<RAM> ram;
    std::unique_ptr<ROM> rom;
    std::unique_ptr<Disk> disk;
    std::unique_ptr<VirtualMemory> mmu;

    /// The devices, at kIntcBase, kTimerBase and kConsoleBase.
    InterruptController intc;
    Timer timer{intc};
    Console console{intc};
    BlockDevice block{intc};

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
        const word ppage = address >> kNumPageOffsetBits;
        mmu->ensure_physical_page_mapping(mmu->current_process(), ppage, ppage);
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
        // Everything is translated before anything is written, so that a store that crosses into
        // a page that cannot be written leaves memory as it was.
        word real_adr[sizeof(dword)];
        for (U8 i = 0; i < n_bytes; i++)
        {
            real_adr[i] = translate_address(address + i, VirtualMemory::AccessType::WRITE);
        }
        for (U8 i = 0; i < n_bytes; i++)
        {
            route_memory(real_adr[i])->write_byte(real_adr[i], val & 0xFF);
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
        return route_memory(address)->read_hword(address);
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

    /**
     * Fetch an instruction. The address must be word aligned, which the caller checks.
     *
     * @param address The virtual address of the instruction.
     * @throws VirtualMemory::PageFaultException if the address is unmapped or not executable.
     * @throws SystemBus::Exception if the instruction is not in RAM.
     */
    inline word fetch_instruction(const word address)
    {
        // The page tables say whether a page can be executed; the swapping memory has its own list.
        const bool walk = mmu->walk_enabled();
        const word real_addr = translate_address(address, walk ? VirtualMemory::AccessType::EXECUTE
                                                               : VirtualMemory::AccessType::READ);
        if (UNLIKELY(!walk && !mmu->can_execute_address(address)))
        {
            VirtualMemory::throw_fault(VirtualMemory::PageFaultException::Reason::EXECUTE_DENIED,
                                       address >> kNumPageOffsetBits,
                                       VirtualMemory::AccessType::EXECUTE);
        }
        if (UNLIKELY(!ram->in_bounds(real_addr)))
        {
            // Code can also run from the ROM, which is where a machine boots from.
            if (rom->in_bounds(real_addr))
            {
                return rom->read_word_aligned(real_addr);
            }
            throw Exception("Instruction fetch outside of RAM and ROM at address "
                            + std::to_string(address));
        }
        return ram->read_word_aligned(real_addr);
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
        const word real_addr = translate_address(address, VirtualMemory::AccessType::WRITE);
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
            const word real_addr = translate_address(address, VirtualMemory::AccessType::WRITE);
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
            const word real_addr = translate_address(address, VirtualMemory::AccessType::WRITE);
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

    // The pages that the virtual memory pages in and out. A page is in one of the memories.
    void read_page(word ppage, byte *out) override
    {
        const word address = ppage << kNumPageOffsetBits;
        route_memory(address)->read_block(address, out, kPageSize);
    }

    void write_page(word ppage, const byte *data) override
    {
        const word address = ppage << kNumPageOffsetBits;
        route_memory(address)->write_block(address, data, kPageSize);
    }

    // For the page table walker: a missing address is reported to it, not thrown.
    bool read_physical_word(word address, word &out) override
    {
        if (UNLIKELY(address % sizeof(word) != 0
                     || !(ram->in_bounds(address) || rom->in_bounds(address)
                          || disk->in_bounds(address))))
        {
            return false;
        }
        out = route_memory(address)->read_word(address);
        return true;
    }

    bool write_physical_word(word address, word value) override
    {
        if (UNLIKELY(address % sizeof(word) != 0 || !ram->in_bounds(address)))
        {
            return false;
        }
        ram->write_word(address, value);
        return true;
    }

    inline word
    translate_address(word address,
                      VirtualMemory::AccessType access = VirtualMemory::AccessType::READ)
    {
        return mmu->translate_address(address, access);
    }

    inline BaseMemory *route_memory(const word address)
    {
        // TODO: 'Likely' specifiers would help
        if (ram->in_bounds(address))
        {
            return ram.get();
        }
        else if (rom->in_bounds(address))
        {
            return rom.get();
        }
        else if (disk->in_bounds(address))
        {
            return disk.get();
        }
        else if (address >= kDeviceBase)
        {
            for (Device *device : {static_cast<Device *>(&intc), static_cast<Device *>(&timer),
                                   static_cast<Device *>(&console), static_cast<Device *>(&block)})
            {
                if (device->in_bounds(address))
                {
                    return device;
                }
            }
            throw Exception("Could not route address " + std::to_string(address) + " to memory.");
        }
        else
        {
            throw Exception("Could not route address " + std::to_string(address) + " to memory.");
        }
    }
};
