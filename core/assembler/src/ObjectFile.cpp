#include "assembler/ObjectFile.h"
#include "assembler/Build.h"
#include "emulator32bit/Emulator32bit.h"
#include "util/Logger.h"
#include "util/Types.h"

ObjectFile::ObjectFile(File* object_file) {
	this->m_file = object_file;

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

	disassemble();
	print();
}

void ObjectFile::disassemble() {
	FileReader reader(m_file);

	std::vector<byte> bytes;
	while (reader.hasNextByte()) {
		bytes.push_back(reader.readByte());
	}

	//TODO
}

void ObjectFile::print() {
	/* Print object file */
	lgr::log(lgr::Logger::LogType::DEBUG, std::stringstream() << "assemble() - Printing object file.");
	printf("%s:\tfile format %s\n\n", (m_file->getFileName() + "." + OBJECT_EXTENSION).c_str(), "belf32-littleemu32");
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