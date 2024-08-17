#define AEMU_ONLY_CRITICAL_LOG

#include "emulator32bit/system_bus.h"

SystemBus::SystemBus(RAM ram, ROM rom, VirtualMemory& mmu) :
	ram(ram),
	rom(rom),
	mmu(mmu)
{

}

SystemBus::SystemBusException::SystemBusException(const std::string& msg) :
	message(msg)
{

}

const char* SystemBus::SystemBusException::what() const noexcept
{
	return message.c_str();
}

void SystemBus::reset()
{
	ram.reset();
	rom.reset(); 	// Do we really want to reset rom??
}