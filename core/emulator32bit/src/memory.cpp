#include "emulator32bit/memory.h"

#include "util/common.h"

BaseMemory::BaseMemory (word npages, word start_page) :
    m_npages (npages),
    m_start_page (start_page),
    m_start_addr (start_page << kNumPageOffsetBits)
{
}

BaseMemory::~BaseMemory ()
{
}

Memory::Memory (word npages, word start_page) :
    BaseMemory (npages, start_page),
    m_data (new byte[(npages << kNumPageOffsetBits)])
{
}

Memory::Memory (Memory &other) :
    BaseMemory (other.m_npages, other.m_start_page),
    m_data (new byte[(other.m_npages << kNumPageOffsetBits)])
{
    for (word i = 0; i < (m_npages << kNumPageOffsetBits); i++)
    {
        m_data[i] = other.m_data[i];
    }
}

Memory::~Memory ()
{
    if (m_data)
    {
        delete[] m_data;
    }
}

void Memory::reset ()
{
    for (word addr = m_start_page << kNumPageOffsetBits;
         addr < get_hi_page () << kNumPageOffsetBits; addr++)
    {
        Memory::write_byte (addr, 0);
    }
}

/*
    RAM
*/
RAM::RAM (word npages, word start_page) :
    Memory (npages, start_page)
{
}

/*
    ROM
*/

ROM::ROM (const byte *rom_data, word npages, word start_page) :
    Memory (npages, start_page)
{
    for (word i = 0; i < npages << kNumPageOffsetBits; i++)
    {
        m_data[i] = rom_data[i];
    }
}

ROM::ROM (word npages, word start_page) :
    Memory (npages, start_page)
{
    for (word i = 0; i < npages << kNumPageOffsetBits; i++)
    {
        m_data[i] = 0;
    }
}

ROM::ROM (File file, word npages, word start_page) :
    Memory (npages, start_page),
    m_save_file (true),
    m_file (file)
{
    FileReader fr (file, std::ios::binary | std::ios::in);
    std::vector<byte> bytes;
    while (fr.has_next_byte ())
    {
        bytes.push_back (fr.read_byte ());
    }

    if (bytes.size () > npages << kNumPageOffsetBits)
    {
        throw ROM_Exception ("ROM File is larger than the specified ROM size "
                             + std::to_string (npages << kNumPageOffsetBits) + " bytes. Got "
                             + std::to_string (bytes.size ()) + " bytes.");
    }

    for (size_t i = 0; i < bytes.size (); i++)
    {
        m_data[i] = bytes[i];
    }
}

ROM::~ROM ()
{
    if (m_save_file)
    {
        // save data to file
        FileWriter fw (m_file, std::ios::out | std::ios::binary);
        for (word i = 0; i < m_npages << kNumPageOffsetBits; i++)
        {
            fw.write (m_data[i]);
        }
    }
}

ROM::ROM_Exception::ROM_Exception (std::string msg) :
    message (msg)
{
}

const char *ROM::ROM_Exception::what () const noexcept
{
    return message.c_str ();
}