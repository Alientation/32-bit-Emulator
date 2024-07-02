#pragma once
#ifndef RAM_H

#include "emulator32bit/Emulator32bitUtil.h"

class Memory {
	public:
		Memory(word mem_size, word lo_addr);
		Memory(Memory& other);
		virtual ~Memory();

		struct MemoryReadException {
			enum class Type {
				AOK,
				OUT_OF_BOUNDS_ADDRESS,
				ACCESS_DENIED
			};

			Type type = Type::AOK;
			word address = 0;
		};

		struct MemoryWriteException {
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

		virtual word read(word address, MemoryReadException &exception, int num_bytes = 4);
		virtual void write(word address, word data, MemoryWriteException &exception, int num_bytes = 4);

		byte read_byte(word address, MemoryReadException &exception);
		hword read_hword(word address, MemoryReadException &exception);
		word read_word(word address, MemoryReadException &exception);

		void write_byte(word address, byte data, MemoryWriteException &exception);
		void write_hword(word address, hword data, MemoryWriteException &exception);
		void write_word(word address, word data, MemoryWriteException &exception);

		void reset();

		virtual bool in_bounds(word address);

	protected:
		word mem_size;
		byte* data;

		word lo_addr;
};

class RAM : public Memory {
	public:
		RAM(word mem_size, word lo_addr);
};

class ROM : public Memory {
	public:
		ROM(const byte* data, word mem_size, word lo_addr);

		void write(word address, word data, MemoryWriteException &exception, int num_bytes = 4) override;
};


#endif