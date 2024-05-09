#include "emulator32bit/SystemBus.h"
#include "assert.h"

SystemBus::SystemBus(RAM *ram, ROM *rom) {
	// Constructor
	mems.push_back(ram);
	mems.push_back(rom);
	this->ram = ram;
	this->rom = rom;
}

SystemBus::~SystemBus() {
	// Destructor
	for (int i = 0; i < mems.size(); i++) {
		mems[i]->~Memory();
	}
}

Memory* SystemBus::route_memory(const word address, SystemBusException *bus_exception) {
	Memory *target = nullptr;
	for (int i = 0; i < mems.size(); i++) {
		if (!mems[i]->in_bounds(address)) {
			continue;
		}

		if (target != nullptr) {
			bus_exception->address = address;
			bus_exception->type = SystemBusException::MULTIPLE_MEMORY_MATCHES;
			return nullptr;
		}

		target = mems[i];
	}

	return target;
}

byte SystemBus::readByte(const word address, SystemBusException *bus_exception, Memory::MemoryReadException *mem_exception) {
	// Read a byte from the system bus
	Memory *target = route_memory(address, bus_exception);
	if (bus_exception->type == SystemBusException::AOK) {
		return target->readByte(address, mem_exception);
	}
	return 0;
}

hword SystemBus::readHalfWord(const word address, SystemBusException *bus_exception, Memory::MemoryReadException *mem_exception) {
	// Read a half word from the system bus
	Memory *target = route_memory(address, bus_exception);
	if (bus_exception->type == SystemBusException::AOK) {
		return target->readHalfWord(address, mem_exception);
	}
	return 0;
}

word SystemBus::readWord(const word address, SystemBusException *bus_exception, Memory::MemoryReadException *mem_exception) {
	// Read a word from the system bus
	Memory *target = route_memory(address, bus_exception);
	if (bus_exception->type == SystemBusException::AOK) {
		return target->readWord(address, mem_exception);
	}
	return 0;
}

void SystemBus::writeByte(const word address, const byte data, SystemBusException *bus_exception, Memory::MemoryWriteException *mem_exception) {
	// Write a byte to the system bus
	Memory *target = route_memory(address, bus_exception);
	if (bus_exception->type == SystemBusException::AOK) {
		target->writeByte(address, data, mem_exception);
	}
}

void SystemBus::writeHalfWord(const word address, const hword data, SystemBusException *bus_exception, Memory::MemoryWriteException *mem_exception) {
	// Write a half word to the system bus
	Memory *target = route_memory(address, bus_exception);
	if (bus_exception->type == SystemBusException::AOK) {
		target->writeHalfWord(address, data, mem_exception);
	}
}

void SystemBus::writeWord(const word address, const word data, SystemBusException *bus_exception, Memory::MemoryWriteException *mem_exception) {
	// Write a word to the system bus
	Memory *target = route_memory(address, bus_exception);
	if (bus_exception->type == SystemBusException::AOK) {
		target->writeWord(address, data, mem_exception);
	}
}