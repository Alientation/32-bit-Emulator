#pragma once

/// The single list of the instructions of the assembly language.
///
/// Generated from it:
///   - the `INSTRUCTION_<NAME>` token types (tokenizer.h),
///   - the keywords of the lexer (tokenizer.cpp),
///   - the table that tells the assembler how to encode each instruction (instructions.cpp,
///     `basm::instruction_spec`).
///
/// To add an instruction: add a row here, in the same place of the list as in the opcode list of
/// the emulator (emulator32bit/opcodes.h), and the opcode, disassembler and executor on that side.
/// Everything the assembler needs follows from the row.
///
/// Several rows can share an opcode. `cmp`, `cmn`, `tst` and `teq` (format O_NO_DEST) carry the
/// opcode of `sub`, `add`, `and` and `eor` and are encoded with S set and `xzr` as the
/// destination. `lsl`, `lsr`, `asr` and `ror` share `shift` (`b` is the ShiftType: 0 to 3), `umull` and
/// `smull` share `mull` (`b` is 1 for the signed one), `bx` and `blx` share `bx` (`b` is 1 for the
/// call).
///
///   F(X, NAME, text, allows_s, format, a, b)
///
///   NAME      suffix of the token type.
///   text      what is written in assembly.
///   allows_s  `<text>s` is the flag setting variant (`adds`).
///   format    InstructionFormat, how the operands are parsed and encoded.
///   a, b      what the format needs: the opcode for most of them, the width and the operation for
///             the atomics, for the loads and stores 1 in `b` if the address need not be aligned
///             (ldur...), for O1, O2 and B2 the variant of the opcode (see above), unused (0)
///             otherwise. These are expressions of the emulator library and
///             only evaluated where the table is built.
///
/// F is the macro that is applied to each row, X is passed through to it. This is how a list that
/// is itself being expanded by a macro argument (BASM_TOKEN_TYPES) can use it too.
///
/// HLT must stay the first row and RET the last one, `is_instruction` is a range check.
///
/// The floating point instructions come in a pair of rows, `.f32` and `.f64`, made by
/// BASM_FP_PAIR: `a` is the function (fpu::kBinaryFn_*, fpu::kUnaryFn_*, or 1 for the signaling
/// `vcmpe`) and `b` the precision bit. The conversions are written out, their names carry both
/// types. `vmov` is not an instruction but a pseudo instruction for the moves and the loading of
/// a constant (format VMOV).
///
/// Two rows of a pair: F(X, NAME_F32, text ".f32", ...) and F(X, NAME_F64, text ".f64", ...).
#define BASM_FP_PAIR(F, X, NAME, text, format, fn)                                                 \
    F(X, NAME##_F32, text ".f32", false, format, fn, 0)                                            \
    F(X, NAME##_F64, text ".f64", false, format, fn, 1)

#define BASM_INSTRUCTION_LIST(F, X)                                                                \
    F(X, HLT, "hlt", false, HLT, 0, 0)                                                             \
    F(X, NOP, "nop", false, NOP, 0, 0)                                                             \
    F(X, ADD, "add", true, O, Emulator32bit::_op_add, 0)                                           \
    F(X, SUB, "sub", true, O, Emulator32bit::_op_sub, 0)                                           \
    F(X, RSB, "rsb", true, O, Emulator32bit::_op_rsb, 0)                                           \
    F(X, ADC, "adc", true, O, Emulator32bit::_op_adc, 0)                                           \
    F(X, SBC, "sbc", true, O, Emulator32bit::_op_sbc, 0)                                           \
    F(X, RSC, "rsc", true, O, Emulator32bit::_op_rsc, 0)                                           \
    F(X, MUL, "mul", true, O, Emulator32bit::_op_mul, 0)                                           \
    F(X, UMULL, "umull", true, O2, Emulator32bit::_op_mull, 0)                                    \
    F(X, SMULL, "smull", true, O2, Emulator32bit::_op_mull, 1)                                    \
    BASM_FP_PAIR(F, X, VADD, "vadd", V2, fpu::kBinaryFn_add)                                       \
    BASM_FP_PAIR(F, X, VSUB, "vsub", V2, fpu::kBinaryFn_sub)                                       \
    BASM_FP_PAIR(F, X, VMUL, "vmul", V2, fpu::kBinaryFn_mul)                                       \
    BASM_FP_PAIR(F, X, VDIV, "vdiv", V2, fpu::kBinaryFn_div)                                       \
    BASM_FP_PAIR(F, X, VMIN, "vmin", V2, fpu::kBinaryFn_min)                                       \
    BASM_FP_PAIR(F, X, VMAX, "vmax", V2, fpu::kBinaryFn_max)                                       \
    BASM_FP_PAIR(F, X, VABS, "vabs", V1, fpu::kUnaryFn_abs)                                        \
    BASM_FP_PAIR(F, X, VNEG, "vneg", V1, fpu::kUnaryFn_neg)                                        \
    BASM_FP_PAIR(F, X, VSQRT, "vsqrt", V1, fpu::kUnaryFn_sqrt)                                     \
    BASM_FP_PAIR(F, X, VRINT, "vrint", V1, fpu::kUnaryFn_rint)                                     \
    BASM_FP_PAIR(F, X, VRINTZ, "vrintz", V1, fpu::kUnaryFn_rintz)                                  \
    BASM_FP_PAIR(F, X, VRINTM, "vrintm", V1, fpu::kUnaryFn_rintm)                                  \
    BASM_FP_PAIR(F, X, VRINTP, "vrintp", V1, fpu::kUnaryFn_rintp)                                  \
    BASM_FP_PAIR(F, X, VRINTA, "vrinta", V1, fpu::kUnaryFn_rinta)                                  \
    F(X, VCVT_S32_F32, "vcvt.s32.f32", false, V1, fpu::kUnaryFn_tos32, 0)                          \
    F(X, VCVT_S32_F64, "vcvt.s32.f64", false, V1, fpu::kUnaryFn_tos32, 1)                          \
    F(X, VCVT_U32_F32, "vcvt.u32.f32", false, V1, fpu::kUnaryFn_tou32, 0)                          \
    F(X, VCVT_U32_F64, "vcvt.u32.f64", false, V1, fpu::kUnaryFn_tou32, 1)                          \
    F(X, VCVTR_S32_F32, "vcvtr.s32.f32", false, V1, fpu::kUnaryFn_tos32r, 0)                       \
    F(X, VCVTR_S32_F64, "vcvtr.s32.f64", false, V1, fpu::kUnaryFn_tos32r, 1)                       \
    F(X, VCVTR_U32_F32, "vcvtr.u32.f32", false, V1, fpu::kUnaryFn_tou32r, 0)                       \
    F(X, VCVTR_U32_F64, "vcvtr.u32.f64", false, V1, fpu::kUnaryFn_tou32r, 1)                       \
    F(X, VCVT_F32_S32, "vcvt.f32.s32", false, V1, fpu::kUnaryFn_froms32, 0)                        \
    F(X, VCVT_F64_S32, "vcvt.f64.s32", false, V1, fpu::kUnaryFn_froms32, 1)                        \
    F(X, VCVT_F32_U32, "vcvt.f32.u32", false, V1, fpu::kUnaryFn_fromu32, 0)                        \
    F(X, VCVT_F64_U32, "vcvt.f64.u32", false, V1, fpu::kUnaryFn_fromu32, 1)                        \
    F(X, VCVT_F64_F32, "vcvt.f64.f32", false, V1, fpu::kUnaryFn_fcvt, 0)                           \
    F(X, VCVT_F32_F64, "vcvt.f32.f64", false, V1, fpu::kUnaryFn_fcvt, 1)                           \
    BASM_FP_PAIR(F, X, VCMP, "vcmp", V3, 0)                                                        \
    BASM_FP_PAIR(F, X, VCMPE, "vcmpe", V3, 1)                                                      \
    BASM_FP_PAIR(F, X, VMOV, "vmov", VMOV, 0)                                                      \
    F(X, AND, "and", true, O, Emulator32bit::_op_and, 0)                                           \
    F(X, ORR, "orr", true, O, Emulator32bit::_op_orr, 0)                                           \
    F(X, EOR, "eor", true, O, Emulator32bit::_op_eor, 0)                                           \
    F(X, BIC, "bic", true, O, Emulator32bit::_op_bic, 0)                                           \
    F(X, LSL, "lsl", true, O1, Emulator32bit::_op_shift, 0)                                            \
    F(X, LSR, "lsr", true, O1, Emulator32bit::_op_shift, 1)                                            \
    F(X, ASR, "asr", true, O1, Emulator32bit::_op_shift, 2)                                            \
    F(X, ROR, "ror", true, O1, Emulator32bit::_op_shift, 3)                                            \
    F(X, CMP, "cmp", false, O_NO_DEST, Emulator32bit::_op_sub, 0)                                  \
    F(X, CMN, "cmn", false, O_NO_DEST, Emulator32bit::_op_add, 0)                                  \
    F(X, TST, "tst", false, O_NO_DEST, Emulator32bit::_op_and, 0)                                  \
    F(X, TEQ, "teq", false, O_NO_DEST, Emulator32bit::_op_eor, 0)                                  \
    F(X, MOV, "mov", true, O3, Emulator32bit::_op_mov, 0)                                          \
    F(X, MVN, "mvn", true, O3, Emulator32bit::_op_mvn, 0)                                          \
    F(X, LDR, "ldr", false, M, Emulator32bit::_op_ldr, 0)                                          \
    F(X, STR, "str", false, M, Emulator32bit::_op_str, 0)                                          \
    F(X, SWP, "swp", false, ATOMIC, Emulator32bit::kAtomicWidth_word,                              \
      Emulator32bit::kAtomicId_swp)                                                                \
    F(X, LDRB, "ldrb", false, M, Emulator32bit::_op_ldrb, 0)                                       \
    F(X, STRB, "strb", false, M, Emulator32bit::_op_strb, 0)                                       \
    F(X, SWPB, "swpb", false, ATOMIC, Emulator32bit::kAtomicWidth_byte,                            \
      Emulator32bit::kAtomicId_swp)                                                                \
    F(X, LDRH, "ldrh", false, M, Emulator32bit::_op_ldrh, 0)                                       \
    F(X, STRH, "strh", false, M, Emulator32bit::_op_strh, 0)                                       \
    F(X, SWPH, "swph", false, ATOMIC, Emulator32bit::kAtomicWidth_hword,                           \
      Emulator32bit::kAtomicId_swp)                                                                \
    F(X, LDUR, "ldur", false, M, Emulator32bit::_op_ldr, 1)                                        \
    F(X, STUR, "stur", false, M, Emulator32bit::_op_str, 1)                                        \
    F(X, LDURH, "ldurh", false, M, Emulator32bit::_op_ldrh, 1)                                     \
    F(X, STURH, "sturh", false, M, Emulator32bit::_op_strh, 1)                                     \
    F(X, UDIV, "udiv", true, O, Emulator32bit::_op_udiv, 0)                                        \
    F(X, SDIV, "sdiv", true, O, Emulator32bit::_op_sdiv, 0)                                        \
    F(X, CSEL, "csel", false, CSEL, Emulator32bit::_op_csel, Emulator32bit::kCselId_csel)          \
    F(X, CSINC, "csinc", false, CSEL, Emulator32bit::_op_csel, Emulator32bit::kCselId_csinc)       \
    F(X, CSINV, "csinv", false, CSEL, Emulator32bit::_op_csel, Emulator32bit::kCselId_csinv)       \
    F(X, CSNEG, "csneg", false, CSEL, Emulator32bit::_op_csel, Emulator32bit::kCselId_csneg)       \
    F(X, CSET, "cset", false, CSET, Emulator32bit::_op_csel, Emulator32bit::kCselId_csinc)         \
    F(X, CSETM, "csetm", false, CSET, Emulator32bit::_op_csel, Emulator32bit::kCselId_csinv)       \
    F(X, CINC, "cinc", false, CINC, Emulator32bit::_op_csel, Emulator32bit::kCselId_csinc)         \
    F(X, CINV, "cinv", false, CINC, Emulator32bit::_op_csel, Emulator32bit::kCselId_csinv)         \
    F(X, CNEG, "cneg", false, CINC, Emulator32bit::_op_csel, Emulator32bit::kCselId_csneg)         \
    F(X, MSR, "msr", false, MSR, 0, 0)                                                             \
    F(X, MRS, "mrs", false, MRS, 0, 0)                                                             \
    F(X, TLBI, "tlbi", false, TLBI, 0, 0)                                                          \
    F(X, ERET, "eret", false, ERET, 0, 0)                                                          \
    F(X, WFI, "wfi", false, WFI, 0, 0)                                                             \
    F(X, BRK, "brk", false, BRK, 0, 0)                                                             \
    F(X, SXTB, "sxtb", false, UNARY, Emulator32bit::kUnaryId_sxtb, 0)                              \
    F(X, SXTH, "sxth", false, UNARY, Emulator32bit::kUnaryId_sxth, 0)                              \
    F(X, UXTB, "uxtb", false, UNARY, Emulator32bit::kUnaryId_uxtb, 0)                              \
    F(X, UXTH, "uxth", false, UNARY, Emulator32bit::kUnaryId_uxth, 0)                              \
    F(X, CLZ, "clz", false, UNARY, Emulator32bit::kUnaryId_clz, 0)                                 \
    F(X, REV, "rev", false, UNARY, Emulator32bit::kUnaryId_rev, 0)                                 \
    F(X, REV16, "rev16", false, UNARY, Emulator32bit::kUnaryId_rev16, 0)                           \
    F(X, LDADD, "ldadd", false, ATOMIC, Emulator32bit::kAtomicWidth_word,                          \
      Emulator32bit::kAtomicId_ldadd)                                                              \
    F(X, LDADDB, "ldaddb", false, ATOMIC, Emulator32bit::kAtomicWidth_byte,                        \
      Emulator32bit::kAtomicId_ldadd)                                                              \
    F(X, LDADDH, "ldaddh", false, ATOMIC, Emulator32bit::kAtomicWidth_hword,                       \
      Emulator32bit::kAtomicId_ldadd)                                                              \
    F(X, LDCLR, "ldclr", false, ATOMIC, Emulator32bit::kAtomicWidth_word,                          \
      Emulator32bit::kAtomicId_ldclr)                                                              \
    F(X, LDCLRB, "ldclrb", false, ATOMIC, Emulator32bit::kAtomicWidth_byte,                        \
      Emulator32bit::kAtomicId_ldclr)                                                              \
    F(X, LDCLRH, "ldclrh", false, ATOMIC, Emulator32bit::kAtomicWidth_hword,                       \
      Emulator32bit::kAtomicId_ldclr)                                                              \
    F(X, LDSET, "ldset", false, ATOMIC, Emulator32bit::kAtomicWidth_word,                          \
      Emulator32bit::kAtomicId_ldset)                                                              \
    F(X, LDSETB, "ldsetb", false, ATOMIC, Emulator32bit::kAtomicWidth_byte,                        \
      Emulator32bit::kAtomicId_ldset)                                                              \
    F(X, LDSETH, "ldseth", false, ATOMIC, Emulator32bit::kAtomicWidth_hword,                       \
      Emulator32bit::kAtomicId_ldset)                                                              \
    F(X, B, "b", false, B1, Emulator32bit::_op_b, 0)                                               \
    F(X, BL, "bl", false, B1, Emulator32bit::_op_bl, 0)                                            \
    F(X, BX, "bx", false, B2, Emulator32bit::_op_bx, 0)                                            \
    F(X, BLX, "blx", false, B2, Emulator32bit::_op_bx, 1)                                         \
    F(X, SWI, "swi", false, SWI, Emulator32bit::_op_swi, 0)                                        \
    F(X, ADRP, "adrp", false, M1, Emulator32bit::_op_adrp, 0)                                      \
    F(X, ADR, "adr", false, M1, Emulator32bit::_op_adr, 0)                                         \
    F(X, RET, "ret", false, RET, 0, 0)
