#include <assembler/assembler.h>

#include <emulator32bit/emulator32bit.h>
#include <emulator32bit/emulator32bit_util.h>
#include <util/loggerv2.h>

#include <string>

#define UNUSED(x) (void)(x)

byte Assembler::parse_register(size_t& tok_i)
{
	expect_token(tok_i, Tokenizer::REGISTERS, "Assembler::parse_register() - Expected register identifier. Got " + m_tokens[tok_i].value);
	Tokenizer::Type type = consume(tok_i).type;

	return type - Tokenizer::Type::REGISTER_X0;							/* register order is assumed to be x0-x29, sp, xzr */
}

void Assembler::parse_shift(size_t& tok_i, int& shift, int& shift_amt)
{
	expect_token(tok_i, {Tokenizer::INSTRUCTION_LSL, Tokenizer::INSTRUCTION_LSR, Tokenizer::INSTRUCTION_ASR, Tokenizer::INSTRUCTION_ROR}, "Assembler::parse_shift() - Expected shift.");

	if (is_token(tok_i, {Tokenizer::INSTRUCTION_LSL})) {
		shift = LSL;
	} else if (is_token(tok_i, {Tokenizer::INSTRUCTION_LSR})) {
		shift = LSR;
	} else if (is_token(tok_i, {Tokenizer::INSTRUCTION_ASR})) {
		shift = ASR;
	} else if (is_token(tok_i, {Tokenizer::INSTRUCTION_ROR})) {
		shift = ROR;
	}
	consume(tok_i);
	skip_tokens(tok_i, "[ \t]");

	expect_token(tok_i, (std::set<Tokenizer::Type>) {Tokenizer::NUMBER_SIGN}, "Assembler::parse_shift() - Expected numeric argument.");
	consume(tok_i);
	shift_amt = parse_expression(tok_i);								/* note, in future, we could change this to create relocation record instead */

	EXPECT_TRUE_SS(shift_amt < (1<<5), std::stringstream() << "Assembler::parse_shift() - Shift amount must fit in 5 bits. Expected < 32, Got: " << shift_amt);
}

Emulator32bit::ConditionCode get_cond_code(Tokenizer::Type type)
{
	switch(type) {
		case Tokenizer::Type::CONDITION_EQ:
			return Emulator32bit::ConditionCode::EQ;
		case Tokenizer::Type::CONDITION_NE:
			return Emulator32bit::ConditionCode::NE;
		case Tokenizer::Type::CONDITION_CS:
			return Emulator32bit::ConditionCode::CS;
		case Tokenizer::Type::CONDITION_HS:
			return Emulator32bit::ConditionCode::HS;
		case Tokenizer::Type::CONDITION_CC:
			return Emulator32bit::ConditionCode::CC;
		case Tokenizer::Type::CONDITION_LO:
			return Emulator32bit::ConditionCode::LO;
		case Tokenizer::Type::CONDITION_MI:
			return Emulator32bit::ConditionCode::MI;
		case Tokenizer::Type::CONDITION_PL:
			return Emulator32bit::ConditionCode::PL;
		case Tokenizer::Type::CONDITION_VS:
			return Emulator32bit::ConditionCode::VS;
		case Tokenizer::Type::CONDITION_VC:
			return Emulator32bit::ConditionCode::VC;
		case Tokenizer::Type::CONDITION_HI:
			return Emulator32bit::ConditionCode::HI;
		case Tokenizer::Type::CONDITION_LS:
			return Emulator32bit::ConditionCode::LS;
		case Tokenizer::Type::CONDITION_GE:
			return Emulator32bit::ConditionCode::GE;
		case Tokenizer::Type::CONDITION_LT:
			return Emulator32bit::ConditionCode::LT;
		case Tokenizer::Type::CONDITION_GT:
			return Emulator32bit::ConditionCode::GT;
		case Tokenizer::Type::CONDITION_LE:
			return Emulator32bit::ConditionCode::LE;
		case Tokenizer::Type::CONDITION_AL:
			return Emulator32bit::ConditionCode::AL;
		case Tokenizer::Type::CONDITION_NV:
			return Emulator32bit::ConditionCode::NV;
		default:
			return Emulator32bit::ConditionCode::NV;
	}
}

word Assembler::parse_format_b1(size_t& tok_i, byte opcode)
{
	consume(tok_i);

	Emulator32bit::ConditionCode condition = Emulator32bit::ConditionCode::AL;
	if (is_token(tok_i, {Tokenizer::PERIOD})) {
		consume(tok_i);
		expect_token(tok_i, Tokenizer::CONDITIONS, "Assembler::parse_format_b1() - Expected condition code to follow period.");
		condition = (Emulator32bit::ConditionCode) get_cond_code(consume(tok_i).type);
	}

	sword value = 0;
	skip_tokens(tok_i, "[ \t]");
	if (is_token(tok_i, {Tokenizer::SYMBOL})) {
		std::string symbol = consume(tok_i).value;
		m_obj.add_symbol(symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK, -1);

		m_obj.rel_text.push_back((ObjectFile::RelocationEntry) {
			.offset = (word) (m_obj.text_section.size() * 4),
			.symbol = m_obj.string_table[symbol],
			.type = ObjectFile::RelocationEntry::Type::R_EMU32_B_OFFSET22,
			.shift = 0,													/* Support shift in future */
			.token = tok_i
		});
	} else {
		word imm = parse_expression(tok_i);
		EXPECT_TRUE_SS(imm < (1 << 24), std::stringstream() << "Assembler::parse_format_b1() - Expected immediate to be 24 bits");
		EXPECT_TRUE_SS((imm & 0b11) == 0, std::stringstream() << "Assembler::parse_format_b1() - Expected immediate to be 4 byte aligned");
		value = bitfield_s32(imm, 0, 24) >> 2;
	}

	return Emulator32bit::asm_format_b1(opcode, condition, value);
}

word Assembler::parse_format_b2(size_t& tok_i, byte opcode)
{
	consume(tok_i);

	Emulator32bit::ConditionCode condition = Emulator32bit::ConditionCode::AL;
	if (is_token(tok_i, {Tokenizer::PERIOD})) {
		consume(tok_i);
		expect_token(tok_i, Tokenizer::CONDITIONS, "Assembler::parse_format_b1() - Expected condition code to follow period.");
		condition = (Emulator32bit::ConditionCode) get_cond_code(consume(tok_i).type);
	}
	skip_tokens(tok_i, "[ \t]");

	byte reg = parse_register(tok_i);
	return Emulator32bit::asm_format_b2(opcode, condition, reg);
}

word Assembler::parse_format_m2(size_t& tok_i, byte opcode)
{
	consume(tok_i);
	skip_tokens(tok_i, "[ \t]");

	byte reg = parse_register(tok_i);
	skip_tokens(tok_i, "[ \t]");

	expect_token(tok_i, (std::set<Tokenizer::Type>) {Tokenizer::COMMA}, "Assembler::parse_format_m2() - Expected second argument.");
	consume(tok_i);
	skip_tokens(tok_i, "[ \t]");

	expect_token(tok_i, {Tokenizer::NUMBER_SIGN}, "Assembler::parse_format_m2() - Expected numeric operand");
	consume(tok_i);

	skip_tokens(tok_i, "[ \t]");
	if (is_token(tok_i, {Tokenizer::RELOCATION_EMU32_ADRP_HI20})) {
		consume(tok_i);
		skip_tokens(tok_i, "[ \t]");
	}

	expect_token(tok_i, {Tokenizer::SYMBOL}, "Assembler::parse_format_m2() - Expected symbol.");
	std::string symbol = consume(tok_i).value;
	m_obj.add_symbol(symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK, -1);

	m_obj.rel_text.push_back((ObjectFile::RelocationEntry)
	{
		.offset = (word) (m_obj.text_section.size() * 4),
		.symbol = m_obj.string_table[symbol],
		.type = ObjectFile::RelocationEntry::Type::R_EMU32_ADRP_HI20,
		.shift = 0,														/* Support shift in future */
		.token = tok_i,
	});

	return Emulator32bit::asm_format_m2(opcode, reg, 0);
}

word Assembler::parse_format_m1(size_t& tok_i, byte opcode)
{
	consume(tok_i);
	skip_tokens(tok_i, "[ \t]");

	byte reg_t = parse_register(tok_i);
	skip_tokens(tok_i, "[ \t]");

	expect_token(tok_i, (std::set<Tokenizer::Type>) {Tokenizer::COMMA}, "Assembler::parse_format_m1() - Expected second argument.");
	consume(tok_i);
	skip_tokens(tok_i, "[ \t]");
	byte reg_n = parse_register(tok_i);
	skip_tokens(tok_i, "[ \t]");

	expect_token(tok_i, (std::set<Tokenizer::Type>) {Tokenizer::COMMA}, "Assembler::parse_format_m1() - Expected third argument.");
	consume(tok_i);
	expect_token(tok_i, (std::set<Tokenizer::Type>) {Tokenizer::OPEN_BRACKET}, "Assembler::parse_format_m1() - Expected open bracket.");
	consume(tok_i);
	byte reg_m = parse_register(tok_i);
	skip_tokens(tok_i, "[ \t]");
	expect_token(tok_i, (std::set<Tokenizer::Type>) {Tokenizer::OPEN_BRACKET}, "Assembler::parse_format_m1() - Expected close bracket.");
	consume(tok_i);

	return Emulator32bit::asm_format_m1(opcode, reg_t, reg_n, reg_m);
}

word Assembler::parse_format_m(size_t& tok_i, byte opcode)
{
	std::string op = consume(tok_i).value;
	bool sign = op.size() > 3 ? op.at(3) == 's' : false;				/* ex: whether the value to be loaded/stored should be interpreted as signed */
	skip_tokens(tok_i, "[ \t]");

	byte reg_t = parse_register(tok_i);								/* target register. For reads, stores read value; for writes, stores write value */
	skip_tokens(tok_i, "[ \t]");

	expect_token(tok_i, (std::set<Tokenizer::Type>) {Tokenizer::COMMA}, "Assembler::parse_format_m() - Expected second argument.");
	consume(tok_i);
	skip_tokens(tok_i, "[ \t]");

	expect_token(tok_i, {Tokenizer::OPEN_BRACKET}, "Assembler::parse_format_m() - Expected open bracket");
	consume(tok_i);
	skip_tokens(tok_i, "[ \t]");

	byte reg_a = parse_register(tok_i);								/* register that contains memory address */
	skip_tokens(tok_i, "[ \t]");

	int addressing_mode = -1;											/* parse the address mode, -1 indicates invalid address mode */
	if (is_token(tok_i, {Tokenizer::CLOSE_BRACKET})) {					/* post indexed, offset is applied to value at register after accessing */
		consume(tok_i);
		skip_tokens(tok_i, "[ \t]");
		addressing_mode = M_POST;
	}

	if (is_token(tok_i, {Tokenizer::COMMA})) {							/* check for offset */
		consume(tok_i);
		skip_tokens(tok_i, "[ \t]");
		if (is_token(tok_i, {Tokenizer::NUMBER_SIGN})) {				/* offset begins with the '#' symbol */
			consume(tok_i);
			skip_tokens(tok_i, "[ \t]");
			word offset = parse_expression(tok_i);
			EXPECT_TRUE_SS(offset < (1<<12), std::stringstream() << "Assembler::parse_format_m() - Offset must be 12 bit value.");

			skip_tokens(tok_i, "[ \t]");
			expect_token(tok_i, (std::set<Tokenizer::Type>) {Tokenizer::CLOSE_BRACKET}, "Assembler::parse_format_m() - Expected close bracket.");
			consume(tok_i);

			if (addressing_mode == -1) {								/* only update addressing mode if not yet determined.
																		   This reduces code repetition, since the way offsets are calculated
																		   are the same for all addressing modes, but differ soley in arrangement */
				if (is_token(tok_i, {Tokenizer::OPERATOR_LOGICAL_NOT})) {
					consume(tok_i);
					addressing_mode = M_PRE;							/* preindexed, offset is applied to value at register before accessing */
				} else {
					addressing_mode = M_OFFSET;							/* simple offset */
				}
			}

			return Emulator32bit::asm_format_m(opcode, sign, reg_t, reg_a, offset, addressing_mode);
		}

		expect_token(tok_i, Tokenizer::REGISTERS, "Assembler::parse_format_m() - Expected register argument.");
		byte reg_b = parse_register(tok_i);							/* since there is a comma, there is another argument that is not the above checked offset */
		int shift = 0;
		int shift_amount = 0;
		skip_tokens(tok_i, "[ \t]");
		if (is_token(tok_i, {Tokenizer::COMMA})) {						/* shift argument */
			consume(tok_i);
			skip_tokens(tok_i, "[ \t]");
			parse_shift(tok_i, shift, shift_amount);
		}

		skip_tokens(tok_i, "[ \t]");
		expect_token(tok_i, (std::set<Tokenizer::Type>) {Tokenizer::CLOSE_BRACKET}, "Assembler::parse_format_m() - Expected close bracket.");
		consume(tok_i);

		if (addressing_mode == -1) {									/* same logic as above, only update addressing mode if not yet determined */
			if (is_token(tok_i, {Tokenizer::OPERATOR_LOGICAL_NOT})) {
				consume(tok_i);
				addressing_mode = M_PRE;								/* preindexed */
			} else {
				addressing_mode = M_OFFSET;								/* simple offset */
			}
		}

		return Emulator32bit::asm_format_m(opcode, sign, reg_t, reg_a, reg_b, shift, shift_amount, addressing_mode);
	}

	/* check for invalid addressing mode */
	EXPECT_FALSE_SS(addressing_mode == -1, std::stringstream() << "Assembler::parse_format_m() - Invalid addressing mode.");
	return Emulator32bit::asm_format_m(opcode, sign, reg_t, reg_a, 0, addressing_mode);
}

word Assembler::parse_format_o3(size_t& tok_i, byte opcode)
{
	// todo, make sure to handle relocation
	bool s = consume(tok_i).value.back() == 's';
	skip_tokens(tok_i, "[ \t]");

	byte reg1 = parse_register(tok_i);
	skip_tokens(tok_i, "[ \t]");

	expect_token(tok_i, {Tokenizer::COMMA}, "Assembler::parse_format_o3() - Expected comma.");
	consume(tok_i);
	skip_tokens(tok_i, "[ \t]");

	if (is_token(tok_i, {Tokenizer::REGISTERS})) {						/* In future, support relocation for immediate value */
		byte operand_reg = parse_register(tok_i);
		skip_tokens(tok_i, "[ \t]");

		word value = 0;
		if (is_token(tok_i, (std::set<Tokenizer::Type>) {Tokenizer::NUMBER_SIGN})) {
			consume(tok_i);
			value = parse_expression(tok_i);
		}

		return Emulator32bit::asm_format_o3(opcode, s, reg1, operand_reg, value);
	} else {
		if (is_token(tok_i, {Tokenizer::RELOCATION_EMU32_MOV_HI13, Tokenizer::RELOCATION_EMU32_MOV_LO19})) {
			Tokenizer::Type relocation = consume(tok_i).type;
			skip_tokens(tok_i, "[ \t]");
			expect_token(tok_i, (std::set<Tokenizer::Type>){Tokenizer::SYMBOL}, "Assembler::parse_format_o() - Expected symbol to follow relocation.");
			std::string symbol = consume(tok_i).value;
			m_obj.add_symbol(symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK, -1);

			m_obj.rel_text.push_back((ObjectFile::RelocationEntry) {
				.offset = (word) (m_obj.text_section.size() * 4),
				.symbol = m_obj.string_table[symbol],
				.type = (relocation == Tokenizer::RELOCATION_EMU32_MOV_HI13 ? ObjectFile::RelocationEntry::Type::R_EMU32_MOV_HI13 : ObjectFile::RelocationEntry::Type::R_EMU32_MOV_LO19),
				.shift = 0,												/* Support shift in future */
				.token = tok_i,
			});

			return Emulator32bit::asm_format_o3(opcode, s, reg1, 0);
		} else {
			expect_token(tok_i, (std::set<Tokenizer::Type>) {Tokenizer::NUMBER_SIGN}, "Assembler::parse_format_o() - Expected numeric argument.");
			consume(tok_i);
			word imm = parse_expression(tok_i);

			EXPECT_TRUE_SS(imm < (1<<14), std::stringstream() << "Assembler::parse_format_o() - Immediate value must be a 14 bit number.");
			return Emulator32bit::asm_format_o3(opcode, s, reg1, imm);
		}
	}

	return 0;
}

word Assembler::parse_format_o2(size_t& tok_i, byte opcode)
{
	bool s = consume(tok_i).value.back() == 's';
	skip_tokens(tok_i, "[ \t]");

	byte reg1 = parse_register(tok_i);
	skip_tokens(tok_i, "[ \t]");

	expect_token(tok_i, {Tokenizer::COMMA}, "Assembler::parse_format_o2() - Expected comma.");
	consume(tok_i);
	skip_tokens(tok_i, "[ \t]");

	byte reg2 = parse_register(tok_i);
	skip_tokens(tok_i, "[ \t]");

	expect_token(tok_i, {Tokenizer::COMMA}, "Assembler::parse_format_o2() - Expected comma.");
	consume(tok_i);
	skip_tokens(tok_i, "[ \t]");

	byte operand_reg1 = parse_register(tok_i);
	skip_tokens(tok_i, "[ \t]");

	expect_token(tok_i, {Tokenizer::COMMA}, "Assembler::parse_format_o2() - Expected comma.");
	consume(tok_i);
	skip_tokens(tok_i, "[ \t]");

	byte operand_reg2 = parse_register(tok_i);

	return Emulator32bit::asm_format_o2(opcode, s, reg1, reg2, operand_reg1, operand_reg2);
}

word Assembler::parse_format_o1(size_t& tok_i, byte opcode)
{
	consume(tok_i);
	skip_tokens(tok_i, "[ \t]");

	byte reg1 = parse_register(tok_i);
	skip_tokens(tok_i, "[ \t]");

	expect_token(tok_i, {Tokenizer::COMMA}, "Assembler::parse_format_o1() - Expected comma.");
	consume(tok_i);
	skip_tokens(tok_i, "[ \t]");

	byte reg2 = parse_register(tok_i);
	skip_tokens(tok_i, "[ \t]");

	expect_token(tok_i, {Tokenizer::COMMA}, "Assembler::parse_format_o1() - Expected comma.");
	consume(tok_i);
	skip_tokens(tok_i, "[ \t]");

	if (is_token(tok_i, Tokenizer::REGISTERS)) {
		byte operand_reg = parse_register(tok_i);
		return Emulator32bit::asm_format_o1(opcode, reg1, reg2, false, operand_reg, 0);
	} else {
		expect_token(tok_i, (std::set<Tokenizer::Type>) {Tokenizer::NUMBER_SIGN}, "Assembler::parse_format_o1() - Expected numeric argument.");
		consume(tok_i);

		int shift_amt = parse_expression(tok_i);
		EXPECT_TRUE_SS(shift_amt < (1<<5), std::stringstream() << "Assembler::parse_format_o1() - Shift amount must fit in 5 bits. Expected < 32, Got: " << shift_amt);
		return Emulator32bit::asm_format_o1(opcode, reg1, reg2, true, 0, shift_amt);
	}
}

word Assembler::parse_format_o(size_t& tok_i, byte opcode)
{
	bool s = consume(tok_i).value.back() == 's';
	skip_tokens(tok_i, "[ \t]");

	byte reg1 = parse_register(tok_i);
	skip_tokens(tok_i, "[ \t]");

	expect_token(tok_i, {Tokenizer::COMMA}, "Assembler::parse_format_o() - Expected comma.");
	consume(tok_i);
	skip_tokens(tok_i, "[ \t]");

	byte reg2 = parse_register(tok_i);
	skip_tokens(tok_i, "[ \t]");

	expect_token(tok_i, {Tokenizer::COMMA}, "Assembler::parse_format_o() - Expected comma.");
	consume(tok_i);
	skip_tokens(tok_i, "[ \t]");

	if (is_token(tok_i, Tokenizer::REGISTERS)) {
		byte operand_reg = parse_register(tok_i);
		skip_tokens(tok_i, "[ \t]");

		// shift
		int shift = 0;
		int shift_amt = 0;
		if (is_token(tok_i, {Tokenizer::COMMA})) {
			consume(tok_i);
			skip_tokens(tok_i, "[ \t]");
			parse_shift(tok_i, shift, shift_amt);
		}

		return Emulator32bit::asm_format_o(opcode, s, reg1, reg2, operand_reg, shift, shift_amt);
	} else {
		word operand = 0;
		expect_token(tok_i, (std::set<Tokenizer::Type>) {Tokenizer::NUMBER_SIGN}, "Assembler::parse_format_o() - Expected numeric argument.");
		consume(tok_i);

		if (is_token(tok_i, {Tokenizer::RELOCATION_EMU32_O_LO12})) {
			consume(tok_i);
			skip_tokens(tok_i, "[ \t]");
			expect_token(tok_i, (std::set<Tokenizer::Type>){Tokenizer::SYMBOL}, "Assembler::parse_format_o() - Expected symbol to follow relocation.");
			std::string symbol = consume(tok_i).value;
			m_obj.add_symbol(symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK, -1);

			m_obj.rel_text.push_back((ObjectFile::RelocationEntry) {
				.offset = (word) (m_obj.text_section.size() * 4),
				.symbol = m_obj.string_table[symbol],
				.type = ObjectFile::RelocationEntry::Type::R_EMU32_O_LO12,
				.shift = 0,													/* Support shift in future */
				.token = tok_i,
			});

		} else if (is_token(tok_i, Tokenizer::LITERAL_NUMBERS)) {
			operand = parse_expression(tok_i);
		} else {
			m_state = Assembler::State::ASSEMBLER_ERROR;
			EXPECT_TRUE_SS(false, std::stringstream() << "Assembler::parse_format_o() - Could not parse token.");
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
 * @param 						tok_i: Reference to current token index
 */
void Assembler::_add(size_t& tok_i)
{
	word instruction = parse_format_o(tok_i, Emulator32bit::_op_add);
	m_obj.text_section.push_back(instruction);
}

/**
 * @brief
 *
 * @param 						tok_i: Reference to current token index
 */
void Assembler::_sub(size_t& tok_i)
{
	word instruction = parse_format_o(tok_i, Emulator32bit::_op_sub);
	m_obj.text_section.push_back(instruction);
}

/**
 * @brief
 *
 * @param 						tok_i: Reference to current token index
 */
void Assembler::_rsb(size_t& tok_i)
{
	word instruction = parse_format_o(tok_i, Emulator32bit::_op_rsb);
	m_obj.text_section.push_back(instruction);
}

/**
 * @brief
 *
 * @param 						tok_i: Reference to current token index
 */
void Assembler::_adc(size_t& tok_i)
{
	word instruction = parse_format_o(tok_i, Emulator32bit::_op_adc);
	m_obj.text_section.push_back(instruction);
}

/**
 * @brief
 *
 * @param 						tok_i: Reference to current token index
 */
void Assembler::_sbc(size_t& tok_i)
{
	word instruction = parse_format_o(tok_i, Emulator32bit::_op_sbc);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_rsc(size_t& tok_i)
{
	word instruction = parse_format_o(tok_i, Emulator32bit::_op_rsc);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_mul(size_t& tok_i)
{
	word instruction = parse_format_o(tok_i, Emulator32bit::_op_mul);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_umull(size_t& tok_i)
{
	word instruction = parse_format_o2(tok_i, Emulator32bit::_op_umull);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_smull(size_t& tok_i)
{
	word instruction = parse_format_o2(tok_i, Emulator32bit::_op_smull);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_vabs_f32(size_t& tok_i)
{
	UNUSED(tok_i);

	EXPECT_TRUE_SS(false, std::stringstream() << "Assembler::_vabs_f32() - Instruction not implemented yet.");
}

void Assembler::_vneg_f32(size_t& tok_i)
{
	UNUSED(tok_i);

	EXPECT_TRUE_SS(false, std::stringstream() << "Assembler::_vneg_f32() - Instruction not implemented yet.");
}

void Assembler::_vsqrt_f32(size_t& tok_i)
{
	UNUSED(tok_i);

	EXPECT_TRUE_SS(false, std::stringstream() << "Assembler::_vsqrt_f32() - Instruction not implemented yet.");
}

void Assembler::_vadd_f32(size_t& tok_i)
{
	UNUSED(tok_i);

	EXPECT_TRUE_SS(false, std::stringstream() << "Assembler::_vadd_f32() - Instruction not implemented yet.");
}

void Assembler::_vsub_f32(size_t& tok_i)
{
	UNUSED(tok_i);

	EXPECT_TRUE_SS(false, std::stringstream() << "Assembler::_vsub_f32() - Instruction not implemented yet.");
}

void Assembler::_vdiv_f32(size_t& tok_i)
{
	UNUSED(tok_i);

	EXPECT_TRUE_SS(false, std::stringstream() << "Assembler::_vdiv_f32() - Instruction not implemented yet.");
}

void Assembler::_vmul_f32(size_t& tok_i)
{
	UNUSED(tok_i);

	EXPECT_TRUE_SS(false, std::stringstream() << "Assembler::_vmul_f32() - Instruction not implemented yet.");
}

void Assembler::_vcmp_f32(size_t& tok_i)
{
	UNUSED(tok_i);

	EXPECT_TRUE_SS(false, std::stringstream() << "Assembler::_vcmp_f32() - Instruction not implemented yet.");
}

void Assembler::_vsel_f32(size_t& tok_i)
{
	UNUSED(tok_i);

	EXPECT_TRUE_SS(false, std::stringstream() << "Assembler::_vsel_f32() - Instruction not implemented yet.");
}

void Assembler::_vcint_u32_f32(size_t& tok_i)
{
	UNUSED(tok_i);

	EXPECT_TRUE_SS(false, std::stringstream() << "Assembler::_vcint_u32_f32() - Instruction not implemented yet.");
}

void Assembler::_vcint_s32_f32(size_t& tok_i)
{
	UNUSED(tok_i);

	EXPECT_TRUE_SS(false, std::stringstream() << "Assembler::_vcint_s32_f32() - Instruction not implemented yet.");
}

void Assembler::_vcflo_u32_f32(size_t& tok_i)
{
	UNUSED(tok_i);

	EXPECT_TRUE_SS(false, std::stringstream() << "Assembler::_vcflo_u32_f32() - Instruction not implemented yet.");
}

void Assembler::_vcflo_s32_f32(size_t& tok_i)
{
	UNUSED(tok_i);

	EXPECT_TRUE_SS(false, std::stringstream() << "Assembler::_vcflo_s32_f32() - Instruction not implemented yet.");
}

void Assembler::_vmov_f32(size_t& tok_i)
{
	UNUSED(tok_i);

	EXPECT_TRUE_SS(false, std::stringstream() << "Assembler::_vmov_f32() - Instruction not implemented yet.");
}

void Assembler::_and(size_t& tok_i)
{
	word instruction = parse_format_o(tok_i, Emulator32bit::_op_and);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_orr(size_t& tok_i)
{
	word instruction = parse_format_o(tok_i, Emulator32bit::_op_orr);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_eor(size_t& tok_i)
{
	word instruction = parse_format_o(tok_i, Emulator32bit::_op_eor);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_bic(size_t& tok_i)
{
	word instruction = parse_format_o(tok_i, Emulator32bit::_op_bic);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_lsl(size_t& tok_i)
{
	word instruction = parse_format_o1(tok_i, Emulator32bit::_op_lsl);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_lsr(size_t& tok_i)
{
	word instruction = parse_format_o1(tok_i, Emulator32bit::_op_lsr);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_asr(size_t& tok_i)
{
	word instruction = parse_format_o1(tok_i, Emulator32bit::_op_asr);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_ror(size_t& tok_i)
{
	word instruction = parse_format_o1(tok_i, Emulator32bit::_op_ror);
	m_obj.text_section.push_back(instruction);
}

void insert_xzr(std::vector<Tokenizer::Token>& tokens, int pos)
{
	std::vector<Tokenizer::Token> insert = {
		Tokenizer::Token(Tokenizer::Type::WHITESPACE_SPACE, " "),
		Tokenizer::Token(Tokenizer::Type::REGISTER_XZR, "xzr"),
		Tokenizer::Token(Tokenizer::Type::COMMA, ","),
	};

	tokens.insert(tokens.begin() + pos, insert.begin(), insert.end());
}

void Assembler::_cmp(size_t& tok_i)
{
	insert_xzr(m_tokens, tok_i+1);

	word instruction = parse_format_o(tok_i, Emulator32bit::_op_cmp);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_cmn(size_t& tok_i)
{
	insert_xzr(m_tokens, tok_i+1);

	word instruction = parse_format_o(tok_i, Emulator32bit::_op_cmn);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_tst(size_t& tok_i)
{
	insert_xzr(m_tokens, tok_i+1);

	word instruction = parse_format_o(tok_i, Emulator32bit::_op_tst);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_teq(size_t& tok_i)
{
	insert_xzr(m_tokens, tok_i+1);

	word instruction = parse_format_o(tok_i, Emulator32bit::_op_teq);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_mov(size_t& tok_i)
{
	word instruction = parse_format_o3(tok_i, Emulator32bit::_op_mov);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_mvn(size_t& tok_i)
{
	word instruction = parse_format_o3(tok_i, Emulator32bit::_op_mvn);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_ldr(size_t& tok_i)
{
	word instruction = parse_format_m(tok_i, Emulator32bit::_op_ldr);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_str(size_t& tok_i)
{
	word instruction = parse_format_m(tok_i, Emulator32bit::_op_str);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_swp(size_t& tok_i)
{
	word instruction = parse_format_m1(tok_i, Emulator32bit::_op_swp);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_ldrb(size_t& tok_i)
{
	word instruction = parse_format_m(tok_i, Emulator32bit::_op_ldrb);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_strb(size_t& tok_i)
{
	word instruction = parse_format_m(tok_i, Emulator32bit::_op_strb);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_swpb(size_t& tok_i)
{
	word instruction = parse_format_m1(tok_i, Emulator32bit::_op_swpb);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_ldrh(size_t& tok_i)
{
	word instruction = parse_format_m(tok_i, Emulator32bit::_op_ldrh);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_strh(size_t& tok_i)
{
	word instruction = parse_format_m(tok_i, Emulator32bit::_op_strh);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_swph(size_t& tok_i)
{
	word instruction = parse_format_m1(tok_i, Emulator32bit::_op_swph);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_b(size_t& tok_i)
{
	word instruction = parse_format_b1(tok_i, Emulator32bit::_op_b);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_bl(size_t& tok_i)
{
	word instruction = parse_format_b1(tok_i, Emulator32bit::_op_bl);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_bx(size_t& tok_i)
{
	word instruction = parse_format_b2(tok_i, Emulator32bit::_op_bx);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_blx(size_t& tok_i)
{
	word instruction = parse_format_b2(tok_i, Emulator32bit::_op_blx);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_swi(size_t& tok_i)
{
	word instruction = parse_format_b1(tok_i, Emulator32bit::_op_swi);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_ret(size_t& tok_i)
{
	tok_i++;

	std::vector<Tokenizer::Token> insert = {
		Tokenizer::Token(Tokenizer::Type::INSTRUCTION_BX, "bx"),
		Tokenizer::Token(Tokenizer::Type::WHITESPACE_SPACE, " "),
		Tokenizer::Token(Tokenizer::Type::REGISTER_X29, "x29"),
	};

	m_tokens.insert(m_tokens.begin() + tok_i, insert.begin(), insert.end());
}

void Assembler::_adrp(size_t& tok_i)
{
	word instruction = parse_format_m2(tok_i, Emulator32bit::_op_adrp);
	m_obj.text_section.push_back(instruction);
}

void Assembler::_hlt(size_t& tok_i)
{
	consume(tok_i);
	word instruction = Emulator32bit::asm_hlt();
	m_obj.text_section.push_back(instruction);
}