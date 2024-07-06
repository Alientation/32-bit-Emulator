#include <assembler/Assembler.h>

#include <emulator32bit/Emulator32bit.h>
#include <emulator32bit/Emulator32bitUtil.h>
#include <util/Logger.h>

#include <string>

byte Assembler::parse_register(int& tokenI) {
	expectToken(tokenI, Tokenizer::REGISTERS, "Assembler::parse_register() - Expected register identifier.");
	Tokenizer::Type type = consume(tokenI).type;

	return type - Tokenizer::Type::REGISTER_X0;							/* register order is assumed to be x0-x29, sp, xzr */
}

word Assembler::parse_format_o3(int& tokenI, byte opcode) {
	// todo, make sure to handle relocation
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
		int shift_amt = parse_expression(tokenI);
		EXPECT_TRUE(shift_amt < (1<<5), lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::parse_format_o1() - Shift amount must fit in 5 bits. Expected < 32, Got: " << shift_amt);
		return Emulator32bit::asm_format_o1(opcode, reg1, reg2, true, 0, shift_amt);
	} else {
		byte operand_reg = parse_register(tokenI);
		return Emulator32bit::asm_format_o1(opcode, reg1, reg2, false, operand_reg, 0);
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
		if (isToken(tokenI, {Tokenizer::INSTRUCTION_LSL, Tokenizer::INSTRUCTION_LSR, Tokenizer::INSTRUCTION_ASR, Tokenizer::INSTRUCTION_ROR})) {
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
			shift_amt = parse_expression(tokenI);							/* note, in future, we could change this to create relocation record instead */

			EXPECT_TRUE(shift_amt < (1<<5), lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::parse_format_o() - Shift amount must fit in 5 bits. Expected < 32, Got: " << shift_amt);
		}

		return Emulator32bit::asm_format_o(opcode, s, reg1, reg2, operand_reg, shift, shift_amt);
	} else {
		word operand = 0;
		expectToken(tokenI, (std::set<Tokenizer::Type>) {Tokenizer::NUMBER_SIGN}, "Assembler::parse_format_o() - Expected numeric argument.");
		consume(tokenI);

		if (isToken(tokenI, {Tokenizer::RELOCATION_EMU32_O_LO12})) {
			skipTokens(tokenI, "[ \t]");
			expectToken(tokenI, (std::set<Tokenizer::Type>){Tokenizer::SYMBOL}, "Assembler::parse_format_o() - Expected symbol to follow relocation.");
			std::string symbol = consume(tokenI).value;
			add_symbol(symbol, 0, SymbolTableEntry::BindingInfo::WEAK, -1);

			rel_text.push_back((RelocationEntry) {
				.offset = (word) (text_section.size() * 4),
				.symbol = string_table[symbol],
				.type = RelocationEntry::Type::R_EMU32_O_LO12,
				.shift = 0,												/* Support shift in future */
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

/**
 * @brief
 *
 * @param 						tokenI: Reference to current token index
 */
void Assembler::_sub(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_sub);
	text_section.push_back(instruction);
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

/**
 * @brief
 *
 * @param 						tokenI: Reference to current token index
 */
void Assembler::_adc(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_adc);
	text_section.push_back(instruction);
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

void Assembler::_rsc(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_rsc);
	text_section.push_back(instruction);
}

void Assembler::_mul(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_mul);
	text_section.push_back(instruction);
}

void Assembler::_umull(int& tokenI) {
	word instruction = parse_format_o2(tokenI, Emulator32bit::_op_umull);
	text_section.push_back(instruction);
}

void Assembler::_smull(int& tokenI) {
	word instruction = parse_format_o2(tokenI, Emulator32bit::_op_smull);
	text_section.push_back(instruction);
}

void Assembler::_vabs_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vabs_f32() - Instruction not implemented yet.");
}

void Assembler::_vneg_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vneg_f32() - Instruction not implemented yet.");
}

void Assembler::_vsqrt_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vsqrt_f32() - Instruction not implemented yet.");
}

void Assembler::_vadd_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vadd_f32() - Instruction not implemented yet.");
}

void Assembler::_vsub_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vsub_f32() - Instruction not implemented yet.");
}

void Assembler::_vdiv_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vdiv_f32() - Instruction not implemented yet.");
}

void Assembler::_vmul_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vmul_f32() - Instruction not implemented yet.");
}

void Assembler::_vcmp_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vcmp_f32() - Instruction not implemented yet.");
}

void Assembler::_vsel_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vsel_f32() - Instruction not implemented yet.");
}

void Assembler::_vcint_u32_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vcint_u32_f32() - Instruction not implemented yet.");
}

void Assembler::_vcint_s32_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vcint_s32_f32() - Instruction not implemented yet.");
}

void Assembler::_vcflo_u32_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vcflo_u32_f32() - Instruction not implemented yet.");
}

void Assembler::_vcflo_s32_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vcflo_s32_f32() - Instruction not implemented yet.");
}

void Assembler::_vmov_f32(int& tokenI) {
	EXPECT_TRUE(false, lgr::Logger::LogType::ERROR, std::stringstream() << "Assembler::_vmov_f32() - Instruction not implemented yet.");
}

void Assembler::_and(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_and);
	text_section.push_back(instruction);
}

void Assembler::_orr(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_orr);
	text_section.push_back(instruction);
}

void Assembler::_eor(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_eor);
	text_section.push_back(instruction);
}

void Assembler::_bic(int& tokenI) {
	word instruction = parse_format_o(tokenI, Emulator32bit::_op_bic);
	text_section.push_back(instruction);
}

void Assembler::_lsl(int& tokenI) {
	word instruction = parse_format_o1(tokenI, Emulator32bit::_op_lsl);
	text_section.push_back(instruction);
}

void Assembler::_lsr(int& tokenI) {
	word instruction = parse_format_o1(tokenI, Emulator32bit::_op_lsr);
	text_section.push_back(instruction);
}

void Assembler::_asr(int& tokenI) {
	word instruction = parse_format_o1(tokenI, Emulator32bit::_op_asr);
	text_section.push_back(instruction);
}

void Assembler::_ror(int& tokenI) {
	word instruction = parse_format_o1(tokenI, Emulator32bit::_op_ror);
	text_section.push_back(instruction);
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

void Assembler::_cmn(int& tokenI) {
	insert_xzr(m_tokens, tokenI+1);

	word instruction = parse_format_o(tokenI, Emulator32bit::_op_cmn);
	text_section.push_back(instruction);
}

void Assembler::_tst(int& tokenI) {
	insert_xzr(m_tokens, tokenI+1);

	word instruction = parse_format_o(tokenI, Emulator32bit::_op_tst);
	text_section.push_back(instruction);
}

void Assembler::_teq(int& tokenI) {
	insert_xzr(m_tokens, tokenI+1);

	word instruction = parse_format_o(tokenI, Emulator32bit::_op_teq);
	text_section.push_back(instruction);
}

void Assembler::_mov(int& tokenI) {

}

void Assembler::_mvn(int& tokenI) {

}

void Assembler::_ldr(int& tokenI) {

}

void Assembler::_str(int& tokenI) {

}

void Assembler::_swp(int& tokenI) {

}

void Assembler::_ldrb(int& tokenI) {

}

void Assembler::_strb(int& tokenI) {

}

void Assembler::_swpb(int& tokenI) {

}

void Assembler::_ldrh(int& tokenI) {

}

void Assembler::_strh(int& tokenI) {

}

void Assembler::_swph(int& tokenI) {

}

void Assembler::_b(int& tokenI) {

}

void Assembler::_bl(int& tokenI) {

}

void Assembler::_bx(int& tokenI) {

}

void Assembler::_blx(int& tokenI) {

}

void Assembler::_swi(int& tokenI) {

}
