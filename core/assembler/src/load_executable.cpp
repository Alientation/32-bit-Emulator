#include "assembler/load_executable.h"
#include "assembler/object_file.h"

#include "util/logger.h"

LoadExecutable::LoadExecutable (Emulator32bit &emu, File exe_file) :
    m_emu (emu),
    m_exe_file (exe_file)
{
    load ();
}

void LoadExecutable::load ()
{ /* For now load starting at address 0 */
    ObjectFile obj (m_exe_file);

    for (ObjectFile::RelocationEntry &rel : obj.rel_text)
    {
        ObjectFile::SymbolTableEntry symbol_entry = obj.symbol_table.at (rel.symbol);

        /* all symbols should have a corresponding definition */
        if (symbol_entry.binding_info == ObjectFile::SymbolTableEntry::BindingInfo::WEAK)
        {
            ERROR ("Linker::link() - Undefined symbol %s",
                   obj.strings.at (symbol_entry.symbol_name).c_str ());
            continue;
        }

        word instr_i = rel.offset / 4;
        word new_abs_value = symbol_entry.symbol_value;
        switch (rel.type)
        {
        case ObjectFile::RelocationEntry::Type::R_EMU32_O_LO12:
            obj.text_section.at (instr_i) = mask_0 (obj.text_section.at (instr_i), 0, 14)
                                            + bitfield_unsigned (new_abs_value, 0, 12);
            break;
        case ObjectFile::RelocationEntry::Type::R_EMU32_ADRP_HI20:
        {
            word start_address = obj.sections[obj.section_table.at (".text")].address;
            int offset = int (new_abs_value >> 12) - ((start_address + rel.offset) >> 12);
            word instr =
                mask_0 (obj.text_section.at (instr_i), 0, 20) + bitfield_unsigned (offset, 0, 20);
            if ((offset >> 20) & 1)
            {
                instr = set_bit (instr, kInstructionUpdateFlagBit, 1);
            }

            obj.text_section.at (instr_i) = instr;
            break;
        }
        case ObjectFile::RelocationEntry::Type::R_EMU32_MOV_LO19:
            obj.text_section.at (instr_i) = mask_0 (obj.text_section.at (instr_i), 0, 19)
                                            + bitfield_unsigned (new_abs_value, 0, 19);
            break;
        case ObjectFile::RelocationEntry::Type::R_EMU32_MOV_HI13:
            obj.text_section.at (instr_i) = mask_0 (obj.text_section.at (instr_i), 0, 19)
                                            + bitfield_unsigned (new_abs_value, 19, 13);
            break;
        case ObjectFile::RelocationEntry::Type::UNDEFINED:
        default:
            ERROR ("Assembler::fill_local() - Unknown relocation entry type (%d)", int (rel.type));
        }
    }

    // text -> data -> bss
    word cur_addr = obj.sections[obj.section_table.at (".text")].address;
    bool physical = obj.sections[obj.section_table.at (".text")].load_at_physical_address;

    if (!physical && obj.text_section.size () > 0)
    {
        // we are assuming that there is no overlap between pages of text, data, and bss sections
        word start = cur_addr >> kNumPageOffsetBits;
        word end = (cur_addr + 4 * obj.text_section.size () - 1) >> kNumPageOffsetBits;
        m_emu.system_bus->mmu->add_vpage (m_emu.system_bus->mmu->current_process (), start,
                                          end - start + 1, false, true);
    }

    for (word instr : obj.text_section)
    {
        if (!physical)
        {
            m_emu.system_bus->write_word (cur_addr, instr);
        }
        else
        {
            m_emu.system_bus->write_unmapped_word (cur_addr, instr);
        }

        cur_addr += 4;
    }

    cur_addr = obj.sections[obj.section_table.at (".data")].address;
    physical = obj.sections[obj.section_table.at (".data")].load_at_physical_address;

    if (!physical && obj.data_section.size () > 0)
    {
        word start = cur_addr >> kNumPageOffsetBits;
        word end = (cur_addr + obj.data_section.size () - 1) >> kNumPageOffsetBits;
        m_emu.system_bus->mmu->add_vpage (m_emu.system_bus->mmu->current_process (), start,
                                          end - start + 1, true, false);
    }

    for (byte data : obj.data_section)
    {
        if (!physical)
        {
            m_emu.system_bus->write_byte (cur_addr, data);
        }
        else
        {
            m_emu.system_bus->write_unmapped_byte (cur_addr, data);
        }
        cur_addr++;
    }

    cur_addr = obj.sections[obj.section_table.at (".bss")].address;
    physical = obj.sections[obj.section_table.at (".bss")].load_at_physical_address;
    if (!physical && obj.bss_section > 0)
    {
        word start = cur_addr >> kNumPageOffsetBits;
        word end = (cur_addr + obj.bss_section - 1) >> kNumPageOffsetBits;
        m_emu.system_bus->mmu->add_vpage (m_emu.system_bus->mmu->current_process (), start,
                                          end - start + 1, true, false);
    }

    for (word i = 0; i < obj.bss_section; i++)
    {
        if (!physical)
        {
            m_emu.system_bus->write_byte (cur_addr, 0);
        }
        else
        {
            m_emu.system_bus->write_unmapped_byte (cur_addr, 0);
        }
        cur_addr++;
    }

    /* start program at _start label */
    if (obj.string_table.find ("_start") == obj.string_table.end ())
    {
        ERROR ("LoadExecutable::load() - Missing required _start entry point of program.");
    }

    VirtualMemory::Exception vm_exception;
    word entry_point = obj.symbol_table.at (obj.string_table.at ("_start")).symbol_value;
    m_emu.set_pc (m_emu.system_bus->mmu->translate_address (entry_point, vm_exception));

    INFO ("Starting emulator at entry point _start at virtual address %x mapped to physical "
          "address %x",
          entry_point, m_emu.get_pc ());
};