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
