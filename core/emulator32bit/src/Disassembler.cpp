#include "emulator32bit/emulator32bit.h"
#include "util/loggerv2.h"

#define UNUSED(x) (void)(x)

std::string disassemble_register(int reg)
{
	if (reg == SP) {
		return "sp";
	} else if (reg == XZR) {
		return "xzr";
	} else {
		return "x" + std::to_string(reg);
	}
}

std::string disassemble_shift(word instruction)
{
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
std::string disassemble_condition(Emulator32bit::ConditionCode condition)
{
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

std::string disassemble_format_b2(word instruction, std::string op)
{
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

std::string disassemble_format_b1(word instruction, std::string op)
{
	std::string disassemble = op;
	Emulator32bit::ConditionCode condition = (Emulator32bit::ConditionCode) bitfield_u32(instruction, 22, 4);
	if (condition != Emulator32bit::ConditionCode::AL) {
		disassemble += "." + disassemble_condition(condition);
	}
	disassemble += " #" + std::to_string(bitfield_s32(instruction, 0, 22));
	return disassemble;
}

std::string disassemble_format_m2(word instruction, std::string op)
{
	std::string disassemble = op + " ";
	disassemble += disassemble_register(bitfield_u32(instruction, 20, 5));
	disassemble += ", ";
	disassemble += "#" + std::to_string(bitfield_u32(instruction, 0, 20));
	return disassemble;
}

std::string disassemble_format_m1(word instruction, std::string op)
{
	std::string disassemble = op + " ";
	disassemble += disassemble_register(bitfield_u32(instruction, 20, 5));
	disassemble += ", ";

	disassemble += disassemble_register(bitfield_u32(instruction, 15, 5));
	disassemble += ", [";

	disassemble += disassemble_register(bitfield_u32(instruction, 9, 5));
	disassemble += "]";
	return disassemble;
}

std::string disassemble_format_m(word instruction, std::string op)
{
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
		ERROR_SS(std::stringstream() << "disassemble_format_m() - Invalid addressing mode "
				"in the disassembly of instruction (" << op << ") " << instruction);
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

std::string disassemble_format_o3(word instruction, std::string op)
{
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

std::string disassemble_format_o2(word instruction, std::string op)
{
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

std::string disassemble_format_o1(word instruction, std::string op)
{
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

std::string disassemble_format_o(word instruction, std::string op)
{
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

std::string disassemble_add(word instruction)
{
	return disassemble_format_o(instruction, "add");
}

std::string disassemble_sub(word instruction)
{
	return disassemble_format_o(instruction, "sub");
}

std::string disassemble_rsb(word instruction)
{
	return disassemble_format_o(instruction, "rsb");
}

std::string disassemble_adc(word instruction)
{
	return disassemble_format_o(instruction, "adc");
}

std::string disassemble_sbc(word instruction)
{
	return disassemble_format_o(instruction, "sbc");
}

std::string disassemble_rsc(word instruction)
{
	return disassemble_format_o(instruction, "rsc");
}

std::string disassemble_mul(word instruction)
{
	return disassemble_format_o(instruction, "mul");
}

std::string disassemble_umull(word instruction)
{
	return disassemble_format_o2(instruction, "umull");
}

std::string disassemble_smull(word instruction)
{
	return disassemble_format_o2(instruction, "smull");
}

std::string disassemble_vabs_f32(word instruction)
{
	UNUSED(instruction);
	return "UNIMPLEMENTED";
}

std::string disassemble_vneg_f32(word instruction)
{
	UNUSED(instruction);
	return "UNIMPLEMENTED";
}

std::string disassemble_vsqrt_f32(word instruction)
{
	UNUSED(instruction);
	return "UNIMPLEMENTED";
}

std::string disassemble_vadd_f32(word instruction)
{
	UNUSED(instruction);
	return "UNIMPLEMENTED";
}

std::string disassemble_vsub_f32(word instruction)
{
	UNUSED(instruction);
	return "UNIMPLEMENTED";
}

std::string disassemble_vdiv_f32(word instruction)
{
	UNUSED(instruction);
	return "UNIMPLEMENTED";
}

std::string disassemble_vmul_f32(word instruction)
{
	UNUSED(instruction);
	return "UNIMPLEMENTED";
}

std::string disassemble_vcmp_f32(word instruction)
{
	UNUSED(instruction);
	return "UNIMPLEMENTED";
}

std::string disassemble_vsel_f32(word instruction)
{
	UNUSED(instruction);
	return "UNIMPLEMENTED";
}

std::string disassemble_vcint_u32_f32(word instruction)
{
	UNUSED(instruction);
	return "UNIMPLEMENTED";
}

std::string disassemble_vcint_s32_f32(word instruction)
{
	UNUSED(instruction);
	return "UNIMPLEMENTED";
}

std::string disassemble_vcflo_u32_f32(word instruction)
{
	UNUSED(instruction);
	return "UNIMPLEMENTED";
}

std::string disassemble_vcflo_s32_f32(word instruction)
{
	UNUSED(instruction);
	return "UNIMPLEMENTED";
}

std::string disassemble_vmov_f32(word instruction)
{
	UNUSED(instruction);
	return "UNIMPLEMENTED";
}

std::string disassemble_and(word instruction)
{
	return disassemble_format_o(instruction, "and");
}

std::string disassemble_orr(word instruction)
{
	return disassemble_format_o(instruction, "orr");
}

std::string disassemble_eor(word instruction)
{
	return disassemble_format_o(instruction, "eor");
}

std::string disassemble_bic(word instruction)
{
	return disassemble_format_o(instruction, "bic");
}

std::string disassemble_lsl(word instruction)
{
	return disassemble_format_o1(instruction, "lsl");
}

std::string disassemble_lsr(word instruction)
{
	return disassemble_format_o1(instruction, "lsr");
}

std::string disassemble_asr(word instruction)
{
	return disassemble_format_o1(instruction, "asr");
}

std::string disassemble_ror(word instruction)
{
	return disassemble_format_o1(instruction, "ror");
}

std::string disassemble_cmp(word instruction)
{
	std::string disassemble = disassemble_format_o(instruction, "cmp");
	return "cmp" + disassemble.substr(disassemble.find_first_of("xzr")+4);
}

std::string disassemble_cmn(word instruction)
{
	std::string disassemble = disassemble_format_o(instruction, "cmn");
	return "cmn" + disassemble.substr(disassemble.find_first_of("xzr")+4);
}

std::string disassemble_tst(word instruction)
{
	std::string disassemble = disassemble_format_o(instruction, "tst");
	return "tst" + disassemble.substr(disassemble.find_first_of("xzr")+4);
}

std::string disassemble_teq(word instruction)
{
	std::string disassemble = disassemble_format_o(instruction, "teq");
	return "teq" + disassemble.substr(disassemble.find_first_of("xzr")+4);
}

std::string disassemble_mov(word instruction)
{
	return disassemble_format_o3(instruction, "mov");
}

std::string disassemble_mvn(word instruction)
{
	return disassemble_format_o3(instruction, "mvn");
}

std::string disassemble_ldr(word instruction)
{
	return disassemble_format_m(instruction, "ldr");
}

std::string disassemble_str(word instruction)
{
	return disassemble_format_m(instruction, "str");
}

std::string disassemble_swp(word instruction)
{
	return disassemble_format_m1(instruction, "swp");
}

std::string disassemble_ldrb(word instruction)
{
	return disassemble_format_m(instruction, "ldrb");
}

std::string disassemble_strb(word instruction)
{
	return disassemble_format_m(instruction, "strb");
}

std::string disassemble_swpb(word instruction)
{
	return disassemble_format_m1(instruction, "swpb");
}

std::string disassemble_ldrh(word instruction)
{
	return disassemble_format_m(instruction, "ldrh");
}

std::string disassemble_strh(word instruction)
{
	return disassemble_format_m(instruction, "strh");
}

std::string disassemble_swph(word instruction)
{
	return disassemble_format_m1(instruction, "swph");
}

std::string disassemble_b(word instruction)
{
	return disassemble_format_b1(instruction, "b");
}

std::string disassemble_bl(word instruction)
{
	return disassemble_format_b1(instruction, "bl");
}

std::string disassemble_bx(word instruction)
{
	return disassemble_format_b2(instruction, "bx");
}

std::string disassemble_blx(word instruction)
{
	return disassemble_format_b2(instruction, "blx");
}

std::string disassemble_swi(word instruction)
{
	return disassemble_format_b1(instruction, "swi");
}

std::string disassemble_adrp(word instruction)
{
	return disassemble_format_m2(instruction, "adrp");
}

std::string disassemble_hlt(word instruction)
{
	UNUSED(instruction);
	return "hlt";
}

/* construct disassembler instruction mapping */
typedef std::string (*DisassemblerFunction)(word);
DisassemblerFunction _disassembler_instructions[64] =
{
	disassemble_hlt,
	disassemble_add,
	disassemble_sub,
	disassemble_rsb,
	disassemble_adc,
	disassemble_sbc,
	disassemble_rsc,
	disassemble_mul,
	disassemble_umull,
	disassemble_smull,
	disassemble_vabs_f32,
	disassemble_vneg_f32,
	disassemble_vsqrt_f32,
	disassemble_vadd_f32,
	disassemble_vsub_f32,
	disassemble_vdiv_f32,
	disassemble_vmul_f32,
	disassemble_vcmp_f32,
	disassemble_vsel_f32,
	disassemble_vcint_u32_f32,								/* slight discrepency with the emulator. check later */
	// disassemble_vcint_s32_f32,								/* slight discrepency with the emulator. check later */
	disassemble_vcflo_u32_f32,								/* slight discrepency with the emulator. check later */
	// disassemble_vcflo_s32_f32,								/* slight discrepency with the emulator. check later */
	disassemble_vmov_f32,
	disassemble_and,
	disassemble_orr,
	disassemble_eor,
	disassemble_bic,
	disassemble_lsl,
	disassemble_lsr,
	disassemble_asr,
	disassemble_ror,
	disassemble_cmp,
	disassemble_cmn,
	disassemble_tst,
	disassemble_teq,
	disassemble_mov,
	disassemble_mvn,
	disassemble_ldr,
	disassemble_ldrb,
	disassemble_ldrh,
	disassemble_str,
	disassemble_strb,
	disassemble_strh,
	disassemble_swp,
	disassemble_swpb,
	disassemble_swph,
	disassemble_b,
	disassemble_bl,
	disassemble_bx,
	disassemble_blx,
	disassemble_swi,

	disassemble_adrp,
};

std::string disassemble_instr(word instr)
{
	return (*_disassembler_instructions[bitfield_u32(instr, 26, 6)])(instr);
}