#pragma once
#ifndef SYSTEM_BUS_H
#define SYSTEM_BUS_H

#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/disk.h"
#include "emulator32bit/memory.h"
#include "emulator32bit/virtual_memory.h"
#include "util/loggerv2.h"

#include <vector>
class SystemBus
{
	public:
		SystemBus(RAM& ram, ROM& rom, Disk& disk, VirtualMemory& mmu);

		/* expose for now */
		RAM& ram;
		ROM& rom;
		Disk& disk;
		VirtualMemory& mmu;

		class SystemBusException : public std::exception
		{
			private:
				std::string message;

			public:
				SystemBusException(const std::string& msg);

				const char* what() const noexcept override;
		};

		inline dword read_val(word address, int n_bytes)
		{
			dword val = 0;
			for (int i = 0; i < n_bytes; i++)
			{
				val <<= 8;
				word real_adr = address;
				real_adr = map_address(address + n_bytes - i - 1);
				BaseMemory *target = route_memory(real_adr);
				val += target->read_byte(real_adr);
			}
			return val;
		}

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

		inline void write_unmapped_word(word address, word data)
		{
			route_memory(address)->write_word(address, data);
		}

		inline void write_val(word address, dword val, int n_bytes)
		{
			for (int i = 0; i < n_bytes; i++)
			{
				word real_adr = address;
				real_adr = map_address(address + i);
				BaseMemory *target = route_memory(real_adr);
				target->write_byte(real_adr, val & 0xFF);
				val >>= 8;
			}
		}

		void reset();

	private:
		inline word map_address(word address)
		{
			VirtualMemory::Exception exception;
			word addr = mmu.map_address(address, exception);

			if (exception.type == VirtualMemory::Exception::Type::AOK)
			{
				return addr;
			}

			if (exception.type == VirtualMemory::Exception::Type::DISK_RETURN_AND_FETCH_SUCCESS)
			{
				exception.type = VirtualMemory::Exception::Type::DISK_FETCH_SUCCESS; /* so the next conditional can handle */

				std::vector<byte> bytes(PAGE_SIZE);

				// EXPECTS page to be part of single memory target
				word p_addr = exception.ppage_return << PAGE_PSIZE;
				BaseMemory *target = route_memory(p_addr);

				for (word i = 0; i < PAGE_SIZE; i++)
				{
					bytes.at(i) = target->read_byte(p_addr + i);
				}

				mmu.m_disk.write_page(exception.disk_page_return, bytes);

				DEBUG_SS(std::stringstream() << "Writing physical page "
						<< std::to_string(exception.ppage_return) << " to disk page "
						<< std::to_string(exception.disk_page_return));
			}

			if (exception.type == VirtualMemory::Exception::Type::DISK_FETCH_SUCCESS)
			{
				/* handle exception by writing page fetched from disk to memory */
				word paddr = exception.ppage_fetch << PAGE_PSIZE;

				// EXPECTS page to be part of single memory target
				BaseMemory *target = route_memory(paddr);

				for (word i = 0; i < PAGE_SIZE; i++)
				{
					target->write_byte(paddr + i, exception.disk_fetch.at(i));
				}

				DEBUG_SS(std::stringstream() << "Reading physical page "
						<< std::to_string(exception.ppage_fetch) << " from disk");
			}

			return addr;
		}

		inline BaseMemory* route_memory(const word address)
		{
			if (ram.in_bounds(address))
			{
				return &ram;
			}
			else if (rom.in_bounds(address))
			{
				return &rom;
			}
			else if (disk.in_bounds(address))
			{
				return &disk;
			}
			else
			{
				throw SystemBusException("Could not route address " + std::to_string(address) + " to memory.");
			}
		}
};

#endif /* SYSTEM_BUS */