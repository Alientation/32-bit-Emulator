#define AEMU_ONLY_CRITICAL_LOG

#include "emulator32bit/system_bus.h"
#include "util/loggerv2.h"

#define UNUSED(x) (void)(x)

SystemBus::Exception SystemBus::hide_sys_bus_exception;
Memory::ReadException SystemBus::hide_mem_read_exception;
Memory::WriteException SystemBus::hide_mem_write_exception;

SystemBus::SystemBus(RAM ram, ROM rom, VirtualMemory& mmu) :
	ram(ram),
	rom(rom),
	mmu(mmu)
{
	// Constructor
	mems.push_back(&this->ram);
	mems.push_back(&this->rom);
}

Memory* SystemBus::route_memory(word address, Exception &bus_exception)
{
	static Memory* prev_routed = nullptr;

	/* Optimize since usually memory accesses have some localization */
	if (prev_routed != nullptr && prev_routed->in_bounds(address)) {
		return prev_routed;
	}

	Memory *target = nullptr;
	for (size_t i = 0; i < mems.size(); i++) {
		if (!mems[i]->in_bounds(address)) {
			continue;
		}

		if (target != nullptr) {
			bus_exception.address = address;
			bus_exception.type = Exception::Type::CONFLICT_ADDRESSES;
			return nullptr;
		}

		target = mems[i];
	}

	if (target == nullptr) {
		bus_exception.address = address;
		bus_exception.type = Exception::Type::INVALID_ADDRESS;
	}

	prev_routed = target;
	return target;
}

word SystemBus::map_address(word address, VirtualMemory::Exception& exception)
{
	word addr = mmu.map_address(address, exception);

	if (exception.type == VirtualMemory::Exception::Type::DISK_RETURN_AND_FETCH_SUCCESS) {
		exception.type = VirtualMemory::Exception::Type::DISK_FETCH_SUCCESS; /* so the next conditional can handle */

		Disk::WriteException disk_write_exception;
		std::vector<byte> bytes(PAGE_SIZE);

		// EXPECTS page to be part of single memory target
		Exception bus_exception;
		word p_addr = exception.ppage_return << PAGE_PSIZE;
		Memory *target = route_memory(p_addr, bus_exception);

		if (bus_exception.type != Exception::Type::AOK) {
			ERROR("Bus is not in an AOK state");
			return 0;
		}

		Memory::ReadException mem_read_exception;
		for (word i = 0; i < PAGE_SIZE; i++) {
			bytes.at(i) = target->read_byte(p_addr + i, mem_read_exception);
		}

		if (mem_read_exception.type != Memory::ReadException::Type::AOK) {
			ERROR("Memory read failed");
			return 0;
		}

		mmu.m_disk.write_page(exception.disk_page_return, bytes, disk_write_exception);

		if (disk_write_exception.type != Disk::WriteException::Type::AOK) {
			ERROR("Disk write failed");
			return 0;
		}

		DEBUG_SS(std::stringstream() << "Writing physical page "
				<< std::to_string(exception.ppage_return) << " to disk page "
				<< std::to_string(exception.disk_page_return));
	}

	if (exception.type == VirtualMemory::Exception::Type::DISK_FETCH_SUCCESS) {
		/* handle exception by writing page fetched from disk to memory */
		word paddr = exception.ppage_fetch << PAGE_PSIZE;
		SystemBus::Exception bus_exception;
		Memory::WriteException mem_write_exception;

		// EXPECTS page to be part of single memory target
		Memory *target = route_memory(paddr, bus_exception);

		if (bus_exception.type != SystemBus::Exception::Type::AOK) {
			ERROR("Bus is not in an AOK state");
			return 0;
		}

		for (word i = 0; i < PAGE_SIZE; i++) {
			target->write_byte(paddr + i, exception.disk_fetch.at(i), mem_write_exception);
		}

		if (mem_write_exception.type != Memory::WriteException::Type::AOK) {
			ERROR("Memory write failed");
			return 0;
		}

		DEBUG_SS(std::stringstream() << "Reading physical page "
				<< std::to_string(exception.ppage_fetch) << " from disk");
	}

	return addr;
}

word SystemBus::map_read_address(word address, SystemBus::Exception &bus_exception,
								 Memory::ReadException &mem_exception)
{
	VirtualMemory::Exception vm_exception;

	word addr = map_address(address, vm_exception);

	// todo, handle exceptions
	UNUSED(bus_exception);
	UNUSED(mem_exception);

	return addr;
}

word SystemBus::map_write_address(word address, SystemBus::Exception &bus_exception,
								  Memory::WriteException &mem_exception)
{
	VirtualMemory::Exception vm_exception;

	word addr = map_address(address, vm_exception);

	// todo, handle exceptions
	UNUSED(bus_exception);
	UNUSED(mem_exception);

	return addr;
}

dword SystemBus::read_val(word address, int n_bytes, Exception &bus_exception,
						  Memory::ReadException &mem_exception, bool memory_mapped)
{
	dword val = 0;
	for (int i = 0; i < n_bytes; i++) {
		val <<= 8;
		word real_adr = address;
		if (memory_mapped) {
			real_adr = map_read_address(address + n_bytes - i - 1, bus_exception, mem_exception);
		}
		Memory *target = route_memory(real_adr, bus_exception);
		if (bus_exception.type != Exception::Type::AOK) {
			return 0;
		}
		val += target->read_byte(real_adr, mem_exception);
	}
	return val;
}

byte SystemBus::read_byte(word address, Exception &bus_exception,
						  Memory::ReadException &mem_exception, bool memory_mapped)
{
	return read_val(address, 1, bus_exception, mem_exception, memory_mapped);
}

hword SystemBus::read_hword(word address, Exception &bus_exception,
							Memory::ReadException &mem_exception, bool memory_mapped)
{
	return read_val(address, 2, bus_exception, mem_exception, memory_mapped);
}

word SystemBus::read_word(word address, Exception &bus_exception,
						  Memory::ReadException &mem_exception, bool memory_mapped)
{
	return read_val(address, 4, bus_exception, mem_exception, memory_mapped);
}

void SystemBus::write_val(word address, dword val, int n_bytes, Exception &bus_exception,
						  Memory::WriteException &mem_exception, bool memory_mapped)
{
	for (int i = 0; i < n_bytes; i++) {
		word real_adr = address;
		if (memory_mapped) {
			real_adr = map_write_address(address + i, bus_exception, mem_exception);
		}
		Memory *target = route_memory(real_adr, bus_exception);
		if (bus_exception.type != Exception::Type::AOK) {
			return;
		}
		target->write_byte(real_adr, val & 0xFF, mem_exception);
		val >>= 8;
	}
}

void SystemBus::write_byte(word address, byte data, Exception &bus_exception,
						   Memory::WriteException &mem_exception, bool memory_mapped)
{
	write_val(address, data, 1, bus_exception, mem_exception, memory_mapped);
}

void SystemBus::write_hword(word address, hword data, Exception &bus_exception,
							Memory::WriteException &mem_exception, bool memory_mapped)
{
	write_val(address, data, 2, bus_exception, mem_exception, memory_mapped);
}

void SystemBus::write_word(word address, word data, Exception &bus_exception,
						   Memory::WriteException &mem_exception, bool memory_mapped)
{
	write_val(address, data, 4, bus_exception, mem_exception, memory_mapped);
}

void SystemBus::reset()
{
	for (Memory *mem : mems) {
		mem->reset();
	}
}