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
		VirtualMemory& mmu;

		class SystemBusException : public std::exception
		{
			private:
				std::string message;

			public:
				SystemBusException(const std::string& msg);

				const char* what() const noexcept override;
		};

		dword read_val(word address, int n_byte);

		/**
		 * Read a byte from the system bus
		 *
		 * @param address The address to read from
		 * @return The byte read from the address
		 */
		inline byte read_byte(word address)
		{
			address = map_address(address);
			return route_memory(address)->read_byte(address);
		}

		inline byte read_unmapped_byte(word address)
		{
			return route_memory(address)->read_byte(address);
		}

		inline hword read_hword(word address)
		{
			if ((address >> PAGE_PSIZE) == ((address + 1) >> PAGE_PSIZE))
			{
				address = map_address(address);
				return route_memory(address)->read_hword(address);
			}

			return read_val(address, 2);
		}

		inline hword read_unmapped_hword(word address)
		{
			return route_memory(address)->read_word(address);
		}

		inline word read_word(word address)
		{
			if ((address >> PAGE_PSIZE) == ((address + 3) >> PAGE_PSIZE))
			{
				address = map_address(address);
				return route_memory(address)->read_word(address);
			}

			return read_val(address, 4);
		}

		inline word read_unmapped_word(word address)
		{
			return route_memory(address)->read_word(address);
		}

		inline word read_word_aligned_ram(word address)
		{
			return ram.read_word_aligned(map_address(address));
		}

		inline word read_unmapped_word_aligned_ram(word address)
		{
			return ram.read_word_aligned(address);
		}

		/**
		 * Write a byte to the system bus
		 *
		 * @param address The address to write to
		 * @param exception The exception raised by the write operation
		 * @param data The byte to write
		 */
		inline void write_byte(word address, byte data)
		{
			address = map_address(address);
			route_memory(address)->write_byte(address, data);
		}

		inline void write_unmapped_byte(word address, byte data)
		{
			route_memory(address)->write_byte(address, data);
		}

		inline void write_hword(word address, hword data)
		{
			if ((address >> PAGE_PSIZE) == ((address + 1) >> PAGE_PSIZE))
			{
				address = map_address(address);
				route_memory(address)->write_hword(address, data);
			}
			else
			{
				write_val(address, data, 2);
			}
		}

		inline void write_unmapped_hword(word address, hword data)
		{
			route_memory(address)->write_hword(address, data);
		}

		inline void write_word(word address, word data)
		{
			if ((address >> PAGE_PSIZE) == ((address + 3) >> PAGE_PSIZE))
			{
				address = map_address(address);
				route_memory(address)->write_word(address, data);
			}
			else
			{
				write_val(address, data, 4);
			}
		}

		void write_val(word address, dword val, int n_bytes);

		void reset();

	private:
		std::vector<Memory*> mems;

		word map_address(word address);

		// todo, instead of verifying that no addresses conflict here, check in the constructor of
		// the system bus
		inline Memory* route_memory(const word address)
		{
			static Memory* prev_routed = nullptr;

			/* Optimize since usually memory accesses have some localization */
			if (prev_routed != nullptr && prev_routed->in_bounds(address)) {
				return prev_routed;
			}

			for (size_t i = 0; i < mems.size(); i++) {
				if (!mems[i]->in_bounds(address)) {
					continue;
				}

				prev_routed = mems[i];
				return mems[i];
			}

			throw SystemBusException("Could not route address " + std::to_string(address) + " to memory.");
		}
};

#endif /* SYSTEM_BUS */