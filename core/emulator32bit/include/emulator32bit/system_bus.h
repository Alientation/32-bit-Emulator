#pragma once

#include "emulator32bit/devices.h"
#include "emulator32bit/disk.h"
#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/memory.h"
#include "emulator32bit/physical_pages.h"

#include <array>
#include <memory>
#include <stdexcept>
#include <string>

/**
 * The physical address space of the machine: it decodes an address to the RAM, the ROM, the disk
 * or one of the devices, and owns all of them. Every address on the bus is physical. Translating
 * the addresses of a program is the job of the MMU (VirtualMemory) and of the MemoryPort that
 * connects the CPU to both.
 *
 * It is also the physical memory that the MMU swaps pages in and out of, and that its page table
 * walker reads, which is what PhysicalPages is.
 */
class SystemBus : public PhysicalPages
{
  public:
    /// The bus owns what it is given. Without a disk it uses a MockDisk.
    SystemBus(std::unique_ptr<RAM> ram, std::unique_ptr<ROM> rom);
    SystemBus(std::unique_ptr<RAM> ram, std::unique_ptr<ROM> rom, std::unique_ptr<Disk> disk);

    /// Saves the disk and the block device.
    ~SystemBus() override;

    SystemBus(const SystemBus &) = delete;
    SystemBus &operator=(const SystemBus &) = delete;

    /* expose for now */
    std::unique_ptr<RAM> ram;
    std::unique_ptr<ROM> rom;
    std::unique_ptr<Disk> disk;

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

    /// The RAM, ROM or disk that has the address, or null.
    BaseMemory *find_storage(word address);

    /// The memory or device that has the address, or null.
    BaseMemory *find_memory(word address);

    /// The memory or device that has the address.
    /// @throws SystemBus::Exception if there is none.
    BaseMemory &route_memory(word address);

    /// The memory or device that a store by a program goes to: route_memory, but the ROM is read
    /// only. (The loader and the host write the ROM through write_block or the ROM itself, that
    /// is how an image gets there.)
    /// @throws SystemBus::Exception if there is none, or the address is in the ROM.
    BaseMemory &route_store(word address);

    /// Writes bytes to a physical address. A block can cross from one page to another.
    /// @throws SystemBus::Exception if there is no memory somewhere in it.
    void write_block(word address, const byte *data, word size);

    /// Resets the RAM and the devices. The ROM and the disk are left as they are.
    void reset();

    // The pages that the virtual memory pages in and out. A page is in one of the memories.
    void read_page(word ppage, byte *out) override;
    void write_page(word ppage, const byte *data) override;
    byte *direct_page(word ppage) override;

    // For the page table walker: a missing address is reported to it, not thrown. A device is
    // not read, that could have an effect.
    bool read_physical_word(word address, word &out) override;
    bool write_physical_word(word address, word value) override;

  private:
    void validate_memory();

    const std::array<Device *, 4> m_devices{&intc, &timer, &console, &block};
};
