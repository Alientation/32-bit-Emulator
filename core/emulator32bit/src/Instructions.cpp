#define AEMU_ONLY_CRITICAL_LOG

#include <emulator32bit/emulator32bit.h>
#include <util/loggerv2.h>

#include <string>

/**
 * @internal
 * @brief 					Useful macros to extract information from instruction bits
 * @hideinitializer
 *
 */
#define _X1(instr) (bitfield_u32(instr, 20, 5))				/*! bits 20 to 24 */
#define _X2(instr) (bitfield_u32(instr, 15, 5))				/*! bits 15 to 19 */
#define _X3(instr) (bitfield_u32(instr, 9, 5))				/*! bits 9 to 14 */
#define _X4(instr) (bitfield_u32(instr, 4, 5))				/*! bits 4 to 9 */

#define UNUSED(x) (void)(x)

/**
 * @internal
 * @brief 					Calculates the new value after applying the specified shift
 *
 * @param 					val: value to shift
 * @param 					shift_type: shift specified by the instruction
 * @param 					imm5: shift amount
 * @return 					shifted value
 *
 */
static word calc_shift(word val, byte shift_type, byte imm5)
{
	switch(shift_type) {
		case 0b00:											/*! LSL */
			DEBUG_SS(std::stringstream() << "LSL " << std::to_string((word)imm5));
			val <<= imm5;
			break;
		case 0b01:											/*! LSR */
			DEBUG_SS(std::stringstream() << "LSR " << std::to_string((word)imm5));
			val >>= imm5;
			break;
		case 0b10: 											/*! ASR */
			DEBUG_SS(std::stringstream() << "ASR " << std::to_string((word)imm5));
			val = ((signed int) val) >> imm5;
			break;
		case 0b11: 											/*! ROR */
		{
			DEBUG_SS(std::stringstream() << "ROR " << std::to_string((word)imm5));
			word rot_bits = val & ((1 << imm5) - 1);
			rot_bits <<= (WORD_BITS - imm5);
			val >>= imm5;
			val &= (1 << (WORD_BITS - imm5)) - 1; 			/*! to be safe and remove bits that will be replaced */
			val |= rot_bits;
			break;
		}
		default:											/*! Invalid shift */
			ERROR("Invalid shift: " + val);
	}
	return val;
}

/**
 * @internal
 * @brief 					Get the carry flag after adding two values
 * @details					yoinked from https://github.com/unicorn-engine/ because I could not
 * 								figure out carry/overflow for subtraction
 *
 * @param[in]				op1: operand 1
 * @param[in]				op2: operand 2
 * @return 					carry flag
 *
 */
static bool get_c_flag_add(word op1, word op2)
{
	word dst_val = op1 + op2;
	return dst_val < op1;
}

/**
 * @internal
 * @brief 					Get the overflow flag after adding two values
 * @details					yoinked from https://github.com/unicorn-engine/ because I could not
 * 								figure out carry/overflow for subtraction
 *
 * @param[in]				op1: operand 1
 * @param[in]				op2: operand 2
 * @return 					overflow flag
 *
 */
static bool get_v_flag_add(word op1, word op2)
{
	word dst_val = op1 + op2;
	return (op1 ^ op2 ^ -1) & (op1 ^ dst_val) & (1U << 31);
}

/**
 * @internal
 * @brief 					Get the carry flag after subtracting two values
 * @details					yoinked from https://github.com/unicorn-engine/ because I could not
 * 								figure out carry/overflow for subtraction
 *
 * @param[in]				op1: operand 1
 * @param[in]				op2: operand 2
 * @return 					carry flag
 *
 */
static bool get_c_flag_sub(word op1, word op2)
{
	word dst_val = op1 - op2;
	return (((~op1 & op2) | (dst_val & (~op1 | op2))) & (1U << 31));
}

/**
 * @internal
 * @brief 					Get the overflow flag after subtracting two values
 * @details					yoinked from https://github.com/unicorn-engine/ because I could not
 * 								figure out carry/overflow for subtraction
 *
 * @param[in]				op1: operand 1
 * @param[in]				op2: operand 2
 * @return 					overflow flag
 *
 */
static bool get_v_flag_sub(word op1, word op2)
{
	word dst_val = op1 - op2;
	return (((op1 ^ op2) & (op1 ^ dst_val)) & (1U << 31));
}

/**
 * @internal
 * @brief					Parse the value of the argument for instruction format O
 * @details					Also used to parse value of argument for some other instruction format like format M which conveniently has a similar structure
 * @hideinitializer
 *
 */
#define FORMAT_O__get_arg(instr) (test_bit(instr, 14) ? bitfield_u32(instr, 0, 14) : calc_shift(read_reg(_X3(instr)), bitfield_u32(instr, 7, 2), bitfield_u32(instr, 2, 2)))

/**
 * @internal
 * @brief					A sequence of bits to add to a @ref Joiner
 *
 */
struct JPart
{
	JPart(int bits, word val = 0) :
		bits(bits), val(val)
	{

	}
	int bits;											/*! Number of bits stored in this part */
	word val;											/*! Contents of the bits stored in this part, stored with the first bit in the most significant bit */
};

/**
 * @internal
 * @brief					A value that is formed by joining @ref JPart
 *
 */
class Joiner
{
	public:
		word val = 0;									/*! Content stored so far */

		/**
		 * @internal
		 * @brief			Add a new @ref JPart
		 *
		 * @param			p: @ref JPart to add
		 * @return 			Reference to this object
		 */
		Joiner& operator<<(JPart p)
		{
			val <<= p.bits;
			val += p.val;
			return *this;
		}

		/**
		 * @internal
		 * @brief			Add filler bits all set to 0
		 *
		 * @param 			bits: Number of bits to add
		 * @return 			Reference to this object
		 */
		Joiner& operator<<(int bits)
		{
			val <<= bits;
			return *this;
		}

		/**
		 * @internal
		 * @brief 			Extract the value of this object
		 *
		 * @return 			word
		 */
		operator word() const
		{
			return val;
		}
};

/**
 * @brief					Constructs instructions of format O with an imm14 operand
 *
 * @param 					opcode: 6 bit identifier of a format O instruction
 * @param 					s: whether condition flags are set
 * @param 					xd: 5 bit destination register identifier
 * @param 					xn: 5 bit operand register identifier
 * @param 					imm14: 14 bit immediate value
 * @return 					instruction word
 */
word Emulator32bit::asm_format_o(byte opcode, bool s, int xd, int xn, int imm14)
{
	return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xd) << JPart(5, xn) << JPart(1, 1) << JPart(14, imm14);
}

/**
 * @brief 					Constructs instructions of format O with an arg operand
 *
 * @param 					opcode: 6 bit identifier of a format O instruction
 * @param 					s: whether condition flags are set
 * @param 					xd: 5 bit destination register identifier
 * @param 					xn: 5 bit operand register identifier
 * @param 					xm: 5 bit operand register identifier
 * @param 					shift: shift type to be applied on the value in the xm register
 * @param 					imm5: shift amount
 * @return 					instruction word
 */
word Emulator32bit::asm_format_o(byte opcode, bool s, int xd, int xn, int xm, int shift, int imm5)
{
	return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xd) << JPart(5, xn) << 1 << JPart(5, xm) << JPart(2, shift) << JPart(5, imm5) << 2;
}

word Emulator32bit::asm_format_o1(byte opcode, int xd, int xn, bool imm, int xm, int imm5)
{
	return Joiner() << JPart(6, opcode) << 1 << JPart(5, xd) << JPart(5, xn) << JPart(1, imm) << JPart(5, xm) << 2 << JPart(5, imm5) << 2;
}

word Emulator32bit::asm_format_o2(byte opcode, bool s, int xlo, int xhi, int xn, int xm)
{
	return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xlo) << JPart(5, xhi) << 1 << JPart(5, xn) << JPart(5, xm) << 4;
}

word Emulator32bit::asm_format_o3(byte opcode, bool s, int xd, int imm19)
{
	return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xd) << JPart(1, 1) << JPart(19, imm19);
}

word Emulator32bit::asm_format_o3(byte opcode, bool s, int xd, int xn, int imm14)
{
	return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xd) << 0 << JPart(5, xn) << JPart(14, imm14);
}

word Emulator32bit::asm_format_m(byte opcode, bool sign, int xt, int xn, int xm, int shift, int imm5, int adr)
{
	return Joiner() << JPart(6, opcode) << JPart(1, sign) << JPart(5, xt) << JPart(5, xn) << 1 << JPart(5, xm) << JPart(2, shift) << JPart(5, imm5) << JPart(2, adr);
}

word Emulator32bit::asm_format_m(byte opcode, bool sign, int xt, int xn, int simm12, int adr)
{
	return Joiner() << JPart(6, opcode) << JPart(1, sign) << JPart(5, xt) << JPart(5, xn) << JPart(1, 1) << JPart(12, simm12) << JPart(2, adr);
}

word Emulator32bit::asm_format_m1(byte opcode, int xt, int xn, int xm)
{
	return Joiner() << JPart(6, opcode) << 1 << JPart(5, xt) << JPart(5, xn) << 1 << JPart(5, xm) << 9;
}

word Emulator32bit::asm_format_m2(byte opcode, int xd, int imm20)
{
	return Joiner() << JPart(6, opcode) << 1 << JPart(5, xd) << JPart(20, imm20);
}

word Emulator32bit::asm_format_b1(byte opcode, ConditionCode cond, sword simm22)
{
	return Joiner() << JPart(6, opcode) << JPart(4, (word) cond) << JPart(22, simm22);
}

word Emulator32bit::asm_format_b2(byte opcode, ConditionCode cond, int xd)
{
	return Joiner() << JPart(6, opcode) << JPart(4, (word) cond) << JPart(5, xd) << 17;
}

void Emulator32bit::_hlt(word instr)
{
	UNUSED(instr);
	throw EmulatorException("HLT Exception");		// todo, instead, an interrupt should be raised or smthn to be handled by the kernel
}

word Emulator32bit::asm_hlt()
{
	return Joiner() << JPart(6, _op_hlt) << 26;
}

void Emulator32bit::_nop(word instr)
{
	UNUSED(instr);
	return; // do nothing
}

word Emulator32bit::asm_nop()
{
	return Joiner() << JPart(6, _op_nop) << 26;
}

void Emulator32bit::_add(word instr)
{
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr));
	word add_val = FORMAT_O__get_arg(instr);
	word dst_val = add_val + xn_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, get_c_flag_add(xn_val, add_val),
				get_v_flag_add(xn_val, add_val));
	}

	DEBUG_SS(std::stringstream() << "add " << std::to_string(add_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val));
	write_reg(xd, dst_val);
}

void Emulator32bit::_sub(word instr)
{
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr));
	word sub_val = FORMAT_O__get_arg(instr);
	word dst_val = xn_val - sub_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, get_c_flag_sub(xn_val, sub_val),
				get_v_flag_sub(xn_val, sub_val));
	}

	DEBUG_SS(std::stringstream() << "sub " << std::to_string(sub_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val));
	write_reg(xd, dst_val);
}

void Emulator32bit::_rsb(word instr)
{
	byte xd = _X1(instr);
	word sub_val = read_reg(_X2(instr));
	word xn_val = FORMAT_O__get_arg(instr);
	word dst_val = xn_val - sub_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, get_c_flag_sub(xn_val, sub_val),
				get_v_flag_sub(xn_val, sub_val));
	}

	DEBUG_SS(std::stringstream() << "rsb " << std::to_string(xn_val) << " "
			<< std::to_string(sub_val) << " = " << std::to_string(dst_val));
	write_reg(xd, dst_val);
}

void Emulator32bit::_adc(word instr)
{
	bool c = test_bit(_pstate, C_FLAG);
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr));
	word add_val = FORMAT_O__get_arg(instr);
	word dst_val = add_val + xn_val + c;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0,
				get_c_flag_add(xn_val + c, add_val) | get_c_flag_add(xn_val, c),
				get_v_flag_add(xn_val + c, add_val) | get_v_flag_add(xn_val, c));
	}

	DEBUG_SS(std::stringstream() << "adc " << std::to_string(add_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val));
	write_reg(xd, dst_val);
}

void Emulator32bit::_sbc(word instr)
{
	bool borrow = test_bit(_pstate, C_FLAG);
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr));
	word sub_val = FORMAT_O__get_arg(instr);
	word dst_val = xn_val - sub_val - borrow;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0,
				get_c_flag_sub(xn_val - borrow, sub_val) | get_c_flag_sub(xn_val, borrow),
				get_v_flag_sub(xn_val - borrow, sub_val) | get_v_flag_sub(xn_val, borrow));
	}

	DEBUG_SS(std::stringstream() << "sbc " << std::to_string(sub_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val));
	write_reg(xd, dst_val);
}

void Emulator32bit::_rsc(word instr)
{
	bool borrow = test_bit(_pstate, C_FLAG);
	byte xd = _X1(instr);
	word sub_val = read_reg(_X2(instr));
	word xn_val = FORMAT_O__get_arg(instr);
	word dst_val = xn_val - sub_val - borrow;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0,
				get_c_flag_sub(xn_val - borrow, sub_val) | get_c_flag_sub(xn_val, borrow),
				get_v_flag_sub(xn_val - borrow, sub_val) | get_v_flag_sub(xn_val, borrow));
	}

	DEBUG_SS(std::stringstream() << "rsc " << std::to_string(xn_val) << " "
			<< std::to_string(sub_val) << " = " << std::to_string(dst_val));
	write_reg(xd, dst_val);
}

void Emulator32bit::_mul(word instr)
{
	byte xd = _X1(instr);
	dword xn_val = read_reg(_X2(instr));
	dword xm_val = FORMAT_O__get_arg(instr);
	dword dst_val = xn_val * xm_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		// according to https://developer.arm.com/documentation/dui0473/m/arm-and-thumb-instructions/smull
		// arm's MUL instruction does not set carry or overflow flags
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
	}

	DEBUG_SS(std::stringstream() << "mul " << std::to_string(xn_val) << " "
			<< std::to_string(xm_val) << " = " << std::to_string(dst_val));

	write_reg(xd, (word) dst_val);
}

void Emulator32bit::_umull(word instr)
{
	byte xlo = _X1(instr);
	byte xhi = _X2(instr);
	dword xn_val = read_reg(_X3(instr));
	dword xm_val = read_reg(_X4(instr));
	dword dst_val = xn_val * xm_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		// according to https://developer.arm.com/documentation/dui0473/m/arm-and-thumb-instructions/umull
		// arm's UMULL instruction does not set carry or overflow flags
		set_NZCV(test_bit(dst_val, 63), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
	}

	DEBUG_SS(std::stringstream() << "mul " << std::to_string(xn_val) << " "
			<< std::to_string(xm_val) << " = " << std::to_string(dst_val));

	write_reg(xlo, (word) dst_val);
	write_reg(xhi, (word) (dst_val >> 32));
}

void Emulator32bit::_smull(word instr)
{
	byte xlo = _X1(instr);
	byte xhi = _X2(instr);
	signed long long xn_val = ((signed long long) read_reg(_X3(instr))) << 32 >> 32;
	signed long long xm_val = ((signed long long) read_reg(_X4(instr))) << 32 >> 32;
	signed long long dst_val = xn_val * xm_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		// according to https://developer.arm.com/documentation/dui0489/c/arm-and-thumb-instructions/multiply-instructions/mul--mla--and-mls
		// arm's UMULL instruction does not set carry or overflow flags
		set_NZCV(test_bit(dst_val, 63), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
	}

	DEBUG_SS(std::stringstream() << "mul " << std::to_string(xn_val) << " "
			<< std::to_string(xm_val) << " = " << std::to_string(dst_val));

	write_reg(xlo, (word) dst_val);
	write_reg(xhi, (word) (dst_val >> 32));
}

// todo WILL DO LATER JUST NOT NOW
void Emulator32bit::_vabs_f32(word instr)
{
	UNUSED(instr);

}

void Emulator32bit::_vneg_f32(word instr)
{
	UNUSED(instr);

}

void Emulator32bit::_vsqrt_f32(word instr)
{
	UNUSED(instr);

}

void Emulator32bit::_vadd_f32(word instr)
{
	UNUSED(instr);

}

void Emulator32bit::_vsub_f32(word instr)
{
	UNUSED(instr);

}

void Emulator32bit::_vdiv_f32(word instr)
{
	UNUSED(instr);

}

void Emulator32bit::_vmul_f32(word instr)
{
	UNUSED(instr);

}

void Emulator32bit::_vcmp_f32(word instr)
{
	UNUSED(instr);

}

void Emulator32bit::_vsel_f32(word instr)
{
	UNUSED(instr);

}

void Emulator32bit::_vcint_f32(word instr)
{
	UNUSED(instr);

}

void Emulator32bit::_vcflo_f32(word instr)
{
	UNUSED(instr);

}

void Emulator32bit::_vmov_f32(word instr)
{
	UNUSED(instr);

}


void Emulator32bit::_and(word instr)
{
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr));
	word and_val = FORMAT_O__get_arg(instr);
	word dst_val = and_val & xn_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		// https://developer.arm.com/documentation/dui0489/h/arm-and-thumb-instructions/and--orr--eor--bic--and-orn
		// N and Z flags are set based of the result, C flag may be set based of the calculation for the second operand
		// but will ignore for now
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
	}

	DEBUG_SS(std::stringstream() << "and " << std::to_string(and_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val));
	write_reg(xd, dst_val);
}

void Emulator32bit::_orr(word instr)
{
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr));
	word or_val = FORMAT_O__get_arg(instr);
	word dst_val = or_val | xn_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		// https://developer.arm.com/documentation/dui0489/h/arm-and-thumb-instructions/and--orr--eor--bic--and-orn
		// N and Z flags are set based of the result, C flag may be set based of the calculation for the second operand
		// but will ignore for now
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
	}

	DEBUG_SS(std::stringstream() << "orr " << std::to_string(or_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val));
	write_reg(xd, dst_val);
}

void Emulator32bit::_eor(word instr)
{
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr));
	word eor_val = FORMAT_O__get_arg(instr);
	word dst_val = eor_val ^ xn_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		// https://developer.arm.com/documentation/dui0489/h/arm-and-thumb-instructions/and--orr--eor--bic--and-orn
		// N and Z flags are set based of the result, C flag may be set based of the calculation for the second operand
		// but will ignore for now
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
	}

	DEBUG_SS(std::stringstream() << "eor " << std::to_string(eor_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val));
	write_reg(xd, dst_val);
}

void Emulator32bit::_bic(word instr)
{
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr));
	word bic_val = FORMAT_O__get_arg(instr);
	word dst_val = (~bic_val) & xn_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		// https://developer.arm.com/documentation/dui0489/h/arm-and-thumb-instructions/and--orr--eor--bic--and-orn
		// N and Z flags are set based of the result, C flag may be set based of the calculation for the second operand
		// but will ignore for now
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
	}

	DEBUG_SS(std::stringstream() << "bic " << std::to_string(bic_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val));
	write_reg(xd, dst_val);
}

void Emulator32bit::_lsl(word instr)
{
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr));
	word lsl_val = test_bit(instr, 14) ? bitfield_u32(instr, 2, 5) : 0xFF & read_reg(_X3(instr));
	word dst_val = xn_val << lsl_val;

	DEBUG_SS(std::stringstream() << "lsl " << std::to_string(lsl_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val));
	write_reg(xd, dst_val);
}

void Emulator32bit::_lsr(word instr)
{
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr));
	word lsl_val = test_bit(instr, 14) ? bitfield_u32(instr, 2, 5) : 0xFF & read_reg(_X3(instr));
	word dst_val = xn_val >> lsl_val;

	DEBUG_SS(std::stringstream() << "lsr " << std::to_string(lsl_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val));
	write_reg(xd, dst_val);
}

void Emulator32bit::_asr(word instr)
{
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr));
	word lsl_val = test_bit(instr, 14) ? bitfield_u32(instr, 2, 5) : 0xFF & read_reg(_X3(instr));
	word dst_val = ((sword) xn_val) >> lsl_val;

	DEBUG_SS(std::stringstream() << "asr " << std::to_string(lsl_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val));
	write_reg(xd, dst_val);
}

void Emulator32bit::_ror(word instr)
{
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr));
	word lsl_val = test_bit(instr, 14) ? bitfield_u32(instr, 2, 5) : 0xFF & read_reg(_X3(instr));
	word dst_val = (xn_val >> lsl_val) | (bitfield_u32(xn_val, 0, lsl_val) << (32 - lsl_val));

	DEBUG_SS(std::stringstream() << "ror " << std::to_string(lsl_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val));
	write_reg(xd, dst_val);
}

// alias to subs
void Emulator32bit::_cmp(word instr)
{
	word xn_val = read_reg(_X2(instr));
	word cmp_val = FORMAT_O__get_arg(instr);
	word dst_val = xn_val - cmp_val;

	set_NZCV(test_bit(dst_val, 31), dst_val == 0, get_c_flag_sub(xn_val, cmp_val),
				get_v_flag_sub(xn_val, cmp_val));

	DEBUG_SS(std::stringstream() << "cmp " << std::to_string(cmp_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val));
}

// alias to adds
void Emulator32bit::_cmn(word instr)
{
	word xn_val = read_reg(_X2(instr));
	word cmn_val = FORMAT_O__get_arg(instr);
	word dst_val = cmn_val + xn_val;

	set_NZCV(test_bit(dst_val, 31), dst_val == 0, get_c_flag_add(xn_val, cmn_val),
			get_v_flag_add(xn_val, cmn_val));

	DEBUG_SS(std::stringstream() << "cmn " << std::to_string(cmn_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val));
}

// alias to ands
void Emulator32bit::_tst(word instr)
{
	word xn_val = read_reg(_X2(instr));
	word tst_val = FORMAT_O__get_arg(instr);
	word dst_val = tst_val & xn_val;

	set_NZCV(test_bit(dst_val, 31), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));

	DEBUG_SS(std::stringstream() << "tst " << std::to_string(tst_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val));
}

// alias to eors
void Emulator32bit::_teq(word instr)
{
	word xn_val = read_reg(_X2(instr));
	word teq_val = FORMAT_O__get_arg(instr);
	word dst_val = teq_val ^ xn_val;

	set_NZCV(test_bit(dst_val, 31), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));

	DEBUG_SS(std::stringstream() << "teq " << std::to_string(teq_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val));
}

void Emulator32bit::_mov(word instr)
{
	byte xd = _X1(instr);
	word mov_val = 0;
	if (test_bit(instr, 19)) {
		mov_val = bitfield_u32(instr, 0, 19);
	} else {
		mov_val = bitfield_u32(instr, 0, 14) + read_reg(bitfield_u32(instr, 14, 5));
	}

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(mov_val, 31), mov_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
	}

	DEBUG_SS(std::stringstream() << "mov " << std::to_string(xd) << " "
			<< std::to_string(mov_val));
	write_reg(xd, mov_val);
}

void Emulator32bit::_mvn(word instr)
{
	byte xd = _X1(instr);
	// word xn_val = read_reg(_X2(instr));
	word mvn_val = 0;
	if (test_bit(instr, 19)) {
		mvn_val = bitfield_u32(instr, 0, 19);
	} else {
		mvn_val = bitfield_u32(instr, 0, 14) + read_reg(bitfield_u32(instr, 14, 5));
	}

	word dst_val = ~mvn_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
	}

	DEBUG_SS(std::stringstream() << "mvn " << std::to_string(xd) << " "
			<< std::to_string(mvn_val) << " = " << std::to_string(dst_val));
	write_reg(xd, dst_val);
}

word Emulator32bit::calc_mem_addr(word xn, sword offset, byte addr_mode)
{
	word mem_addr = 0;
	word xn_val = read_reg(xn);
	if (addr_mode == 0) {
		mem_addr = xn_val + offset;
	} else if (addr_mode == 1) {
		mem_addr = xn_val + offset;
		write_reg(xn, mem_addr);
	} else if (addr_mode == 2) {
		mem_addr = xn_val;
		write_reg(xn, xn_val + offset);
	} else {
		throw EmulatorException("Bad memory address mode " + std::to_string(addr_mode));
	}
	return mem_addr;
}

void Emulator32bit::_ldr(word instr)
{
	byte xt = _X1(instr);
	byte xn = _X2(instr);
	bool simm = test_bit(instr, 14);
	sword offset = 0;
	if (simm) {
		offset = bitfield_s32(instr, 2, 12);
	} else {
		offset = FORMAT_O__get_arg(instr);
	}

	byte address_mode = bitfield_u32(instr, 0, 2);
	word mem_addr = calc_mem_addr(xn, offset, address_mode);
	word read_val = system_bus.read_word(mem_addr);

	if (address_mode == 0) {
		DEBUG_SS(std::stringstream() << "ldr " << std::to_string(xt) << ", [" << std::to_string(xn) << ", " << std::to_string(offset)
				<< "] [" << std::to_string(mem_addr) << "] = " << std::to_string(read_val));
	} else if (address_mode == 1) {
		DEBUG_SS(std::stringstream() << "ldr " << std::to_string(xt) << ", [" << std::to_string(xn) << ", " << std::to_string(offset)
				<< "]! [" << std::to_string(mem_addr) << "] = " << std::to_string(read_val));
	} else {
		DEBUG_SS(std::stringstream() << "ldr " << std::to_string(xt) << ", [" << std::to_string(xn) << "], " << std::to_string(offset)
				<< " [" << std::to_string(mem_addr) << "] = " << std::to_string(read_val));
	}
	write_reg(xt, read_val);
}

void Emulator32bit::_ldrb(word instr)
{
	bool sign = test_bit(instr, 25);
	byte xt = _X1(instr);
	byte xn = _X2(instr);
	bool simm = test_bit(instr, 14);
	sword offset = 0;
	if (simm) {
		offset = bitfield_s32(instr, 2, 12);
	} else {
		offset = FORMAT_O__get_arg(instr);
	}

	byte address_mode = bitfield_u32(instr, 0, 2);
	word mem_addr = calc_mem_addr(xn, offset, address_mode);
	word read_val = system_bus.read_byte(mem_addr);
	if (sign) {
		read_val = (sword) ((byte) read_val);
	}

	if (address_mode == 0) {
		DEBUG_SS(std::stringstream() << "ldr" << (sign ? "sb " : "b ") << std::to_string(xt) << ", [" << std::to_string(xn) << ", "
				<< offset << "] [" << std::to_string(mem_addr) << "] = " << std::to_string(read_val));
	} else if (address_mode == 1) {
		DEBUG_SS(std::stringstream() << "ldr" << (sign ? "sb " : "b ") << ", [" << std::to_string(xn) << ", "
				<< offset << "]! [" << std::to_string(mem_addr) << "] = " << std::to_string(read_val));
	} else {
		DEBUG_SS(std::stringstream() << "ldr" << (sign ? "sb " : "b ") << ", [" << std::to_string(xn) << "], "
				<< offset << " [" << std::to_string(mem_addr) << "] = " << std::to_string(read_val));
	}
	write_reg(xt, read_val);
}

void Emulator32bit::_ldrh(word instr)
{
	bool sign = test_bit(instr, 25);
	byte xt = _X1(instr);
	byte xn = _X2(instr);
	bool simm = test_bit(instr, 14);
	sword offset = 0;
	if (simm) {
		offset = bitfield_s32(instr, 2, 12);
	} else {
		offset = FORMAT_O__get_arg(instr);
	}

	byte address_mode = bitfield_u32(instr, 0, 2);
	word mem_addr = calc_mem_addr(xn, offset, address_mode);
	word read_val = system_bus.read_hword(mem_addr);
	if (sign) {
		read_val = (sword) ((hword) read_val);
	}

	if (address_mode == 0) {
		DEBUG_SS(std::stringstream() << "ldr" << (sign ? "sh " : "h ") << std::to_string(xt) << ", [" << std::to_string(xn) << ", "
				<< offset << "] [" << std::to_string(mem_addr) << "] = " << std::to_string(read_val));
	} else if (address_mode == 1) {
		DEBUG_SS(std::stringstream() << "ldr" << (sign ? "sh " : "h ") << ", [" << std::to_string(xn) << ", "
				<< offset << "]! [" << std::to_string(mem_addr) << "] = " << std::to_string(read_val));
	} else {
		DEBUG_SS(std::stringstream() << "ldr" << (sign ? "sh " : "h ") << ", [" << std::to_string(xn) << "], "
				<< offset << " [" << std::to_string(mem_addr) << "] = " << std::to_string(read_val));
	}
	write_reg(xt, read_val);
}

void Emulator32bit::_str(word instr)
{
	byte xt = _X1(instr);
	byte xn = _X2(instr);
	bool simm = test_bit(instr, 14);
	sword offset = 0;
	if (simm) {
		offset = bitfield_s32(instr, 2, 12);
	} else {
		offset = FORMAT_O__get_arg(instr);
	}

	byte address_mode = bitfield_u32(instr, 0, 2);
	word mem_addr = calc_mem_addr(xn, offset, address_mode);
	word write_val = read_reg(xt);

	if (address_mode == 0) {
		DEBUG_SS(std::stringstream() << "str " << std::to_string(xt) << ", [" << std::to_string(xn) << ", "
				<< offset << "] [" << std::to_string(mem_addr) << "]= " << std::to_string(write_val));
	} else if (address_mode == 1) {
		DEBUG_SS(std::stringstream() << "str " << std::to_string(xt) << ", [" << std::to_string(xn) << ", "
				<< offset << "]! [" << std::to_string(mem_addr) << "] = " << std::to_string(write_val));
	} else {
		DEBUG_SS(std::stringstream() << "str " << std::to_string(xt) << ", [" << std::to_string(xn) << "], "
				<< offset << " [" << std::to_string(mem_addr) << "] = " << std::to_string(write_val));
	}
	system_bus.write_word(mem_addr, write_val);
}

void Emulator32bit::_strb(word instr)
{
	bool sign = test_bit(instr, 25);
	byte xt = _X1(instr);
	byte xn = _X2(instr);
	bool simm = test_bit(instr, 14);
	sword offset = 0;
	if (simm) {
		offset = bitfield_s32(instr, 2, 12);
	} else {
		offset = FORMAT_O__get_arg(instr);
	}

	byte address_mode = bitfield_u32(instr, 0, 2);
	word mem_addr = calc_mem_addr(xn, offset, address_mode);
	word write_val = read_reg(xt);
	if (sign) {
		write_val = (sword) ((byte) write_val);
	}

	if (address_mode == 0) {
		DEBUG_SS(std::stringstream() << "str" << (sign ? "sb " : "b ") << std::to_string(xt) << ", [" << std::to_string(xn) << ", "
				<< offset << "] [" << std::to_string(mem_addr) << "] = " << std::to_string(write_val));
	} else if (address_mode == 1) {
		DEBUG_SS(std::stringstream() << "str" << (sign ? "sb " : "b ") << ", [" << std::to_string(xn) << ", "
				<< offset << "]! [" << std::to_string(mem_addr) << "] = " << std::to_string(write_val));
	} else {
		DEBUG_SS(std::stringstream() << "str" << (sign ? "sb " : "b ") << ", [" << std::to_string(xn) << "], "
				<< offset << " [" << std::to_string(mem_addr) << "] = " << std::to_string(write_val));
	}
	system_bus.write_byte(mem_addr, write_val);
}

void Emulator32bit::_strh(word instr)
{
	bool sign = test_bit(instr, 25);
	byte xt = _X1(instr);
	byte xn = _X2(instr);
	bool simm = test_bit(instr, 14);
	sword offset = 0;
	if (simm) {
		offset = bitfield_s32(instr, 2, 12);
	} else {
		offset = FORMAT_O__get_arg(instr);
	}

	byte address_mode = bitfield_u32(instr, 0, 2);
	word mem_addr = calc_mem_addr(xn, offset, address_mode);
	word write_val = read_reg(xt);
	if (sign) {
		write_val = (sword) ((hword)write_val);
	}

	if (address_mode == 0) {
		DEBUG_SS(std::stringstream() << "str" << (sign ? "sh " : "h ") << std::to_string(xt) << ", [" << std::to_string(xn) << ", "
				<< offset << "] [" << std::to_string(mem_addr) << "] = " << std::to_string(write_val));
	} else if (address_mode == 1) {
		DEBUG_SS(std::stringstream() << "str" << (sign ? "sh " : "h ") << ", [" << std::to_string(xn) << ", "
				<< offset << "]! [" << std::to_string(mem_addr) << "] = " << std::to_string(write_val));
	} else {
		DEBUG_SS(std::stringstream() << "str" << (sign ? "sh " : "h ") << ", [" << std::to_string(xn) << "], "
				<< offset << " [" << std::to_string(mem_addr) << "] = " << std::to_string(write_val));
	}
	system_bus.write_hword(mem_addr, write_val);
}

void Emulator32bit::_swp(word instr)
{
	byte xt = _X1(instr);
	byte xn = _X2(instr);
	byte xm = _X3(instr);
	word mem_adr = read_reg(xm);

	DEBUG_SS(std::stringstream() << "swp " << std::to_string(xt) << " " << std::to_string(xn) << " [" << std::to_string(xm) << "]");

	word val_reg = read_reg(xn);
	word val_mem = system_bus.read_word(mem_adr);

	write_reg(xt, val_mem);
	system_bus.write_word(mem_adr, val_reg);
}

void Emulator32bit::_swpb(word instr)
{
	byte xt = _X1(instr);
	byte xn = _X2(instr);
	byte xm = _X3(instr);
	word mem_adr = read_reg(xm);

	DEBUG_SS(std::stringstream() << "swpb " << std::to_string(xt) << " " << std::to_string(xn) << " [" << std::to_string(xm) << "]");

	word val_reg = read_reg(xn) & 0xFF;
	word val_mem = system_bus.read_byte(mem_adr);

	write_reg(xt, (val_reg & ~(0xFF)) + val_mem);
	system_bus.write_byte(mem_adr, val_reg);
}

void Emulator32bit::_swph(word instr)
{
	byte xt = _X1(instr);
	byte xn = _X2(instr);
	byte xm = _X3(instr);
	word mem_adr = read_reg(xm);

	DEBUG_SS(std::stringstream() << "swph " << std::to_string(xt) << " " << std::to_string(xn) << " [" << std::to_string(xm) << "]");

	word val_reg = read_reg(xn) & 0xFFFF;
	word val_mem = system_bus.read_byte(mem_adr);

	write_reg(xt, (val_reg & ~(0xFFFF)) + val_mem);
	system_bus.write_byte(mem_adr, val_reg);
}


void Emulator32bit::_b(word instr)
{


	byte cond = bitfield_u32(instr, 22, 4);
	if (check_cond(_pstate, cond)) {
		_pc += (bitfield_s32(instr, 0, 22) << 2) - 4;			/* account for execution loop incrementing _pc by 4 */
	}
	DEBUG_SS(std::stringstream() << "b " << std::to_string(cond));
}

void Emulator32bit::_bl(word instr)
{
	byte cond = bitfield_u32(instr, 22, 4);
	if (check_cond(_pstate, cond)) {
		write_reg(LINKR, _pc+4);
		_pc += (bitfield_s32(instr, 0, 22) << 2) - 4;
	}
	DEBUG_SS(std::stringstream() << "bl " << std::to_string(cond));
}

void Emulator32bit::_bx(word instr)
{
	byte cond = bitfield_u32(instr, 22, 4);
	byte reg = bitfield_u32(instr, 17, 5);
	if (check_cond(_pstate, cond)) {
		_pc = (sword) read_reg(reg) - 4;
	}
	DEBUG_SS(std::stringstream() << "bx " << std::to_string(reg) << " (" << std::to_string(cond) << ")");
}

void Emulator32bit::_blx(word instr)
{
	byte cond = bitfield_u32(instr, 22, 4);
	byte reg = bitfield_u32(instr, 17, 5);
	if (check_cond(_pstate, cond)) {
		write_reg(LINKR, _pc+4);
		_pc = (sword) read_reg(reg) - 4;
	}
	DEBUG_SS(std::stringstream() << "blx " << std::to_string(reg) << "(" << std::to_string(cond) << ")");
}

void Emulator32bit::_adrp(word instr)
{
	byte xd = _X1(instr);
	word imm20 = bitfield_u32(instr, 0, 20) << 12;

	write_reg(xd, imm20);

	DEBUG_SS(std::stringstream() << "adrp " << std::to_string(xd) << " " << std::to_string(imm20));
}