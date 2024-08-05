#define AEMU_ONLY_CRITICAL_LOG

#include "emulator32bit/system_bus.h"

SystemBus::SystemBus(RAM ram, ROM rom, VirtualMemory& mmu) :
	ram(ram),
	rom(rom),
	mmu(mmu)
{
	// Constructor
	mems.push_back(&this->ram);
	mems.push_back(&this->rom);
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
	for (Memory *mem : mems)
	{
		mem->reset();
	}
}