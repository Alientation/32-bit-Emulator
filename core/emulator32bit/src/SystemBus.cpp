#include <emulator32bit/SystemBus.h>

#include <assert.h>

SystemBus::SystemBusException SystemBus::hide_sys_bus_exception;
Memory::MemoryReadException SystemBus::hide_mem_read_exception;
Memory::MemoryWriteException SystemBus::hide_mem_write_exception;

SystemBus::SystemBus(RAM ram, ROM rom, VirtualMemory& mmu) : ram(ram), rom(rom), mmu(mmu) {
	// Constructor
	mems.push_back(&this->ram);
	mems.push_back(&this->rom);
}

Memory* SystemBus::route_memory(word address, SystemBusException &bus_exception) {
	Memory *target = nullptr;
	for (int i = 0; i < mems.size(); i++) {
		if (!mems[i]->in_bounds(address)) {
			printf("%x -> %x - %x\n", address, mems[i]->get_lo_page(), mems[i]->get_hi_page());
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

word map_read_address(VirtualMemory& mmu, word address, SystemBus::SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception) {
	VirtualMemory::Exception vm_exception;

	return mmu.map_address(address, vm_exception);
}

word map_write_address(VirtualMemory& mmu, word address, SystemBus::SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception) {
	VirtualMemory::Exception vm_exception;

	return mmu.map_address(address, vm_exception);
}

dword SystemBus::read_val(word address, int n_bytes, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception, bool memory_mapped) {
	dword val = 0;
	for (int i = 0; i < n_bytes; i++) {
		val <<= 8;
		word real_adr = address;
		if (memory_mapped) {
			real_adr = map_read_address(mmu, address + n_bytes - i - 1, bus_exception, mem_exception);
		}
		Memory *target = route_memory(real_adr, bus_exception);
		if (bus_exception.type != SystemBusException::AOK) {
			return 0;
		}
		val += target->read_byte(real_adr, mem_exception);
	}
	return val;
}

byte SystemBus::read_byte(word address, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception, bool memory_mapped) {
	return read_val(address, 1, bus_exception, mem_exception, memory_mapped);
}

hword SystemBus::read_hword(word address, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception, bool memory_mapped) {
	return read_val(address, 2, bus_exception, mem_exception, memory_mapped);
}

word SystemBus::read_word(word address, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception, bool memory_mapped) {
	return read_val(address, 4, bus_exception, mem_exception, memory_mapped);
}

void SystemBus::write_val(word address, dword val, int n_bytes, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception, bool memory_mapped) {
	dword copy = val;
	for (int i = 0; i < n_bytes; i++) {
		word real_adr = address;
		if (memory_mapped) {
			real_adr = map_write_address(mmu, address + i, bus_exception, mem_exception);
		}
		Memory *target = route_memory(real_adr, bus_exception);
		if (bus_exception.type != SystemBusException::AOK) {
			printf("bus write val state not AOK warning\n");
			return;
		}
		target->write_byte(real_adr, val & 0xFF, mem_exception);
		val >>= 8;
	}

	Memory::MemoryReadException read_exception;
	dword r_val = read_val(address, n_bytes, bus_exception, read_exception);
}

void SystemBus::write_byte(word address, byte data, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception, bool memory_mapped) {
	write_val(address, data, 1, bus_exception, mem_exception, memory_mapped);
}

void SystemBus::write_hword(word address, hword data, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception, bool memory_mapped) {
	write_val(address, data, 2, bus_exception, mem_exception, memory_mapped);
}

void SystemBus::write_word(word address, word data, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception, bool memory_mapped) {
	write_val(address, data, 4, bus_exception, mem_exception, memory_mapped);
}

void SystemBus::reset() {
	for (Memory *mem : mems) {
		mem->reset();
	}
}