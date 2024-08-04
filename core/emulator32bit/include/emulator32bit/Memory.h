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

		class MemoryReadException : public std::exception
		{
			private:
				std::string message;

			public:
				MemoryReadException(const std::string& msg);

				const char* what() const noexcept override;
		};

		class MemoryWriteException : public std::exception
		{
			private:
				std::string message;

			public:
				MemoryWriteException(const std::string& msg);

				const char* what() const noexcept override;
		};

		virtual word read(word address, int num_bytes = 4);
		virtual void write(word address, word value, int num_bytes = 4);

		byte read_byte(word address);
		hword read_hword(word address);
		word read_word(word address);

		inline word read_word_aligned(word address) {
			return ((word*) data)[(address - lo_addr) >> 2];
		}

		void write_byte(word address, byte value);
		void write_hword(word address, hword value);
		void write_word(word address, word value);

		void reset();

		word get_mem_pages();
		word get_lo_page();
		word get_hi_page();

		virtual bool in_bounds(word address);

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

		void write(word address, word value, int num_bytes = 4) override;

		void flash(word address, word value, int num_bytes = 4);

		void flash_word(word address, word value);
		void flash_hword(word address, hword value);
		void flash_byte(word address, byte value);
};

#endif /* MEMORY_H */