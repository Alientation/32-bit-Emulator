#pragma once

#include "emulator32bit/devices.h"
#include "emulator32bit/disk.h"
#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/memory.h"
#include "emulator32bit/virtual_memory.h"

#include <array>
#include <memory>
#include <stdexcept>
#include <string>

class SystemBus : private VirtualMemory::PhysicalPages
{
  public:
    /// The bus owns what it is given. Without a disk it uses a MockDisk, and without a virtual
    /// memory it makes one over the frames of the RAM.
    SystemBus(std::unique_ptr<RAM> ram, std::unique_ptr<ROM> rom);
    SystemBus(std::unique_ptr<RAM> ram, std::unique_ptr<ROM> rom, std::unique_ptr<Disk> disk);
    SystemBus(std::unique_ptr<RAM> ram, std::unique_ptr<ROM> rom, std::unique_ptr<Disk> disk,
              std::unique_ptr<VirtualMemory> mmu);

    /// Saves the disk and the block device.
    ~SystemBus();

    SystemBus(const SystemBus &) = delete;
    SystemBus &operator=(const SystemBus &) = delete;

    /* expose for now */
    std::unique_ptr<RAM> ram;
    std::unique_ptr<ROM> rom;
    std::unique_ptr<Disk> disk;
    std::unique_ptr<VirtualMemory> mmu;

    /// The devices, at kIntcBase, kTimerBase, kConsoleBase and kBlockBase.
    InterruptController intc;
    Timer timer{intc};
    Console console{intc};
    BlockDevice block{intc};

    /// An address that no memory or device answers to.
    class Exception : public std::runtime_error
    {
      public:
        using std::runtime_error::runtime_error;
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
     * Read a byte, half word or word (T is `byte`, `hword` or `word`) at a virtual address. The
     * value is little endian in memory, and may cross a page.
     *
     * @throws VirtualMemory::PageFaultException if the access is not allowed.
     * @throws SystemBus::Exception if no memory is at the physical address.
     */
    template <class T> inline T read(const word address)
    {
        if constexpr (sizeof(T) > sizeof(byte))
        {
            if (UNLIKELY(!is_within_page(address, sizeof(T)))) return T(read_val(address, sizeof(T)));
        }

        const word real_addr = translate_address(address);
        if (LIKELY(ram->in_bounds(real_addr))) return mem_read<T>(*ram, real_addr);
        return mem_read<T>(route_memory(real_addr), real_addr);
    }

    /**
     * Write a byte, half word or word (T is `byte`, `hword` or `word`) at a virtual address. The
     * value is little endian in memory, and may cross a page. A store that crosses into a page
     * that cannot be written leaves memory as it was.
     *
     * @throws VirtualMemory::PageFaultException if the access is not allowed.
     * @throws SystemBus::Exception if no memory is at the physical address.
     */
    template <class T> inline void write(const word address, const T data)
    {
        if constexpr (sizeof(T) > sizeof(byte))
        {
            if (UNLIKELY(!is_within_page(address, sizeof(T))))
            {
                write_val(address, data, sizeof(T));
                return;
            }
        }

        const word real_addr = translate_address(address, VirtualMemory::AccessType::WRITE);
        if (LIKELY(ram->in_bounds(real_addr)))
        {
            mem_write<T>(*ram, real_addr, data);
            return;
        }
        mem_write<T>(route_memory(real_addr), real_addr, data);
    }

    inline byte read_byte(const word address)
    {
        return read<byte>(address);
    }

    inline hword read_hword(const word address)
    {
        return read<hword>(address);
    }

    inline word read_word(const word address)
    {
        return read<word>(address);
    }

    inline void write_byte(const word address, const byte data)
    {
        write<byte>(address, data);
    }

    inline void write_hword(const word address, const hword data)
    {
        write<hword>(address, data);
    }

    inline void write_word(const word address, const word data)
    {
        write<word>(address, data);
    }

    /// Writes a byte to a physical address, which the current process gets mapped to itself.
    inline void write_unmapped_byte(const word address, const byte data)
    {
        ensure_unmapped_mapping(address);
        route_memory(address).write_byte(address, data);
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
        if (LIKELY(ram->in_bounds(real_addr))) return ram->read_word_aligned(real_addr);
        return fetch_outside_ram(real_addr, address);
    }

    /// Resets the RAM and the devices. The ROM, the disk and the MMU are left as they are.
    void reset();

  private:
    /// What the constructors do once they own the memories: checks the layout and tells the
    /// virtual memory and the block device where the physical memory is.
    void attach();

    void validate_memory();

    /// A value that crosses a page, put together byte by byte. The pages are translated one by
    /// one, and the stores only after all of them were.
    dword read_val(word address, U8 n_bytes);
    void write_val(word address, dword val, U8 n_bytes);

    /// What is left of a fetch that is not in RAM: the ROM, or an exception.
    word fetch_outside_ram(word real_addr, word address);

    // The pages that the virtual memory pages in and out. A page is in one of the memories.
    void read_page(word ppage, byte *out) override
    {
        const word address = ppage << kNumPageOffsetBits;
        route_memory(address).read_block(address, out, kPageSize);
    }

    void write_page(word ppage, const byte *data) override
    {
        const word address = ppage << kNumPageOffsetBits;
        route_memory(address).write_block(address, data, kPageSize);
    }

    // For the page table walker: a missing address is reported to it, not thrown. A device is
    // not read, that could have an effect.
    bool read_physical_word(word address, word &out) override
    {
        BaseMemory *memory = address % sizeof(word) == 0 ? find_storage(address) : nullptr;
        if (UNLIKELY(memory == nullptr)) return false;
        out = memory->read_word(address);
        return true;
    }

    bool write_physical_word(word address, word value) override
    {
        if (UNLIKELY(address % sizeof(word) != 0 || !ram->in_bounds(address))) return false;
        ram->write_word(address, value);
        return true;
    }

    inline word
    translate_address(word address,
                      VirtualMemory::AccessType access = VirtualMemory::AccessType::READ)
    {
        return mmu->translate_address(address, access);
    }

    /// The RAM, ROM or disk that has the address, or null.
    BaseMemory *find_storage(word address);

    /// The memory or device that has the address, or null.
    BaseMemory *find_memory(word address);

    /// The memory or device that has the address.
    /// @throws SystemBus::Exception if there is none.
    BaseMemory &route_memory(word address);

    const std::array<Device *, 4> m_devices{&intc, &timer, &console, &block};
};
