#include "emulator32bit/Memory.h"
#include "assert.h"
#include "typeinfo"
#include "iostream"

Memory::Memory(const word mem_size, const word lo_addr, const word hi_addr) {
	assert(lo_addr >= 0 && hi_addr >= lo_addr);
	assert(mem_size == hi_addr - lo_addr + 1);

	this->mem_size = mem_size;
	this->data = new byte[mem_size];

	this->lo_addr = lo_addr;
	this->hi_addr = hi_addr;
}

Memory::~Memory() {
	delete[] this->data;
}

bool Memory::in_bounds(const word address) {
	return address >= this->lo_addr && address <= this->hi_addr;
}

byte Memory::readByte(const word address, MemoryReadException *exception) {
	assert(exception != nullptr);
	
	if (address < this->lo_addr || address > this->hi_addr
		|| address - lo_addr >= mem_size) {
		exception->type = MemoryReadException::OUT_OF_BOUNDS_ADDRESS;
		exception->address = address;
		return 0;
	}

	return this->data[address - lo_addr];
}

hword Memory::readHalfWord(const word address, MemoryReadException *exception) {
	assert(exception != nullptr);
	
	if (address < this->lo_addr || address > this->hi_addr
		|| address - lo_addr + 1 >= mem_size) {
		exception->type = MemoryReadException::OUT_OF_BOUNDS_ADDRESS;
		exception->address = address;
		return 0;
	}

	return bytes_to_hword(this->data[address - lo_addr], this->data[address - lo_addr + 1]);
}

word Memory::readWord(const word address, MemoryReadException *exception) {
	assert(exception != nullptr);
	
	if (address < this->lo_addr || address > this->hi_addr
		|| address - lo_addr + 3 >= mem_size) {
		exception->type = MemoryReadException::OUT_OF_BOUNDS_ADDRESS;
		exception->address = address;
		return 0;
	}

	return bytes_to_word(this->data[address - lo_addr], this->data[address - lo_addr + 1],
		this->data[address - lo_addr + 2], this->data[address - lo_addr + 3]);
}

void Memory::writeByte(const word address, const byte data, MemoryWriteException *exception) {
	assert(exception != nullptr);
	
	if (address < this->lo_addr || address > this->hi_addr
		|| address - lo_addr >= mem_size) {
		exception->type = MemoryWriteException::OUT_OF_BOUNDS_ADDRESS;
		exception->address = address;
		return;
	}

	this->data[address - lo_addr] = data;
}

void Memory::writeHalfWord(const word address, const hword data, MemoryWriteException *exception) {
	assert(exception != nullptr);
	
	if (address < this->lo_addr || address > this->hi_addr
		|| address - lo_addr + 1 >= mem_size) {
		exception->type = MemoryWriteException::OUT_OF_BOUNDS_ADDRESS;
		exception->address = address;
		return;
	}

	this->data[address - lo_addr] = byte_from_word(data, 0);
	this->data[address - lo_addr + 1] = byte_from_word(data, 1);
}

void Memory::writeWord(const word address, const word data, MemoryWriteException *exception) {
	assert(exception != nullptr);

	if (address < this->lo_addr || address > this->hi_addr
		|| address - lo_addr + 3 >= mem_size) {
		exception->type = MemoryWriteException::OUT_OF_BOUNDS_ADDRESS;
		exception->address = address;
		return;
	}

	this->data[address - lo_addr] = byte_from_word(data, 0);
	this->data[address - lo_addr + 1] = byte_from_word(data, 1);
	this->data[address - lo_addr + 2] = byte_from_word(data, 2);
	this->data[address - lo_addr + 3] = byte_from_word(data, 3);
}


/*
	RAM
*/

byte RAM::readByte(const word address, MemoryReadException *exception) {
	assert(exception != nullptr);

	// Read a byte from the RAM
	return Memory::readByte(address, exception);
}

hword RAM::readHalfWord(const word address, MemoryReadException *exception) {
	assert(exception != nullptr);
	
	// Read a half word from the RAM
	return Memory::readHalfWord(address, exception);
}

word RAM::readWord(const word address, MemoryReadException *exception) {
	assert(exception != nullptr);
	
	// Read a word from the RAM
	return Memory::readWord(address, exception);
}

void RAM::writeByte(const word address, const byte data, MemoryWriteException *exception) {
	assert(exception != nullptr);

	// Write a byte to the RAM
	Memory::writeByte(address, data, exception);
}

void RAM::writeHalfWord(const word address, const hword data, MemoryWriteException *exception) {
	assert(exception != nullptr);

	// Write a half word to the RAM
	Memory::writeHalfWord(address, data, exception);
}

void RAM::writeWord(const word address, const word data, MemoryWriteException *exception) {
	assert(exception != nullptr);

	// Write a word to the RAM
	Memory::writeWord(address, data, exception);
}


/*
	ROM
*/

ROM::ROM(const byte rom_data[], const word lo_addr, const word hi_addr) : Memory(hi_addr - lo_addr + 1, lo_addr, hi_addr) {
	for (word i = 0; i < mem_size; i++) {
		this->data[i] = rom_data[i];
	}
}

byte ROM::readByte(const word address, MemoryReadException *exception) {
	assert(exception != nullptr);

	// Read a byte from the ROM
	return Memory::readByte(address, exception);
}

hword ROM::readHalfWord(const word address, MemoryReadException *exception) {
	assert(exception != nullptr);

	// Read a half word from the ROM
	return Memory::readHalfWord(address, exception);
}

word ROM::readWord(const word address, MemoryReadException *exception) {
	assert(exception != nullptr);

	// Read a word from the ROM
	return Memory::readWord(address, exception);
}

void ROM::writeByte(const word address, const byte data, MemoryWriteException *exception) {
	assert(exception != nullptr);

	exception->type = MemoryWriteException::MemoryWriteExceptionType::ACCESS_DENIED;
}

void ROM::writeHalfWord(const word address, const hword data, MemoryWriteException *exception) {
	assert(exception != nullptr);

	exception->type = MemoryWriteException::MemoryWriteExceptionType::ACCESS_DENIED;
}

void ROM::writeWord(const word address, const word data, MemoryWriteException *exception) {
	assert(exception != nullptr);

	exception->type = MemoryWriteException::MemoryWriteExceptionType::ACCESS_DENIED;
}