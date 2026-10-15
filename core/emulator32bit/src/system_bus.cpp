#include "emulator32bit/system_bus.h"

#include "util/logger.h"

#include <algorithm>
#include <iterator>

[[noreturn, gnu::cold]] static void throw_unroutable(const word address)
{
    throw SystemBus::BusError("Could not route address " + std::to_string(address) + " to memory.");
}

[[noreturn, gnu::cold]] static void throw_read_only(const word address)
{
    throw SystemBus::BusError("Could not write to address " + std::to_string(address)
                              + ", it is in the ROM.");
}

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
    display.set_ram(this->ram.get());
    timer.set_clock(&clock);
    block.set_clock(&clock);
    display.set_clock(&clock);
    // So that the clock sees a request right after the store that made it.
    sysctl.set_clock(&clock);
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
        AEMU_ERROR("The disk could not be saved: {}", error.what());
    }

    try
    {
        if (!block.save())
        {
            AEMU_ERROR("The block device could not be saved.");
        }
    }
    catch (const std::exception &error)
    {
        AEMU_ERROR("The block device could not be saved: {}", error.what());
    }
}

void SystemBus::validate_memory() const
{
    struct Named
    {
        const char *name;
        const BaseMemory *memory;
    };

    const Named memories[] = {
        {.name = "RAM", .memory = ram.get()},
        {.name = "ROM", .memory = rom.get()},
        {.name = "DISK", .memory = disk.get()},
    };

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

BaseMemory *SystemBus::find_storage(const word address) const
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

BaseMemory &SystemBus::route_store(const word address)
{
    BaseMemory &memory = route_memory(address);
    if (UNLIKELY(&memory == rom.get())) throw_read_only(address);
    return memory;
}

void SystemBus::write_block(word address, const byte *data, word size)
{
    // Page by page, the pages of a block do not have to be in the same memory.
    while (size != 0)
    {
        const word left_in_page = kPageSize - (address & (kPageSize - 1));
        const word in_page = size < left_in_page ? size : left_in_page;
        route_memory(address).write_block(address, data, in_page);
        address += in_page;
        data += in_page;
        size -= in_page;
    }
}

void SystemBus::reset()
{
    // The clock needs nothing: a run ends with the devices up to date and starts by looking at
    // when their next event is.
    ram->reset();
    intc.reset();
    timer.reset();
    console.reset();
    block.reset();
    display.reset();
    sysctl.reset();
}

void SystemBus::read_page(byte *const out, const word ppage)
{
    const word address = ppage << kNumPageOffsetBits;
    route_memory(address).read_block(out, address, kPageSize);
}

void SystemBus::write_page(const word ppage, const byte *data)
{
    const word address = ppage << kNumPageOffsetBits;
    route_memory(address).write_block(address, data, kPageSize);
}

byte *SystemBus::direct_page(const word ppage)
{
    // Only the RAM: the ROM, the disk and the devices are not memory to copy into and out of.
    const word address = ppage << kNumPageOffsetBits;
    return ram->in_bounds(address) ? ram->bytes_at(address) : nullptr;
}

bool SystemBus::read_physical_word(word &out, const word address)
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
