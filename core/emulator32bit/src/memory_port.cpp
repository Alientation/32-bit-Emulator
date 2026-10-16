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
    for (U8 i = 0; i < n_bytes; i++) m_bus.route_store(real_addr[i]);

    // Each byte is translated again just before it is stored: in the swapping memory, bringing in
    // the second page can evict the first, and its address from above is then another page's. (A
    // page does not move between RAM and ROM, so the routing above still holds.)
    for (U8 i = 0; i < n_bytes; i++)
    {
        const word now = m_mmu.translate_address(address + i, VirtualMemory::AccessType::WRITE);
        m_bus.route_store(now).write_byte(now, val & 0xFF);
        val >>= 8;
    }
}

void MemoryPort::write_block(word address, const byte *data, word size)
{
    while (size != 0)
    {
        const word left_in_page = kPageSize - (address & (kPageSize - 1));
        const word in_page = size < left_in_page ? size : left_in_page;
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

    throw SystemBus::BusError("Instruction fetch outside of RAM and ROM at address "
                              + std::to_string(address));
}
