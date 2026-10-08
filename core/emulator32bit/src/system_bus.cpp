#include "emulator32bit/system_bus.h"

#include "util/logger.h"

#include <algorithm>
#include <iterator>

namespace
{

[[noreturn, gnu::cold]] void throw_unroutable(const word address)
{
    throw SystemBus::Exception("Could not route address " + std::to_string(address)
                               + " to memory.");
}

} // namespace

SystemBus::SystemBus(std::unique_ptr<RAM> ram, std::unique_ptr<ROM> rom) :
    SystemBus(std::move(ram), std::move(rom), std::make_unique<MockDisk>())
{
}

SystemBus::SystemBus(std::unique_ptr<RAM> ram, std::unique_ptr<ROM> rom,
                     std::unique_ptr<Disk> disk) :
    ram(std::move(ram)),
    rom(std::move(rom)),
    disk(std::move(disk))
{
    validate_memory();
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

void SystemBus::write_block(word address, const byte *data, word size)
{
    // Page by page, the pages of a block do not have to be in the same memory.
    while (size != 0)
    {
        const word in_page = std::min<word>(size, kPageSize - (address & (kPageSize - 1)));
        route_memory(address).write_block(address, data, in_page);
        address += in_page;
        data += in_page;
        size -= in_page;
    }
}

void SystemBus::reset()
{
    ram->reset();
    intc.reset();
    timer.reset();
    console.reset();
    block.reset();
}

void SystemBus::read_page(const word ppage, byte *out)
{
    const word address = ppage << kNumPageOffsetBits;
    route_memory(address).read_block(address, out, kPageSize);
}

void SystemBus::write_page(const word ppage, const byte *data)
{
    const word address = ppage << kNumPageOffsetBits;
    route_memory(address).write_block(address, data, kPageSize);
}

bool SystemBus::read_physical_word(const word address, word &out)
{
    BaseMemory *memory = address % sizeof(word) == 0 ? find_storage(address) : nullptr;
    if (UNLIKELY(memory == nullptr)) return false;
    out = memory->read_word(address);
    return true;
}

bool SystemBus::write_physical_word(const word address, const word value)
{
    if (UNLIKELY(address % sizeof(word) != 0 || !ram->in_bounds(address))) return false;
    ram->write_word(address, value);
    return true;
}
