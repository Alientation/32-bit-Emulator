#pragma once
#ifndef MEMORY_H
#define MEMORY_H

#include "emulator32bit/emulator32bit_util.h"
#include "util/file.h"

#include <string>

class Memory
{
	public:
		Memory(word mem_pages, word lo_page);
		Memory(Memory& other);
		virtual ~Memory();

		// FOR SEG FAULTS, WE CAN USE SIGNAL HANDLERS TO CATCH AND HANDLE THEM IN THE KERNEL, WITHOUT
		// HAVING AN EXPENSIVE CONDITIONAL CHECK EVERYTIME MEMORY IS ACCESSED
		virtual inline byte read_byte(word address)
		{
			return data[address - (lo_page << PAGE_PSIZE)];
		}

		virtual inline hword read_hword(word address)
		{
			address -= lo_addr;
			return ((hword*)(data + (address & 1)))[address >> 1];
		}

		virtual inline word read_word(word address)
		{
			address -= lo_addr;
			return ((word*)(data + (address & 0b11)))[address >> 2];
		}

		virtual inline word read_word_aligned(word address)
		{
			return ((word*) data)[(address - lo_addr) >> 2];
		}

		virtual inline void write_byte(word address, byte value)
		{
			data[address - (lo_page << PAGE_PSIZE)] = value;
		}

		virtual inline void write_hword(word address, hword value)
		{
			address -= lo_addr;
			((hword*)(data + (address & 1)))[address >> 1] = value;
		}

		virtual inline void write_word(word address, word value)
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
		ROM(File file, word mem_pages, word lo_page);
		~ROM() override;

		class ROM_Exception : public std::exception
		{
			private:
				std::string message;

			public:
				ROM_Exception(std::string msg);
				const char* what() const noexcept override;
		};


	private:
		bool save_file = false;
		File file;
};

#endif /* MEMORY_H */