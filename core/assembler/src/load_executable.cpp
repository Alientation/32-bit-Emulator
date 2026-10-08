#include "assembler/load_executable.h"
#include "assembler/object_file.h"

#include "util/logger.h"

#include <map>
#include <span>

namespace
{

/// Where a section is loaded. Sections that are not placed at a physical address live in virtual
/// memory, which needs the pages to be mapped before they are written.
struct Placement
{
    word address;
    bool physical;
};

Placement placement_of(ObjectFile &obj, const std::string &section)
{
    const ObjectFile::SectionHeader &header = obj.sections[obj.section_table.at(section)];
    return {.address = header.address, .physical = header.load_at_physical_address};
}

/// What the pages that hold the sections can be used for once the program runs: write, execute.
using PagePermissions = std::map<word, std::pair<bool, bool>>;

/// Adds the pages of a section to the process. A page can hold more than one section (by default
/// .bss directly follows .data), and then has the permissions of both. The pages can be written
/// while the program is loaded, `permissions` is what they are afterwards.
void map_pages(VirtualMemory &mmu, word start_addr, word size, bool write, bool execute,
               PagePermissions &permissions)
{
    const long long pid = mmu.current_process();
    const word last_vpage = (start_addr + size - 1) >> kNumPageOffsetBits;
    for (word vpage = start_addr >> kNumPageOffsetBits;; vpage++)
    {
        if (!mmu.has_vpage(pid, vpage))
        {
            mmu.add_vpage(pid, vpage, 1, true, false);
        }
        permissions[vpage].first |= write;
        permissions[vpage].second |= execute;

        // Not `vpage <= last_vpage` in the loop condition, the last page can be the last one there is.
        if (vpage == last_vpage)
        {
            break;
        }
    }
}

/// Copies `bytes` to the section's address.
void copy_section(Emulator32bit &emu, Placement where, std::span<const byte> bytes, bool write,
                  bool execute, PagePermissions &permissions)
{
    if (bytes.empty())
    {
        return;
    }

    SystemBus &bus = *emu.system_bus;
    if (!where.physical)
    {
        map_pages(*bus.mmu, where.address, bytes.size(), write, execute, permissions);
    }

    for (size_t i = 0; i < bytes.size(); i++)
    {
        if (where.physical)
        {
            bus.write_unmapped_byte(where.address + i, bytes[i]);
        }
        else
        {
            bus.write_byte(where.address + i, bytes[i]);
        }
    }
}

} // namespace

LoadExecutable::LoadExecutable(Emulator32bit &emu, File exe_file) :
    m_emu(emu),
    m_exe_file(exe_file)
{
}

void LoadExecutable::load()
{
    ObjectFile obj(m_exe_file);

    AEMU_CHECK(obj.rel_text.empty() && obj.rel_data.empty() && obj.rel_bss.empty(),
               "LoadExecutable::load() - '{}' still has relocations. It was not produced by the "
               "linker.",
               m_exe_file.get_path());

    /* .text is stored as words, the others as bytes. Words are little endian in memory. */
    std::vector<byte> text;
    text.reserve(obj.text_section.size() * 4);
    for (const word instr : obj.text_section)
    {
        for (int shift = 0; shift < 32; shift += 8)
        {
            text.push_back(byte(instr >> shift));
        }
    }

    PagePermissions permissions;
    copy_section(m_emu, placement_of(obj, ".text"), text, false, true, permissions);
    copy_section(m_emu, placement_of(obj, ".data"), obj.data_section, true, false, permissions);
    copy_section(m_emu, placement_of(obj, ".bss"), std::vector<byte>(obj.bss_section, 0), true,
                 false, permissions);

    // The program runs with the permissions of its sections, code cannot be written.
    VirtualMemory &mmu = *m_emu.system_bus->mmu;
    for (const auto &[vpage, access] : permissions)
    {
        mmu.set_vpage_permissions(mmu.current_process(), vpage, vpage, access.first, access.second);
    }

    /* start program at _start label */
    const auto start = obj.string_table.find("_start");
    AEMU_CHECK(start != obj.string_table.end()
                   && obj.symbol_table.at(start->second).section != U32(-1),
               "LoadExecutable::load() - Missing required _start entry point of program.");

    // The PC is a virtual address, instruction fetch translates it.
    const word entry_point = obj.symbol_table.at(start->second).symbol_value;
    m_emu.set_pc(entry_point);

    AEMU_INFO("Starting emulator at entry point _start at virtual address {:x}", entry_point);
}
