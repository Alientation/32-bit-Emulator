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
				MULTIPLE_MEMORY_MATCHES
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
		byte readByte(const word address, SystemBusException *bus_exception, Memory::MemoryReadException *mem_exception);
		hword readHalfWord(const word address, SystemBusException *bus_exception, Memory::MemoryReadException *mem_exception);
		word readWord(const word address, SystemBusException *bus_exception, Memory::MemoryReadException *mem_exception);

		/**
		 * Write a byte to the system bus
		 * 
		 * @param address The address to write to
		 * @param exception The exception raised by the write operation
		 * @param data The byte to write
		 */
		void writeByte(const word address, const byte data, SystemBusException *bus_exception, Memory::MemoryWriteException *mem_exception);
		void writeHalfWord(const word address, const hword data, SystemBusException *bus_exception, Memory::MemoryWriteException *mem_exception);
		void writeWord(const word address, const word data, SystemBusException *bus_exception, Memory::MemoryWriteException *mem_exception);

	private:
		std::vector<Memory*> mems;
		RAM *ram;
		ROM *rom;

		Memory* route_memory(const word address, SystemBusException *bus_exception);
};

#endif