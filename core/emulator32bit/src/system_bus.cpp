#define AEMU_ONLY_CRITICAL_LOG

#include "emulator32bit/system_bus.h"
#include "util/loggerv2.h"

#define UNUSED(x) (void)(x)

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

word SystemBus::map_address(word address)
{
 	VirtualMemory::Exception exception;
	word addr = mmu.map_address(address, exception);

	if (exception.type == VirtualMemory::Exception::Type::AOK)
	{
		return addr;
	}

	if (exception.type == VirtualMemory::Exception::Type::DISK_RETURN_AND_FETCH_SUCCESS) {
		exception.type = VirtualMemory::Exception::Type::DISK_FETCH_SUCCESS; /* so the next conditional can handle */

		std::vector<byte> bytes(PAGE_SIZE);

		// EXPECTS page to be part of single memory target
		word p_addr = exception.ppage_return << PAGE_PSIZE;
		Memory *target = route_memory(p_addr);

		for (word i = 0; i < PAGE_SIZE; i++) {
			bytes.at(i) = target->read_byte(p_addr + i);
		}

		mmu.m_disk.write_page(exception.disk_page_return, bytes);

		DEBUG_SS(std::stringstream() << "Writing physical page "
				<< std::to_string(exception.ppage_return) << " to disk page "
				<< std::to_string(exception.disk_page_return));
	}

	if (exception.type == VirtualMemory::Exception::Type::DISK_FETCH_SUCCESS) {
		/* handle exception by writing page fetched from disk to memory */
		word paddr = exception.ppage_fetch << PAGE_PSIZE;

		// EXPECTS page to be part of single memory target
		Memory *target = route_memory(paddr);

		for (word i = 0; i < PAGE_SIZE; i++) {
			target->write_byte(paddr + i, exception.disk_fetch.at(i));
		}

		DEBUG_SS(std::stringstream() << "Reading physical page "
				<< std::to_string(exception.ppage_fetch) << " from disk");
	}

	return addr;
}

dword SystemBus::read_val(word address, int n_bytes)
{
	dword val = 0;
	for (int i = 0; i < n_bytes; i++)
	{
		val <<= 8;
		word real_adr = address;
		real_adr = map_address(address + n_bytes - i - 1);
		Memory *target = route_memory(real_adr);
		val += target->read_byte(real_adr);
	}
	return val;
}

void SystemBus::write_val(word address, dword val, int n_bytes)
{
	for (int i = 0; i < n_bytes; i++) {
		word real_adr = address;
		real_adr = map_address(address + i);
		Memory *target = route_memory(real_adr);
		target->write_byte(real_adr, val & 0xFF);
		val >>= 8;
	}
}

void SystemBus::reset()
{
	for (Memory *mem : mems) {
		mem->reset();
	}
}