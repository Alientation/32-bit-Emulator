#pragma once
#ifndef RAM_H

#include "emulator32bit/Emulator32bitUtil.h"

class Memory {
	public:
		Memory(word mem_size, word lo_addr);
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

		byte readByte(word address, MemoryReadException &exception);
		hword readHalfWord(word address, MemoryReadException &exception);
		word readWord(word address, MemoryReadException &exception);

		void writeByte(word address, byte data, MemoryWriteException &exception);
		void writeHalfWord(word address, hword data, MemoryWriteException &exception);
		void writeWord(word address, word data, MemoryWriteException &exception);

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
		ROM(const byte (&rom_data)[], word mem_size, word lo_addr);

		void write(word address, word data, MemoryWriteException &exception, int num_bytes = 4) override;
};


#endif