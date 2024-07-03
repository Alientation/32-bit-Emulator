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

	if (target == nullptr) {
		bus_exception.address = address;
		bus_exception.type = SystemBusException::INVALID_ADDRESS;
	}

	return target;
}

byte SystemBus::read_byte(word address, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception) {
	// Read a byte from the system bus
	Memory *target = route_memory(address, bus_exception);
	if (bus_exception.type == SystemBusException::AOK) {
		return target->read_byte(address, mem_exception);
	}
	return 0;
}

byte SystemBus::read_byte(word address) {
	SystemBusException bus_exception;
	Memory::MemoryReadException mem_exception;
	return read_byte(address, bus_exception, mem_exception);
}

hword SystemBus::read_hword(word address, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception) {
	// Read a half word from the system bus
	Memory *target = route_memory(address, bus_exception);
	if (bus_exception.type == SystemBusException::AOK) {
		return target->read_hword(address, mem_exception);
	}
	return 0;
}

hword SystemBus::read_hword(word address) {
	SystemBusException bus_exception;
	Memory::MemoryReadException mem_exception;
	return read_hword(address, bus_exception, mem_exception);
}

word SystemBus::read_word(word address, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception) {
	// Read a word from the system bus
	Memory *target = route_memory(address, bus_exception);
	if (bus_exception.type == SystemBusException::AOK) {
		return target->read_word(address, mem_exception);
	}
	return 0;
}

word SystemBus::read_word(word address) {
	SystemBusException bus_exception;
	Memory::MemoryReadException mem_exception;
	return read_word(address, bus_exception, mem_exception);
}

void SystemBus::write_byte(word address, byte data, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception) {
	// Write a byte to the system bus
	Memory *target = route_memory(address, bus_exception);
	if (bus_exception.type == SystemBusException::AOK) {
		target->write_byte(address, data, mem_exception);
	}
}

void SystemBus::write_byte(word address, byte data) {
	SystemBusException bus_exception;
	Memory::MemoryWriteException mem_exception;
	write_byte(address, data, bus_exception, mem_exception);
}

void SystemBus::write_hword(word address, hword data, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception) {
	// Write a half word to the system bus
	Memory *target = route_memory(address, bus_exception);
	if (bus_exception.type == SystemBusException::AOK) {
		target->write_hword(address, data, mem_exception);
	}
}

void SystemBus::write_hword(word address, hword data) {
	SystemBusException bus_exception;
	Memory::MemoryWriteException mem_exception;
	write_hword(address, data, bus_exception, mem_exception);
}

void SystemBus::write_word(word address, word data, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception) {
	// Write a word to the system bus
	Memory *target = route_memory(address, bus_exception);
	if (bus_exception.type == SystemBusException::AOK) {
		target->write_word(address, data, mem_exception);
	}
}

void SystemBus::write_word(word address, word data) {
	SystemBusException bus_exception;
	Memory::MemoryWriteException mem_exception;
	write_word(address, data, bus_exception, mem_exception);
}

void SystemBus::reset() {
	for (Memory *mem : mems) {
		mem->reset();
	}
}