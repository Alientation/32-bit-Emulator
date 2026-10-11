#pragma once

#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/memory.h"
#include "emulator32bit/system_bus.h"
#include "emulator32bit/virtual_memory.h"

/// How the CPU reaches memory: the load/store unit. The addresses of a program are virtual, so
/// every access is translated by the MMU and then made on the (physical) system bus.
///
/// The port refers to the bus and to the MMU, the CPU owns all three.
class MemoryPort
{
  public:
    /// @param bus The physical address space that the accesses end up on.
    /// @param mmu The MMU that translates the virtual addresses of a program.
    MemoryPort(SystemBus &bus, VirtualMemory &mmu);

    MemoryPort(const MemoryPort &) = delete;
    MemoryPort &operator=(const MemoryPort &) = delete;

    /// Whether the bytes at an address are all in one page, so that one translation covers them.
    ///
    /// @param address The start address (lowest address of the block of data).
    /// @param n_bytes The size of the data. Data is located at [address, address + n_bytes - 1].
    /// @return Whether the data lies within one page.
    static constexpr bool is_within_page(const word address, const U8 n_bytes)
    {
        return (address >> kNumPageOffsetBits) == ((address + n_bytes - 1) >> kNumPageOffsetBits);
    }

    /// Read a byte, half word or word (T is `byte`, `hword` or `word`) at a virtual address. The
    /// value is little endian in memory, and may cross a page.
    ///
    /// @param address The virtual address.
    /// @return The value.
    /// @throws VirtualMemory::PageFault if the access is not allowed.
    /// @throws SystemBus::BusError if no memory is at the physical address.
    template<class T>
    inline T read(const word address)
    {
        if constexpr (sizeof(T) > sizeof(byte))
        {
            if (UNLIKELY(!is_within_page(address, sizeof(T))))
                return T(read_val(address, sizeof(T)));
        }

        return read_aligned<T>(address);
    }

    /// read<T> for an address that lies within one page, which an address that is a multiple of
    /// sizeof(T) always does: one translation and no check for a page boundary.
    ///
    /// @param address The virtual address.
    /// @return The value.
    /// @throws VirtualMemory::PageFault if the access is not allowed.
    /// @throws SystemBus::BusError if no memory is at the physical address.
    template<class T>
    inline T read_aligned(const word address)
    {
        const word real_addr = m_mmu.translate_address(address);
        if (LIKELY(m_ram->in_bounds(real_addr))) return mem_read<T>(*m_ram, real_addr);
        return mem_read<T>(m_bus.route_memory(real_addr), real_addr);
    }

    /// Write a byte, half word or word (T is `byte`, `hword` or `word`) at a virtual address. The
    /// value is little endian in memory, and may cross a page. A store that crosses into a page
    /// that cannot be written leaves memory as it was.
    ///
    /// @param address The virtual address.
    /// @param data The value to store.
    /// @throws VirtualMemory::PageFault if the access is not allowed.
    /// @throws SystemBus::BusError if no memory is at the physical address.
    template<class T>
    inline void write(const word address, const T data)
    {
        if constexpr (sizeof(T) > sizeof(byte))
        {
            if (UNLIKELY(!is_within_page(address, sizeof(T))))
            {
                write_val(address, data, sizeof(T));
                return;
            }
        }

        write_aligned<T>(address, data);
    }

    /// write<T> for an address that lies within one page, see read_aligned<T>.
    ///
    /// @param address The virtual address.
    /// @param data The value to store.
    /// @throws VirtualMemory::PageFault if the access is not allowed.
    /// @throws SystemBus::BusError if no memory is at the physical address.
    template<class T>
    inline void write_aligned(const word address, const T data)
    {
        const word real_addr = m_mmu.translate_address(address, VirtualMemory::AccessType::WRITE);
        if (LIKELY(m_ram->in_bounds(real_addr)))
        {
            mem_write<T>(*m_ram, real_addr, data);
            return;
        }
        mem_write<T>(m_bus.route_store(real_addr), real_addr, data);
    }

    // The sized forms of read<T> and write<T>, at a virtual address. Each one has the exceptions of
    // read<T> or write<T>.

    /// @param address The virtual address.
    /// @return The byte.
    inline byte read_byte(const word address)
    {
        return read<byte>(address);
    }

    /// @param address The virtual address.
    /// @return The half word.
    inline hword read_hword(const word address)
    {
        return read<hword>(address);
    }

    /// @param address The virtual address.
    /// @return The word.
    inline word read_word(const word address)
    {
        return read<word>(address);
    }

    /// @param address The virtual address.
    /// @param data The byte to store.
    inline void write_byte(const word address, const byte data)
    {
        write<byte>(address, data);
    }

    /// @param address The virtual address.
    /// @param data The half word to store.
    inline void write_hword(const word address, const hword data)
    {
        write<hword>(address, data);
    }

    /// @param address The virtual address.
    /// @param data The word to store.
    inline void write_word(const word address, const word data)
    {
        write<word>(address, data);
    }

    /// Write bytes at a virtual address, a translation for each page and not for each byte.
    /// Meant for loading a program. The pages before one that cannot be written have been written.
    ///
    /// @param address The virtual address of the first byte.
    /// @param data The bytes to store.
    /// @param size The number of bytes.
    /// @throws VirtualMemory::PageFault if the access is not allowed.
    /// @throws SystemBus::BusError if no memory is at the physical address.
    void write_block(word address, const byte *data, word size);

    /// Fetch an instruction. The address must be word aligned, which the caller checks.
    ///
    /// @param address The virtual address of the instruction.
    /// @return The instruction.
    /// @throws VirtualMemory::PageFault if the address is unmapped or not executable.
    /// @throws SystemBus::BusError if the instruction is not in RAM or ROM.
    inline word fetch_instruction(const word address)
    {
        const word real_addr = m_mmu.translate_fetch(address);
        if (LIKELY(m_ram->in_bounds(real_addr))) return m_ram->read_word_aligned(real_addr);
        return fetch_outside_ram(real_addr, address);
    }

  private:
    /// Reads a value that crosses a page, put together byte by byte.
    ///
    /// @param address The virtual address of the first byte.
    /// @param n_bytes The size of the value.
    /// @return The value.
    dword read_val(word address, U8 n_bytes);

    /// Writes a value that crosses a page. The pages are translated one by one, and the stores
    /// are made only after all of them were.
    ///
    /// @param address The virtual address of the first byte.
    /// @param val The value to store.
    /// @param n_bytes The size of the value.
    void write_val(word address, dword val, U8 n_bytes);

    /// What is left of a fetch that is not in RAM: the ROM, or an exception.
    ///
    /// @param real_addr The physical address of the instruction.
    /// @param address The virtual address of the instruction.
    /// @return The instruction.
    word fetch_outside_ram(word real_addr, word address);

    SystemBus &m_bus;
    VirtualMemory &m_mmu;

    /// The RAM and ROM of the bus, which the accesses that matter go to without asking the bus.
    RAM *const m_ram;
    ROM *const m_rom;
};
