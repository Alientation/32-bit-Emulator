#include "emulator32bit/memory.h"

Memory::Memory(word mem_pages, word lo_page) :
	mem_pages(mem_pages),
	lo_page(lo_page),
	data(new byte[(mem_pages << PAGE_PSIZE)])
{

}

Memory::Memory(Memory& other) :
	mem_pages(other.mem_pages),
	lo_page(other.lo_page)
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

word Memory::read(word address, ReadException &exception, int num_bytes)
{
	if (!in_bounds(address) || !in_bounds(address + num_bytes - 1)) {
		exception.type = ReadException::Type::OUT_OF_BOUNDS_ADDRESS;
		exception.address = address;
		return 0;
	}

	address -= lo_page << PAGE_PSIZE;
	word value = 0;
	for (int i = num_bytes - 1; i >= 0; i--) {
		value <<= 8;
		value += data[(word) (address + i)];
	}
	return value;
}

void Memory::write(word address, word value, WriteException &exception, int num_bytes)
{
	if (!in_bounds(address) || !in_bounds(address + num_bytes - 1)) {
		exception.type = WriteException::Type::OUT_OF_BOUNDS_ADDRESS;
		exception.address = address;
		exception.value = value;
		exception.num_bytes = num_bytes;
		return;
	}

	address -= lo_page << PAGE_PSIZE;
	for (int i = 0; i < num_bytes; i++) {
		data[(word) (address + i)] = value & 0xFF;
		value >>= 8;
	}
}


byte Memory::read_byte(word address, ReadException &exception)
{
	if (!in_bounds(address)) {
		exception.type = ReadException::Type::OUT_OF_BOUNDS_ADDRESS;
		exception.address = address;
		return 0;
	}

	address -= lo_page << PAGE_PSIZE;
	return data[address];
}

hword Memory::read_hword(word address, ReadException &exception)
{
	return read(address, exception, 2);
}

word Memory::read_word(word address, ReadException &exception)
{
	return read(address, exception, 4);
}

void Memory::write_byte(word address, byte value, WriteException &exception)
{
	if (!in_bounds(address)) {
		exception.type = WriteException::Type::OUT_OF_BOUNDS_ADDRESS;
		exception.address = address;
		exception.value = value;
		exception.num_bytes = 1;
		return;
	}

	address -= lo_page << PAGE_PSIZE;
	data[address] = value;
}

void Memory::write_hword(word address, hword value, WriteException &exception)
{
	write(address, value, exception, 2);
}

void Memory::write_word(word address, word value, WriteException &exception)
{
	write(address, value, exception, 4);
}

void Memory::reset()
{
	for (word addr = lo_page << PAGE_PSIZE; addr < get_hi_page() << PAGE_PSIZE; addr++) {
		WriteException exception;
		Memory::write_byte(addr, 0, exception);
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

void ROM::write(word address, word value, WriteException &exception, int num_bytes)
{
	exception.type = WriteException::Type::ACCESS_DENIED;
	exception.address = address;
	exception.value = value;
	exception.num_bytes = num_bytes;
}

void ROM::flash(word address, word data, WriteException &exception, int num_bytes) {
	Memory::write(address, data, exception, num_bytes);
}

void ROM::flash_word(word address, word data, WriteException &exception) {
	flash(address, data, exception, 4);
}

void ROM::flash_hword(word address, hword data, WriteException &exception) {
	flash(address, data, exception, 2);
}

void ROM::flash_byte(word address, byte data, WriteException &exception) {
	flash(address, data, exception, 1);
}
