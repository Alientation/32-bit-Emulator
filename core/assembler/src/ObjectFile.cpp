#include "assembler/ObjectFile.h"
#include "assembler/Build.h"
#include "emulator32bit/Emulator32bit.h"
#include "util/Logger.h"
#include "util/Types.h"

#include <fstream>

ObjectFile::ObjectFile() {
	this->m_state = State::NO_STATE;

	/* construct disassembler instruction mapping */
	_disassembler_instructions[Emulator32bit::_op_hlt] = disassemble_hlt;
	_disassembler_instructions[Emulator32bit::_op_add] = disassemble_add;
	_disassembler_instructions[Emulator32bit::_op_sub] = disassemble_sub;
	_disassembler_instructions[Emulator32bit::_op_rsb] = disassemble_rsb;
	_disassembler_instructions[Emulator32bit::_op_adc] = disassemble_adc;
	_disassembler_instructions[Emulator32bit::_op_sbc] = disassemble_sbc;
	_disassembler_instructions[Emulator32bit::_op_rsc] = disassemble_rsc;
	_disassembler_instructions[Emulator32bit::_op_mul] = disassemble_mul;
	_disassembler_instructions[Emulator32bit::_op_umull] = disassemble_umull;
	_disassembler_instructions[Emulator32bit::_op_smull] = disassemble_smull;
	_disassembler_instructions[Emulator32bit::_op_vabs_f32] = disassemble_vabs_f32;
	_disassembler_instructions[Emulator32bit::_op_vneg_f32] = disassemble_vneg_f32;
	_disassembler_instructions[Emulator32bit::_op_vsqrt_f32] = disassemble_vsqrt_f32;
	_disassembler_instructions[Emulator32bit::_op_vadd_f32] = disassemble_vadd_f32;
	_disassembler_instructions[Emulator32bit::_op_vsub_f32] = disassemble_vsub_f32;
	_disassembler_instructions[Emulator32bit::_op_vdiv_f32] = disassemble_vdiv_f32;
	_disassembler_instructions[Emulator32bit::_op_vmul_f32] = disassemble_vmul_f32;
	_disassembler_instructions[Emulator32bit::_op_vcmp_f32] = disassemble_vcmp_f32;
	_disassembler_instructions[Emulator32bit::_op_vsel_f32] = disassemble_vsel_f32;
	// _disassembler_instructions[Emulator32bit::_op_vcint_u32_f32] = disassemble_vcint_u32_f32;	/* slight discrepency with the emulator. check later */
	// _disassembler_instructions[Emulator32bit::_op_vcint_s32_f32] = disassemble_vcint_s32_f32;
	// _disassembler_instructions[Emulator32bit::_op_vcflo_u32_f32] = disassemble_vcflo_u32_f32;
	// _disassembler_instructions[Emulator32bit::_op_vcflo_s32_f32] = disassemble_vcflo_s32_f32;
	_disassembler_instructions[Emulator32bit::_op_vmov_f32] = disassemble_vmov_f32;
	_disassembler_instructions[Emulator32bit::_op_and] = disassemble_and;
	_disassembler_instructions[Emulator32bit::_op_orr] = disassemble_orr;
	_disassembler_instructions[Emulator32bit::_op_eor] = disassemble_eor;
	_disassembler_instructions[Emulator32bit::_op_bic] = disassemble_bic;
	_disassembler_instructions[Emulator32bit::_op_lsl] = disassemble_lsl;
	_disassembler_instructions[Emulator32bit::_op_lsr] = disassemble_lsr;
	_disassembler_instructions[Emulator32bit::_op_asr] = disassemble_asr;
	_disassembler_instructions[Emulator32bit::_op_ror] = disassemble_ror;
	_disassembler_instructions[Emulator32bit::_op_cmp] = disassemble_cmp;
	_disassembler_instructions[Emulator32bit::_op_cmn] = disassemble_cmn;
	_disassembler_instructions[Emulator32bit::_op_tst] = disassemble_tst;
	_disassembler_instructions[Emulator32bit::_op_teq] = disassemble_teq;
	_disassembler_instructions[Emulator32bit::_op_mov] = disassemble_mov;
	_disassembler_instructions[Emulator32bit::_op_mvn] = disassemble_mvn;
	_disassembler_instructions[Emulator32bit::_op_ldr] = disassemble_ldr;
	_disassembler_instructions[Emulator32bit::_op_str] = disassemble_str;
	_disassembler_instructions[Emulator32bit::_op_swp] = disassemble_swp;
	_disassembler_instructions[Emulator32bit::_op_ldrb] = disassemble_ldrb;
	_disassembler_instructions[Emulator32bit::_op_strb] = disassemble_strb;
	_disassembler_instructions[Emulator32bit::_op_swpb] = disassemble_swpb;
	_disassembler_instructions[Emulator32bit::_op_ldrh] = disassemble_ldrh;
	_disassembler_instructions[Emulator32bit::_op_strh] = disassemble_strh;
	_disassembler_instructions[Emulator32bit::_op_swph] = disassemble_swph;
	_disassembler_instructions[Emulator32bit::_op_b] = disassemble_b;
	_disassembler_instructions[Emulator32bit::_op_bl] = disassemble_bl;
	_disassembler_instructions[Emulator32bit::_op_bx] = disassemble_bx;
	_disassembler_instructions[Emulator32bit::_op_blx] = disassemble_blx;
	_disassembler_instructions[Emulator32bit::_op_swi] = disassemble_swi;

	_disassembler_instructions[Emulator32bit::_op_adrp] = disassemble_adrp;
}

void ObjectFile::read_object_file(File obj_file) {
	m_obj_file = obj_file;
	disassemble();

	/* since errors in disassemble will early return before setting state to success, check for early return */
	if (m_state == State::DISASSEMBLING) {
		m_state = State::DISASSEMBLED_ERROR;
	}

	print();
}

void ObjectFile::disassemble() {
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Disassembling");
	m_state = State::DISASSEMBLING;
	FileReader file_reader(m_obj_file, std::ios::in | std::ios::binary);

	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Reading bytes");
	std::vector<byte> bytes;
	while (file_reader.hasNextByte()) {
		bytes.push_back(file_reader.readByte());
	}

	ByteReader reader(bytes);

	/* BELF Header */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Reading BELF Header");
	byte expected[4] = {'B','E','L','F'};							/* 0-3 */
	for (int i = 0; i < sizeof(expected)/sizeof(expected[0]); i++) {
		if (expected[i] != reader.read_byte()) {
			// todo logger
			return;
		}
	}
	reader.skip_bytes(12);											/* 4-15 */
	file_type = reader.read_hword();								/* 16-17 */
	target_machine = reader.read_hword();							/* 18-19 */
	flags = reader.read_hword();									/* 20-21 */
	n_sections = reader.read_hword();								/* 22-23 */

	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Belf Header = (filetype=" <<
		std::to_string(file_type) << ", target_machine=" << std::to_string(target_machine)
		<< ", flags=" << std::to_string(flags) << ", n_sections=" << n_sections << ")");

	/* Section headers */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Reading section headers");
	ByteReader section_headers_reader(bytes);
	ByteReader section_headers_start_reader(bytes);
	section_headers_start_reader.skip_bytes(bytes.size() - 8);
	dword section_header_start = section_headers_start_reader.read_dword();
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Section Header Start = " << std::to_string(section_header_start));
	section_headers_reader.skip_bytes(section_header_start);
	for (int i = 0; i < n_sections; i++) {
		SectionHeader section_header = {
			.section_name = (int) section_headers_reader.read_dword(),
			.type = (SectionHeader::Type) section_headers_reader.read_word(),
			.section_start = (word) section_headers_reader.read_dword(),
			.section_size = (word) section_headers_reader.read_dword(),
			.entry_size = (word) section_headers_reader.read_dword(),
		};

		sections.push_back(section_header);

		lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Reading section " << i
				<< " (name=" << std::to_string(section_header.section_name) << ", type=" << std::to_string((int)section_header.type)
				<< ", section_start=" << std::to_string(section_header.section_start) << ", section_size="
				<< std::to_string(section_header.section_size) << ", entry_size=" << std::to_string(section_header.entry_size) << ")");
	}

	/* Sections */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Reading " << n_sections << " sections");
	for (int section_i = 0; section_i < n_sections; section_i++) {
		SectionHeader &section_header = sections[section_i];
		switch(section_header.type) {
			case SectionHeader::Type::TEXT:
				lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Disassembling Text Section");
				for (int i = 0; i < section_header.section_size; i+=4) {
					text_section.push_back(reader.read_word(false));
				}
				break;
			case SectionHeader::Type::DATA:
				lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Disassembling Data Section");
				for (int i = 0; i < section_header.section_size; i++) {
					data_section.push_back(reader.read_byte());
				}
				break;
			case SectionHeader::Type::BSS:
				lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Disassembling BSS Section");
				bss_section = reader.read_dword();
				break;
			case SectionHeader::Type::SYMTAB:
				lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Disassembling Symbol Table Section");
				for (int i = 0; i < section_header.section_size; i += SYMBOL_TABLE_ENTRY_SIZE) {
					SymbolTableEntry symbol = {
						.symbol_name = (int) reader.read_dword(),
						.symbol_value = (word) reader.read_dword(),
						.binding_info = (SymbolTableEntry::BindingInfo) reader.read_hword(),
						.section = (int) reader.read_dword(),
					};

					symbol_table[symbol.symbol_name] = symbol;
					lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Symbol entry = (symbol_name="
							<< std::to_string(symbol.symbol_name) << ", symbol_value=" << std::to_string(symbol.symbol_value) << ", binding_info="
							<< std::to_string((int) symbol.binding_info) << ", section=" << std::to_string(symbol.section));
				}
				break;
			case SectionHeader::Type::REL_TEXT:
				lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Disassembling Rel.Text Section");
				for (int i = 0; i < section_header.section_size; i+=RELOCATION_ENTRY_SIZE) {
					RelocationEntry rel = {
						.offset = (word) reader.read_dword(),
						.symbol = (int) reader.read_dword(),
						.type = (RelocationEntry::Type) reader.read_word(),
						.shift = (word) reader.read_dword(),
					};
					rel_text.push_back(rel);
				}
				break;
			case SectionHeader::Type::REL_DATA:
				lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Disassembling Rel.Data Section");
				for (int i = 0; i < section_header.section_size; i+=RELOCATION_ENTRY_SIZE) {
					RelocationEntry rel = {
						.offset = (word) reader.read_dword(),
						.symbol = (int) reader.read_dword(),
						.type = (RelocationEntry::Type) reader.read_word(),
						.shift = (word) reader.read_dword(),
					};
					rel_data.push_back(rel);
				}
				break;
			case SectionHeader::Type::REL_BSS:
				lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Disassembling Rel.BSS Section");
				for (int i = 0; i < section_header.section_size; i+=RELOCATION_ENTRY_SIZE) {
					RelocationEntry rel = {
						.offset = (word) reader.read_dword(),
						.symbol = (int) reader.read_dword(),
						.type = (RelocationEntry::Type) reader.read_word(),
						.shift = (word) reader.read_dword(),
					};
					rel_bss.push_back(rel);
				}
				break;
			case SectionHeader::Type::STRTAB: {
				lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Disassembling String Table section");
				std::string current_string;
				for (int i = 0; i < section_header.section_size; i++) {
					byte b = reader.read_byte();
					if (b == '\0') {
						string_table[current_string] = strings.size();
						strings.push_back(current_string);
						current_string = "";
					} else {
						current_string += b;
					}
				}
				break;
			}
			default:
				lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "ObjectFile::disassemble() - Invalid Section Type");
				return;
		}
	}

	/* Fill in section table */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Filling in Section table");
	for (int i = 0; i < sections.size(); i++) {
		section_table[strings[sections[i].section_name]] = i;
	}

	m_state = State::DISASSEMBLED_SUCCESS;
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::disassemble() - Finished disassembling");
}

int ObjectFile::add_section(const std::string& section_name, SectionHeader header) {
	EXPECT_TRUE(section_table.find(section_name) == section_table.end(), lgr::Logger::LogType::ERROR, std::stringstream() << "ObjectFile::add_section() - Section name exists in section table");

	header.section_name = add_string(section_name);
	section_table[section_name] = sections.size();
	sections.push_back(header);
	n_sections++;
	return sections.size() - 1;
}

int ObjectFile::add_string(const std::string& string) {
	EXPECT_TRUE(string_table.find(string) == string_table.end(), lgr::Logger::LogType::ERROR, std::stringstream() << "ObjectFile::add_string() - String name exists in string table");

	string_table[string] = string_table.size();
	strings.push_back(string);
	return strings.size()-1;
}

/**
 * @brief 					Adds a symbol to the symbol table
 *
 * @param symbol 			symbol string
 * @param value 			value of the symbol if it is defined
 * @param binding_info 		visiblity of the symbol
 * @param section 			section it is defined in. -1 if not defined in a section
 */
void ObjectFile::add_symbol(const std::string& symbol, word value, SymbolTableEntry::BindingInfo binding_info, int section) {
	if (string_table.find(symbol) == string_table.end()) {				/*! If symbol does not exist yet, create it */
		string_table[symbol] = strings.size();
		strings.push_back(symbol);
		symbol_table[string_table[symbol]] = (SymbolTableEntry) {
			.symbol_name = string_table[symbol],
			.symbol_value = value,
			.binding_info = binding_info,
			.section = section,
		};
	} else {
		SymbolTableEntry &symbol_entry = symbol_table[string_table[symbol]];
		if (symbol_entry.section == -1 && section != -1) {
			symbol_entry.section = section;
			symbol_entry.symbol_value = value;
		} else if (symbol_entry.section != -1 && section != -1) {
			lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "ObjectFile::add_symbol() - Multiple definition of symbol "
					<< symbol << " at sections " << strings[sections[section].section_name] << " and "
					<< strings[sections[symbol_entry.section].section_name] << ".");
			return;
		}

		if (binding_info == SymbolTableEntry::BindingInfo::GLOBAL
				|| (binding_info == SymbolTableEntry::BindingInfo::LOCAL &&
				symbol_entry.binding_info == SymbolTableEntry::BindingInfo::WEAK)) {
			symbol_entry.binding_info = binding_info;
		}
	}
}

void ObjectFile::write_object_file(File obj_file) {
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::write_object_file() - Writing to object file.");
	m_state = State::WRITING;
	m_obj_file = obj_file;

	// clearing object file
	std::ofstream ofs;
	ofs.open(obj_file.getFilePath(), std::ofstream::out | std::ofstream::trunc);
	ofs.close();

	// create writer for object file
	FileWriter m_writer = FileWriter(obj_file, std::ios::in | std::ios::binary);

	ByteWriter byte_writer(m_writer);
	int current_byte = 0;

	/* BELF Header */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::write_objectFile() - Writing BELF header.");
	m_writer.write("BELF");												/*! BELF magic number header */
	byte_writer << ByteWriter::Data(0, 12);								/*! Unused padding */
	byte_writer << ByteWriter::Data(file_type, 2);						/*! Object file type */
	byte_writer << ByteWriter::Data(target_machine, 2);					/*! Target machine */
	byte_writer << ByteWriter::Data(0, 2);								/*! Flags */
	byte_writer << ByteWriter::Data(sections.size(), 2);				/*! Number of sections */
	current_byte += BELF_HEADER_SIZE;

	/* Text Section */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::write_object_file() - Writing .text section.");
	for (int i = 0; i < text_section.size(); i++) {
		byte_writer << ByteWriter::Data(text_section.at(i), 4, false);
	}
	sections[section_table[".text"]].section_size = text_section.size() * 4;
	sections[section_table[".text"]].section_start = current_byte;
	current_byte += text_section.size() * 4;

	/* Data Section */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::write_object_file() - Writing .data section.");
	for (int i = 0; i < data_section.size(); i++) {
		byte_writer << ByteWriter::Data(data_section.at(i), 1);
	}
	sections[section_table[".data"]].section_size = data_section.size();
	sections[section_table[".data"]].section_start = current_byte;
	current_byte += data_section.size();

	/* BSS Section */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::write_object_file() - Writing .bss section. Size " << bss_section << " bytes.");
	byte_writer << ByteWriter::Data(bss_section, BSS_SECTION_SIZE);
	sections[section_table[".bss"]].section_size = bss_section;
	sections[section_table[".bss"]].section_start = current_byte;
	current_byte += BSS_SECTION_SIZE;

	/* Symbol Table */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::write_object_file() - Writing .symtab section.");
	for (std::pair<int, SymbolTableEntry> symbol : symbol_table) {
		byte_writer << ByteWriter::Data(symbol.second.symbol_name, 8);
		byte_writer << ByteWriter::Data(symbol.second.symbol_value, 8);
		byte_writer << ByteWriter::Data((short) symbol.second.binding_info, 2);
		byte_writer << ByteWriter::Data(symbol.second.section, 8);

		lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::write_object_file() - symbol " <<
				strings[symbol.second.symbol_name] << " = " << std::to_string(symbol.second.symbol_value)
				<< " (" << std::to_string((int)symbol.second.binding_info) << ")[" << std::to_string(symbol.second.section) << "]");
	}
	sections[section_table[".symtab"]].section_size = symbol_table.size() * SYMBOL_TABLE_ENTRY_SIZE;
	sections[section_table[".symtab"]].section_start = current_byte;
	current_byte += symbol_table.size() * SYMBOL_TABLE_ENTRY_SIZE;

	/* rel.text Section */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::write_object_file() - Writing .rel.text section.");
	for (int i = 0; i < rel_text.size(); i++) {
		byte_writer << ByteWriter::Data(rel_text[i].offset, 8);
		byte_writer << ByteWriter::Data(rel_text[i].symbol, 8);
		byte_writer << ByteWriter::Data((int) rel_text[i].type, 4);
		byte_writer << ByteWriter::Data(rel_text[i].shift, 8);
	}
	sections[section_table[".rel.text"]].section_size = rel_text.size() * RELOCATION_ENTRY_SIZE;
	sections[section_table[".rel.text"]].section_start = current_byte;
	current_byte += rel_text.size() * RELOCATION_ENTRY_SIZE;

	/* rel.data Section */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::write_object_file() - Writing .rel.data section.");
	for (int i = 0; i < rel_data.size(); i++) {
		byte_writer << ByteWriter::Data(rel_data[i].offset, 8);
		byte_writer << ByteWriter::Data(rel_data[i].symbol, 8);
		byte_writer << ByteWriter::Data((int) rel_data[i].type, 4);
		byte_writer << ByteWriter::Data(rel_data[i].shift, 8);
	}
	sections[section_table[".rel.data"]].section_size = rel_data.size() * RELOCATION_ENTRY_SIZE;
	sections[section_table[".rel.data"]].section_start = current_byte;
	current_byte += rel_data.size() * RELOCATION_ENTRY_SIZE;

	/* rel.bss Section */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::write_object_file() - Writing .rel.bss section.");
	for (int i = 0; i < rel_bss.size(); i++) {
		byte_writer << ByteWriter::Data(rel_bss[i].offset, 8);
		byte_writer << ByteWriter::Data(rel_bss[i].symbol, 8);
		byte_writer << ByteWriter::Data((int) rel_bss[i].type, 4);
		byte_writer << ByteWriter::Data(rel_bss[i].shift, 8);
	}
	sections[section_table[".rel.bss"]].section_size = rel_bss.size() * RELOCATION_ENTRY_SIZE;
	sections[section_table[".rel.bss"]].section_start = current_byte;
	current_byte += rel_bss.size() * RELOCATION_ENTRY_SIZE;

	/* String Table */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::write_object_file() - Writing .strtab section.");
	int size = 0;
	for (int i = 0; i < strings.size(); i++) {
		m_writer.write(strings[i]);
		byte_writer << ByteWriter::Data(0, 1);							/* Null terminated string */
		size += strings[i].size() + 1;
	}
	sections[section_table[".strtab"]].section_size = size;
	sections[section_table[".strtab"]].section_start = current_byte;
	current_byte += size;

	/* Section headers */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::write_object_file() - Writing Section headers.");
	for (int i = 0; i < sections.size(); i++) {
		byte_writer << ByteWriter::Data(sections[i].section_name, 8);
		byte_writer << ByteWriter::Data((int) sections[i].type, 4);
		byte_writer << ByteWriter::Data(sections[i].section_start, 8);
		byte_writer << ByteWriter::Data(sections[i].section_size, 8);
		byte_writer << ByteWriter::Data(sections[i].entry_size, 8);
	}
	/* For easy access */
	byte_writer << ByteWriter::Data(current_byte, 8);
	current_byte += 8;
	current_byte += sections.size() * SECTION_HEADER_SIZE;

	m_writer.close();

	m_state = State::WRITING_SUCCESS;
}

void ObjectFile::print() {
	/* Print object file */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ObjectFile::print() - Printing object file.");

	/* Don't print object files that could not be disassembled */
	if (m_state != State::DISASSEMBLED_SUCCESS) {
		printf("ERROR: Cannot print object file. Disassembly failed.");
		return;
	}

	printf("%s:\tfile format %s\n\n", (m_obj_file.getFileName() + "." + OBJECT_EXTENSION).c_str(), "belf32-littleemu32");
	printf("SYMBOL TABLE:\n");
	for (std::pair<int, SymbolTableEntry> symbol : symbol_table) {
		char visibility = ' ';
		if (symbol.second.binding_info == SymbolTableEntry::BindingInfo::GLOBAL) {
			visibility = 'g';
		} else if (symbol.second.binding_info == SymbolTableEntry::BindingInfo::LOCAL) {
			visibility = 'l';
		}

		std::string section_name = "*UND*";
		if (symbol.second.section != -1) {
			section_name = strings[sections[symbol.second.section].section_name];
		}

		// printf("%.16llx %c\t %s\t\t %.16llx %s\n", (dword) symbol.second.symbol_value, visibility, section_name.c_str(),(dword) 0, strings[symbol.second.symbol_name].c_str());
		std::cout << prettyStringifyValue(stringifyHex((dword) symbol.second.symbol_value))
				<< " " << visibility << "\t " << section_name << "\t\t " <<
				prettyStringifyValue(stringifyHex((dword) 0)) << " " << strings[symbol.second.symbol_name]
				<< "\n";
	}

	printf("\nContents of section .data:");
	int data_address_width = std::__bit_width(data_section.size() / 16);
	if (data_address_width < 4) {
		data_address_width = 4;
	}
	std::string data_address_format = "\n%." + std::to_string(data_address_width) + "hx ";
	for (int i = 0; i < data_section.size(); i++) {
		if (i % 16 == 0) {
			printf(data_address_format.c_str(), i);
		}
		if (i % 4 == 0 && i % 16 != 0) {
			printf(" ");
		}
		printf("%.2hhx", data_section[i]);
	}

	printf("\n\nDisassembly of section .text:\n");
	std::unordered_map<int, int> label_map;
	for (std::pair<int, SymbolTableEntry> symbol : symbol_table) {
		if (sections[symbol.second.section].type != SectionHeader::Type::TEXT || strings[symbol.second.symbol_name].find("::SCOPE") != std::string::npos) {
			continue;
		}

		label_map[symbol.second.symbol_value] = symbol.second.symbol_name;
	}

	std::unordered_map<int, RelocationEntry> rel_text_map;
	for (int i = 0; i < rel_text.size(); i++) {
		rel_text_map[rel_text[i].offset] = rel_text[i];
	}

	if (label_map.find(0) == label_map.end()) {
		// printf("%.16llx:", (dword) 0);
		std::cout << prettyStringifyValue(stringifyHex((dword) 0)) << ":";
	}

	int text_address_width = std::__bit_width(text_section.size() / 4);
	if (text_address_width < 4) {
		text_address_width = 4;
	}
	std::string text_address_format = "\n%" + std::to_string(text_address_width) + "hx";
	std::string relocation_spacing = "\n%" + std::to_string(text_address_width) + "s";
	std::string current_label = "";
	for (int i = 0; i < text_section.size(); i++) {
		if (label_map.find(i*4) != label_map.end()) {
			if (i != 0) {
				printf("\n\n");
			}
			current_label = strings[label_map[i*4]];
			// printf("\n%.16llx <%s>:", (dword) i*4, current_label.c_str());
			std::cout << prettyStringifyValue(stringifyHex((dword) i*4)) << " <" << current_label << ">:";
		}
		std::string disassembly = (this->*_disassembler_instructions[bitfield_u32(text_section[i], 26, 6)])(text_section[i]);
		printf(text_address_format.c_str(), i*4);

		if (disassembly.find_first_of(' ') != std::string::npos) {
			std::string op = disassembly.substr(0, disassembly.find_first_of(' '));
			std::string operands = disassembly.substr(disassembly.find_first_of(' ') + 1);
			printf(":\t%.8lx\t%.12s\t\t%s", text_section[i], op.c_str(), operands.c_str());
			switch (bitfield_u32(text_section[i], 26, 6)) {
				case Emulator32bit::_op_b:
				case Emulator32bit::_op_bl:
					printf(" <%s+0x%hx>", current_label.c_str(), bitfield_s32(text_section[i], 0, 22)*4);
			}
		} else {
			printf(":\t%.8lx\t%.12s", text_section[i], disassembly.c_str());
		}

		/* Check if there is a relocation record here */
		if (rel_text_map.find(i*4) != rel_text_map.end()) {
			printf(relocation_spacing.c_str(), "");
			printf(" \t%hx: ", (dword) i*4);

			RelocationEntry entry = rel_text_map[i*4];
			switch (entry.type) {
				case RelocationEntry::Type::R_EMU32_O_LO12:
					printf("R_EMU32_O_LO12 ");
					break;
				case RelocationEntry::Type::R_EMU32_ADRP_HI20:
					printf("R_EMU32_ADRP_HI20 ");
					break;
				case RelocationEntry::Type::R_EMU32_MOV_LO19:
					printf("R_EMU32_MOV_LO19 ");
					break;
				case RelocationEntry::Type::R_EMU32_MOV_HI13:
					printf("R_EMU32_MOV_HI13 ");
					break;
				case RelocationEntry::Type::R_EMU32_B_OFFSET22:
					printf("R_EMU32_B_OFFSET22 ");
					break;
				case RelocationEntry::Type::UNDEFINED:
					printf("<ERROR> ");
					break;
			}

			printf("%s", strings[symbol_table[entry.symbol].symbol_name].c_str());
		}
	}
	printf("\n");
}

std::string disassemble_register(int reg) {
	if (reg == SP) {
		return "sp";
	} else if (reg == XZR) {
		return "xzr";
	} else {
		return "x" + std::to_string(reg);
	}
}

std::string disassemble_shift(word instruction) {
	std::string disassemble;
	switch (bitfield_u32(instruction, 7, 2)) {
		case LSL:
			disassemble = "lsl #";
			break;
		case LSR:
			disassemble = "lsr #";
			break;
		case ASR:
			disassemble = "asr #";
			break;
		case ROR:
			disassemble = "ror #";
			break;
	}

	disassemble += std::to_string(bitfield_u32(instruction, 2, 5));
	return disassemble;
}
std::string disassemble_condition(Emulator32bit::ConditionCode condition) {
	switch(condition) {
		case Emulator32bit::ConditionCode::EQ:
			return "eq";
		case Emulator32bit::ConditionCode::NE:
			return "ne";
		case Emulator32bit::ConditionCode::CS:
			return "cs";
		case Emulator32bit::ConditionCode::CC:
			return "cc";
		case Emulator32bit::ConditionCode::MI:
			return "mi";
		case Emulator32bit::ConditionCode::PL:
			return "pl";
		case Emulator32bit::ConditionCode::VS:
			return "vs";
		case Emulator32bit::ConditionCode::VC:
			return "vc";
		case Emulator32bit::ConditionCode::HI:
			return "hi";
		case Emulator32bit::ConditionCode::LS:
			return "ls";
		case Emulator32bit::ConditionCode::GE:
			return "ge";
		case Emulator32bit::ConditionCode::LT:
			return "lt";
		case Emulator32bit::ConditionCode::GT:
			return "gt";
		case Emulator32bit::ConditionCode::LE:
			return "le";
		case Emulator32bit::ConditionCode::AL:
			return "al";
		case Emulator32bit::ConditionCode::NV:
			return "nv";
	}
	return "INVALID";
}

std::string disassemble_format_b2(word instruction, std::string op) {
	std::string disassemble = op;
	Emulator32bit::ConditionCode condition = (Emulator32bit::ConditionCode) bitfield_u32(instruction, 22, 4);
	if (condition != Emulator32bit::ConditionCode::AL) {
		disassemble += "." + disassemble_condition(condition);
	}

	if (bitfield_u32(instruction, 17, 5) == 29) {
		disassemble = "ret";
	} else {
		disassemble += " " + disassemble_register(bitfield_u32(instruction, 17, 5));
	}

	return disassemble;
}

std::string disassemble_format_b1(word instruction, std::string op) {
	std::string disassemble = op;
	Emulator32bit::ConditionCode condition = (Emulator32bit::ConditionCode) bitfield_u32(instruction, 22, 4);
	if (condition != Emulator32bit::ConditionCode::AL) {
		disassemble += "." + disassemble_condition(condition);
	}
	disassemble += " #" + std::to_string(bitfield_s32(instruction, 0, 22));
	return disassemble;
}

std::string disassemble_format_m2(word instruction, std::string op) {
	std::string disassemble = op + " ";
	disassemble += disassemble_register(bitfield_u32(instruction, 20, 5));
	disassemble += ", ";
	disassemble += "#" + std::to_string(bitfield_u32(instruction, 0, 20));
	return disassemble;
}

std::string disassemble_format_m1(word instruction, std::string op) {
	std::string disassemble = op + " ";
	disassemble += disassemble_register(bitfield_u32(instruction, 20, 5));
	disassemble += ", ";

	disassemble += disassemble_register(bitfield_u32(instruction, 15, 5));
	disassemble += ", [";

	disassemble += disassemble_register(bitfield_u32(instruction, 9, 5));
	disassemble += "]";
	return disassemble;
}

std::string disassemble_format_m(word instruction, std::string op) {
	std::string disassemble = op;

	if (test_bit(instruction, 25)) {
		disassemble.insert(disassemble.begin() + 3, 's');
	}
	disassemble += " ";

	disassemble += disassemble_register(bitfield_u32(instruction, 20, 5));
	disassemble += ", ";

	disassemble += "[";
	disassemble += disassemble_register(bitfield_u32(instruction, 15, 5));
	int adr_mode = bitfield_u32(instruction, 0, 2);
	if (adr_mode != M_PRE && adr_mode != M_OFFSET && adr_mode != M_POST) {
		EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream()
				<< "ObjectFile::disassemble_format_m() - Invalid addressing mode in the disassembly of instruction ("
				<< op << ") " << instruction);
	}

	if (test_bit(instruction, 14)) {
		int simm12 = bitfield_s32(instruction, 2, 12);
		if (simm12 == 0 ){
			disassemble += "]";
		}else if (adr_mode == M_PRE) {
			disassemble += ", #" + std::to_string(simm12) + "]!";
		} else if (adr_mode == M_OFFSET) {
			disassemble += ", #" + std::to_string(simm12) + "]";
		} else if (adr_mode == M_POST) {
			disassemble += "], #" + std::to_string(simm12);
		}
	} else {
		std::string reg = disassemble_register(bitfield_u32(instruction, 9, 5));
		std::string shift = "";
		if (bitfield_u32(instruction, 2, 5) > 0) {
			shift = ", " + disassemble_shift(instruction);
		}

		if (adr_mode == M_PRE) {
			disassemble += ", " + reg + ", " + shift + "]!";
		} else if (adr_mode == M_OFFSET) {
			disassemble += ", " + reg + ", " + shift + "]";
		} else if (adr_mode == M_POST) {
			disassemble += "], " + reg + ", " + shift;
		}
	}
	return disassemble;
}

std::string disassemble_format_o3(word instruction, std::string op) {
	std::string disassemble = op;
	if (test_bit(instruction, 25)) {
		disassemble += "s";
	}
	disassemble += " ";

	disassemble += disassemble_register(bitfield_u32(instruction, 20, 5));
	disassemble += ", ";

	if (test_bit(instruction, 19)) {
		disassemble += "#" + std::to_string(bitfield_u32(instruction, 0, 19));
	} else {
		disassemble += disassemble_register(bitfield_u32(instruction, 14, 5));
		if (bitfield_u32(instruction, 0, 14) > 0) {
			disassemble += " " + std::to_string(bitfield_u32(instruction, 0, 14));
		}
	}
	return disassemble;
}

std::string disassemble_format_o2(word instruction, std::string op) {
	std::string disassemble = op;
	if (test_bit(instruction, 25)) {
		disassemble += "s";
	}
	disassemble += " ";

	disassemble += disassemble_register(bitfield_u32(instruction, 20, 5));
	disassemble += ", ";

	disassemble += disassemble_register(bitfield_u32(instruction, 15, 5));
	disassemble += ", ";

	disassemble += disassemble_register(bitfield_u32(instruction, 9, 5));
	disassemble += ", ";

	disassemble += disassemble_register(bitfield_u32(instruction, 4, 5));
	disassemble += ", ";

	return disassemble;
}

std::string disassemble_format_o1(word instruction, std::string op) {
	std::string disassemble = op + " ";

	disassemble += disassemble_register(bitfield_u32(instruction, 20, 5));
	disassemble += ", ";

	disassemble += disassemble_register(bitfield_u32(instruction, 15, 5));
	disassemble += ", ";

	if (test_bit(instruction, 14)) {
		disassemble += "#";
		disassemble += std::to_string(bitfield_u32(instruction, 0, 14));
	} else {
		disassemble += disassemble_register(bitfield_u32(instruction, 9, 5));
	}
	return disassemble;
}

std::string disassemble_format_o(word instruction, std::string op) {
	std::string disassemble = op;
	if (test_bit(instruction, 25)) {
		disassemble += "s";
	}
	disassemble += " ";

	disassemble += disassemble_register(bitfield_u32(instruction, 20, 5));
	disassemble += ", ";

	disassemble += disassemble_register(bitfield_u32(instruction, 15, 5));
	disassemble += ", ";

	if (test_bit(instruction, 14)) {
		disassemble += "#" + std::to_string(bitfield_u32(instruction, 0, 14));
	} else {
		disassemble += disassemble_register(bitfield_u32(instruction, 9, 5));

		if (bitfield_u32(instruction, 2, 5) > 0) {
			disassemble += ", " + disassemble_shift(instruction);
		}
	}
	return disassemble;
}

std::string ObjectFile::disassemble_add(word instruction) {
	return disassemble_format_o(instruction, "add");
}

std::string ObjectFile::disassemble_sub(word instruction) {
	return disassemble_format_o(instruction, "sub");
}

std::string ObjectFile::disassemble_rsb(word instruction) {
	return disassemble_format_o(instruction, "rsb");
}

std::string ObjectFile::disassemble_adc(word instruction) {
	return disassemble_format_o(instruction, "adc");
}

std::string ObjectFile::disassemble_sbc(word instruction) {
	return disassemble_format_o(instruction, "sbc");
}

std::string ObjectFile::disassemble_rsc(word instruction) {
	return disassemble_format_o(instruction, "rsc");
}

std::string ObjectFile::disassemble_mul(word instruction) {
	return disassemble_format_o(instruction, "mul");
}

std::string ObjectFile::disassemble_umull(word instruction) {
	return disassemble_format_o2(instruction, "umull");
}

std::string ObjectFile::disassemble_smull(word instruction) {
	return disassemble_format_o2(instruction, "smull");
}

std::string ObjectFile::disassemble_vabs_f32(word instruction) {
	return "UNIMPLEMENTED";
}

std::string ObjectFile::disassemble_vneg_f32(word instruction) {
	return "UNIMPLEMENTED";
}

std::string ObjectFile::disassemble_vsqrt_f32(word instruction) {
	return "UNIMPLEMENTED";
}

std::string ObjectFile::disassemble_vadd_f32(word instruction) {
	return "UNIMPLEMENTED";
}

std::string ObjectFile::disassemble_vsub_f32(word instruction) {
	return "UNIMPLEMENTED";
}

std::string ObjectFile::disassemble_vdiv_f32(word instruction) {
	return "UNIMPLEMENTED";
}

std::string ObjectFile::disassemble_vmul_f32(word instruction) {
	return "UNIMPLEMENTED";
}

std::string ObjectFile::disassemble_vcmp_f32(word instruction) {
	return "UNIMPLEMENTED";
}

std::string ObjectFile::disassemble_vsel_f32(word instruction) {
	return "UNIMPLEMENTED";
}

std::string ObjectFile::disassemble_vcint_u32_f32(word instruction) {
	return "UNIMPLEMENTED";
}

std::string ObjectFile::disassemble_vcint_s32_f32(word instruction) {
	return "UNIMPLEMENTED";
}

std::string ObjectFile::disassemble_vcflo_u32_f32(word instruction) {
	return "UNIMPLEMENTED";
}

std::string ObjectFile::disassemble_vcflo_s32_f32(word instruction) {
	return "UNIMPLEMENTED";
}

std::string ObjectFile::disassemble_vmov_f32(word instruction) {
	return "UNIMPLEMENTED";
}

std::string ObjectFile::disassemble_and(word instruction) {
	return disassemble_format_o(instruction, "and");
}

std::string ObjectFile::disassemble_orr(word instruction) {
	return disassemble_format_o(instruction, "orr");
}

std::string ObjectFile::disassemble_eor(word instruction) {
	return disassemble_format_o(instruction, "eor");
}

std::string ObjectFile::disassemble_bic(word instruction) {
	return disassemble_format_o(instruction, "bic");
}

std::string ObjectFile::disassemble_lsl(word instruction) {
	return disassemble_format_o1(instruction, "lsl");
}

std::string ObjectFile::disassemble_lsr(word instruction) {
	return disassemble_format_o1(instruction, "lsr");
}

std::string ObjectFile::disassemble_asr(word instruction) {
	return disassemble_format_o1(instruction, "asr");
}

std::string ObjectFile::disassemble_ror(word instruction) {
	return disassemble_format_o1(instruction, "ror");
}

std::string ObjectFile::disassemble_cmp(word instruction) {
	std::string disassemble = disassemble_format_o(instruction, "cmp");
	return "cmp" + disassemble.substr(disassemble.find_first_of("xzr")+4);
}

std::string ObjectFile::disassemble_cmn(word instruction) {
	std::string disassemble = disassemble_format_o(instruction, "cmn");
	return "cmn" + disassemble.substr(disassemble.find_first_of("xzr")+4);
}

std::string ObjectFile::disassemble_tst(word instruction) {
	std::string disassemble = disassemble_format_o(instruction, "tst");
	return "tst" + disassemble.substr(disassemble.find_first_of("xzr")+4);
}

std::string ObjectFile::disassemble_teq(word instruction) {
	std::string disassemble = disassemble_format_o(instruction, "teq");
	return "teq" + disassemble.substr(disassemble.find_first_of("xzr")+4);
}

std::string ObjectFile::disassemble_mov(word instruction) {
	return disassemble_format_o3(instruction, "mov");
}

std::string ObjectFile::disassemble_mvn(word instruction) {
	return disassemble_format_o3(instruction, "mvn");
}

std::string ObjectFile::disassemble_ldr(word instruction) {
	return disassemble_format_m(instruction, "ldr");
}

std::string ObjectFile::disassemble_str(word instruction) {
	return disassemble_format_m(instruction, "str");
}

std::string ObjectFile::disassemble_swp(word instruction) {
	return disassemble_format_m1(instruction, "swp");
}

std::string ObjectFile::disassemble_ldrb(word instruction) {
	return disassemble_format_m(instruction, "ldrb");
}

std::string ObjectFile::disassemble_strb(word instruction) {
	return disassemble_format_m(instruction, "strb");
}

std::string ObjectFile::disassemble_swpb(word instruction) {
	return disassemble_format_m1(instruction, "swpb");
}

std::string ObjectFile::disassemble_ldrh(word instruction) {
	return disassemble_format_m(instruction, "ldrh");
}

std::string ObjectFile::disassemble_strh(word instruction) {
	return disassemble_format_m(instruction, "strh");
}

std::string ObjectFile::disassemble_swph(word instruction) {
	return disassemble_format_m1(instruction, "swph");
}

std::string ObjectFile::disassemble_b(word instruction) {
	return disassemble_format_b1(instruction, "b");
}

std::string ObjectFile::disassemble_bl(word instruction) {
	return disassemble_format_b1(instruction, "bl");
}

std::string ObjectFile::disassemble_bx(word instruction) {
	return disassemble_format_b2(instruction, "bx");
}

std::string ObjectFile::disassemble_blx(word instruction) {
	return disassemble_format_b2(instruction, "blx");
}

std::string ObjectFile::disassemble_swi(word instruction) {
	return disassemble_format_b1(instruction, "swi");
}

std::string ObjectFile::disassemble_adrp(word instruction) {
	return disassemble_format_m2(instruction, "adrp");
}

std::string ObjectFile::disassemble_hlt(word instruction) {
	return "hlt";
}