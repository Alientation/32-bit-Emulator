#include "assembler/LoadExecutable.h"
#include "assembler/ObjectFile.h"
#include "util/Logger.h"

LoadExecutable::LoadExecutable(Emulator32bit& emu, File exe_file) : m_emu(emu), m_exe_file(exe_file) {
	load();
}


void LoadExecutable::load(word start_addr) {											/* For now load starting at address 0 */
	ObjectFile obj(m_exe_file);

	for (ObjectFile::RelocationEntry& rel : obj.rel_text) {
		ObjectFile::SymbolTableEntry symbol_entry = obj.symbol_table.at(rel.symbol);

		/* all symbols should have a corresponding definition */
		if (symbol_entry.binding_info == ObjectFile::SymbolTableEntry::BindingInfo::WEAK) {
			lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Linker::link() - Error, symbol definition is not found.");
			continue;
		}

		word instr_i = rel.offset/4;
		word new_abs_value = symbol_entry.symbol_value + start_addr;
		switch (rel.type) {
			case ObjectFile::RelocationEntry::Type::R_EMU32_O_LO12:
				obj.text_section[instr_i] = mask_0(obj.text_section[rel.offset/4], 0, 14) + bitfield_u32(new_abs_value, 0, 12);
				break;
			case ObjectFile::RelocationEntry::Type::R_EMU32_ADRP_HI20:
				obj.text_section[instr_i] = mask_0(obj.text_section[rel.offset/4], 0, 20) + bitfield_u32(new_abs_value, 12, 20);
				break;
			case ObjectFile::RelocationEntry::Type::R_EMU32_MOV_LO19:
				obj.text_section[instr_i] = mask_0(obj.text_section[rel.offset/4], 0, 19) + bitfield_u32(new_abs_value, 0, 19);
				break;
			case ObjectFile::RelocationEntry::Type::R_EMU32_MOV_HI13:
				obj.text_section[instr_i] = mask_0(obj.text_section[rel.offset/4], 0, 19) + bitfield_u32(new_abs_value, 19, 13);
				break;
			case ObjectFile::RelocationEntry::Type::UNDEFINED:
			default:
				lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::fill_local() - Unknown relocation entry type.");
		}
	}

	// text -> data -> bss
	word cur_addr = start_addr;
	for (word instr : obj.text_section) {
		m_emu.system_bus.write_word(cur_addr, instr);
		cur_addr += 4;
	}

	for (byte data : obj.data_section) {
		m_emu.system_bus.write_byte(cur_addr, data);
		cur_addr++;
	}

	for (int i = 0; i < obj.bss_section; i++) {
		m_emu.system_bus.write_byte(cur_addr, 0);
		cur_addr++;
	}
};