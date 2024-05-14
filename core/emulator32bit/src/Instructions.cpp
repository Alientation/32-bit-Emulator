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

// helper functions
static inline word calc_shift(word val, byte shift_type, byte imm5) {
	switch(shift_type) {
		case 0b00: // LSL
			val <<= imm5;
			break;
		case 0b01: // LSR
			val >>= imm5;
			break;
		case 0b10: // ASR
			val = ((signed int) val) >> imm5;
			break;
		case 0b11: // ROR
		{
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
	word xn = _X2(instr);
	word xn_val = read_reg(xn, exception);
	word add = 0;
	if (test_bit(instr, 14)) { // ?imm
		// imm
		add = bitfield_u32(instr, 0, 14);
	} else {
		add = calc_shift(read_reg(_X3(instr), exception), bitfield_u32(instr, 7, 2), bitfield_u32(instr, 2, 2));		
	}

	word val = add + xn_val;

	std::cout << "_ADD: " << val << "\n";

	// check to update NZCV
	if (test_bit(instr, 25)) { // ?S
		bool N = test_bit(val, 31);
		bool Z = val == 0;
		bool C = (xn_val + add) < val;
		bool V = C != test_bit((set_bit(xn_val, 31, 0) + set_bit(add, 31, 0)), 31);

		set_NZCV(N, Z, C, V);
	}

	log(lgr::Logger::LogType::DEBUG, std::stringstream() << "add " << std::to_string(add) << " " << std::to_string(read_reg(xn, exception)) << " = " << std::to_string(val) << "\n");

	write_reg(xd, val, exception);
}

word Emulator32bit::asm_add(bool s, int xd, int xn, int imm14) {
	return Joiner() << JPart(6, _op_add) << JPart(1, s) << JPart(5, xd) << JPart(5, xn) << JPart(1, 1) << JPart(14, imm14);
}

word Emulator32bit::asm_add(bool s, int xd, int xn, int xm, int shift, int imm5) {
	return Joiner() << JPart(6, _op_add) << JPart(1, s) << JPart(5, xd) << JPart(5, xn) << JPart(1, 0) << JPart(5, xm) << JPart(2, shift) << JPart(5, imm5) << 2;
}

void Emulator32bit::_sub(word instr, EmulatorException& exception) {



	// check to update NZCV	
}

void Emulator32bit::_rsb(word instr, EmulatorException& exception) {}
void Emulator32bit::_adc(word instr, EmulatorException& exception) {}
void Emulator32bit::_sbc(word instr, EmulatorException& exception) {}
void Emulator32bit::_rsc(word instr, EmulatorException& exception) {}
void Emulator32bit::_mul(word instr, EmulatorException& exception) {}
void Emulator32bit::_umull(word instr, EmulatorException& exception) {}
void Emulator32bit::_smull(word instr, EmulatorException& exception) {}
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
void Emulator32bit::_and(word instr, EmulatorException& exception) {}
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