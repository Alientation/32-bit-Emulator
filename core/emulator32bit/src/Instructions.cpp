#include <emulator32bit/Emulator32bit.h>
#include <util/Logger.h>

#include <string>

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
static inline word calc_shift(word val, byte shift_type, byte imm5) {
	switch(shift_type) {
		case 0b00: // LSL
			log(lgr::Logger::LogType::DEBUG, std::stringstream() << "LSL " << std::to_string((word)imm5) << "\n");
			val <<= imm5;
			break;
		case 0b01: // LSR
			log(lgr::Logger::LogType::DEBUG, std::stringstream() << "LSR " << std::to_string((word)imm5) << "\n");
			val >>= imm5;
			break;
		case 0b10: // ASR
			log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ASR " << std::to_string((word)imm5) << "\n");
			val = ((signed int) val) >> imm5;
			break;
		case 0b11: // ROR
		{
			log(lgr::Logger::LogType::DEBUG, std::stringstream() << "ROR " << std::to_string((word)imm5) << "\n");
			word rot_bits = val & ((1 << imm5) - 1);
			rot_bits <<= (WORD_BITS - imm5);
			val >>= imm5;
			val &= (1 << (WORD_BITS - imm5)) - 1; // to be safe and remove bits that will be replaced
			val |= rot_bits;
			break;
		}
		default: // error
			lgr::log(lgr::Logger::LogType::ERROR, "Invalid shift: " + val);
	}
	return val;
}

// yoinked from https://github.com/unicorn-engine/ because I could not figure out carry/overflow for subtraction
static inline bool get_c_flag_add(word op1, word op2) {
	word dst_val = op1 + op2;
	return dst_val < op1;
}

static inline bool get_v_flag_add(word op1, word op2) {
	word dst_val = op1 + op2;
	return (op1 ^ op2 ^ -1) & (op1 ^ dst_val) & (1U << 31);
}

static inline bool get_c_flag_sub(word op1, word op2) {
	word dst_val = op1 - op2;
	return (((~op1 & op2) | (dst_val & (~op1 | op2))) & (1U << 31));
}

static inline bool get_v_flag_sub(word op1, word op2) {
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

word Emulator32bit::asm_format_o(byte opcode, bool s, int xd, int xn, int imm14) {
	return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xd) << JPart(5, xn) << JPart(1, 1) << JPart(14, imm14);
}

word Emulator32bit::asm_format_o(byte opcode, bool s, int xd, int xn, int xm, int shift, int imm5) {
	return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xd) << JPart(5, xn) << 1 << JPart(5, xm) << JPart(2, shift) << JPart(5, imm5) << 2;
}

word Emulator32bit::asm_format_o2(byte opcode, bool s, int xlo, int xhi, int xn, int xm) {
	return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xlo) << JPart(5, xhi) << 1 << JPart(5, xn) << JPart(5, xm) << 4;
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
	word xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word add_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = add_val + xn_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, get_c_flag_add(xn_val, add_val), 
				get_v_flag_add(xn_val, add_val));
	}

	log(lgr::Logger::LogType::DEBUG, std::stringstream() << "add " << std::to_string(add_val) << " " 
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_sub(word instr, EmulatorException& exception) {
	word xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word sub_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = xn_val - sub_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, get_c_flag_sub(xn_val, sub_val), 
				get_v_flag_sub(xn_val, sub_val));
	}

	log(lgr::Logger::LogType::DEBUG, std::stringstream() << "sub " << std::to_string(sub_val) << " " 
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_rsb(word instr, EmulatorException& exception) {
	word xd = _X1(instr);
	word sub_val = read_reg(_X2(instr), exception);
	word xn_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = xn_val - sub_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, get_c_flag_sub(xn_val, sub_val), 
				get_v_flag_sub(xn_val, sub_val));
	}

	log(lgr::Logger::LogType::DEBUG, std::stringstream() << "rsb " << std::to_string(xn_val) << " " 
			<< std::to_string(sub_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_adc(word instr, EmulatorException& exception) {
	bool c = test_bit(_pstate, C_FLAG);
	word xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word add_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = add_val + xn_val + c;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, 
				get_c_flag_add(xn_val + c, add_val) | get_c_flag_add(xn_val, c), 
				get_v_flag_add(xn_val + c, add_val) | get_v_flag_add(xn_val, c));
	}

	log(lgr::Logger::LogType::DEBUG, std::stringstream() << "adc " << std::to_string(add_val) << " " 
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_sbc(word instr, EmulatorException& exception) {
	bool borrow = test_bit(_pstate, C_FLAG);
	word xd = _X1(instr);
	word xn_val = read_reg(_X2(instr), exception);
	word sub_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = xn_val - sub_val - borrow;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0,
				get_c_flag_sub(xn_val - borrow, sub_val) | get_c_flag_sub(xn_val, borrow),
				get_v_flag_sub(xn_val - borrow, sub_val) | get_v_flag_sub(xn_val, borrow));
	}

	log(lgr::Logger::LogType::DEBUG, std::stringstream() << "sbc " << std::to_string(sub_val) << " " 
			<< std::to_string(xn_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_rsc(word instr, EmulatorException& exception) {
	bool borrow = test_bit(_pstate, C_FLAG);
	word xd = _X1(instr);
	word sub_val = read_reg(_X2(instr), exception);
	word xn_val = FORMAT_O__get_arg(instr, exception);
	word dst_val = xn_val - sub_val - borrow;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		set_NZCV(test_bit(dst_val, 31), dst_val == 0,
				get_c_flag_sub(xn_val - borrow, sub_val) | get_c_flag_sub(xn_val, borrow),
				get_v_flag_sub(xn_val - borrow, sub_val) | get_v_flag_sub(xn_val, borrow));
	}

	log(lgr::Logger::LogType::DEBUG, std::stringstream() << "rsc " << std::to_string(xn_val) << " " 
			<< std::to_string(sub_val) << " = " << std::to_string(dst_val) << "\n");
	write_reg(xd, dst_val, exception);
}

void Emulator32bit::_mul(word instr, EmulatorException& exception) {
	word xd = _X1(instr);
	dword xn_val = read_reg(_X2(instr), exception);
	dword xm_val = FORMAT_O__get_arg(instr, exception);
	dword dst_val = xn_val * xm_val;

	// check to update NZCV
	if (test_bit(instr, S_BIT)) { // ?S
		// according to https://developer.arm.com/documentation/dui0489/c/arm-and-thumb-instructions/multiply-instructions/mul--mla--and-mls
		// arm's MUL instruction does not set carry or overflow flags
		set_NZCV(test_bit(dst_val, 31), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
	}

	log(lgr::Logger::LogType::DEBUG, std::stringstream() << "mul " << std::to_string(xn_val) << " " 
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
		// according to https://developer.arm.com/documentation/dui0489/c/arm-and-thumb-instructions/multiply-instructions/mul--mla--and-mls
		// arm's UMULL instruction does not set carry or overflow flags
		set_NZCV(test_bit(dst_val, 63), dst_val == 0, test_bit(_pstate, C_FLAG), test_bit(_pstate, V_FLAG));
	}

	log(lgr::Logger::LogType::DEBUG, std::stringstream() << "mul " << std::to_string(xn_val) << " " 
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

	log(lgr::Logger::LogType::DEBUG, std::stringstream() << "mul " << std::to_string(xn_val) << " " 
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

}

void Emulator32bit::_orr(word instr, EmulatorException& exception) {}
void Emulator32bit::_eor(word instr, EmulatorException& exception) {}
void Emulator32bit::_bic(word instr, EmulatorException& exception) {}
void Emulator32bit::_lsl(word instr, EmulatorException& exception) {}
void Emulator32bit::_lsr(word instr, EmulatorException& exception) {}
void Emulator32bit::_asr(word instr, EmulatorException& exception) {}
void Emulator32bit::_ror(word instr, EmulatorException& exception) {}
void Emulator32bit::_cmp(word instr, EmulatorException& exception) {}
void Emulator32bit::_cmn(word instr, EmulatorException& exception) {}
void Emulator32bit::_tst(word instr, EmulatorException& exception) {}
void Emulator32bit::_teq(word instr, EmulatorException& exception) {}
void Emulator32bit::_mov(word instr, EmulatorException& exception) {}
void Emulator32bit::_mvn(word instr, EmulatorException& exception) {}
void Emulator32bit::_ldr(word instr, EmulatorException& exception) {}
void Emulator32bit::_ldrb(word instr, EmulatorException& exception) {}
void Emulator32bit::_ldrh(word instr, EmulatorException& exception) {}
void Emulator32bit::_str(word instr, EmulatorException& exception) {}
void Emulator32bit::_strb(word instr, EmulatorException& exception) {}
void Emulator32bit::_strh(word instr, EmulatorException& exception) {}
void Emulator32bit::_swp(word instr, EmulatorException& exception) {}
void Emulator32bit::_swpb(word instr, EmulatorException& exception) {}
void Emulator32bit::_swph(word instr, EmulatorException& exception) {}
void Emulator32bit::_b(word instr, EmulatorException& exception) {}
void Emulator32bit::_bl(word instr, EmulatorException& exception) {}
void Emulator32bit::_bx(word instr, EmulatorException& exception) {}
void Emulator32bit::_blx(word instr, EmulatorException& exception) {}
void Emulator32bit::_swi(word instr, EmulatorException& exception) {}