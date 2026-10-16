#include "assembler/load_executable.h"

#include "assembler/object_file.h"
#include "util/logger.h"

#include <iterator>
#include <map>
#include <span>

/// Where a section is loaded. Sections that are not placed at a physical address live in virtual
/// memory, which needs the pages to be mapped before they are written.
struct Placement
{
    std::string name;
    word address;
    bool physical;
};

static Placement placement_of(const ObjectFile &obj, const std::string &section)
{
    const ObjectFile::SectionHeader &header = obj.sections[obj.section_table.at(section)];
    return {
        .name = section, .address = header.address, .physical = header.load_at_physical_address};
}

/// Fatal if the section is not in memory that the program can be loaded into: at virtual addresses
/// it has to end below the devices (kDeviceBase, which are not translated), at physical addresses
/// every page has to be RAM, ROM or disk. The file says where a section goes, so neither is a given.
static void check_placement(const SystemBus &bus, const Placement &where, const word size)
{
    const U64 end = U64(where.address) + size;
    if (!where.physical)
    {
        AEMU_CHECK(end <= kDeviceBase,
                   "The section {} at {:#x} with {} bytes reaches the devices at {:#x}.",
                   where.name, where.address, size, kDeviceBase);
        return;
    }

    // The pages below are counted up to the last one, which must not wrap. On a machine without
    // memory in the top page the loop would stop at a page without memory anyway, which is why the
    // fuzz corpus cannot show that this check is needed.
    AEMU_CHECK(end <= U64(1) << kNumWordBits,
               "The section {} at {:#x} with {} bytes goes past the end of the address space.",
               where.name, where.address, size);
    const word last_page = (where.address + size - 1) >> kNumPageOffsetBits;
    for (word page = where.address >> kNumPageOffsetBits;; page++)
    {
        AEMU_CHECK(bus.find_storage(page << kNumPageOffsetBits) != nullptr,
                   "The section {} is placed at the physical address {:#x}, which has no memory.",
                   where.name, page << kNumPageOffsetBits);
        if (page == last_page)
        {
            break;
        }
    }
}

/// What the pages that hold the sections can be used for once the program runs: write, execute.
using PagePermissions = std::map<word, std::pair<bool, bool>>;

/// Adds the pages of a section to the process. A page can hold more than one section (by default
/// .bss directly follows .data), and then has the permissions of both. The pages can be written
/// while the program is loaded, `permissions` is what they are afterwards.
static void map_pages(PagePermissions &permissions, VirtualMemory &mmu, const word start_addr,
                      const word size, const bool write, const bool execute)
{
    const S64 pid = mmu.current_process();
    const word last_vpage = (start_addr + size - 1) >> kNumPageOffsetBits;
    for (word vpage = start_addr >> kNumPageOffsetBits;; vpage++)
    {
        if (!mmu.has_vpage(pid, vpage))
        {
            mmu.add_vpage(pid, vpage, 1, true, false);
        }
        permissions[vpage].first |= write;
        permissions[vpage].second |= execute;

        // Not `vpage <= last_vpage` in the loop condition, the last page can be the last one there
        // is.
        if (vpage == last_vpage)
        {
            break;
        }
    }
}

/// Maps the pages of a section at a physical address to themselves, so that the program can reach
/// them. Fatal if a section at a virtual address was loaded into one of the pages already.
static void map_physical_pages(VirtualMemory &mmu, const Placement &where, const word size)
{
    const S64 pid = mmu.current_process();
    const word last_page = (where.address + size - 1) >> kNumPageOffsetBits;
    for (word page = where.address >> kNumPageOffsetBits;; page++)
    {
        try
        {
            mmu.ensure_physical_page_mapping(pid, page, page);
        }
        catch (const VirtualMemory::VPageAlreadyMapped &)
        {
            AEMU_FATAL("The section {} at the physical address {:#x} shares the page {:#x} with a "
                       "section at a virtual address.",
                       where.name, where.address, page << kNumPageOffsetBits);
        }

        // Not `page <= last_page` in the loop condition, the last page can be the last one.
        if (page == last_page)
        {
            break;
        }
    }
}

/// Copies `bytes` to the section's address.
static void copy_section(PagePermissions &permissions, Emulator32bit &emu, const Placement &where,
                         const std::span<const byte> bytes, const bool write, const bool execute)
{
    if (bytes.empty())
    {
        return;
    }

    const word size = bytes.size();
    check_placement(*emu.system_bus, where, size);
    if (where.physical)
    {
        map_physical_pages(*emu.mmu, where, size);
        emu.system_bus->write_block(where.address, bytes.data(), size);
        return;
    }

    map_pages(permissions, *emu.mmu, where.address, size, write, execute);
    emu.memory.write_block(where.address, bytes.data(), size);
}

/// A page of zeros, written over physical memory that has to be zeros.
static constexpr byte kZeroPage[kPageSize] = {};

/// Gives a section of zeros (.bss, a nobits section) its memory. At virtual addresses the pages are
/// only mapped, so a large section costs nothing until the program touches it: a new page is zeros
/// (Disk::get_free_page), and so is a page that a section at a physical address has, since
/// mapping it reads a new disk page into it before that section's bytes are written. Physical
/// memory is written a page at a time.
static void zero_section(PagePermissions &permissions, Emulator32bit &emu, const Placement &where,
                         const word size, const bool write, const bool execute)
{
    if (size == 0)
    {
        return;
    }

    check_placement(*emu.system_bus, where, size);
    if (!where.physical)
    {
        map_pages(permissions, *emu.mmu, where.address, size, write, execute);
        return;
    }

    map_physical_pages(*emu.mmu, where, size);
    for (word done = 0; done < size;)
    {
        const word left = size - done;
        const word chunk = left < kPageSize ? left : kPageSize;
        emu.system_bus->write_block(where.address + done, kZeroPage, chunk);
        done += chunk;
    }
}

LoadExecutable::LoadExecutable(Emulator32bit &emu, const File &exe_file) :
    m_emu(emu),
    m_exe_file(exe_file)
{
}

LoadExecutable::LoadExecutable(Emulator32bit &emu, std::vector<byte> exe_bytes) :
    m_emu(emu),
    m_exe_bytes(std::move(exe_bytes)),
    m_in_memory(true)
{
}

void LoadExecutable::load()
{
    ObjectFile obj;
    if (m_in_memory)
    {
        obj.read_object_file(m_exe_bytes);
    }
    else
    {
        obj.read_object_file(m_exe_file);
    }

    bool has_relocations = !obj.rel_text.empty() || !obj.rel_bss.empty();
    for (const ObjectFile::ByteSection &section : ObjectFile::byte_sections())
    {
        has_relocations |= !(obj.*section.relocations).empty();
    }
    for (const ObjectFile::UserSection &section : obj.user_sections)
    {
        has_relocations |= !section.relocations.empty();
    }
    AEMU_CHECK(!has_relocations,
               "'{}' still has relocations. It was not produced by the "
               "linker.",
               m_exe_file.get_path());

    // .text is stored as words, the others as bytes. Words are little endian in memory.
    std::vector<byte> text;
    text.reserve(obj.text_section.size() * kInstructionSize);
    for (const word instr : obj.text_section)
    {
        for (S32 shift = 0; shift < kNumWordBits; shift += kNumByteBits)
        {
            text.push_back(byte(instr >> shift));
        }
    }

    PagePermissions permissions;
    copy_section(permissions, m_emu, placement_of(obj, ".text"), text, false, true);
    for (const ObjectFile::ByteSection &section : ObjectFile::byte_sections())
    {
        copy_section(permissions, m_emu, placement_of(obj, section.name), obj.*section.bytes,
                     section.writable, false);
    }
    for (const ObjectFile::UserSection &section : obj.user_sections)
    {
        // A nobits section has no bytes in the file, only a size (zeros, like .bss).
        if (section.nobits)
        {
            zero_section(permissions, m_emu, placement_of(obj, section.name), section.zero_size,
                         section.writable, section.executable);
        }
        else
        {
            copy_section(permissions, m_emu, placement_of(obj, section.name), section.bytes,
                         section.writable, section.executable);
        }
    }
    zero_section(permissions, m_emu, placement_of(obj, ".bss"), obj.bss_section, true, false);

    // The program runs with the permissions of its sections, so code cannot be written. One call
    // for each run of pages with the same permissions: a call flushes the TLB, and a large .bss
    // has a million pages.
    VirtualMemory &mmu = *m_emu.mmu;
    PagePermissions::const_iterator run = permissions.begin();
    while (run != permissions.end())
    {
        PagePermissions::const_iterator next = std::next(run);
        word last_vpage = run->first;
        while (next != permissions.end() && next->first == last_vpage + 1
               && next->second == run->second)
        {
            last_vpage = next->first;
            next++;
        }
        mmu.set_vpage_permissions(mmu.current_process(), run->first, last_vpage, run->second.first,
                                  run->second.second);
        run = next;
    }

    // Start the program at the _start label. The string can be in the file without the symbol.
    const std::unordered_map<std::string, U32>::const_iterator start =
        obj.string_table.find("_start");
    const std::map<U32, ObjectFile::SymbolTableEntry>::const_iterator symbol =
        start == obj.string_table.end() ? obj.symbol_table.cend()
                                        : obj.symbol_table.find(start->second);
    AEMU_CHECK(symbol != obj.symbol_table.end() && symbol->second.section != U32(-1),
               "Missing required _start entry point of program.");

    // The PC is a virtual address, instruction fetch translates it.
    const word entry_point = symbol->second.symbol_value;
    m_emu.set_pc(entry_point);

    AEMU_INFO("Starting emulator at entry point _start at virtual address {:x}", entry_point);
}
