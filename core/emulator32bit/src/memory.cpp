#include "emulator32bit/memory.h"

#define UNUSED(x) (void)(x)


BaseMemory::BaseMemory(word mem_pages, word lo_page) :
	mem_pages(mem_pages),
	lo_page(lo_page),
	lo_addr(lo_page << PAGE_PSIZE)
{

}

BaseMemory::~BaseMemory()
{

}


Memory::Memory(word mem_pages, word lo_page) :
	BaseMemory(mem_pages, lo_page),
	data(new byte[(mem_pages << PAGE_PSIZE)])
{

}

Memory::Memory(Memory& other) :
	BaseMemory(other.mem_pages, other.lo_page),
	data(new byte[(other.mem_pages << PAGE_PSIZE)])
{
	for (word i = 0; i < (mem_pages << PAGE_PSIZE); i++)
	{
		data[i] = other.data[i];
	}
}

Memory::~Memory()
{
	if (data)
	{
		delete[] data;
	}
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

ROM::ROM(File file, word mem_pages, word lo_page) :
	Memory(mem_pages, lo_page),
	save_file(true),
	file(file)
{
	FileReader fr(file, std::ios::binary | std::ios::in);
	std::vector<byte> bytes;
	while (fr.has_next_byte())
	{
		bytes.push_back(fr.read_byte());
	}

	if (bytes.size() > mem_pages << PAGE_PSIZE) {
		throw ROM_Exception("ROM File is larger than the specified ROM size " +
				std::to_string(mem_pages << PAGE_PSIZE) + " bytes. Got " +
				std::to_string(bytes.size()) + " bytes.");
	}

	for (size_t i = 0; i < bytes.size(); i++) {
		data[i] = bytes[i];
	}
}

ROM::~ROM()
{
	if (save_file)
	{
		// save data to file
		FileWriter fw(file, std::ios::out | std::ios::binary);
		for (word i = 0; i < mem_pages << PAGE_PSIZE; i++)
		{
			fw.write(data[i]);
		}
	}
}

ROM::ROM_Exception::ROM_Exception(std::string msg) :
	message(msg)
{

}

const char* ROM::ROM_Exception::what() const noexcept
{
	return message.c_str();
}