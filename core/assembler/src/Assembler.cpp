#include "assembler/Assembler.h"
#include "emulator32bit/Emulator32bit.h"
#include "util/Logger.h"
#include "util/Types.h"

#include <fstream>
#include <regex>

using namespace lgr;

Assembler::Assembler(Process *process, File *processed_file, std::string output_path) {
	m_process = process;
	m_inputFile = processed_file;

	if (output_path.empty()) {
		m_outputFile = new File(m_inputFile->getFileName(), OBJECT_EXTENSION, processed_file->getFileDirectory(), true);
	} else {
		m_outputFile = new File(output_path, true);
	}

	EXPECT_TRUE(m_process->isValidProcessedFile(processed_file), Logger::LogType::ERROR, std::stringstream() << "Assembler::Assembler() - Invalid processed file: " << processed_file->getExtension());

	m_state = State::NOT_ASSEMBLED;
	m_tokens = Tokenizer::tokenize(processed_file);

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

Assembler::~Assembler() {
	delete m_outputFile;
}

Assembler::State Assembler::get_state() {
	return this->m_state;
}


int Assembler::add_section(const std::string section_name, SectionHeader header) {
	EXPECT_TRUE(section_table.find(section_name) == section_table.end(), lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::add_section() - Section name exists in section table");

	header.section_name = add_string(section_name);
	section_table[section_name] = sections.size();
	sections.push_back(header);

	return sections.size() - 1;
}

int Assembler::add_string(const std::string string) {
	EXPECT_TRUE(string_table.find(string) == string_table.end(), lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::add_string() - String name exists in string table");

	string_table[string] = string_table.size();
	strings.push_back(string);
	return strings.size()-1;
}

// todo, filter out all spaces and tabs
File* Assembler::assemble() {
	log(Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Assembling file: " << m_inputFile->getFileName());

	EXPECT_TRUE(m_state == State::NOT_ASSEMBLED, Logger::LogType::ERROR, std::stringstream() << "Assembler::assemble() - Assembler is not in the NOT ASSEMBLED state");
	m_state = State::ASSEMBLING;

	// clearing object file
	std::ofstream ofs;
	ofs.open(m_outputFile->getFilePath(), std::ofstream::out | std::ofstream::trunc);
	ofs.close();

	add_section(".text", (SectionHeader) {
		.section_name = 0,
		.type = SectionHeader::Type::TEXT,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 4,
	});

	add_section(".data", (SectionHeader) {
		.section_name = 0,
		.type = SectionHeader::Type::DATA,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 0,
	});

	add_section(".bss", (SectionHeader) {
		.section_name = 0,
		.type = SectionHeader::Type::BSS,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 0,
	});

	add_section(".symtab", (SectionHeader) {
		.section_name = 0,
		.type = SectionHeader::Type::REL_TEXT,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 26,
	});

	add_section(".rel.text", (SectionHeader) {
		.section_name = 0,
		.type = SectionHeader::Type::REL_TEXT,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 28,
	});

	add_section(".rel.data", (SectionHeader) {
		.section_name = 0,
		.type = SectionHeader::Type::REL_DATA,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 28,
	});

	add_section(".rel.bss", (SectionHeader) {
		.section_name = 0,
		.type = SectionHeader::Type::REL_BSS,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 28,
	});

	add_section(".strtab", (SectionHeader) {
		.section_name = 0,
		.type = SectionHeader::Type::STRTAB,
		.section_start = 0,
		.section_size = 0,
		.entry_size = 0,
	});


	// parse tokens
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Parsing tokens.");
	for (int i = 0; i < m_tokens.size(); ) {
		Tokenizer::Token& token = m_tokens[i];
        log(Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Assembling token " << i << ": " << token.to_string());

        // skip non code or directives
        if (isToken(i, Tokenizer::WHITESPACES) || isToken(i, Tokenizer::COMMENTS)) {
            i++;
            continue;
        }

		// perform logic on current token
		if (token.type == Tokenizer::LABEL) {
			if (current_section == Section::NONE) {
				log(Logger::LogType::ERROR, std::stringstream() << "Assembler::assemble() - Label must be located in a section.");
				m_state = State::ASSEMBLER_ERROR;
				break;
			}

			std::string symbol = token.value.substr(0, token.value.size()-1) + (scopes.empty() ? "" : "::SCOPE:" + std::to_string(scopes.back()));
			if (current_section == Section::TEXT) {
				add_symbol(symbol, text_section.size() * 4, SymbolTableEntry::BindingInfo::LOCAL, 0);
			} else if (current_section == Section::DATA) {
				add_symbol(symbol, text_section.size() * 4, SymbolTableEntry::BindingInfo::LOCAL, 1);
			} else if (current_section == Section::BSS) {
				add_symbol(symbol, text_section.size() * 4, SymbolTableEntry::BindingInfo::LOCAL, 2);
			}
			i++;
		} else if (instructions.find(token.type) != instructions.end()) {
			if (current_section != Section::TEXT) {
				log(Logger::LogType::ERROR, std::stringstream() << "Assembler::assemble() - Code must be located in .text section.");
				m_state = State::ASSEMBLER_ERROR;
				break;
			}
			(this->*instructions[token.type])(i);
		} else if (directives.find(token.type) != directives.end()) {
			(this->*directives[token.type])(i);
		} else {
			log(Logger::LogType::ERROR, std::stringstream() << "Assembler::assemble() - Cannot parse token " << i << " " << token.to_string());
			m_state = State::ASSEMBLER_ERROR;
			break;
		}
	}
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Finished parsing tokens.");

	/* Parse through second time to fill in local symbol values */
	fill_local();

	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Writing to object file.");
	// create writer for object file
	m_writer = new FileWriter(m_outputFile);

	ByteWriter byte_writer(m_writer);
	int current_byte = 0;

	/* BELF Header */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Writing BELF header.");
	m_writer->write("BELF");											/*! BELF magic number header */
	byte_writer << ByteWriter::Data(0, 12);								/*! Unused padding */
	byte_writer << ByteWriter::Data(RELOCATABLE_FILE_TYPE, 2);			/*! Object file type */
	byte_writer << ByteWriter::Data(EMU_32BIT_MACHINE_ID, 2);			/*! Target machine */
	byte_writer << ByteWriter::Data(0, 2);								/*! Flags */
	byte_writer << ByteWriter::Data(8, 2);								/*! Number of sections */
	current_byte += 24;

	/* Text Section */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Writing .text section.");
	for (int i = 0; i < text_section.size(); i++) {
		byte_writer << ByteWriter::Data(text_section.at(i), 4, false);
	}
	sections[section_table[".text"]].section_size = text_section.size() * 4;
	sections[section_table[".text"]].section_start = current_byte;
	current_byte += text_section.size() * 4;

	/* Data Section */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Writing .data section.");
	for (int i = 0; i < data_section.size(); i++) {
		byte_writer << ByteWriter::Data(data_section.at(i), 1);
	}
	sections[section_table[".data"]].section_size = data_section.size();
	sections[section_table[".data"]].section_start = current_byte;
	current_byte += data_section.size();

	/* BSS Section */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Writing .bss section. Size " << bss_section << " bytes.");
	byte_writer << ByteWriter::Data(0, bss_section);
	sections[section_table[".bss"]].section_size = bss_section;
	sections[section_table[".bss"]].section_start = current_byte;
	current_byte += bss_section;

	/* Symbol Table */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Writing .symtab section.");
	const int SYMBOL_TABLE_ENTRY_SIZE = 26;
	for (std::pair<int, SymbolTableEntry> symbol : symbol_table) {
		byte_writer << ByteWriter::Data(symbol.second.symbol_name, 8);
		byte_writer << ByteWriter::Data(symbol.second.symbol_value, 8);
		byte_writer << ByteWriter::Data((int) symbol.second.binding_info, 2);
		byte_writer << ByteWriter::Data(symbol.second.section, 8);
	}
	sections[section_table[".symtab"]].section_size = symbol_table.size() * SYMBOL_TABLE_ENTRY_SIZE;
	sections[section_table[".symtab"]].section_start = current_byte;
	current_byte += symbol_table.size() * SYMBOL_TABLE_ENTRY_SIZE;

	/* rel.text Section */
	const int RELOCATION_ENTRY_SIZE = 28;
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Writing .rel.text section.");
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
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Writing .rel.data section.");
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
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Writing .rel.bss section.");
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
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Writing .strtab section.");
	int size = 0;
	for (int i = 0; i < strings.size(); i++) {
		m_writer->write(strings[i]);
		byte_writer << ByteWriter::Data(0, 1);							/* Null terminated string */
		size += strings[i].size() + 1;
	}
	sections[section_table[".rel.bss"]].section_size = rel_bss.size() * RELOCATION_ENTRY_SIZE;
	sections[section_table[".rel.bss"]].section_start = current_byte;
	current_byte += rel_bss.size() * RELOCATION_ENTRY_SIZE;

	/* Section header */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Writing Section headers.");
	const int SECTION_HEADER_SIZE = 36;
	for (int i = 0; i < sections.size(); i++) {
		byte_writer << ByteWriter::Data(sections[i].section_name, 8);
		byte_writer << ByteWriter::Data((int) sections[i].type, 4);
		byte_writer << ByteWriter::Data(sections[i].section_start, 8);
		byte_writer << ByteWriter::Data(sections[i].section_size, 8);
		byte_writer << ByteWriter::Data(sections[i].entry_size, 8);
	}
	current_byte += sections.size() * SECTION_HEADER_SIZE;

	m_writer->close();
    delete m_writer;

	if (m_state == State::ASSEMBLING) {
		m_state = State::ASSEMBLED;
		log(Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Assembled file: " << m_inputFile->getFileName());
	}

	/* Print object file */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Printing object file.");
	printf("%s:\tfile format %s\n\n", (m_outputFile->getFileName() + "." + OBJECT_EXTENSION).c_str(), "belf32-littleemu32");
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
		printf("%hhx", data_section[i]);
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

	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Assembler::assemble() - Finished printing object file.");
	return m_outputFile;
}


void Assembler::fill_local() {
	int tokenI = 0;

	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Assembler::file_local() - Parsing relocation entries to fill in known values.");
	std::vector<int> local_scope;
	int local_count_scope = 0;
	for (int i = 0; i < rel_text.size(); i++) {
		RelocationEntry &rel = rel_text.at(i);
		lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Assembler::file_local() - Evaluating relocation entry " << strings[symbol_table[rel.symbol].symbol_name]);

		while (tokenI < rel.token && tokenI < m_tokens.size()) {
			if (m_tokens[tokenI].type == Tokenizer::ASSEMBLER_SCOPE) {
				local_scope.push_back(local_count_scope++);
			} else if (m_tokens[tokenI].type == Tokenizer::ASSEMBLER_SCEND) {
				local_scope.pop_back();
			}

			tokenI++;
		}

		// first find if symbol is defined in local scope
		SymbolTableEntry symbol_entry;
		bool found_local = false;
		std::string symbol = strings[symbol_table[rel.symbol].symbol_name];
		for (int scopeI = local_scope.size()-1; scopeI >= 0; scopeI--) {
			std::string local_symbol_name = symbol + "::SCOPE:" + std::to_string(local_scope[scopeI]);
			if (string_table.find(local_symbol_name) == string_table.end()) {
				continue;
			}

			symbol_entry = symbol_table[string_table[local_symbol_name]];
			found_local = true;
			break;
		}

		if (!found_local) {
			if (symbol_table.at(rel.symbol).binding_info != SymbolTableEntry::BindingInfo::WEAK) {
				symbol_entry = symbol_table.at(rel.symbol);
			} else {
				continue;
			}
		}

		switch (rel.type) {
			case RelocationEntry::Type::R_EMU32_O_LO12:
				text_section[rel.offset/4] = mask_0(text_section[rel.offset/4], 0, 14) + bitfield_u32(symbol_entry.symbol_value, 0, 12);
				break;
			case RelocationEntry::Type::R_EMU32_ADRP_HI20:
				text_section[rel.offset/4] = mask_0(text_section[rel.offset/4], 0, 20) + bitfield_u32(symbol_entry.symbol_value, 12, 20);
				break;
			case RelocationEntry::Type::R_EMU32_MOV_LO19:
				text_section[rel.offset/4] = mask_0(text_section[rel.offset/4], 0, 19) + bitfield_u32(symbol_entry.symbol_value, 0, 19);
				break;
			case RelocationEntry::Type::R_EMU32_MOV_HI13:
				text_section[rel.offset/4] = mask_0(text_section[rel.offset/4], 0, 19) + bitfield_u32(symbol_entry.symbol_value, 19, 13);
				break;
			case RelocationEntry::Type::R_EMU32_B_OFFSET22:
				EXPECT_TRUE((symbol_entry.symbol_value & 0b11) == 0, lgr::Logger::LogType::ERROR, std::stringstream()
						<< "Assembler::fill_local() - Expected relocation value for R_EMU32_B_OFFSET22 to be 4 byte aligned. Got "
						<< symbol_entry.symbol_value);
				text_section[rel.offset/4] = mask_0(text_section[rel.offset/4], 0, 22) + bitfield_u32(bitfield_s32(symbol_entry.symbol_value, 2, 22) - rel.offset/4, 0, 22);
				break;
			case RelocationEntry::Type::UNDEFINED:
			default:
				lgr::log(lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::fill_local() - Unknown relocation entry type.");
		}

		rel_text.erase(rel_text.begin() + i);							/*! For now, simply delete from vector. In future look to optimize */
		i--;															/*! Offset the for loop increment */
	}

	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "Assembler::file_local() - Finished parsing relocation entries.");
}


/**
 * Skips tokens that match the given regex.
 *
 * @param regex matches tokens to skip.
 * @param tokenI the index of the current token.
 */
void Assembler::skipTokens(int& tokenI, const std::string& regex) {
	while (inBounds(tokenI) && std::regex_match(m_tokens[tokenI].value, std::regex(regex))) {
		tokenI++;
	}
}

/**
 * Skips tokens that match the given types.
 *
 * @param tokenI the index of the current token.
 * @param tokenTypes the types to match.
 */
void Assembler::skipTokens(int& tokenI, const std::set<Tokenizer::Type>& tokenTypes) {
    while (inBounds(tokenI) && tokenTypes.find(m_tokens[tokenI].type) != tokenTypes.end()) {
        tokenI++;
    }
}

/**
 * Expects the current token to exist.
 *
 * @param tokenI the index of the expected token.
 * @param errorMsg the error message to throw if the token does not exist.
 */
bool Assembler::expectToken(int tokenI, const std::string& errorMsg) {
	EXPECT_TRUE(inBounds(tokenI), Logger::LogType::ERROR, std::stringstream(errorMsg));
    return true;
}

bool Assembler::expectToken(int tokenI, const std::set<Tokenizer::Type>& expectedTypes, const std::string& errorMsg) {
	EXPECT_TRUE(inBounds(tokenI), Logger::LogType::ERROR, std::stringstream(errorMsg));
	EXPECT_TRUE(expectedTypes.find(m_tokens[tokenI].type) != expectedTypes.end(), Logger::LogType::ERROR, std::stringstream(errorMsg));
    return true;
}

/**
 * Returns whether the current token matches the given types.
 *
 * @param tokenI the index of the current token.
 * @param tokenTypes the types to match.
 *
 * @return true if the current token matches the given types.
 */
bool Assembler::isToken(int tokenI, const std::set<Tokenizer::Type>& tokenTypes, const std::string& errorMsg) {
    expectToken(tokenI, errorMsg);
    return tokenTypes.find(m_tokens[tokenI].type) != tokenTypes.end();
}

/**
 * Returns whether the current token index is within the bounds of the tokens list.
 *
 * @param tokenI the index of the current token
 *
 * @return true if the token index is within the bounds of the tokens list.
 */
bool Assembler::inBounds(int tokenI) {
    return tokenI < m_tokens.size();
}

/**
 * Consumes the current token.
 *
 * @param tokenI the index of the current token.
 * @param errorMsg the error message to throw if the token does not exist.
 *
 * @returns the value of the consumed token.
 */
Tokenizer::Token& Assembler::consume(int& tokenI, const std::string& errorMsg) {
    expectToken(tokenI, errorMsg);
    return m_tokens[tokenI++];
}

/**
 * Consumes the current token and checks it matches the given types.
 *
 * @param tokenI the index of the current token.
 * @param expectedTypes the expected types of the token.
 * @param errorMsg the error message to throw if the token does not have the expected type.
 *
 * @returns the value of the consumed token.
 */
Tokenizer::Token& Assembler::consume(int& tokenI, const std::set<Tokenizer::Type>& expectedTypes, const std::string& errorMsg) {
    expectToken(tokenI, errorMsg);
	EXPECT_TRUE(expectedTypes.find(m_tokens[tokenI].type) != expectedTypes.end(), Logger::LogType::ERROR, std::stringstream() << errorMsg << " - Unexpected end of file.");
    return m_tokens[tokenI++];
}
