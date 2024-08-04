#pragma once
#ifndef SYSTEM_BUS_H
#define SYSTEM_BUS_H

#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/memory.h"
#include "emulator32bit/virtual_memory.h"

#include <vector>
class SystemBus
{
	public:
		SystemBus(RAM ram, ROM rom, VirtualMemory& mmu);

		/* expose for now */
		RAM ram;
		ROM rom;
		VirtualMemory& mmu;				// todo, we don't need MMU here, we could just called

		class SystemBusException : public std::exception
		{
			private:
				std::string message;

			public:
				SystemBusException(const std::string& msg);

				const char* what() const noexcept override;
		};

		/**
		 * Read a byte from the system bus
		 *
		 * @param address The address to read from
		 * @param exception The exception raised by the read operation
		 * @return The byte read from the address
		 */
		byte read_byte(word address, bool memory_mapped = true);
		hword read_hword(word address, bool memory_mapped = true);
		word read_word(word address, bool memory_mapped = true);
		word read_word_aligned_ram(word address, bool memory_mapped = true);


		dword read_val(word address, int n_bytes, bool memory_mapped = true);

		/**
		 * Write a byte to the system bus
		 *
		 * @param address The address to write to
		 * @param exception The exception raised by the write operation
		 * @param data The byte to write
		 */
		void write_byte(word address, byte data, bool memory_mapped = true);
		void write_hword(word address, hword data, bool memory_mapped = true);
		void write_word(word address, word data, bool memory_mapped = true);

		void write_val(word address, dword val, int n_bytes, bool memory_mapped = true);

		void reset();

	private:
		std::vector<Memory*> mems;

		word map_address(word address);
		Memory* route_memory(const word address);
};

#endif /* SYSTEM_BUS */