#include "emulator32bit/system_bus.h"

SystemBus::SystemBus (RAM *ram, ROM *rom) :
    ram (ram),
    rom (rom),
    disk (new MockDisk ()),
    mmu (new VirtualMemory (disk))
{
}

SystemBus::SystemBus (RAM *ram, ROM *rom, Disk *disk, VirtualMemory *mmu) :
    ram (ram),
    rom (rom),
    disk (disk),
    mmu (mmu)
{
}

SystemBus::~SystemBus ()
{
    disk->save ();
    delete ram;
    delete rom;
    delete disk;
    delete mmu;
}

SystemBus::Exception::Exception (const std::string &msg) :
    message (msg)
{
}

const char *SystemBus::Exception::what () const noexcept
{
    return message.c_str ();
}

void SystemBus::reset ()
{
    ram->reset ();
}