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
///   F(X, NAME, text, allows_s, format, a, b)
///
///   NAME      suffix of the token type.
///   text      what is written in assembly.
///   allows_s  `<text>s` is the flag setting variant (`adds`).
///   format    InstructionFormat, how the operands are parsed and encoded.
///   a, b      what the format needs: the opcode for most of them, the width and the operation for
///             the atomics, unused (0) otherwise. These are expressions of the emulator library and
///             only evaluated where the table is built.
///
/// F is the macro that is applied to each row, X is passed through to it. This is how a list that
/// is itself being expanded by a macro argument (BASM_TOKEN_TYPES) can use it too.
///
/// HLT must stay the first row and RET the last one, `is_instruction` is a range check.
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
    F(X, UMULL, "umull", true, O2, Emulator32bit::_op_umull, 0)                                    \
    F(X, SMULL, "smull", true, O2, Emulator32bit::_op_smull, 0)                                    \
    F(X, VABS, "vabs.f32", false, UNIMPLEMENTED, 0, 0)                                             \
    F(X, VNEG, "vneg.f32", false, UNIMPLEMENTED, 0, 0)                                             \
    F(X, VSQRT, "vsqrt.f32", false, UNIMPLEMENTED, 0, 0)                                           \
    F(X, VADD, "vadd.f32", false, UNIMPLEMENTED, 0, 0)                                             \
    F(X, VSUB, "vsub.f32", false, UNIMPLEMENTED, 0, 0)                                             \
    F(X, VDIV, "vdiv.f32", false, UNIMPLEMENTED, 0, 0)                                             \
    F(X, VMUL, "vmul.f32", false, UNIMPLEMENTED, 0, 0)                                             \
    F(X, VCMP, "vcmp.f32", false, UNIMPLEMENTED, 0, 0)                                             \
    F(X, VSEL, "vsel.f32", false, UNIMPLEMENTED, 0, 0)                                             \
    F(X, VCINT, "vcint.u32.f32", false, UNIMPLEMENTED, 0, 0)                                       \
    F(X, VCFLO, "vcflo.u32.f32", false, UNIMPLEMENTED, 0, 0)                                       \
    F(X, VMOV, "vmov.f32", false, UNIMPLEMENTED, 0, 0)                                             \
    F(X, AND, "and", true, O, Emulator32bit::_op_and, 0)                                           \
    F(X, ORR, "orr", true, O, Emulator32bit::_op_orr, 0)                                           \
    F(X, EOR, "eor", true, O, Emulator32bit::_op_eor, 0)                                           \
    F(X, BIC, "bic", true, O, Emulator32bit::_op_bic, 0)                                           \
    F(X, LSL, "lsl", true, O1, Emulator32bit::_op_lsl, 0)                                          \
    F(X, LSR, "lsr", true, O1, Emulator32bit::_op_lsr, 0)                                          \
    F(X, ASR, "asr", true, O1, Emulator32bit::_op_asr, 0)                                          \
    F(X, ROR, "ror", true, O1, Emulator32bit::_op_ror, 0)                                          \
    F(X, CMP, "cmp", false, O_NO_DEST, Emulator32bit::_op_cmp, 0)                                  \
    F(X, CMN, "cmn", false, O_NO_DEST, Emulator32bit::_op_cmn, 0)                                  \
    F(X, TST, "tst", false, O_NO_DEST, Emulator32bit::_op_tst, 0)                                  \
    F(X, TEQ, "teq", false, O_NO_DEST, Emulator32bit::_op_teq, 0)                                  \
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
    F(X, BLX, "blx", false, B2, Emulator32bit::_op_blx, 0)                                         \
    F(X, SWI, "swi", false, SWI, Emulator32bit::_op_swi, 0)                                        \
    F(X, ADRP, "adrp", false, M1, Emulator32bit::_op_adrp, 0)                                      \
    F(X, RET, "ret", false, RET, 0, 0)
