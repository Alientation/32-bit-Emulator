#pragma once
#ifndef SystemBus_H

#include "emulator32bit/Emulator32bitUtil.h"
#include "emulator32bit/Memory.h"
#include "emulator32bit/VirtualMemory.h"

#include <vector>

class SystemBus {
	public:
		SystemBus(RAM ram, ROM rom);

		/* expose since we want syscalls to be able to control the virtual memory management */
		VirtualMemory mmu;

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
		byte read_byte(word address, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception);
		byte read_byte(word address);
		hword read_hword(word address, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception);
		hword read_hword(word address);
		word read_word(word address, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception);
		word read_word(word address);

		dword read_val(word address, int n_bytes, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception);

		/**
		 * Write a byte to the system bus
		 *
		 * @param address The address to write to
		 * @param exception The exception raised by the write operation
		 * @param data The byte to write
		 */
		void write_byte(word address, byte data, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception);
		void write_byte(word address, byte data);
		void write_hword(word address, hword data, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception);
		void write_hword(word address, hword data);
		void write_word(word address, word data, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception);
		void write_word(word address, word data);

		void write_val(word address, dword val, int n_bytes, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception);

		void reset();

	private:
		std::vector<Memory*> mems;
		RAM ram;
		ROM rom;

		Memory* route_memory(const word address, SystemBusException &bus_exception);
};

#endif