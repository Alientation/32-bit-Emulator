#include <emulator32bit/SystemBus.h>

#include <assert.h>

SystemBus::SystemBus(RAM ram, ROM rom) : ram(ram), rom(rom) {
	// Constructor
	mems.push_back(&this->ram);
	mems.push_back(&this->rom);
}

Memory* SystemBus::route_memory(word address, SystemBusException &bus_exception) {
	Memory *target = nullptr;
	for (int i = 0; i < mems.size(); i++) {
		if (!mems[i]->in_bounds(address)) {
			continue;
		}

		if (target != nullptr) {
			bus_exception.address = address;
			bus_exception.type = SystemBusException::CONFLICT_ADDRESSES;
			return nullptr;
		}

		target = mems[i];
	}

	return target;
}

byte SystemBus::readByte(word address, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception) {
	// Read a byte from the system bus
	Memory *target = route_memory(address, bus_exception);
	if (bus_exception.type == SystemBusException::AOK) {
		return target->readByte(address, mem_exception);
	}
	return 0;
}

byte SystemBus::readByte(word address) {
	SystemBusException bus_exception;
	Memory::MemoryReadException mem_exception;
	return readByte(address, bus_exception, mem_exception);
}

hword SystemBus::readHalfWord(word address, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception) {
	// Read a half word from the system bus
	Memory *target = route_memory(address, bus_exception);
	if (bus_exception.type == SystemBusException::AOK) {
		return target->readHalfWord(address, mem_exception);
	}
	return 0;
}

hword SystemBus::readHalfWord(word address) {
	SystemBusException bus_exception;
	Memory::MemoryReadException mem_exception;
	return readHalfWord(address, bus_exception, mem_exception);
}

word SystemBus::readWord(word address, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception) {
	// Read a word from the system bus
	Memory *target = route_memory(address, bus_exception);
	if (bus_exception.type == SystemBusException::AOK) {
		return target->readWord(address, mem_exception);
	}
	return 0;
}

word SystemBus::readWord(word address) {
	SystemBusException bus_exception;
	Memory::MemoryReadException mem_exception;
	return readWord(address, bus_exception, mem_exception);
}

void SystemBus::writeByte(word address, byte data, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception) {
	// Write a byte to the system bus
	Memory *target = route_memory(address, bus_exception);
	if (bus_exception.type == SystemBusException::AOK) {
		target->writeByte(address, data, mem_exception);
	}
}

void SystemBus::writeByte(word address, byte data) {
	SystemBusException bus_exception;
	Memory::MemoryWriteException mem_exception;
	writeByte(address, data, bus_exception, mem_exception);
}

void SystemBus::writeHalfWord(word address, hword data, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception) {
	// Write a half word to the system bus
	Memory *target = route_memory(address, bus_exception);
	if (bus_exception.type == SystemBusException::AOK) {
		target->writeHalfWord(address, data, mem_exception);
	}
}

void SystemBus::writeHalfWord(word address, hword data) {
	SystemBusException bus_exception;
	Memory::MemoryWriteException mem_exception;
	writeHalfWord(address, data, bus_exception, mem_exception);
}

void SystemBus::writeWord(word address, word data, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception) {
	// Write a word to the system bus
	Memory *target = route_memory(address, bus_exception);
	if (bus_exception.type == SystemBusException::AOK) {
		target->writeWord(address, data, mem_exception);
	}
}

void SystemBus::writeWord(word address, word data) {
	SystemBusException bus_exception;
	Memory::MemoryWriteException mem_exception;
	writeWord(address, data, bus_exception, mem_exception);
}

void SystemBus::reset() {
	for (Memory *mem : mems) {
		mem->reset();
	}
}