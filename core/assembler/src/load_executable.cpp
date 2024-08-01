#include "assembler/load_executable.h"
#include "assembler/object_file.h"
#include "util/loggerv2.h"

LoadExecutable::LoadExecutable(Emulator32bit& emu, File exe_file) : m_emu(emu), m_exe_file(exe_file)
{
	load();
}

void LoadExecutable::load(word start_addr)
{											/* For now load starting at address 0 */
	ObjectFile obj(m_exe_file);

	for (ObjectFile::RelocationEntry& rel : obj.rel_text) {
		ObjectFile::SymbolTableEntry symbol_entry = obj.symbol_table.at(rel.symbol);

		/* all symbols should have a corresponding definition */
		if (symbol_entry.binding_info == ObjectFile::SymbolTableEntry::BindingInfo::WEAK) {
			ERROR_SS(std::stringstream() << "Linker::link() - Undefined symbol " << obj.strings.at(symbol_entry.symbol_name));
			continue;
		}

		word instr_i = rel.offset/4;
		word new_abs_value = symbol_entry.symbol_value + start_addr;
		switch (rel.type) {
			case ObjectFile::RelocationEntry::Type::R_EMU32_O_LO12:
				obj.text_section.at(instr_i) = mask_0(obj.text_section.at(rel.offset/4), 0, 14) + bitfield_u32(new_abs_value, 0, 12);
				break;
			case ObjectFile::RelocationEntry::Type::R_EMU32_ADRP_HI20:
				obj.text_section.at(instr_i) = mask_0(obj.text_section.at(rel.offset/4), 0, 20) + bitfield_u32(new_abs_value, 12, 20);
				break;
			case ObjectFile::RelocationEntry::Type::R_EMU32_MOV_LO19:
				obj.text_section.at(instr_i) = mask_0(obj.text_section.at(rel.offset/4), 0, 19) + bitfield_u32(new_abs_value, 0, 19);
				break;
			case ObjectFile::RelocationEntry::Type::R_EMU32_MOV_HI13:
				obj.text_section.at(instr_i) = mask_0(obj.text_section.at(rel.offset/4), 0, 19) + bitfield_u32(new_abs_value, 19, 13);
				break;
			case ObjectFile::RelocationEntry::Type::UNDEFINED:
			default:
				ERROR_SS(std::stringstream() << "Assembler::fill_local() - Unknown relocation entry type (" << std::to_string((int)rel.type) << ")");
		}
	}

	// text -> data -> bss
	word cur_addr = start_addr;
	for (word instr : obj.text_section) {
		SystemBus::Exception sys_bus_exception;
		Memory::WriteException mem_write_exception;
		m_emu.system_bus.write_word(cur_addr, instr, sys_bus_exception, mem_write_exception);
		cur_addr += 4;
	}

	for (byte data : obj.data_section) {
		SystemBus::Exception sys_bus_exception;
		Memory::WriteException mem_write_exception;
		m_emu.system_bus.write_byte(cur_addr, data, sys_bus_exception, mem_write_exception);
		cur_addr++;
	}

	for (word i = 0; i < obj.bss_section; i++) {
		SystemBus::Exception sys_bus_exception;
		Memory::WriteException mem_write_exception;
		m_emu.system_bus.write_byte(cur_addr, 0, sys_bus_exception, mem_write_exception);
		cur_addr++;
	}

	/* start program at _start label */
	if (obj.string_table.find("_start") == obj.string_table.end()) {
		ERROR("LoadExecutable::load() - Missing required _start entry point of program.");
	}

	VirtualMemory::Exception vm_exception;
	m_emu._pc = m_emu.mmu->map_address(obj.symbol_table.at(obj.string_table.at("_start")).symbol_value + start_addr, vm_exception);
};