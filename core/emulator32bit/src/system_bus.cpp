#include "emulator32bit/system_bus.h"

#include "util/logger.h"

#include <iterator>

namespace
{

[[noreturn, gnu::cold]] void throw_unroutable(const word address)
{
    throw SystemBus::Exception("Could not route address " + std::to_string(address)
                               + " to memory.");
}

} // namespace

SystemBus::SystemBus(RAM *ram, ROM *rom) :
    SystemBus(ram, rom, new MockDisk())
{
}

SystemBus::SystemBus(RAM *ram, ROM *rom, Disk *disk) :
    SystemBus(ram, rom, disk,
              new VirtualMemory(disk, ram->get_lo_page(), ram->get_mem_pages()))
{
}

SystemBus::SystemBus(RAM *ram, ROM *rom, Disk *disk, VirtualMemory *mmu) :
    ram(ram),
    rom(rom),
    disk(disk),
    mmu(mmu)
{
    validate_memory();
    this->mmu->set_physical_pages(this);
    block.set_dma_memory(this->ram.get());
}

SystemBus::~SystemBus()
{
    // A destructor must not throw, and saving fails with an exception when the fatal action is
    // Throw. A failure to save the disk does not keep the block device from being saved.
    try
    {
        disk->save();
    }
    catch (const std::exception &error)
    {
        AEMU_ERROR("SystemBus::~SystemBus() - The disk could not be saved: {}", error.what());
    }

    try
    {
        if (!block.save())
        {
            AEMU_ERROR("SystemBus::~SystemBus() - The block device could not be saved.");
        }
    }
    catch (const std::exception &error)
    {
        AEMU_ERROR("SystemBus::~SystemBus() - The block device could not be saved: {}",
                   error.what());
    }
}

void SystemBus::validate_memory()
{
    struct Named
    {
        const char *name;
        const BaseMemory *memory;
    };
    const Named memories[] = {{"RAM", ram.get()}, {"ROM", rom.get()}, {"DISK", disk.get()}};

    for (size_t i = 0; i < std::size(memories); i++)
    {
        for (size_t j = i + 1; j < std::size(memories); j++)
        {
            const auto &[a_name, a] = memories[i];
            const auto &[b_name, b] = memories[j];
            AEMU_CHECK(!a->overlap(*b),
                       "Invalid memory layout. {} overlaps with {} memory. ({},{}) U ({},{})",
                       a_name, b_name, a->get_lo_page(), a->get_hi_page(), b->get_lo_page(),
                       b->get_hi_page());
        }
    }

    for (const auto &[name, memory] : memories)
    {
        AEMU_CHECK(memory->get_mem_pages() == 0
                       || U64(memory->get_lo_page()) + memory->get_mem_pages()
                              <= (kDeviceBase >> kNumPageOffsetBits),
                   "Invalid memory layout. {} ({},{}) overlaps the devices, which start at "
                   "{:#x}.",
                   name, memory->get_lo_page(), memory->get_hi_page(), kDeviceBase);
    }
}

dword SystemBus::read_val(const word address, const U8 n_bytes)
{
    dword val = 0;
    for (U8 i = 0; i < n_bytes; i++)
    {
        const word real_addr = translate_address(address + n_bytes - i - 1);
        val = (val << 8) + route_memory(real_addr).read_byte(real_addr);
    }
    return val;
}

void SystemBus::write_val(const word address, dword val, const U8 n_bytes)
{
    // Everything is translated before anything is written, so that a store that crosses into
    // a page that cannot be written leaves memory as it was.
    word real_addr[sizeof(dword)];
    for (U8 i = 0; i < n_bytes; i++)
    {
        real_addr[i] = translate_address(address + i, VirtualMemory::AccessType::WRITE);
    }
    for (U8 i = 0; i < n_bytes; i++)
    {
        route_memory(real_addr[i]).write_byte(real_addr[i], val & 0xFF);
        val >>= 8;
    }
}

word SystemBus::fetch_outside_ram(const word real_addr, const word address)
{
    // Code can also run from the ROM, which is where a machine boots from.
    if (rom->in_bounds(real_addr)) return rom->read_word_aligned(real_addr);

    throw Exception("Instruction fetch outside of RAM and ROM at address "
                    + std::to_string(address));
}

BaseMemory *SystemBus::find_storage(const word address)
{
    if (ram->in_bounds(address)) return ram.get();
    if (rom->in_bounds(address)) return rom.get();
    if (disk->in_bounds(address)) return disk.get();
    return nullptr;
}

BaseMemory *SystemBus::find_memory(const word address)
{
    if (BaseMemory *storage = find_storage(address)) return storage;

    if (address >= kDeviceBase)
    {
        for (Device *device : m_devices)
        {
            if (device->in_bounds(address)) return device;
        }
    }
    return nullptr;
}

BaseMemory &SystemBus::route_memory(const word address)
{
    BaseMemory *memory = find_memory(address);
    if (UNLIKELY(memory == nullptr)) throw_unroutable(address);
    return *memory;
}

void SystemBus::reset()
{
    ram->reset();
    intc.reset();
    timer.reset();
    console.reset();
    block.reset();
}
