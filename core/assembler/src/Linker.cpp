#include "assembler/linker.h"
#include "util/loggerv2.h"

Linker::Linker(std::vector<ObjectFile> obj_files, File exe_file) : m_obj_files(obj_files), m_exe_file(exe_file)
{
	link();
}



void Linker::link()
{
	ObjectFile exe_obj_file;

	exe_obj_file.file_type = EXECUTABLE_FILE_TYPE;
	exe_obj_file.target_machine = EMU_32BIT_MACHINE_ID;
	exe_obj_file.flags = 0;

	exe_obj_file.add_section(".text", (ObjectFile::SectionHeader) {
		.section_name = 0,
		.type = ObjectFile::SectionHeader::Type::TEXT,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 4,
	});

	exe_obj_file.add_section(".data", (ObjectFile::SectionHeader) {
		.section_name = 0,
		.type = ObjectFile::SectionHeader::Type::DATA,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 0,
	});

	exe_obj_file.add_section(".bss", (ObjectFile::SectionHeader) {
		.section_name = 0,
		.type = ObjectFile::SectionHeader::Type::BSS,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 0,
	});

	exe_obj_file.add_section(".symtab", (ObjectFile::SectionHeader) {
		.section_name = 0,
		.type = ObjectFile::SectionHeader::Type::SYMTAB,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 26,
	});

	exe_obj_file.add_section(".rel.text", (ObjectFile::SectionHeader) {
		.section_name = 0,
		.type = ObjectFile::SectionHeader::Type::REL_TEXT,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 28,
	});

	exe_obj_file.add_section(".rel.data", (ObjectFile::SectionHeader) {
		.section_name = 0,
		.type = ObjectFile::SectionHeader::Type::REL_DATA,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 28,
	});

	exe_obj_file.add_section(".rel.bss", (ObjectFile::SectionHeader) {
		.section_name = 0,
		.type = ObjectFile::SectionHeader::Type::REL_BSS,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 28,
	});

	exe_obj_file.add_section(".strtab", (ObjectFile::SectionHeader) {
		.section_name = 0,
		.type = ObjectFile::SectionHeader::Type::STRTAB,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 0,
	});

	/* Add all .text section together. order of obj files in list is the order they will be in memory */
	for (ObjectFile& obj_file : m_obj_files) {
		exe_obj_file.text_section.insert(exe_obj_file.text_section.end(), obj_file.text_section.begin(), obj_file.text_section.end());
	}

	/* .data section */
	for (ObjectFile& obj_file : m_obj_files) {
		exe_obj_file.data_section.insert(exe_obj_file.data_section.end(), obj_file.data_section.begin(), obj_file.data_section.end());
	}

	/* .bss section */
	for (ObjectFile& obj_file : m_obj_files) {
		exe_obj_file.bss_section += obj_file.bss_section;
	}

	/* .symtab section */
	word offset_text = 0;												/* bytes from the start of exe file text section */
	word offset_data = exe_obj_file.text_section.size() * 4;			/* Same as above */
	word offset_bss = offset_data + exe_obj_file.bss_section;			/* Same as above, sections are ordered like this. text->data->bss */
	for (int i = 0; i < m_obj_files.size(); i++) {
		ObjectFile& obj_file = m_obj_files.at(i);
		for (auto& pair : obj_file.symbol_table) {
			std::string symbol_name = obj_file.strings[pair.first];

			if (pair.second.binding_info == ObjectFile::SymbolTableEntry::BindingInfo::LOCAL) {
				symbol_name += ":LOCAL:" + std::to_string(i);
			}

			word val = pair.second.symbol_value;
			if (pair.second.section == obj_file.section_table.at(".text")) {
				val += offset_text;
			} else if (pair.second.section == obj_file.section_table.at(".data")) {
				val += offset_data;
			} else if (pair.second.section == obj_file.section_table.at(".bss")) {
				val += offset_bss;
			}

			exe_obj_file.add_symbol(symbol_name, val, pair.second.binding_info, pair.second.section);
			/* Updated current obj file symbol table (pair is passed as reference), this will be used to assist with
				relocation by mapping this symbol to the corresponding symbol in the exe file */
			pair.second.symbol_name = exe_obj_file.string_table.at(symbol_name);
			pair.second.symbol_value = val;
			pair.second.binding_info = exe_obj_file.symbol_table[pair.second.symbol_name].binding_info;
		}

		offset_text += obj_file.text_section.size() * 4;
		offset_data += obj_file.data_section.size();
		offset_bss += obj_file.bss_section;
	}

	/* .rel.text section */
	offset_text = 0;
	for (ObjectFile& obj_file : m_obj_files) {
		for (ObjectFile::RelocationEntry& rel : obj_file.rel_text) {
			/* exe file's symbol table contains the most recent updated version of the symbol across all obj files.
				Since all obj file symbols have been converted to point towards the exe file symbol table, we have to find the symbol located
				in this obj file which the symbol name will be the index into the combined string table. */
			ObjectFile::SymbolTableEntry symbol_entry = exe_obj_file.symbol_table.at(obj_file.symbol_table.at(rel.symbol).symbol_name);

			/* all symbols should have a corresponding definition */
			if (symbol_entry.binding_info == ObjectFile::SymbolTableEntry::BindingInfo::WEAK) {
				ERROR_SS(std::stringstream() << "Linker::link() - Error, symbol definition is not found.");
				continue;
			}

			word instr_i = (offset_text + rel.offset)/4;
			switch (rel.type) {											/* Only fill in relocations that are relative offsets since we do not know where the exe file will be in memory */
				case ObjectFile::RelocationEntry::Type::R_EMU32_O_LO12:
					// exe_obj_file.text_section[instr_i] = mask_0(obj_file.text_section[rel.offset/4], 0, 14) + bitfield_u32(symbol_entry.symbol_value, 0, 12);
				case ObjectFile::RelocationEntry::Type::R_EMU32_ADRP_HI20:
					// exe_obj_file.text_section[instr_i] = mask_0(obj_file.text_section[rel.offset/4], 0, 20) + bitfield_u32(symbol_entry.symbol_value, 12, 20);
				case ObjectFile::RelocationEntry::Type::R_EMU32_MOV_LO19:
					// exe_obj_file.text_section[instr_i] = mask_0(obj_file.text_section[rel.offset/4], 0, 19) + bitfield_u32(symbol_entry.symbol_value, 0, 19);
				case ObjectFile::RelocationEntry::Type::R_EMU32_MOV_HI13:
					// exe_obj_file.text_section[instr_i] = mask_0(obj_file.text_section[rel.offset/4], 0, 19) + bitfield_u32(symbol_entry.symbol_value, 19, 13);
					break;
				case ObjectFile::RelocationEntry::Type::R_EMU32_B_OFFSET22:
					EXPECT_TRUE_SS((symbol_entry.symbol_value & 0b11) == 0, std::stringstream()
							<< "Assembler::fill_local() - Expected relocation value for R_EMU32_B_OFFSET22 to be 4 byte aligned. Got "
							<< symbol_entry.symbol_value);
					exe_obj_file.text_section[instr_i] = mask_0(obj_file.text_section[rel.offset/4], 0, 22) + bitfield_u32(bitfield_s32(symbol_entry.symbol_value, 2, 22) - instr_i, 0, 22);
					continue;
				case ObjectFile::RelocationEntry::Type::UNDEFINED:
				default:
					ERROR_SS(std::stringstream() << "Assembler::fill_local() - Unknown relocation entry type.");
			}

			/* relocation is not a relative offset, add to exe file relocation to be resolved when the exe file is loaded into memory */
			exe_obj_file.rel_text.push_back((ObjectFile::RelocationEntry) {
				.offset = rel.offset + offset_text,
				.symbol = obj_file.symbol_table.at(rel.symbol).symbol_name,
				.type = rel.type,
				.shift = rel.shift,
			});
		}

		offset_text += 4 * obj_file.text_section.size();
	}

	// offset_data = 0;
	// offset_bss = 0;
	/* .rel.data section */
	/* .rel.bss section */

	// offset_data += obj_file.data_section.size();
	// offset_bss += obj_file.bss_section;


	exe_obj_file.write_object_file(m_exe_file);
}