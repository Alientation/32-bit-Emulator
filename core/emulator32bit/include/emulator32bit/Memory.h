#pragma once
#ifndef MEMORY_H
#define MEMORY_H

#include "emulator32bit/emulator32bit_util.h"

class Memory
{
	public:
		Memory(word mem_pages, word lo_page);
		Memory(Memory& other);
		virtual ~Memory();

		struct ReadException {
			enum class Type {
				AOK,
				OUT_OF_BOUNDS_ADDRESS,
				ACCESS_DENIED
			};

			Type type = Type::AOK;
			word address = 0;
		};

		struct WriteException {
			enum class Type {
				AOK,
				OUT_OF_BOUNDS_ADDRESS,
				ACCESS_DENIED
			};

			Type type = Type::AOK;
			word address = 0;
			word value = 0;
			int num_bytes = 0;
		};

		virtual word read(word address, ReadException &exception, int num_bytes = 4);
		virtual void write(word address, word value, WriteException &exception, int num_bytes = 4);

		byte read_byte(word address, ReadException &exception);
		hword read_hword(word address, ReadException &exception);
		word read_word(word address, ReadException &exception);

		void write_byte(word address, byte value, WriteException &exception);
		void write_hword(word address, hword value, WriteException &exception);
		void write_word(word address, word value, WriteException &exception);

		void reset();

		word get_mem_pages();
		word get_lo_page();
		word get_hi_page();

		virtual bool in_bounds(word address);

	protected:
		word mem_pages;
		word lo_page;
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

		void write(word address, word value, WriteException &exception, int num_bytes = 4) override;

		void flash(word address, word value, WriteException &exception, int num_bytes = 4);

		void flash_word(word address, word value, WriteException &exception);
		void flash_hword(word address, hword value, WriteException &exception);
		void flash_byte(word address, byte value, WriteException &exception);
};

#endif /* MEMORY_H */