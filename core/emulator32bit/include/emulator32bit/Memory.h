#pragma once
#ifndef MEMORY_H
#define MEMORY_H

#include "emulator32bit/emulator32bit_util.h"

#include <string>

class Memory
{
	public:
		Memory(word mem_pages, word lo_page);
		Memory(Memory& other);
		virtual ~Memory();

		// FOR SEG FAULTS, WE CAN USE SIGNAL HANDLERS TO CATCH AND HANDLE THEM IN THE KERNEL, WITHOUT
		// HAVING AN EXPENSIVE CONDITIONAL CHECK EVERYTIME MEMORY IS ACCESSED
		inline byte read_byte(word address)
		{
			return data[address - (lo_page << PAGE_PSIZE)];
		}

		inline hword read_hword(word address)
		{
			address -= lo_addr;
			return ((hword*)(data + (address & 1)))[address >> 1];
		}

		inline word read_word(word address)
		{
			address -= lo_addr;
			return ((word*)(data + (address & 0b11)))[address >> 2];
		}

		inline word read_word_aligned(word address)
		{
			return ((word*) data)[(address - lo_addr) >> 2];
		}

		inline void write_byte(word address, byte value)
		{
			data[address - (lo_page << PAGE_PSIZE)] = value;
		}

		inline void write_hword(word address, hword value)
		{
			address -= lo_addr;
			((hword*)(data + (address & 1)))[address >> 1] = value;
		}

		inline void write_word(word address, word value)
		{
			address -= lo_addr;
			((word*)(data + (address & 0b11)))[address >> 2] = value;
		}


		void reset();

		inline word get_mem_pages()
		{
			return mem_pages;
		}

		inline word get_lo_page()
		{
			return lo_page;
		}

		inline word get_hi_page()
		{
			return lo_page + mem_pages - 1;
		}

		inline bool in_bounds(word address)
		{
			return address >= (lo_page << PAGE_PSIZE) && address < ((get_hi_page()+1) << PAGE_PSIZE);
		}

	protected:
		word mem_pages;
		word lo_page;
		word lo_addr;
		byte* data;
};

class RAM : public Memory
{
	public:
		RAM(word mem_pages, word lo_page);
};

class ROM : public Memory
{
	public:
		ROM(const byte* data, word mem_pages, word lo_page);

		// void write(word address, word value, int num_bytes = 4) override;

		// void flash(word address, word value, int num_bytes = 4);

		// void flash_word(word address, word value);
		// void flash_hword(word address, hword value);
		// void flash_byte(word address, byte value);
};

#endif /* MEMORY_H */