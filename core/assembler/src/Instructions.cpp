#include <assembler/Assembler.h>

#include <emulator32bit/Emulator32bit.h>
#include <emulator32bit/Emulator32bitUtil.h>
#include <util/Logger.h>

#include <string>

byte Assembler::parse_register(int& tokenI) {
	expectToken(tokenI, Tokenizer::REGISTERS, "Assembler::parse_register() - Expected register identifier. Got " + m_tokens[tokenI].value);
	Tokenizer::Type type = consume(tokenI).type;

	return type - Tokenizer::Type::REGISTER_X0;							/*! register order is assumed to be x0-x29, sp, xzr */
}

void Assembler::parse_shift(int& tokenI, int& shift, int& shift_amt) {
	expectToken(tokenI, {Tokenizer::INSTRUCTION_LSL, Tokenizer::INSTRUCTION_LSR, Tokenizer::INSTRUCTION_ASR, Tokenizer::INSTRUCTION_ROR}, "Assembler::parse_shift() - Expected shift.");

	if (isToken(tokenI, {Tokenizer::INSTRUCTION_LSL})) {
		shift = LSL;
	} else if (isToken(tokenI, {Tokenizer::INSTRUCTION_LSR})) {
		shift = LSR;
	} else if (isToken(tokenI, {Tokenizer::INSTRUCTION_ASR})) {
		shift = ASR;
	} else if (isToken(tokenI, {Tokenizer::INSTRUCTION_ROR})) {
		shift = ROR;
	}
	skipTokens(tokenI, "[ \t]");

	expectToken(tokenI, (std::set<Tokenizer::Type>) {Tokenizer::NUMBER_SIGN}, "Assembler::parse_shift() - Expected numeric argument.");
	consume(tokenI);
	shift_amt = parse_expression(tokenI);								/*! note, in future, we could change this to create relocation record instead */

	EXPECT_TRUE(shift_amt < (1<<5), lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::parse_shift() - Shift amount must fit in 5 bits. Expected < 32, Got: " << shift_amt);
}

word Assembler::parse_format_b1(int& tokenI, byte opcode) {
	consume(tokenI);

	Emulator32bit::ConditionCode condition = Emulator32bit::ConditionCode::AL;
	if (isToken(tokenI, {Tokenizer::PERIOD})) {
		consume(tokenI);
		expectToken(tokenI, Tokenizer::CONDITIONS, "Assembler::parse_format_b1() - Expected condition code to follow period.");
		condition = (Emulator32bit::ConditionCode) (consume(tokenI).type - Tokenizer::CONDITION_EQ);
	}

	sword value = 0;
	skipTokens(tokenI, "[ \t]");
	if (isToken(tokenI, {Tokenizer::SYMBOL})) {
		std::string symbol = consume(tokenI).value;
		add_symbol(symbol, 0, SymbolTableEntry::BindingInfo::WEAK, -1);

		rel_text.push_back((RelocationEntry) {
			.offset = (word) (text_section.size() * 4),
			.symbol = string_table[symbol],
			.type = RelocationEntry::Type::R_EMU32_B_OFFSET22,
			.shift = 0,													/*! Support shift in future */
		});
	} else {
		word imm = parse_expression(tokenI);
		EXPECT_TRUE(imm < (1 << 24), lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::parse_format_b1() - Expected immediate to be 24 bits");
		EXPECT_TRUE((imm & 0b11) == 0, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::parse_format_b1() - Expected immediate to be 4 byte aligned");
		value = bitfield_s32(imm, 0, 24) >> 2;
	}

	return Emulator32bit::asm_format_b1(opcode, condition, value);
}

word Assembler::parse_format_b2(int& tokenI, byte opcode) {
	consume(tokenI);

	Emulator32bit::ConditionCode condition = Emulator32bit::ConditionCode::AL;
	if (isToken(tokenI, {Tokenizer::PERIOD})) {
		consume(tokenI);
		expectToken(tokenI, Tokenizer::CONDITIONS, "Assembler::parse_format_b1() - Expected condition code to follow period.");
		condition = (Emulator32bit::ConditionCode) (consume(tokenI).type - Tokenizer::CONDITION_EQ);
	}
	skipTokens(tokenI, "[ \t]");

	byte reg = parse_register(tokenI);
	return Emulator32bit::asm_format_b2(opcode, condition, reg);
}

word Assembler::parse_format_m2(int& tokenI, byte opcode) {
	consume(tokenI);
	skipTokens(tokenI, "[ \t]");

	byte reg = parse_register(tokenI);
	skipTokens(tokenI, "[ \t]");

	expectToken(tokenI, (std::set<Tokenizer::Type>) {Tokenizer::COMMA}, "Assembler::parse_format_m2() - Expected second argument.");
	consume(tokenI);
	skipTokens(tokenI, "[ \t]");

	expectToken(tokenI, {Tokenizer::NUMBER_SIGN}, "Assembler::parse_format_m2() - Expected numeric operand");
	consume(tokenI);

	skipTokens(tokenI, "[ \t]");
	if (isToken(tokenI, {Tokenizer::RELOCATION_EMU32_ADRP_HI20})) {
		consume(tokenI);
		skipTokens(tokenI, "[ \t]");
	}

	expectToken(tokenI, {Tokenizer::SYMBOL}, "Assembler::parse_format_m2() - Expected symbol.");
	std::string symbol = consume(tokenI).value;
	add_symbol(symbol, 0, SymbolTableEntry::BindingInfo::WEAK, -1);

	rel_text.push_back((RelocationEntry) {
		.offset = (word) (text_section.size() * 4),
		.symbol = string_table[symbol],
		.type = RelocationEntry::Type::R_EMU32_ADRP_HI20,
		.shift = 0,														/*! Support shift in future */
	});

	return Emulator32bit::asm_format_m2(opcode, reg, 0);
}

word Assembler::parse_format_m1(int& tokenI, byte opcode) {
	consume(tokenI);
	skipTokens(tokenI, "[ \t]");

	byte reg_t = parse_register(tokenI);
	skipTokens(tokenI, "[ \t]");

	expectToken(tokenI, (std::set<Tokenizer::Type>) {Tokenizer::COMMA}, "Assembler::parse_format_m1() - Expected second argument.");
	consume(tokenI);
	skipTokens(tokenI, "[ \t]");
	byte reg_n = parse_register(tokenI);
	skipTokens(tokenI, "[ \t]");

	expectToken(tokenI, (std::set<Tokenizer::Type>) {Tokenizer::COMMA}, "Assembler::parse_format_m1() - Expected third argument.");
	consume(tokenI);
	expectToken(tokenI, (std::set<Tokenizer::Type>) {Tokenizer::OPEN_BRACKET}, "Assembler::parse_format_m1() - Expected open bracket.");
	consume(tokenI);
	byte reg_m = parse_register(tokenI);
	skipTokens(tokenI, "[ \t]");
	expectToken(tokenI, (std::set<Tokenizer::Type>) {Tokenizer::OPEN_BRACKET}, "Assembler::parse_format_m1() - Expected close bracket.");
	consume(tokenI);

	return Emulator32bit::asm_format_m1(opcode, reg_t, reg_n, reg_m);
}

word Assembler::parse_format_m(int& tokenI, byte opcode) {
	std::string op = consume(tokenI).value;
	bool sign = op.size() > 3 ? op.at(3) == 's' : false;				/*! ex: whether the value to be loaded/stored should be interpreted as signed */
	skipTokens(tokenI, "[ \t]");

	byte reg_t = parse_register(tokenI);								/*! target register. For reads, stores read value; for writes, stores write value */
	skipTokens(tokenI, "[ \t]");

	expectToken(tokenI, (std::set<Tokenizer::Type>) {Tokenizer::COMMA}, "Assembler::parse_format_m() - Expected second argument.");
	consume(tokenI);
	skipTokens(tokenI, "[ \t]");

	expectToken(tokenI, {Tokenizer::OPEN_BRACKET}, "Assembler::parse_format_m() - Expected open bracket");
	consume(tokenI);
	skipTokens(tokenI, "[ \t]");

	byte reg_a = parse_register(tokenI);								/*! register that contains memory address */
	skipTokens(tokenI, "[ \t]");

	int addressing_mode = -1;											/*! parse the address mode, -1 indicates invalid address mode */
	if (isToken(tokenI, {Tokenizer::CLOSE_BRACKET})) {					/*! post indexed, offset is applied to value at register after accessing */
		consume(tokenI);
		skipTokens(tokenI, "[ \t]");
		addressing_mode = M_POST;
	}

	if (isToken(tokenI, {Tokenizer::COMMA})) {							/*! check for offset */
		consume(tokenI);
		skipTokens(tokenI, "[ \t]");
		if (isToken(tokenI, {Tokenizer::NUMBER_SIGN})) {				/*! offset begins with the '#' symbol */
			consume(tokenI);
			skipTokens(tokenI, "[ \t]");
			word offset = parse_expression(tokenI);
			EXPECT_TRUE(offset < (1<<12), lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::parse_format_m() - Offset must be 12 bit value.");

			skipTokens(tokenI, "[ \t]");
			expectToken(tokenI, (std::set<Tokenizer::Type>) {Tokenizer::CLOSE_BRACKET}, "Assembler::parse_format_m() - Expected close bracket.");
			consume(tokenI);

			if (addressing_mode == -1) {								/*! only update addressing mode if not yet determined.
																		   This reduces code repetition, since the way offsets are calculated
																		   are the same for all addressing modes, but differ soley in arrangement */
				if (isToken(tokenI, {Tokenizer::OPERATOR_LOGICAL_NOT})) {
					consume(tokenI);
					addressing_mode = M_PRE;							/*! preindexed, offset is applied to value at register before accessing */
				} else {
					addressing_mode = M_OFFSET;							/*! simple offset */
				}
			}

			return Emulator32bit::asm_format_m(opcode, sign, reg_t, reg_a, offset, addressing_mode);
		}

		expectToken(tokenI, Tokenizer::REGISTERS, "Assembler::parse_format_m() - Expected register argument.");
		byte reg_b = parse_register(tokenI);							/*! since there is a comma, there is another argument that is not the above checked offset */
		int shift = 0;
		int shift_amount = 0;
		skipTokens(tokenI, "[ \t]");
		if (isToken(tokenI, {Tokenizer::COMMA})) {						/*! shift argument */
			skipTokens(tokenI, "[ \t]");
			parse_shift(tokenI, shift, shift_amount);
		}

		skipTokens(tokenI, "[ \t]");
		expectToken(tokenI, (std::set<Tokenizer::Type>) {Tokenizer::CLOSE_BRACKET}, "Assembler::parse_format_m() - Expected close bracket.");
		consume(tokenI);

		if (addressing_mode == -1) {									/*! same logic as above, only update addressing mode if not yet determined */
			if (isToken(tokenI, {Tokenizer::OPERATOR_LOGICAL_NOT})) {
				consume(tokenI);
				addressing_mode = M_PRE;								/*! preindexed */
			} else {
				addressing_mode = M_OFFSET;								/*! simple offset */
			}
		}

		return Emulator32bit::asm_format_m(opcode, sign, reg_t, reg_a, reg_b, shift, shift_amount, addressing_mode);
	}

	/* check for invalid addressing mode */
	EXPECT_FALSE(addressing_mode == -1, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::parse_format_m() - Invalid addressing mode.");
	return Emulator32bit::asm_format_m(opcode, sign, reg_t, reg_a, 0, addressing_mode);
}

word Assembler::parse_format_o3(int& tokenI, byte opcode) {
	// todo, make sure to handle relocation
	bool s = consume(tokenI).value.back() == 's';
	skipTokens(tokenI, "[ \t]");

	byte reg1 = parse_register(tokenI);
	skipTokens(tokenI, "[ \t]");

	expectToken(tokenI, {Tokenizer::COMMA}, "Assembler::parse_format_o3() - Expected comma.");
	consume(tokenI);
	skipTokens(tokenI, "[ \t]");

	if (isToken(tokenI, {Tokenizer::REGISTERS})) {						/*! In future, support relocation for immediate value */
		byte operand_reg = parse_register(tokenI);
		skipTokens(tokenI, "[ \t]");

		word value = 0;
		if (isToken(tokenI, (std::set<Tokenizer::Type>) {Tokenizer::NUMBER_SIGN})) {
			consume(tokenI);
			value = parse_expression(tokenI);
		}

		return Emulator32bit::asm_format_o3(opcode, s, reg1, operand_reg, value);
	} else {
		if (isToken(tokenI, {Tokenizer::RELOCATION_EMU32_MOV_HI13, Tokenizer::RELOCATION_EMU32_MOV_LO19})) {
			Tokenizer::Type relocation = consume(tokenI).type;
			skipTokens(tokenI, "[ \t]");
			expectToken(tokenI, (std::set<Tokenizer::Type>){Tokenizer::SYMBOL}, "Assembler::parse_format_o() - Expected symbol to follow relocation.");
			std::string symbol = consume(tokenI).value;
			add_symbol(symbol, 0, SymbolTableEntry::BindingInfo::WEAK, -1);

			rel_text.push_back((RelocationEntry) {
				.offset = (word) (text_section.size() * 4),
				.symbol = string_table[symbol],
				.type = (relocation == Tokenizer::RELOCATION_EMU32_MOV_HI13 ? RelocationEntry::Type::R_EMU32_MOV_HI13 : RelocationEntry::Type::R_EMU32_MOV_LO19),
				.shift = 0,												/*! Support shift in future */
			});

			return Emulator32bit::asm_format_o3(opcode, s, reg1, 0);
		} else {
			expectToken(tokenI, (std::set<Tokenizer::Type>) {Tokenizer::NUMBER_SIGN}, "Assembler::parse_format_o() - Expected numeric argument.");
			consume(tokenI);
			word imm = parse_expression(tokenI);

			EXPECT_TRUE(imm < (1<<14), lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::parse_format_o() - Immediate value must be a 14 bit number.");
			return Emulator32bit::asm_format_o3(opcode, s, reg1, imm);
		}
	}

	return 0;
}

word Assembler::parse_format_o2(int& tokenI, byte opcode) {
	bool s = consume(tokenI).value.back() == 's';
	skipTokens(tokenI, "[ \t]");

	byte reg1 = parse_register(tokenI);
	skipTokens(tokenI, "[ \t]");

	expectToken(tokenI, {Tokenizer::COMMA}, "Assembler::parse_format_o2() - Expected comma.");
	consume(tokenI);
	skipTokens(tokenI, "[ \t]");

	byte reg2 = parse_register(tokenI);
	skipTokens(tokenI, "[ \t]");

	expectToken(tokenI, {Tokenizer::COMMA}, "Assembler::parse_format_o2() - Expected comma.");
	consume(tokenI);
	skipTokens(tokenI, "[ \t]");

	byte operand_reg1 = parse_register(tokenI);
	skipTokens(tokenI, "[ \t]");

	expectToken(tokenI, {Tokenizer::COMMA}, "Assembler::parse_format_o2() - Expected comma.");
	consume(tokenI);
	skipTokens(tokenI, "[ \t]");

	byte operand_reg2 = parse_register(tokenI);

	return Emulator32bit::asm_format_o2(opcode, s, reg1, reg2, operand_reg1, operand_reg2);
}

word Assembler::parse_format_o1(int& tokenI, byte opcode) {
	consume(tokenI);
	skipTokens(tokenI, "[ \t]");

	byte reg1 = parse_register(tokenI);
	skipTokens(tokenI, "[ \t]");

	expectToken(tokenI, {Tokenizer::COMMA}, "Assembler::parse_format_o1() - Expected comma.");
	consume(tokenI);
	skipTokens(tokenI, "[ \t]");

	byte reg2 = parse_register(tokenI);
	skipTokens(tokenI, "[ \t]");

	expectToken(tokenI, {Tokenizer::COMMA}, "Assembler::parse_format_o1() - Expected comma.");
	consume(tokenI);
	skipTokens(tokenI, "[ \t]");

	if (isToken(tokenI, Tokenizer::REGISTERS)) {
		byte operand_reg = parse_register(tokenI);
		return Emulator32bit::asm_format_o1(opcode, reg1, reg2, false, operand_reg, 0);
	} else {
		expectToken(tokenI, (std::set<Tokenizer::Type>) {Tokenizer::NUMBER_SIGN}, "Assembler::parse_format_o1() - Expected numeric argument.");
		consume(tokenI);

		int shift_amt = parse_expression(tokenI);
		EXPECT_TRUE(shift_amt < (1<<5), lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::parse_format_o1() - Shift amount must fit in 5 bits. Expected < 32, Got: " << shift_amt);
		return Emulator32bit::asm_format_o1(opcode, reg1, reg2, true, 0, shift_amt);
	}
}

word Assembler::parse_format_o(int& tokenI, byte opcode) {
	bool s = consume(tokenI).value.back() == 's';
	skipTokens(tokenI, "[ \t]");

	byte reg1 = parse_register(tokenI);
	skipTokens(tokenI, "[ \t]");

	expectToken(tokenI, {Tokenizer::COMMA}, "Assembler::parse_format_o() - Expected comma.");
	consume(tokenI);
	skipTokens(tokenI, "[ \t]");

	byte reg2 = parse_register(tokenI);
	skipTokens(tokenI, "[ \t]");

	expectToken(tokenI, {Tokenizer::COMMA}, "Assembler::parse_format_o() - Expected comma.");
	consume(tokenI);
	skipTokens(tokenI, "[ \t]");

	if (isToken(tokenI, Tokenizer::REGISTERS)) {
		byte operand_reg = parse_register(tokenI);
		skipTokens(tokenI, "[ \t]");

		// shift
		int shift = 0;
		int shift_amt = 0;
		if (isToken(tokenI, {Tokenizer::COMMA})) {
			consume(tokenI);
			skipTokens(tokenI, "[ \t]");
			parse_shift(tokenI, shift, shift_amt);
		}

		return Emulator32bit::asm_format_o(opcode, s, reg1, reg2, operand_reg, shift, shift_amt);
	} else {
		word operand = 0;
		expectToken(tokenI, (std::set<Tokenizer::Type>) {Tokenizer::NUMBER_SIGN}, "Assembler::parse_format_o() - Expected numeric argument.");
		consume(tokenI);

		if (isToken(tokenI, {Tokenizer::RELOCATION_EMU32_O_LO12})) {
			consume(tokenI);
			skipTokens(tokenI, "[ \t]");
			expectToken(tokenI, (std::set<Tokenizer::Type>){Tokenizer::SYMBOL}, "Assembler::parse_format_o() - Expected symbol to follow relocation.");
			std::string symbol = consume(tokenI).value;
			add_symbol(symbol, 0, SymbolTableEntry::BindingInfo::WEAK, -1);

			rel_text.push_back((RelocationEntry) {
				.offset = (word) (text_section.size() * 4),
				.symbol = string_table[symbol],
				.type = RelocationEntry::Type::R_EMU32_O_LO12,
				.shift = 0,													/*! Support shift in future */
			});

		} else if (isToken(tokenI, Tokenizer::LITERAL_NUMBERS)) {
			operand = parse_expression(tokenI);
		} else {
			m_state = Assembler::State::ASSEMBLER_ERROR;
			EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::parse_format_o() - Could not parse token.");
		}

		return Emulator32bit::asm_format_o(opcode, s, reg1, reg2, operand);
	}
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
				<< "Assembler::disassemble_format_m() - Invalid addressing mode in the disassembly of instruction ("
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


/**
 * @brief
 *
 * add x1, x2, x3
 * add x1, x2, #40
 * add x1, x2, x3, lsl 4
 * add x1, x2, :lo12:symbol
 * NOT SUPPORTED -- add x1, x2, :lo12:symbol + 4
 *
 * @param 						tokenI: Reference to current token index
 */
void Assembler::_add(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_add);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_add(word instruction) {
	return disassemble_format_o(instruction, "add");
}

/**
 * @brief
 *
 * @param 						tokenI: Reference to current token index
 */
void Assembler::_sub(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_sub);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_sub(word instruction) {
	return disassemble_format_o(instruction, "sub");
}

/**
 * @brief
 *
 * @param 						tokenI: Reference to current token index
 */
void Assembler::_rsb(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_rsb);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_rsb(word instruction) {
	return disassemble_format_o(instruction, "rsb");
}

/**
 * @brief
 *
 * @param 						tokenI: Reference to current token index
 */
void Assembler::_adc(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_adc);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_adc(word instruction) {
	return disassemble_format_o(instruction, "adc");
}

/**
 * @brief
 *
 * @param 						tokenI: Reference to current token index
 */
void Assembler::_sbc(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_sbc);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_sbc(word instruction) {
	return disassemble_format_o(instruction, "sbc");
}

void Assembler::_rsc(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_rsc);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_rsc(word instruction) {
	return disassemble_format_o(instruction, "rsc");
}

void Assembler::_mul(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_mul);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_mul(word instruction) {
	return disassemble_format_o(instruction, "mul");
}

void Assembler::_umull(int& tokenI) {
	word instruction = parse_format_o2(tokenI, Emulator32bit::_op_umull);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_umull(word instruction) {
	return disassemble_format_o2(instruction, "umull");
}

void Assembler::_smull(int& tokenI) {
	word instruction = parse_format_o2(tokenI, Emulator32bit::_op_smull);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_smull(word instruction) {
	return disassemble_format_o2(instruction, "smull");
}

void Assembler::_vabs_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vabs_f32() - Instruction not implemented yet.");
}

std::string Assembler::disassemble_vabs_f32(word instruction) {
	return "UNIMPLEMENTED";
}

void Assembler::_vneg_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vneg_f32() - Instruction not implemented yet.");
}

std::string Assembler::disassemble_vneg_f32(word instruction) {
	return "UNIMPLEMENTED";
}

void Assembler::_vsqrt_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vsqrt_f32() - Instruction not implemented yet.");
}

std::string Assembler::disassemble_vsqrt_f32(word instruction) {
	return "UNIMPLEMENTED";
}

void Assembler::_vadd_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vadd_f32() - Instruction not implemented yet.");
}

std::string Assembler::disassemble_vadd_f32(word instruction) {
	return "UNIMPLEMENTED";
}

void Assembler::_vsub_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vsub_f32() - Instruction not implemented yet.");
}

std::string Assembler::disassemble_vsub_f32(word instruction) {
	return "UNIMPLEMENTED";
}

void Assembler::_vdiv_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vdiv_f32() - Instruction not implemented yet.");
}

std::string Assembler::disassemble_vdiv_f32(word instruction) {
	return "UNIMPLEMENTED";
}

void Assembler::_vmul_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vmul_f32() - Instruction not implemented yet.");
}

std::string Assembler::disassemble_vmul_f32(word instruction) {
	return "UNIMPLEMENTED";
}

void Assembler::_vcmp_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vcmp_f32() - Instruction not implemented yet.");
}

std::string Assembler::disassemble_vcmp_f32(word instruction) {
	return "UNIMPLEMENTED";
}

void Assembler::_vsel_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vsel_f32() - Instruction not implemented yet.");
}

std::string Assembler::disassemble_vsel_f32(word instruction) {
	return "UNIMPLEMENTED";
}

void Assembler::_vcint_u32_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vcint_u32_f32() - Instruction not implemented yet.");
}

std::string Assembler::disassemble_vcint_u32_f32(word instruction) {
	return "UNIMPLEMENTED";
}

void Assembler::_vcint_s32_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vcint_s32_f32() - Instruction not implemented yet.");
}

std::string Assembler::disassemble_vcint_s32_f32(word instruction) {
	return "UNIMPLEMENTED";
}

void Assembler::_vcflo_u32_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vcflo_u32_f32() - Instruction not implemented yet.");
}

std::string Assembler::disassemble_vcflo_u32_f32(word instruction) {
	return "UNIMPLEMENTED";
}

void Assembler::_vcflo_s32_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vcflo_s32_f32() - Instruction not implemented yet.");
}

std::string Assembler::disassemble_vcflo_s32_f32(word instruction) {
	return "UNIMPLEMENTED";
}

void Assembler::_vmov_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vmov_f32() - Instruction not implemented yet.");
}

std::string Assembler::disassemble_vmov_f32(word instruction) {
	return "UNIMPLEMENTED";
}

void Assembler::_and(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_and);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_and(word instruction) {
	return disassemble_format_o(instruction, "and");
}

void Assembler::_orr(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_orr);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_orr(word instruction) {
	return disassemble_format_o(instruction, "orr");
}

void Assembler::_eor(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_eor);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_eor(word instruction) {
	return disassemble_format_o(instruction, "eor");
}

void Assembler::_bic(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_bic);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_bic(word instruction) {
	return disassemble_format_o(instruction, "bic");
}

void Assembler::_lsl(int& tokenI) {
	word instruction = parse_format_o1(tokenI, Emulator32bit::_op_lsl);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_lsl(word instruction) {
	return disassemble_format_o1(instruction, "lsl");
}

void Assembler::_lsr(int& tokenI) {
	word instruction = parse_format_o1(tokenI, Emulator32bit::_op_lsr);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_lsr(word instruction) {
	return disassemble_format_o1(instruction, "lsr");
}

void Assembler::_asr(int& tokenI) {
	word instruction = parse_format_o1(tokenI, Emulator32bit::_op_asr);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_asr(word instruction) {
	return disassemble_format_o1(instruction, "asr");
}

void Assembler::_ror(int& tokenI) {
	word instruction = parse_format_o1(tokenI, Emulator32bit::_op_ror);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_ror(word instruction) {
	return disassemble_format_o1(instruction, "ror");
}

void insert_xzr(std::vector<Tokenizer::Token> tokens, int pos) {
	std::vector<Tokenizer::Token> insert = {
		Tokenizer::Token(Tokenizer::Type::WHITESPACE_SPACE, " "),
		Tokenizer::Token(Tokenizer::Type::REGISTER_XZR, "xzr"),
		Tokenizer::Token(Tokenizer::Type::COMMA, ","),
	};

	tokens.insert(tokens.begin() + pos, insert.begin(), insert.end());
}

void Assembler::_cmp(int& tokenI) {
	insert_xzr(m_tokens, tokenI+1);

	word instruction = parse_format_o(tokenI, Emulator32bit::_op_cmp);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_cmp(word instruction) {
	std::string disassemble = disassemble_format_o(instruction, "cmp");
	return "cmp" + disassemble.substr(disassemble.find_first_of("xzr")+4);
}

void Assembler::_cmn(int& tokenI) {
	insert_xzr(m_tokens, tokenI+1);

	word instruction = parse_format_o(tokenI, Emulator32bit::_op_cmn);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_cmn(word instruction) {
	std::string disassemble = disassemble_format_o(instruction, "cmn");
	return "cmn" + disassemble.substr(disassemble.find_first_of("xzr")+4);
}

void Assembler::_tst(int& tokenI) {
	insert_xzr(m_tokens, tokenI+1);

	word instruction = parse_format_o(tokenI, Emulator32bit::_op_tst);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_tst(word instruction) {
	std::string disassemble = disassemble_format_o(instruction, "tst");
	return "tst" + disassemble.substr(disassemble.find_first_of("xzr")+4);
}

void Assembler::_teq(int& tokenI) {
	insert_xzr(m_tokens, tokenI+1);

	word instruction = parse_format_o(tokenI, Emulator32bit::_op_teq);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_teq(word instruction) {
	std::string disassemble = disassemble_format_o(instruction, "teq");
	return "teq" + disassemble.substr(disassemble.find_first_of("xzr")+4);
}

void Assembler::_mov(int& tokenI) {
	word instruction = parse_format_o3(tokenI, Emulator32bit::_op_mov);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_mov(word instruction) {
	return disassemble_format_o3(instruction, "mov");
}

void Assembler::_mvn(int& tokenI) {
	word instruction = parse_format_o3(tokenI, Emulator32bit::_op_mvn);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_mvn(word instruction) {
	return disassemble_format_o3(instruction, "mvn");
}

void Assembler::_ldr(int& tokenI) {
	word instruction = parse_format_m(tokenI, Emulator32bit::_op_ldr);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_ldr(word instruction) {
	return disassemble_format_m(instruction, "ldr");
}

void Assembler::_str(int& tokenI) {
	word instruction = parse_format_m(tokenI, Emulator32bit::_op_str);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_str(word instruction) {
	return disassemble_format_m(instruction, "str");
}

void Assembler::_swp(int& tokenI) {
	word instruction = parse_format_m1(tokenI, Emulator32bit::_op_swp);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_swp(word instruction) {
	return disassemble_format_m1(instruction, "swp");
}

void Assembler::_ldrb(int& tokenI) {
	word instruction = parse_format_m(tokenI, Emulator32bit::_op_ldrb);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_ldrb(word instruction) {
	return disassemble_format_m(instruction, "ldrb");
}

void Assembler::_strb(int& tokenI) {
	word instruction = parse_format_m(tokenI, Emulator32bit::_op_strb);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_strb(word instruction) {
	return disassemble_format_m(instruction, "strb");
}

void Assembler::_swpb(int& tokenI) {
	word instruction = parse_format_m1(tokenI, Emulator32bit::_op_swpb);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_swpb(word instruction) {
	return disassemble_format_m1(instruction, "swpb");
}

void Assembler::_ldrh(int& tokenI) {
	word instruction = parse_format_m(tokenI, Emulator32bit::_op_ldrh);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_ldrh(word instruction) {
	return disassemble_format_m(instruction, "ldrh");
}

void Assembler::_strh(int& tokenI) {
	word instruction = parse_format_m(tokenI, Emulator32bit::_op_strh);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_strh(word instruction) {
	return disassemble_format_m(instruction, "strh");
}

void Assembler::_swph(int& tokenI) {
	word instruction = parse_format_m1(tokenI, Emulator32bit::_op_swph);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_swph(word instruction) {
	return disassemble_format_m1(instruction, "swph");
}

void Assembler::_b(int& tokenI) {
	word instruction = parse_format_b1(tokenI, Emulator32bit::_op_b);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_b(word instruction) {
	return disassemble_format_b1(instruction, "b");
}

void Assembler::_bl(int& tokenI) {
	word instruction = parse_format_b1(tokenI, Emulator32bit::_op_bl);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_bl(word instruction) {
	return disassemble_format_b1(instruction, "bl");
}

void Assembler::_bx(int& tokenI) {
	word instruction = parse_format_b2(tokenI, Emulator32bit::_op_bx);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_bx(word instruction) {
	return disassemble_format_b2(instruction, "bx");
}

void Assembler::_blx(int& tokenI) {
	word instruction = parse_format_b2(tokenI, Emulator32bit::_op_blx);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_blx(word instruction) {
	return disassemble_format_b2(instruction, "blx");
}

void Assembler::_swi(int& tokenI) {
	word instruction = parse_format_b1(tokenI, Emulator32bit::_op_swi);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_swi(word instruction) {
	return disassemble_format_b1(instruction, "swi");
}


void Assembler::_ret(int& tokenI) {
	tokenI++;

	std::vector<Tokenizer::Token> insert = {
		Tokenizer::Token(Tokenizer::Type::INSTRUCTION_BX, "bx"),
		Tokenizer::Token(Tokenizer::Type::WHITESPACE_SPACE, " "),
		Tokenizer::Token(Tokenizer::Type::REGISTER_X29, "x29"),
	};

	m_tokens.insert(m_tokens.begin() + tokenI, insert.begin(), insert.end());
}

void Assembler::_adrp(int& tokenI) {
	word instruction = parse_format_m2(tokenI, Emulator32bit::_op_adrp);
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_adrp(word instruction) {
	return disassemble_format_m2(instruction, "adrp");
}

void Assembler::_hlt(int& tokenI) {
	consume(tokenI);
	word instruction = Emulator32bit::asm_hlt();
	text_section.push_back(instruction);
}

std::string Assembler::disassemble_hlt(word instruction) {
	return "hlt";
}