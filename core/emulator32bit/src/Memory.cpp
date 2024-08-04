#include "emulator32bit/memory.h"

#define UNUSED(x) (void)(x)
Memory::Memory(word mem_pages, word lo_page) :
	mem_pages(mem_pages),
	lo_page(lo_page),
	lo_addr(lo_page << PAGE_PSIZE),
	data(new byte[(mem_pages << PAGE_PSIZE)])
{

}

Memory::Memory(Memory& other) :
	mem_pages(other.mem_pages),
	lo_page(other.lo_page),
	lo_addr(lo_page << PAGE_PSIZE)
{
	data = new byte[(mem_pages << PAGE_PSIZE)];
	for (word i = 0; i < (mem_pages << PAGE_PSIZE); i++) {
		data[i] = other.data[i];
	}
}

Memory::~Memory()
{
	if (data)
		delete[] data;
}


Memory::MemoryReadException::MemoryReadException(const std::string& msg) :
	message(msg)
{

}

const char* Memory::MemoryReadException::what() const noexcept
{
	return message.c_str();
}

Memory::MemoryWriteException::MemoryWriteException(const std::string& msg) :
	message(msg)
{

}

const char* Memory::MemoryWriteException::what() const noexcept
{
	return message.c_str();
}

word Memory::get_mem_pages()
{
	return mem_pages;
}

word Memory::get_lo_page()
{
	return lo_page;
}

word Memory::get_hi_page()
{
	return lo_page + mem_pages - 1;
}

bool Memory::in_bounds(word address)
{
	return address >= (lo_page << PAGE_PSIZE) && address < ((get_hi_page()+1) << PAGE_PSIZE);
}

word Memory::read(word address, int num_bytes)
{
	if (!in_bounds(address) || !in_bounds(address + num_bytes - 1)) {
		throw MemoryReadException("Out of bounds address " + std::to_string(address) + " - " +
				std::to_string(address + num_bytes - 1));
	}

	address -= lo_page << PAGE_PSIZE;
	word value = 0;
	for (int i = num_bytes - 1; i >= 0; i--) {
		value <<= 8;
		value += data[(word) (address + i)];
	}
	return value;
}

void Memory::write(word address, word value, int num_bytes)
{
	if (!in_bounds(address) || !in_bounds(address + num_bytes - 1)) {
		throw MemoryWriteException("Out of bounds address " + std::to_string(address) + " - " +
				std::to_string(address + num_bytes - 1));
	}

	address -= lo_page << PAGE_PSIZE;
	for (int i = 0; i < num_bytes; i++) {
		data[(word) (address + i)] = value & 0xFF;
		value >>= 8;
	}
}


byte Memory::read_byte(word address)
{
	if (!in_bounds(address)) {
		throw MemoryReadException("Out of bounds address " + std::to_string(address));
	}

	address -= lo_page << PAGE_PSIZE;
	return data[address];
}

hword Memory::read_hword(word address)
{
	return read(address, 2);
}

word Memory::read_word(word address)
{
	return read(address, 4);
}

void Memory::write_byte(word address, byte value)
{
	if (!in_bounds(address)) {
		throw MemoryWriteException("Out of bounds address " + std::to_string(address));
	}

	address -= lo_page << PAGE_PSIZE;
	data[address] = value;
}

void Memory::write_hword(word address, hword value)
{
	write(address, value, 2);
}

void Memory::write_word(word address, word value)
{
	write(address, value, 4);
}

void Memory::reset()
{
	for (word addr = lo_page << PAGE_PSIZE; addr < get_hi_page() << PAGE_PSIZE; addr++) {
		Memory::write_byte(addr, 0);
	}
}


/*
	RAM
*/
RAM::RAM(word mem_pages, word lo_page) :
	Memory(mem_pages, lo_page)
{

}


/*
	ROM
*/

ROM::ROM(const byte* rom_data, word mem_pages, word lo_page) :
	Memory(mem_pages, lo_page)
{
	for (word i = 0; i < mem_pages << PAGE_PSIZE; i++) {
		data[i] = rom_data[i];
	}
}

void ROM::write(word address, word value, int num_bytes)
{
	UNUSED(address);
	UNUSED(value);
	UNUSED(num_bytes);

	throw MemoryWriteException("Cannot write to ROM.");
}

void ROM::flash(word address, word data, int num_bytes) {
	Memory::write(address, data, num_bytes);
}

void ROM::flash_word(word address, word data) {
	flash(address, data, 4);
}

void ROM::flash_hword(word address, hword data) {
	flash(address, data, 2);
}

void ROM::flash_byte(word address, byte data) {
	flash(address, data, 1);
}
