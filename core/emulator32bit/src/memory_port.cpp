#include "emulator32bit/memory_port.h"

#include <algorithm>
#include <string>

MemoryPort::MemoryPort(SystemBus &bus, VirtualMemory &mmu) :
    m_bus(bus),
    m_mmu(mmu),
    m_ram(bus.ram.get()),
    m_rom(bus.rom.get())
{
}

dword MemoryPort::read_val(const word address, const U8 n_bytes)
{
    dword val = 0;
    for (U8 i = 0; i < n_bytes; i++)
    {
        const word real_addr = m_mmu.translate_address(address + n_bytes - i - 1);
        val = (val << 8) + m_bus.route_memory(real_addr).read_byte(real_addr);
    }
    return val;
}

void MemoryPort::write_val(const word address, dword val, const U8 n_bytes)
{
    // Everything is translated before anything is written, so that a store that crosses into
    // a page that cannot be written leaves memory as it was.
    word real_addr[sizeof(dword)];
    for (U8 i = 0; i < n_bytes; i++)
    {
        real_addr[i] = m_mmu.translate_address(address + i, VirtualMemory::AccessType::WRITE);
    }
    // The same goes for the ROM, which is not known to be one until it is routed.
    BaseMemory *target[sizeof(dword)];
    for (U8 i = 0; i < n_bytes; i++) target[i] = &m_bus.route_store(real_addr[i]);
    for (U8 i = 0; i < n_bytes; i++)
    {
        target[i]->write_byte(real_addr[i], val & 0xFF);
        val >>= 8;
    }
}

void MemoryPort::write_block(word address, const byte *data, word size)
{
    while (size != 0)
    {
        const word in_page = std::min<word>(size, kPageSize - (address & (kPageSize - 1)));
        const word real_addr = m_mmu.translate_address(address, VirtualMemory::AccessType::WRITE);
        m_bus.route_store(real_addr).write_block(real_addr, data, in_page);
        address += in_page;
        data += in_page;
        size -= in_page;
    }
}

word MemoryPort::fetch_outside_ram(const word real_addr, const word address)
{
    // Code can also run from the ROM, which is where a machine boots from.
    if (m_rom->in_bounds(real_addr)) return m_rom->read_word_aligned(real_addr);

    throw SystemBus::Exception("Instruction fetch outside of RAM and ROM at address "
                               + std::to_string(address));
}
