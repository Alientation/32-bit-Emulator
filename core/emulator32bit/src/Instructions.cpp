#include <emulator32bit/Emulator32bit.h>
#include <util/Logger.h>

#include <string>

using namespace lgr;

// useful macros
// bits 20 to 24
#define _X1(instr) (bitfield_u32(instr, 20, 5))
// bits 15 to 19
#define _X2(instr) (bitfield_u32(instr, 15, 5))
// bits 9 to 14
#define _X3(instr) (bitfield_u32(instr, 9, 5))
// bits 4 to 9
#define _X4(instr) (bitfield_u32(instr, 4, 5))

// helper functions
static word calc_shift(word val, byte shift_type, byte imm5) {
	switch(shift_type) {
		case 0b00: // LSL
			log(Logger::LogType::DEBUG, std::stringstream() << "LSL " << std::to_string((word)imm5) << "\n");
			val <<= imm5;
			break;
		case 0b01: // LSR
			log(Logger::LogType::DEBUG, std::stringstream() << "LSR " << std::to_string((word)imm5) << "\n");
			val >>= imm5;
			break;
		case 0b10: // ASR
			log(Logger::LogType::DEBUG, std::stringstream() << "ASR " << std::to_string((word)imm5) << "\n");
			val = ((signed int) val) >> imm5;
			break;
		case 0b11: // ROR
		{
			log(Logger::LogType::DEBUG, std::stringstream() << "ROR " << std::to_string((word)imm5) << "\n");
			word rot_bits = val & ((1 << imm5) - 1);
			rot_bits <<= (WORD_BITS - imm5);
			val >>= imm5;
			val &= (1 << (WORD_BITS - imm5)) - 1; // to be safe and remove bits that will be replaced
			val |= rot_bits;
			break;
		}
		default: // error
			log(Logger::LogType::ERROR, "Invalid shift: " + val);
	}
	return val;
}

// yoinked from https://github.com/unicorn-engine/ because I could not figure out carry/overflow for subtraction
static bool get_c_flag_add(word op1, word op2) {
	word dst_val = op1 + op2;
	return dst_val < op1;
}

static bool get_v_flag_add(word op1, word op2) {
	word dst_val = op1 + op2;
	return (op1 ^ op2 ^ -1) & (op1 ^ dst_val) & (1U << 31);
}

static bool get_c_flag_sub(word op1, word op2) {
	word dst_val = op1 - op2;
	return (((~op1 & op2) | (dst_val & (~op1 | op2))) & (1U << 31));
}

static bool get_v_flag_sub(word op1, word op2) {
	word dst_val = op1 - op2;
	return (((op1 ^ op2) & (op1 ^ dst_val)) & (1U << 31));
}

#define FORMAT_O__get_arg(instr, exception) (test_bit(instr, 14) ? bitfield_u32(instr, 0, 14) : calc_shift(read_reg(_X3(instr), exception), bitfield_u32(instr, 7, 2), bitfield_u32(instr, 2, 2)))

struct JPart {
	JPart(int bits, word val = 0) : bits(bits), val(val) {}
	int bits;
	word val;
};
class Joiner {
	public:
		word val = 0;
		Joiner& operator<<(JPart p) {
			val <<= p.bits;
			val += p.val;
			return *this;
		}

		Joiner& operator<<(int bits) {
			val <<= bits;
			return *this;
		}

		operator word() const { return val; }
};

bool Emulator32bit::check_cond(word pstate, byte cond) {
	bool N = test_bit(pstate, N_FLAG);
	bool Z = test_bit(pstate, Z_FLAG);
	bool C = test_bit(pstate, C_FLAG);
	bool V = test_bit(pstate, V_FLAG);

	switch((ConditionCode) cond) {
		case ConditionCode::EQ:
			return Z;
		case ConditionCode::NE:
			return !Z;
		case ConditionCode::CS:
			return C;
		case ConditionCode::CC:
			return !C;
		case ConditionCode::MI:
			return N;
		case ConditionCode::PL:
			return !N;
		case ConditionCode::VS:
			return V;
		case ConditionCode::VC:
			return !V;
		case ConditionCode::HI:
			return C && !Z;
		case ConditionCode::LS:
			return !C && !Z;
		case ConditionCode::GE:
			return N==V;
		case ConditionCode::LT:
			return N!=V;
		case ConditionCode::GT:
			return !Z && (N==V);
		case ConditionCode::LE:
			return Z && (N!=V);
		case ConditionCode::AL:
			return true;
		case ConditionCode::NV:
			return false;
	}
	return false;
}

word Emulator32bit::asm_format_o(byte opcode, bool s, int xd, int xn, int imm14) {
	return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xd) << JPart(5, xn) << JPart(1, 1) << JPart(14, imm14);
}

word Emulator32bit::asm_format_o(byte opcode, bool s, int xd, int xn, int xm, int shift, int imm5) {
	return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xd) << JPart(5, xn) << 1 << JPart(5, xm) << JPart(2, shift) << JPart(5, imm5) << 2;
}

word Emulator32bit::asm_format_o1(byte opcode, int xd, int xn, bool imm, int xm, int imm5) {
	return Joiner() << JPart(6, opcode) << 1 << JPart(5, xd) << JPart(5, xn) << JPart(1, imm) << JPart(5, xm) << 2 << JPart(5, imm5) << 2;
}

word Emulator32bit::asm_format_o2(byte opcode, bool s, int xlo, int xhi, int xn, int xm) {
	return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xlo) << JPart(5, xhi) << 1 << JPart(5, xn) << JPart(5, xm) << 4;
}

word Emulator32bit::asm_format_m(byte opcode, bool sign, int xt, int xn, int xm, int shift, int imm5, int adr) {
	return Joiner() << JPart(6, opcode) << JPart(1, sign) << JPart(5, xt) << JPart(5, xn) << 1 << JPart(5, xm) << JPart(2, shift) << JPart(5, imm5) << JPart(2, adr);
}

word Emulator32bit::asm_format_m(byte opcode, bool sign, int xt, int xn, int simm12, int adr) {
	return Joiner() << JPart(6, opcode) << JPart(1, sign) << JPart(5, xt) << JPart(5, xn) << JPart(1, 1) << JPart(12, simm12) << JPart(2, adr);
}

word Emulator32bit::asm_format_m1(byte opcode, int xt, int xn, int xm) {
	return Joiner() << JPart(6, opcode) << 1 << JPart(5, xt) << JPart(5, xn) << 1 << JPart(5, xm) << 9;
}

word Emulator32bit::asm_format_b1(byte opcode, ConditionCode cond, sword simm22) {
	return Joiner() << JPart(6, opcode) << JPart(4, (word) cond) << JPart(22, simm22);
}

word Emulator32bit::asm_format_b2(byte opcode, ConditionCode cond, int xd) {
	return Joiner() << JPart(6, opcode) << JPart(4, (word) cond) << JPart(5, xd) << 17;
}

void Emulator32bit::_hlt(word instr, EmulatorException& exception) {
	exception.type = EmulatorException::Type::HALT;
}

word Emulator32bit::asm_hlt() {
	return Joiner() << JPart(6, _op_hlt) << 26;
}

void Emulator32bit::_nop(word instr, EmulatorException& exception) {
	return; // do nothing
}

word Emulator32bit::asm_nop() {
	return Joiner() << JPart(6, _op_nop) << 26;
}

void Emulator32bit::_add(word instr, EmulatorException& exception) {
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word add_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = add_val + xn_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, get_c_flag_add(xn_val, add_val),
				get_v_flag_add(xn_val, add_val));
	}

	log(Logger::LogType::DEBUG, std::stringstream() << "add " << std::to_string(add_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_sub(word instr, EmulatorException& exception) {
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word sub_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = xn_val - sub_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, get_c_flag_sub(xn_val, sub_val),
				get_v_flag_sub(xn_val, sub_val));
	}

	log(Logger::LogType::DEBUG, std::stringstream() << "sub " << std::to_string(sub_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_rsb(word instr, EmulatorException& exception) {
	byte xd = _X1(instr);
	word sub_val = read_reg(_X2(instr), exception);
	word xn_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = xn_val - sub_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, get_c_flag_sub(xn_val, sub_val),
				get_v_flag_sub(xn_val, sub_val));
	}

	log(Logger::LogType::DEBUG, std::stringstream() << "rsb " << std::to_string(xn_val) << " "
			<< std::to_string(sub_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_adc(word instr, EmulatorException& exception) {
	bool c = test_bit(_pstate, C_FLAG);
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word add_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = add_val + xn_val + c;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0,
				get_c_flag_add(xn_val + c, add_val) | get_c_flag_add(xn_val, c),
				get_v_flag_add(xn_val + c, add_val) | get_v_flag_add(xn_val, c));
	}

	log(Logger::LogType::DEBUG, std::stringstream() << "adc " << std::to_string(add_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_sbc(word instr, EmulatorException& exception) {
	bool borrow = test_bit(_pstate, C_FLAG);
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word sub_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = xn_val - sub_val - borrow;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0,
				get_c_flag_sub(xn_val - borrow, sub_val) | get_c_flag_sub(xn_val, borrow),
				get_v_flag_sub(xn_val - borrow, sub_val) | get_v_flag_sub(xn_val, borrow));
	}

	log(Logger::LogType::DEBUG, std::stringstream() << "sbc " << std::to_string(sub_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_rsc(word instr, EmulatorException& exception) {
	bool borrow = test_bit(_pstate, C_FLAG);
	byte xd = _X1(instr);
	word sub_val = read_reg(_X2(instr), exception);
	word xn_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = xn_val - sub_val - borrow;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0,
				get_c_flag_sub(xn_val - borrow, sub_val) | get_c_flag_sub(xn_val, borrow),
				get_v_flag_sub(xn_val - borrow, sub_val) | get_v_flag_sub(xn_val, borrow));
	}

	log(Logger::LogType::DEBUG, std::stringstream() << "rsc " << std::to_string(xn_val) << " "
			<< std::to_string(sub_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_mul(word instr, EmulatorException& exception) {
	byte xd = _X1(instr);
	dword xn_val = read_reg(_X2(instr), exception);
	dword xm_val = FORMAT_O__get_arg(instr, exception);
	dword dst_val = xn_val * xm_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		// according to https://developer.arm.com/documentation/dui0473/m/arm-and-thumb-instructions/smull
		// arm's MUL instruction does not set carry or overflow flags
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
	}

	log(Logger::LogType::DEBUG, std::stringstream() << "mul " << std::to_string(xn_val) << " "
			<< std::to_string(xm_val) << " = " << std::to_string(dst_val) << "\n");

	write_reg(xd, (word) dst_val, exception);
}

void Emulator32bit::_umull(word instr, EmulatorException& exception) {
	byte xlo = _X1(instr);
	byte xhi = _X2(instr);
	dword xn_val = read_reg(_X3(instr), exception);
	dword xm_val = read_reg(_X4(instr), exception);
	dword dst_val = xn_val * xm_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		// according to https://developer.arm.com/documentation/dui0473/m/arm-and-thumb-instructions/umull
		// arm's UMULL instruction does not set carry or overflow flags
		set_NZCV(test_bit(dst_val, 63), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
	}

	log(Logger::LogType::DEBUG, std::stringstream() << "mul " << std::to_string(xn_val) << " "
			<< std::to_string(xm_val) << " = " << std::to_string(dst_val) << "\n");

	write_reg(xlo, (word) dst_val, exception);
	write_reg(xhi, (word) (dst_val >> 32), exception);
}

void Emulator32bit::_smull(word instr, EmulatorException& exception) {
	byte xlo = _X1(instr);
	byte xhi = _X2(instr);
	signed long long xn_val = ((signed long long) read_reg(_X3(instr), exception)) << 32 >> 32;
	signed long long xm_val = ((signed long long) read_reg(_X4(instr), exception)) << 32 >> 32;
	signed long long dst_val = xn_val * xm_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		// according to https://developer.arm.com/documentation/dui0489/c/arm-and-thumb-instructions/multiply-instructions/mul--mla--and-mls
		// arm's UMULL instruction does not set carry or overflow flags
		set_NZCV(test_bit(dst_val, 63), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
	}

	log(Logger::LogType::DEBUG, std::stringstream() << "mul " << std::to_string(xn_val) << " "
			<< std::to_string(xm_val) << " = " << std::to_string(dst_val) << "\n");

	write_reg(xlo, (word) dst_val, exception);
	write_reg(xhi, (word) (dst_val >> 32), exception);
}

// todo WILL DO LATER JUST NOT NOW
void Emulator32bit::_vabs_f32(word instr, EmulatorException& exception) {}
void Emulator32bit::_vneg_f32(word instr, EmulatorException& exception) {}
void Emulator32bit::_vsqrt_f32(word instr, EmulatorException& exception) {}
void Emulator32bit::_vadd_f32(word instr, EmulatorException& exception) {}
void Emulator32bit::_vsub_f32(word instr, EmulatorException& exception) {}
void Emulator32bit::_vdiv_f32(word instr, EmulatorException& exception) {}
void Emulator32bit::_vmul_f32(word instr, EmulatorException& exception) {}
void Emulator32bit::_vcmp_f32(word instr, EmulatorException& exception) {}
void Emulator32bit::_vsel_f32(word instr, EmulatorException& exception) {}
void Emulator32bit::_vcint_f32(word instr, EmulatorException& exception) {}
void Emulator32bit::_vcflo_f32(word instr, EmulatorException& exception) {}
void Emulator32bit::_vmov_f32(word instr, EmulatorException& exception) {}

void Emulator32bit::_and(word instr, EmulatorException& exception) {
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word and_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = and_val & xn_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		// https://developer.arm.com/documentation/dui0489/h/arm-and-thumb-instructions/and--orr--eor--bic--and-orn
		// N and Z flags are set based of the result, C flag may be set based of the calculation for the second operand
		// but will ignore for now
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
	}

	log(Logger::LogType::DEBUG, std::stringstream() << "and " << std::to_string(and_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_orr(word instr, EmulatorException& exception) {
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word or_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = or_val | xn_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		// https://developer.arm.com/documentation/dui0489/h/arm-and-thumb-instructions/and--orr--eor--bic--and-orn
		// N and Z flags are set based of the result, C flag may be set based of the calculation for the second operand
		// but will ignore for now
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
	}

	log(Logger::LogType::DEBUG, std::stringstream() << "orr " << std::to_string(or_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_eor(word instr, EmulatorException& exception) {
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word eor_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = eor_val ^ xn_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		// https://developer.arm.com/documentation/dui0489/h/arm-and-thumb-instructions/and--orr--eor--bic--and-orn
		// N and Z flags are set based of the result, C flag may be set based of the calculation for the second operand
		// but will ignore for now
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
	}

	log(Logger::LogType::DEBUG, std::stringstream() << "eor " << std::to_string(eor_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_bic(word instr, EmulatorException& exception) {
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word bic_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = (~bic_val) & xn_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		// https://developer.arm.com/documentation/dui0489/h/arm-and-thumb-instructions/and--orr--eor--bic--and-orn
		// N and Z flags are set based of the result, C flag may be set based of the calculation for the second operand
		// but will ignore for now
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
	}

	log(Logger::LogType::DEBUG, std::stringstream() << "bic " << std::to_string(bic_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_lsl(word instr, EmulatorException& exception) {
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word lsl_val = test_bit(instr, 14) ? bitfield_u32(instr, 2, 5) : 0xFF & read_reg(_X3(instr), exception);
	word dst_val = xn_val << lsl_val;

	log(Logger::LogType::DEBUG, std::stringstream() << "lsl " << std::to_string(lsl_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_lsr(word instr, EmulatorException& exception) {
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word lsl_val = test_bit(instr, 14) ? bitfield_u32(instr, 2, 5) : 0xFF & read_reg(_X3(instr), exception);
	word dst_val = xn_val >> lsl_val;

	log(Logger::LogType::DEBUG, std::stringstream() << "lsr " << std::to_string(lsl_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_asr(word instr, EmulatorException& exception) {
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word lsl_val = test_bit(instr, 14) ? bitfield_u32(instr, 2, 5) : 0xFF & read_reg(_X3(instr), exception);
	word dst_val = ((sword) xn_val) >> lsl_val;

	log(Logger::LogType::DEBUG, std::stringstream() << "asr " << std::to_string(lsl_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_ror(word instr, EmulatorException& exception) {
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word lsl_val = test_bit(instr, 14) ? bitfield_u32(instr, 2, 5) : 0xFF & read_reg(_X3(instr), exception);
	word dst_val = (xn_val >> lsl_val) | (bitfield_u32(xn_val, 0, lsl_val) << (32 - lsl_val));

	log(Logger::LogType::DEBUG, std::stringstream() << "ror " << std::to_string(lsl_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

// alias to subs
void Emulator32bit::_cmp(word instr, EmulatorException& exception) {
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word cmp_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = xn_val - cmp_val;

	set_NZCV(test_bit(dst_val, 31), dst_val == 0, get_c_flag_sub(xn_val, cmp_val),
				get_v_flag_sub(xn_val, cmp_val));

	log(Logger::LogType::DEBUG, std::stringstream() << "cmp " << std::to_string(cmp_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
}

// alias to adds
void Emulator32bit::_cmn(word instr, EmulatorException& exception) {
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word cmn_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = cmn_val + xn_val;

	set_NZCV(test_bit(dst_val, 31), dst_val == 0, get_c_flag_add(xn_val, cmn_val),
			get_v_flag_add(xn_val, cmn_val));

	log(Logger::LogType::DEBUG, std::stringstream() << "cmn " << std::to_string(cmn_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
}

// alias to ands
void Emulator32bit::_tst(word instr, EmulatorException& exception) {
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word tst_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = tst_val & xn_val;

	set_NZCV(test_bit(dst_val, 31), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));

	log(Logger::LogType::DEBUG, std::stringstream() << "tst " << std::to_string(tst_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
}

// alias to eors
void Emulator32bit::_teq(word instr, EmulatorException& exception) {
	byte xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word teq_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = teq_val ^ xn_val;

	set_NZCV(test_bit(dst_val, 31), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));

	log(Logger::LogType::DEBUG, std::stringstream() << "teq " << std::to_string(teq_val) << " "
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
}

// TODO this is memory inefficient.. XN_VAL is not used, make a separate format for mov and mvn to make use of extra space
void Emulator32bit::_mov(word instr, EmulatorException& exception) {
	byte xd = _X1(instr);
	// word xn_val = read_reg(_X2(instr), exception);
	word mov_val = FORMAT_O__get_arg(instr, exception);

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(mov_val, 31), mov_val == 0, 0, test_bit(_pstate, V_FLAG));
	}

	log(Logger::LogType::DEBUG, std::stringstream() << "mov " << std::to_string(xd) << " "
			<< std::to_string(mov_val) << "\n");
	write_reg(xd, mov_val, exception);
}

void Emulator32bit::_mvn(word instr, EmulatorException& exception) {
	byte xd = _X1(instr);
	// word xn_val = read_reg(_X2(instr), exception);
	word mvn_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = ~mvn_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(mvn_val, 31), mvn_val == 0, 0, test_bit(_pstate, V_FLAG));
	}

	log(Logger::LogType::DEBUG, std::stringstream() << "mvn " << std::to_string(xd) << " "
			<< std::to_string(mvn_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

word Emulator32bit::calc_mem_addr(word xn, sword offset, byte addr_mode, EmulatorException& exception) {
	word mem_addr = 0;
	word xn_val = read_reg(xn, exception);
	if (addr_mode == 0) {
		mem_addr = xn_val + offset;
	} else if (addr_mode == 1) {
		mem_addr = xn_val + offset;
		write_reg(xn, mem_addr, exception);
	} else if (addr_mode == 2) {
		mem_addr = xn_val;
		write_reg(xn, xn_val + offset, exception);
	} else {
		exception.type = EmulatorException::Type::BAD_INSTR;
	}
	return mem_addr;
}

void Emulator32bit::_ldr(word instr, EmulatorException& exception) {
	byte xt = _X1(instr);
	byte xn = _X2(instr);
	word xn_val = read_reg(xn, exception);
	bool simm = test_bit(instr, 14);
	sword offset = 0;
	if (simm) {
		offset = bitfield_s32(instr, 2, 12);
	} else {
		offset = FORMAT_O__get_arg(instr, exception);
	}

	byte address_mode = bitfield_u32(instr, 0, 2);
	word mem_addr = calc_mem_addr(xn, offset, address_mode, exception);

	if (address_mode == 0) {
		log(Logger::LogType::DEBUG, std::stringstream() << "ldr " << std::to_string(xt) << ", [" << std::to_string(xn) << ", " << offset << "]\n");
	} else if (address_mode == 1) {
		log(Logger::LogType::DEBUG, std::stringstream() << "ldr " << std::to_string(xt) << ", [" << std::to_string(xn) << ", " << offset << "]!\n");
	} else {
		log(Logger::LogType::DEBUG, std::stringstream() << "ldr " << std::to_string(xt) << ", [" << std::to_string(xn) << "], " << offset << "\n");
	}
	write_reg(xt, system_bus.read_word(mem_addr, exception.sys_bus_exception, exception.mem_read_exception), exception);
}

void Emulator32bit::_ldrb(word instr, EmulatorException& exception) {
	bool sign = test_bit(instr, 25);
	byte xt = _X1(instr);
	byte xn = _X2(instr);
	word xn_val = read_reg(xn, exception);
	bool simm = test_bit(instr, 14);
	sword offset = 0;
	if (simm) {
		offset = bitfield_s32(instr, 2, 12);
	} else {
		offset = FORMAT_O__get_arg(instr, exception);
	}

	byte address_mode = bitfield_u32(instr, 0, 2);
	word mem_addr = calc_mem_addr(xn, offset, address_mode, exception);
	word read_val = system_bus.read_byte(mem_addr, exception.sys_bus_exception, exception.mem_read_exception);
	if (sign) {
		read_val = (sword) ((byte) read_val);
	}

	if (address_mode == 0) {
		log(Logger::LogType::DEBUG, std::stringstream() << "ldr" << (sign ? "sb " : "b ") << std::to_string(xt) << ", [" << std::to_string(xn) << ", " << offset << "]\n");
	} else if (address_mode == 1) {
		log(Logger::LogType::DEBUG, std::stringstream() << "ldr " << (sign ? "sb " : "b ") << ", [" << std::to_string(xn) << ", " << offset << "]!\n");
	} else {
		log(Logger::LogType::DEBUG, std::stringstream() << "ldr " << (sign ? "sb " : "b ") << ", [" << std::to_string(xn) << "], " << offset << "\n");
	}
	write_reg(xt, read_val, exception);
}

void Emulator32bit::_ldrh(word instr, EmulatorException& exception) {
	bool sign = test_bit(instr, 25);
	byte xt = _X1(instr);
	byte xn = _X2(instr);
	word xn_val = read_reg(xn, exception);
	bool simm = test_bit(instr, 14);
	sword offset = 0;
	if (simm) {
		offset = bitfield_s32(instr, 2, 12);
	} else {
		offset = FORMAT_O__get_arg(instr, exception);
	}

	byte address_mode = bitfield_u32(instr, 0, 2);
	word mem_addr = calc_mem_addr(xn, offset, address_mode, exception);
	word read_val = system_bus.read_hword(mem_addr, exception.sys_bus_exception, exception.mem_read_exception);
	if (sign) {
		read_val = (sword) ((hword) read_val);
	}

	if (address_mode == 0) {
		log(Logger::LogType::DEBUG, std::stringstream() << "ldr" << (sign ? "sh " : "h ") << std::to_string(xt) << ", [" << std::to_string(xn) << ", " << offset << "]\n");
	} else if (address_mode == 1) {
		log(Logger::LogType::DEBUG, std::stringstream() << "ldr " << (sign ? "sh " : "h ") << ", [" << std::to_string(xn) << ", " << offset << "]!\n");
	} else {
		log(Logger::LogType::DEBUG, std::stringstream() << "ldr " << (sign ? "sh " : "h ") << ", [" << std::to_string(xn) << "], " << offset << "\n");
	}
	write_reg(xt, read_val, exception);
}

void Emulator32bit::_str(word instr, EmulatorException& exception) {
	byte xt = _X1(instr);
	byte xn = _X2(instr);
	word xn_val = read_reg(xn, exception);
	bool simm = test_bit(instr, 14);
	sword offset = 0;
	if (simm) {
		offset = bitfield_s32(instr, 2, 12);
	} else {
		offset = FORMAT_O__get_arg(instr, exception);
	}

	byte address_mode = bitfield_u32(instr, 0, 2);
	word mem_addr = calc_mem_addr(xn, offset, address_mode, exception);

	if (address_mode == 0) {
		log(Logger::LogType::DEBUG, std::stringstream() << "str " << std::to_string(xt) << ", [" << std::to_string(xn) << ", " << offset << "]\n");
	} else if (address_mode == 1) {
		log(Logger::LogType::DEBUG, std::stringstream() << "str " << std::to_string(xt) << ", [" << std::to_string(xn) << ", " << offset << "]!\n");
	} else {
		log(Logger::LogType::DEBUG, std::stringstream() << "str " << std::to_string(xt) << ", [" << std::to_string(xn) << "], " << offset << "\n");
	}
	system_bus.write_word(mem_addr, read_reg(xt, exception), exception.sys_bus_exception, exception.mem_write_exception);
}

void Emulator32bit::_strb(word instr, EmulatorException& exception) {
	bool sign = test_bit(instr, 25);
	byte xt = _X1(instr);
	byte xn = _X2(instr);
	word xn_val = read_reg(xn, exception);
	bool simm = test_bit(instr, 14);
	sword offset = 0;
	if (simm) {
		offset = bitfield_s32(instr, 2, 12);
	} else {
		offset = FORMAT_O__get_arg(instr, exception);
	}

	byte address_mode = bitfield_u32(instr, 0, 2);
	word mem_addr = calc_mem_addr(xn, offset, address_mode, exception);
	word write_val = system_bus.read_byte(mem_addr, exception.sys_bus_exception, exception.mem_read_exception);
	if (sign) {
		write_val = (sword) ((byte) write_val);
	}

	if (address_mode == 0) {
		log(Logger::LogType::DEBUG, std::stringstream() << "str" << (sign ? "sb " : "b ") << std::to_string(xt) << ", [" << std::to_string(xn) << ", " << offset << "]\n");
	} else if (address_mode == 1) {
		log(Logger::LogType::DEBUG, std::stringstream() << "str " << (sign ? "sb " : "b ") << ", [" << std::to_string(xn) << ", " << offset << "]!\n");
	} else {
		log(Logger::LogType::DEBUG, std::stringstream() << "str " << (sign ? "sb " : "b ") << ", [" << std::to_string(xn) << "], " << offset << "\n");
	}
	system_bus.write_byte(mem_addr, write_val, exception.sys_bus_exception, exception.mem_write_exception);
}

void Emulator32bit::_strh(word instr, EmulatorException& exception) {
	bool sign = test_bit(instr, 25);
	byte xt = _X1(instr);
	byte xn = _X2(instr);
	word xn_val = read_reg(xn, exception);
	bool simm = test_bit(instr, 14);
	sword offset = 0;
	if (simm) {
		offset = bitfield_s32(instr, 2, 12);
	} else {
		offset = FORMAT_O__get_arg(instr, exception);
	}

	byte address_mode = bitfield_u32(instr, 0, 2);
	word mem_addr = calc_mem_addr(xn, offset, address_mode, exception);
	word write_val = system_bus.read_hword(mem_addr, exception.sys_bus_exception, exception.mem_read_exception);
	if (sign) {
		write_val = (sword) ((hword) write_val);
	}

	if (address_mode == 0) {
		log(Logger::LogType::DEBUG, std::stringstream() << "str" << (sign ? "sh " : "h ") << std::to_string(xt) << ", [" << std::to_string(xn) << ", " << offset << "]\n");
	} else if (address_mode == 1) {
		log(Logger::LogType::DEBUG, std::stringstream() << "str " << (sign ? "sh " : "h ") << ", [" << std::to_string(xn) << ", " << offset << "]!\n");
	} else {
		log(Logger::LogType::DEBUG, std::stringstream() << "str " << (sign ? "sh " : "h ") << ", [" << std::to_string(xn) << "], " << offset << "\n");
	}
	system_bus.write_hword(mem_addr, write_val, exception.sys_bus_exception, exception.mem_write_exception);
}

void Emulator32bit::_swp(word instr, EmulatorException& exception) {
	byte xt = _X1(instr);
	byte xn = _X2(instr);
	byte xm = _X3(instr);
	word mem_adr = read_reg(xm, exception);

	log(Logger::LogType::DEBUG, std::stringstream() << "swp " << std::to_string(xt) << " " << std::to_string(xn) << " [" << std::to_string(xm) << "]\n");

	word val_reg = read_reg(xn, exception);
	word val_mem = system_bus.read_word(mem_adr, exception.sys_bus_exception, exception.mem_read_exception);

	write_reg(xt, val_mem, exception);
	system_bus.write_word(mem_adr, val_reg, exception.sys_bus_exception, exception.mem_write_exception);
}

void Emulator32bit::_swpb(word instr, EmulatorException& exception) {
	byte xt = _X1(instr);
	byte xn = _X2(instr);
	byte xm = _X3(instr);
	word mem_adr = read_reg(xm, exception);

	log(Logger::LogType::DEBUG, std::stringstream() << "swpb " << std::to_string(xt) << " " << std::to_string(xn) << " [" << std::to_string(xm) << "]\n");

	word val_reg = read_reg(xn, exception) & 0xFF;
	word val_mem = system_bus.read_byte(mem_adr, exception.sys_bus_exception, exception.mem_read_exception);

	write_reg(xt, (val_reg & ~(0xFF)) + val_mem, exception);
	system_bus.write_byte(mem_adr, val_reg, exception.sys_bus_exception, exception.mem_write_exception);
}

void Emulator32bit::_swph(word instr, EmulatorException& exception) {
	byte xt = _X1(instr);
	byte xn = _X2(instr);
	byte xm = _X3(instr);
	word mem_adr = read_reg(xm, exception);

	log(Logger::LogType::DEBUG, std::stringstream() << "swph " << std::to_string(xt) << " " << std::to_string(xn) << " [" << std::to_string(xm) << "]\n");

	word val_reg = read_reg(xn, exception) & 0xFFFF;
	word val_mem = system_bus.read_byte(mem_adr, exception.sys_bus_exception, exception.mem_read_exception);

	write_reg(xt, (val_reg & ~(0xFFFF)) + val_mem, exception);
	system_bus.write_byte(mem_adr, val_reg, exception.sys_bus_exception, exception.mem_write_exception);
}


void Emulator32bit::_b(word instr, EmulatorException& exception) {
	byte cond = bitfield_u32(instr, 22, 4);
	if (check_cond(_pstate, cond)) {
		_pc += (bitfield_s32(instr, 0, 22) << 2) - 4;			/* account for execution loop incrementing _pc by 4 */
	}
}

void Emulator32bit::_bl(word instr, EmulatorException& exception) {
	byte cond = bitfield_u32(instr, 22, 4);
	if (check_cond(_pstate, cond)) {
		write_reg(_x[29], _pc+4, exception);
		_pc += (bitfield_s32(instr, 0, 22) << 2) - 4;
	}
}

void Emulator32bit::_bx(word instr, EmulatorException& exception) {
	byte cond = bitfield_u32(instr, 22, 4);
	if (check_cond(_pstate, cond)) {
		_pc += (sword) read_reg(bitfield_u32(instr, 17, 5), exception) - 4;
	}
}

void Emulator32bit::_blx(word instr, EmulatorException& exception) {
	byte cond = bitfield_u32(instr, 22, 4);
	if (check_cond(_pstate, cond)) {
		write_reg(_x[29], _pc+4, exception);
		_pc += (sword) read_reg(bitfield_u32(instr, 17, 5), exception) - 4;
	}
}

void Emulator32bit::_swi(word instr, EmulatorException& exception) {
	byte cond = bitfield_u32(instr, 22, 4);
	if (check_cond(_pstate, cond)) {
		// software interrupts.. perfect to add functionality to this like console print,
		// file operations, ports, etc
	}
}
