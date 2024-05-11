#pragma once
#ifndef SystemBus_H

#include "emulator32bit/Emulator32bitUtil.h"
#include "emulator32bit/Memory.h"

#include "vector"

class SystemBus {
	public:
		SystemBus(RAM *ram, ROM *rom);
		~SystemBus();

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
		byte readByte(word address, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception);
		hword readHalfWord(word address, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception);
		word readWord(word address, SystemBusException &bus_exception, Memory::MemoryReadException &mem_exception);

		/**
		 * Write a byte to the system bus
		 * 
		 * @param address The address to write to
		 * @param exception The exception raised by the write operation
		 * @param data The byte to write
		 */
		void writeByte(word address, byte data, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception);
		void writeHalfWord(word address, hword data, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception);
		void writeWord(word address, word data, SystemBusException &bus_exception, Memory::MemoryWriteException &mem_exception);

	private:
		std::vector<Memory*> mems;
		RAM *ram;
		ROM *rom;

		Memory* route_memory(const word address, SystemBusException &bus_exception);
};

#endif