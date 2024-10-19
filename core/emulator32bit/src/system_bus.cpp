#define AEMU_ONLY_CRITICAL_LOG

#include "emulator32bit/system_bus.h"

SystemBus::SystemBus(RAM& ram, ROM& rom, Disk& disk, VirtualMemory& mmu) :
	ram(ram),
	rom(rom),
	disk(disk),
	mmu(mmu)
{

}

SystemBus::Exception::Exception(const std::string& msg) :
	message(msg)
{

}

const char* SystemBus::Exception::what() const noexcept
{
	return message.c_str();
}

void SystemBus::reset()
{
	ram.reset();
	rom.reset(); 	// Do we really want to reset rom??
}