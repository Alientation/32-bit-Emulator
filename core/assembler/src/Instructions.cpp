#include <assembler/Assembler.h>

#include <emulator32bit/Emulator32bit.h>
#include <emulator32bit/Emulator32bitUtil.h>
#include <util/Logger.h>

#include <string>

/**
 * @brief
 *
 * add x1, x2, x3
 * add x1, x2, #40
 * add x1, x2, x3, lsl 4
 * add x1, x2, :lo12:symbol
 * add x1, x2, :lo12:symbol + 4
 *
 * @param 						tokenI: Reference to current token index
 */
void Assembler::_add(int& tokenI) {
	consume(tokenI);

}

/**
 * @brief
 *
 * @param 						tokenI: Reference to current token index
 */
void Assembler::_sub(int& tokenI) {

}

/**
 * @brief
 *
 * @param 						tokenI: Reference to current token index
 */
void Assembler::_rsb(int& tokenI) {

}

/**
 * @brief
 *
 * @param 						tokenI: Reference to current token index
 */
void Assembler::_adc(int& tokenI) {

}

/**
 * @brief
 *
 * @param 						tokenI: Reference to current token index
 */
void Assembler::_sbc(int& tokenI) {

}

void Assembler::_rsc(int& tokenI) {

}

void Assembler::_mul(int& tokenI) {

}

void Assembler::_umull(int& tokenI) {

}

void Assembler::_smull(int& tokenI) {

}

void Assembler::_vabs_f32(int& tokenI) {

}

void Assembler::_vneg_f32(int& tokenI) {

}

void Assembler::_vsqrt_f32(int& tokenI) {

}

void Assembler::_vadd_f32(int& tokenI) {

}

void Assembler::_vsub_f32(int& tokenI) {

}

void Assembler::_vdiv_f32(int& tokenI) {

}

void Assembler::_vmul_f32(int& tokenI) {

}

void Assembler::_vcmp_f32(int& tokenI) {

}

void Assembler::_vsel_f32(int& tokenI) {

}

void Assembler::_vcint_u32_f32(int& tokenI) {

}

void Assembler::_vcint_s32_f32(int& tokenI) {

}

void Assembler::_vcflo_u32_f32(int& tokenI) {

}

void Assembler::_vcflo_s32_f32(int& tokenI) {

}

void Assembler::_vmov_f32(int& tokenI) {

}

void Assembler::_and(int& tokenI) {

}

void Assembler::_orr(int& tokenI) {

}

void Assembler::_eor(int& tokenI) {

}

void Assembler::_bic(int& tokenI) {

}

void Assembler::_lsl(int& tokenI) {

}

void Assembler::_lsr(int& tokenI) {

}

void Assembler::_asr(int& tokenI) {

}

void Assembler::_ror(int& tokenI) {

}

void Assembler::_cmp(int& tokenI) {

}

void Assembler::_cmn(int& tokenI) {

}

void Assembler::_tst(int& tokenI) {

}

void Assembler::_teq(int& tokenI) {

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
