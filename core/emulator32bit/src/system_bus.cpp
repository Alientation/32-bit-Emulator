#include "emulator32bit/system_bus.h"

#include "util/logger.h"

SystemBus::SystemBus(RAM *ram, ROM *rom) :
    ram(ram),
    rom(rom),
    disk(new MockDisk()),
    mmu(new VirtualMemory(disk))
{
    validate_memory();
}

SystemBus::SystemBus(RAM *ram, ROM *rom, Disk *disk, VirtualMemory *mmu) :
    ram(ram),
    rom(rom),
    disk(disk),
    mmu(mmu)
{
    validate_memory();
}

SystemBus::~SystemBus()
{
    disk->save();
    delete ram;
    delete rom;
    delete disk;
    delete mmu;
}

void SystemBus::validate_memory()
{
    AEMU_CHECK(!ram->overlap(*rom),
               "Invalid memory layout. RAM overlaps with ROM memory. (%u,%u) U (%u,%u)",
               ram->get_lo_page(), ram->get_hi_page(), rom->get_lo_page(), rom->get_hi_page());

    AEMU_CHECK(!ram->overlap(*disk),
               "Invalid memory layout. RAM overlaps with DISK memory. (%u,%u) U (%u,%u)",
               ram->get_lo_page(), ram->get_hi_page(), disk->get_lo_page(), disk->get_hi_page());

    AEMU_CHECK(!rom->overlap(*disk),
               "Invalid memory layout. ROM overlaps with DISK memory. (%u,%u) U (%u,%u)",
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
}