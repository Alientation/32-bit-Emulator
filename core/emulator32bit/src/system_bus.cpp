#include "emulator32bit/system_bus.h"

#include "util/logger.h"

SystemBus::SystemBus(RAM *ram, ROM *rom) :
    ram(ram),
    rom(rom),
    disk(new MockDisk()),
    mmu(new VirtualMemory(disk.get(), ram->get_lo_page(), ram->get_mem_pages()))
{
    validate_memory();
    mmu->set_physical_pages(this);
}

SystemBus::SystemBus(RAM *ram, ROM *rom, Disk *disk, VirtualMemory *mmu) :
    ram(ram),
    rom(rom),
    disk(disk),
    mmu(mmu)
{
    validate_memory();
    this->mmu->set_physical_pages(this);
}

SystemBus::~SystemBus()
{
    // A destructor must not throw, and saving fails with an exception when the fatal action is
    // Throw.
    try
    {
        disk->save();
        if (!block.save())
        {
            AEMU_ERROR("SystemBus::~SystemBus() - The block device could not be saved.");
        }
    }
    catch (const std::exception &error)
    {
        AEMU_ERROR("SystemBus::~SystemBus() - The disk could not be saved: {}", error.what());
    }
}

void SystemBus::validate_memory()
{
    AEMU_CHECK(!ram->overlap(*rom),
               "Invalid memory layout. RAM overlaps with ROM memory. ({},{}) U ({},{})",
               ram->get_lo_page(), ram->get_hi_page(), rom->get_lo_page(), rom->get_hi_page());

    AEMU_CHECK(!ram->overlap(*disk),
               "Invalid memory layout. RAM overlaps with DISK memory. ({},{}) U ({},{})",
               ram->get_lo_page(), ram->get_hi_page(), disk->get_lo_page(), disk->get_hi_page());

    for (const BaseMemory *memory :
         {static_cast<const BaseMemory *>(ram.get()), static_cast<const BaseMemory *>(rom.get()),
          static_cast<const BaseMemory *>(disk.get())})
    {
        AEMU_CHECK(memory->get_mem_pages() == 0
                       || U64(memory->get_lo_page()) + memory->get_mem_pages()
                              <= (kDeviceBase >> kNumPageOffsetBits),
                   "Invalid memory layout. Memory ({},{}) overlaps the devices, which start at "
                   "{:#x}.",
                   memory->get_lo_page(), memory->get_hi_page(), kDeviceBase);
    }

    AEMU_CHECK(!rom->overlap(*disk),
               "Invalid memory layout. ROM overlaps with DISK memory. ({},{}) U ({},{})",
               rom->get_lo_page(), rom->get_hi_page(), disk->get_lo_page(), disk->get_hi_page());
}

SystemBus::Exception::Exception(const std::string &msg) :
    message(msg)
{
}

const char *SystemBus::Exception::what() const noexcept
{
    return message.c_str();
}

void SystemBus::reset()
{
    ram->reset();
    intc.reset();
    timer.reset();
    console.reset();
    block.reset();
}