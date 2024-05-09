#pragma once
#ifndef RAM_H

#include "typeinfo"
#include "emulator32bit/Emulator32bitUtil.h"

class Memory {
	public:
		Memory(const word mem_size, const word lo_addr, const word hi_addr);
		virtual ~Memory();

		struct MemoryReadException {
			enum MemoryReadExceptionType {
				AOK,
				OUT_OF_BOUNDS_ADDRESS,
				ACCESS_DENIED
			};

			MemoryReadExceptionType type = AOK;
			word address = 0;
		};

		struct MemoryWriteException {
			enum MemoryWriteExceptionType {
				AOK,
				OUT_OF_BOUNDS_ADDRESS,
				ACCESS_DENIED
			};

			MemoryWriteExceptionType type = AOK;
			word address = 0;
		};	

		virtual byte readByte(const word address, MemoryReadException *exception);
		virtual hword readHalfWord(const word address, MemoryReadException *exception);
		virtual word readWord(const word address, MemoryReadException *exception);

		virtual void writeByte(const word address, const byte data, MemoryWriteException *exception);
		virtual void writeHalfWord(const word address, const hword data, MemoryWriteException *exception);
		virtual void writeWord(const word address, const word data, MemoryWriteException *exception);

		virtual bool in_bounds(const word address);
	
	protected:
		word mem_size;
		byte* data;
		
		word lo_addr;
		word hi_addr;
};

class RAM : public Memory {
	public:
		RAM(const word mem_size, const word lo_addr, const word hi_addr) : Memory(mem_size, lo_addr, hi_addr) {}

		/**
		 * Read a byte from the RAM
		 * 
		 * @param address The address to read from
		 * @param exception The exception raised by the read operation
		 * @return The byte read from the address
		 */
		byte readByte(const word address, MemoryReadException *exception) override;

		/**
		 * Read a half word from the RAM
		 * 
		 * @param address The address to read from
		 * @param exception The exception raised by the read operation
		 * @return The half word read from the address
		 */
		hword readHalfWord(const word address, MemoryReadException *exception) override;

		/**
		 * Read a word from the RAM
		 * 
		 * @param address The address to read from
		 * @param exception The exception raised by the read operation
		 * @return The word read from the address
		 */
		word readWord(const word address, MemoryReadException *exception) override;

		/**
		 * Write a byte to the RAM
		 * 
		 * @param address The address to write to
		 * @param exception The exception raised by the write operation
		 * @param data The byte to write
		 */
		void writeByte(const word address, const byte data, MemoryWriteException *exception) override;

		/**
		 * Write a half word to the RAM
		 * 
		 * @param address The address to write to
		 * @param exception The exception raised by the write operation
		 * @param data The half word to write
		 */
		void writeHalfWord(const word address, const hword data, MemoryWriteException *exception) override;

		/**
		 * Write a word to the RAM
		 * 
		 * @param address The address to write to
		 * @param exception The exception raised by the write operation
		 * @param data The word to write
		 */
		void writeWord(const word address, const word data, MemoryWriteException *exception) override;
};

class ROM : public Memory {
	public:
		ROM(const byte rom_data[], const word lo_addr, const word hi_addr);

		/**
		 * Read a byte from the ROM
		 * 
		 * @param address The address to read from
		 * @param exception The exception raised by the read operation
		 * @return The byte read from the address
		 */
		byte readByte(const word address, MemoryReadException *exception) override;

		/**
		 * Read a half word from the ROM
		 * 
		 * @param address The address to read from
		 * @param exception The exception raised by the read operation
		 * @return The half word read from the address
		 */
		hword readHalfWord(const word address, MemoryReadException *exception) override;

		/**
		 * Read a word from the ROM
		 * 
		 * @param address The address to read from
		 * @param exception The exception raised by the read operation
		 * @return The word read from the address
		 */
		word readWord(const word address, MemoryReadException *exception) override;

		/**
		 * Write a byte to the ROM
		 * 
		 * @param address The address to write to
		 * @param exception The exception raised by the write operation
		 * @param data The byte to write
		 */
		void writeByte(const word address, const byte data, MemoryWriteException *exception) override;

		/**
		 * Write a half word to the ROM
		 * 
		 * @param address The address to write to
		 * @param exception The exception raised by the write operation
		 * @param data The half word to write
		 */
		void writeHalfWord(const word address, const hword data, MemoryWriteException *exception) override;

		/**
		 * Write a word to the ROM
		 * 
		 * @param address The address to write to
		 * @param exception The exception raised by the write operation
		 * @param data The word to write
		 */
		void writeWord(const word address, const word data, MemoryWriteException *exception) override;
};


#endif