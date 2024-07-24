#pragma once
#ifndef SystemBus_H

#include "emulator32bit/Emulator32bitUtil.h"
#include "emulator32bit/Memory.h"
#include "emulator32bit/VirtualMemory.h"

#include <vector>

class SystemBus {
	public:
		SystemBus(RAM ram, ROM rom, VirtualMemory& mmu);
		struct SystemBusException {
			enum SystemBusExceptionType {
				AOK,
				INVALID_ADDRESS,
				CONFLICT_ADDRESSES
			};

			SystemBusExceptionType type = AOK;
			word address = 0;
		};

		/**
		 * Read a byte from the system bus
		 *
		 * @param address The address to read from
		 * @param exception The exception raised by the read operation
		 * @return The byte read from the address
		 */
		byte read_byte(word address, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception, bool memory_mapped = true);
		hword read_hword(word address, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception, bool memory_mapped = true);
		word read_word(word address, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception, bool memory_mapped = true);

		dword read_val(word address, int n_bytes, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception, bool memory_mapped = true);

		/**
		 * Write a byte to the system bus
		 *
		 * @param address The address to write to
		 * @param exception The exception raised by the write operation
		 * @param data The byte to write
		 */
		void write_byte(word address, byte data, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception, bool memory_mapped = true);
		void write_hword(word address, hword data, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception, bool memory_mapped = true);
		void write_word(word address, word data, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception, bool memory_mapped = true);

		void write_val(word address, dword val, int n_bytes, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception, bool memory_mapped = true);

		void reset();

	private:
		std::vector<Memory*> mems;
		RAM ram;
		ROM rom;
		VirtualMemory& mmu;				// todo, we don't need MMU here, we could just called

		Memory* route_memory(const word address, SystemBusException &bus_exception);
};

#endif