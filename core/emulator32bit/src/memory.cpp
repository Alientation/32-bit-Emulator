#include "emulator32bit/memory.h"

#define UNUSED(x) (void)(x)


BaseMemory::BaseMemory(word npages, word start_page) :
    npages(npages),
    start_page(start_page),
    start_addr(start_page << PAGE_PSIZE)
{

}

BaseMemory::~BaseMemory()
{

}


Memory::Memory(word npages, word start_page) :
    BaseMemory(npages, start_page),
    data(new byte[(npages << PAGE_PSIZE)])
{

}

Memory::Memory(Memory& other) :
    BaseMemory(other.npages, other.start_page),
    data(new byte[(other.npages << PAGE_PSIZE)])
{
    for (word i = 0; i < (npages << PAGE_PSIZE); i++)
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
    for (word addr = start_page << PAGE_PSIZE; addr < get_hi_page() << PAGE_PSIZE; addr++) {
        Memory::write_byte(addr, 0);
    }
}


/*
    RAM
*/
RAM::RAM(word npages, word start_page) :
    Memory(npages, start_page)
{

}


/*
    ROM
*/

ROM::ROM(const byte* rom_data, word npages, word start_page) :
    Memory(npages, start_page)
{
    for (word i = 0; i < npages << PAGE_PSIZE; i++) {
        data[i] = rom_data[i];
    }
}

ROM::ROM(File file, word npages, word start_page) :
    Memory(npages, start_page),
    save_file(true),
    file(file)
{
    FileReader fr(file, std::ios::binary | std::ios::in);
    std::vector<byte> bytes;
    while (fr.has_next_byte())
    {
        bytes.push_back(fr.read_byte());
    }

    if (bytes.size() > npages << PAGE_PSIZE) {
        throw ROM_Exception("ROM File is larger than the specified ROM size " +
                std::to_string(npages << PAGE_PSIZE) + " bytes. Got " +
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
        for (word i = 0; i < npages << PAGE_PSIZE; i++)
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