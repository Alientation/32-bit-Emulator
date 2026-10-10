#include "disassembler/disassembler.h"

#include "emulator32bit/alu.h"
#include "emulator32bit/encoding.h"
#include "emulator32bit/fpu.h"
#include "emulator32bit/opcodes.h"
#include "util/common.h"
#include "util/logger.h"

static std::string disassemble_gpr(word instruction, U8 offset)
{
    return register_name(bitfield_unsigned(instruction, offset, 5));
}

/// The `lsl #n` operand of a format M or O instruction with a shifted register.
static std::string disassemble_shift_operand(word instruction)
{
    std::string disassemble;
    switch (static_cast<ShiftType>(bitfield_unsigned<7, 2>(instruction)))
    {
    case ShiftType::SHIFT_LSL:
        disassemble = "lsl ";
        break;
    case ShiftType::SHIFT_LSR:
        disassemble = "lsr ";
        break;
    case ShiftType::SHIFT_ASR:
        disassemble = "asr ";
        break;
    case ShiftType::SHIFT_ROR:
        disassemble = "ror ";
        break;
    default:
        disassemble = "ERROR_SHIFT";
        break;
    }

    disassemble += std::to_string(bitfield_unsigned<2, 5>(instruction));
    return disassemble;
}

static std::string disassemble_condition(ConditionCode condition)
{
    switch (condition)
    {
    case ConditionCode::EQ:
        return "eq";
    case ConditionCode::NE:
        return "ne";
    case ConditionCode::CS:
        return "cs";
    case ConditionCode::CC:
        return "cc";
    case ConditionCode::MI:
        return "mi";
    case ConditionCode::PL:
        return "pl";
    case ConditionCode::VS:
        return "vs";
    case ConditionCode::VC:
        return "vc";
    case ConditionCode::HI:
        return "hi";
    case ConditionCode::LS:
        return "ls";
    case ConditionCode::GE:
        return "ge";
    case ConditionCode::LT:
        return "lt";
    case ConditionCode::GT:
        return "gt";
    case ConditionCode::LE:
        return "le";
    case ConditionCode::AL:
        return "al";
    case ConditionCode::NV:
        return "nv";
    }
    return "ERROR_CONDITION";
}

static std::string disassemble_format_b2(word instruction, std::string op)
{
    std::string disassemble = op;
    ConditionCode condition = (ConditionCode) bitfield_unsigned<22, 4>(instruction);
    if (condition != ConditionCode::AL)
    {
        disassemble += "." + disassemble_condition(condition);
    }

    if (op == "bx" && bitfield_unsigned<17, 5>(instruction) == 29)
    {
        disassemble = "ret";
    }
    else
    {
        disassemble += " " + disassemble_gpr(instruction, 17);
    }

    return disassemble;
}

static std::string disassemble_format_b1(word instruction, std::string op)
{
    std::string disassemble = op;
    ConditionCode condition = (ConditionCode) bitfield_unsigned<22, 4>(instruction);
    if (condition != ConditionCode::AL)
    {
        disassemble += "." + disassemble_condition(condition);
    }
    disassemble += " " + std::to_string(sword(bitfield_signed<0, 22>(instruction)));
    return disassemble;
}

static std::string disassemble_format_m1(word instruction, std::string op)
{
    std::string disassemble = op + " ";
    disassemble += disassemble_gpr(instruction, 20);
    disassemble += ", ";

    int32_t offset = bitfield_unsigned<0, 20>(instruction);
    if (test_bit<kInstructionUpdateFlagBit>(instruction))
    {
        offset -= 1 << 20;
    }
    disassemble += std::to_string(offset);
    return disassemble;
}

/// @param op the name of the instruction, `ldr` for the aligned and the unaligned form
/// @param has_unaligned_form whether the unaligned form exists (ldur, ldurh, stur, sturh)
static std::string disassemble_format_m(word instruction, std::string op,
                                        bool has_unaligned_form = true)
{
    std::string disassemble = op;

    const U8 adr_mode = bitfield_unsigned<0, 2>(instruction);
    const bool unaligned = adr_mode == U8(Encoding::AddrType::ADDR_UNALIGNED);
    if (unaligned && has_unaligned_form)
    {
        // ldr -> ldur, strh -> sturh
        disassemble.insert(disassemble.begin() + 2, 'u');
    }

    if (test_bit<25>(instruction))
    {
        // ldrb -> ldrsb, ldurh -> ldursh
        disassemble.insert(disassemble.begin() + (unaligned && has_unaligned_form ? 4 : 3), 's');
    }
    disassemble += " ";

    disassemble += disassemble_gpr(instruction, 20);
    disassemble += ", ";

    disassemble += "[";
    disassemble += disassemble_gpr(instruction, 15);
    if (adr_mode != U8(Encoding::AddrType::ADDR_PRE_INC)
        && adr_mode != U8(Encoding::AddrType::ADDR_OFFSET)
        && adr_mode != U8(Encoding::AddrType::ADDR_POST_INC)
        && !(unaligned && has_unaligned_form))
    {
        AEMU_FATAL("disassemble_format_m() - Invalid addressing mode "
                   "in the disassembly of instruction ({}) {}",
                   op.c_str(), instruction);
    }

    if (test_bit<14>(instruction))
    {
        int simm12 = bitfield_signed<2, 12>(instruction);
        if (simm12 == 0)
        {
            disassemble += "]";
        }
        else if (adr_mode == U8(Encoding::AddrType::ADDR_PRE_INC))
        {
            disassemble += ", " + std::to_string(simm12) + "]!";
        }
        else if (adr_mode == U8(Encoding::AddrType::ADDR_OFFSET) || unaligned)
        {
            disassemble += ", " + std::to_string(simm12) + "]";
        }
        else if (adr_mode == U8(Encoding::AddrType::ADDR_POST_INC))
        {
            disassemble += "], " + std::to_string(simm12);
        }
    }
    else
    {
        const std::string reg = disassemble_gpr(instruction, 9);
        std::string shift = "";
        if (bitfield_unsigned<2, 5>(instruction) > 0)
        {
            shift = ", " + disassemble_shift_operand(instruction);
        }

        if (adr_mode == U8(Encoding::AddrType::ADDR_PRE_INC))
        {
            disassemble += ", " + reg + ", " + shift + "]!";
        }
        else if (adr_mode == U8(Encoding::AddrType::ADDR_OFFSET) || unaligned)
        {
            disassemble += ", " + reg + ", " + shift + "]";
        }
        else if (adr_mode == U8(Encoding::AddrType::ADDR_POST_INC))
        {
            disassemble += "], " + reg + ", " + shift;
        }
    }
    return disassemble;
}

static std::string disassemble_format_o3(word instruction, std::string op)
{
    std::string disassemble = op;
    if (test_bit<25>(instruction))
    {
        disassemble += "s";
    }
    disassemble += " ";

    disassemble += disassemble_gpr(instruction, 20);
    disassemble += ", ";

    if (test_bit<19>(instruction))
    {
        disassemble += std::to_string(bitfield_unsigned<0, 19>(instruction));
    }
    else
    {
        disassemble += disassemble_gpr(instruction, 14);
        if (bitfield_unsigned<0, 14>(instruction) > 0)
        {
            disassemble += " " + std::to_string(bitfield_unsigned<0, 14>(instruction));
        }
    }
    return disassemble;
}

static std::string disassemble_format_o2(word instruction, std::string op)
{
    std::string disassemble = op;
    if (test_bit<25>(instruction))
    {
        disassemble += "s";
    }
    disassemble += " ";

    disassemble += disassemble_gpr(instruction, 20);
    disassemble += ", ";

    disassemble += disassemble_gpr(instruction, 15);
    disassemble += ", ";

    disassemble += disassemble_gpr(instruction, 9);
    disassemble += ", ";

    disassemble += disassemble_gpr(instruction, 4);

    return disassemble;
}

static std::string disassemble_format_o1(word instruction, std::string op)
{
    std::string disassemble = op;
    if (test_bit<25>(instruction))
    {
        disassemble += "s";
    }
    disassemble += " ";

    disassemble += disassemble_gpr(instruction, 20);
    disassemble += ", ";

    disassemble += disassemble_gpr(instruction, 15);
    disassemble += ", ";

    if (test_bit<14>(instruction))
    {
        disassemble += std::to_string(bitfield_unsigned<2, 5>(instruction));
    }
    else
    {
        disassemble += disassemble_gpr(instruction, 9);
    }
    return disassemble;
}

/// @param compare the name of the comparison that `op` with S and an `xzr` destination is
///        (`subs xzr, ...` is `cmp`), or null if it has none
static std::string disassemble_format_o(word instruction, std::string op,
                                        const char *compare = nullptr)
{
    const bool is_compare =
        compare != nullptr && test_bit<25>(instruction)
        && bitfield_unsigned<20, 5>(instruction) == register_to_U8(Register::XZR);

    std::string disassemble = is_compare ? std::string(compare) : op;
    if (!is_compare && test_bit<25>(instruction))
    {
        disassemble += "s";
    }
    disassemble += " ";

    if (!is_compare)
    {
        disassemble += disassemble_gpr(instruction, 20);
        disassemble += ", ";
    }

    disassemble += disassemble_gpr(instruction, 15);
    disassemble += ", ";

    if (test_bit<14>(instruction))
    {
        disassemble += std::to_string(bitfield_unsigned<0, 14>(instruction));
    }
    else
    {
        disassemble += disassemble_gpr(instruction, 9);

        if (bitfield_unsigned<2, 5>(instruction) > 0)
        {
            disassemble += ", " + disassemble_shift_operand(instruction);
        }
    }
    return disassemble;
}

static std::string disassemble_hlt(word instruction)
{
    UNUSED(instruction);
    return "hlt";
}

static std::string disassemble_nop(word instruction)
{
    UNUSED(instruction);
    return "nop";
}

/// The name of a system register as the assembler reads it.
static std::string disassemble_sysreg(const U8 sysreg)
{
    const char *name = Encoding::sysreg_name(sysreg);
    return name != nullptr ? name : "sysreg" + std::to_string(sysreg);
}

static std::string disassemble_msr(word instruction)
{
    const U8 sysreg = bitfield_unsigned<17, 5>(instruction);
    const bool imm = test_bit<16>(instruction);
    if (imm)
    {
        const word val = bitfield_unsigned<0, 16>(instruction);
        return "msr " + disassemble_sysreg(sysreg) + ", " + std::to_string(val);
    }
    else
    {
        return "msr " + disassemble_sysreg(sysreg) + ", " + disassemble_gpr(instruction, 11);
    }
}

static std::string disassemble_mrs(word instruction)
{
    const U8 sysreg = bitfield_unsigned<11, 5>(instruction);
    return "mrs " + disassemble_gpr(instruction, 17) + ", " + disassemble_sysreg(sysreg);
}

static std::string disassemble_eret(word instruction)
{
    UNUSED(instruction);
    return "eret";
}

static std::string disassemble_wfi(word instruction)
{
    UNUSED(instruction);
    return "wfi";
}

static std::string disassemble_brk(word instruction)
{
    return "brk " + std::to_string(bitfield_unsigned<0, 22>(instruction));
}

static std::string disassemble_tlbi(word instruction)
{
    return test_bit<16>(instruction) ? "tlbi " + disassemble_gpr(instruction, 17) : "tlbi";
}

static std::string disassemble_atomic(word instruction)
{
    word atop = bitfield_unsigned<0, 4>(instruction);
    const byte width = bitfield_unsigned<4, 2>(instruction);

    std::string disassemble;
    switch (atop)
    {
    case Encoding::kAtomicId_swp:
        disassemble = "swp";
        break;
    case Encoding::kAtomicId_ldadd:
        disassemble = "ldadd";
        break;
    case Encoding::kAtomicId_ldclr:
        disassemble = "ldclr";
        break;
    case Encoding::kAtomicId_ldset:
        disassemble = "ldset";
        break;
    default:
        return "ERROR: INVALID ATOMIC";
    }

    switch (width)
    {
    case Encoding::kAtomicWidth_word:
        break;
    case Encoding::kAtomicWidth_byte:
        disassemble += "b";
        break;
    case Encoding::kAtomicWidth_hword:
        disassemble += "h";
        break;
    default:
        return "ERROR: INVALID ATOMIC WIDTH";
    }

    disassemble += " ";
    disassemble += disassemble_gpr(instruction, 17) + ", " + disassemble_gpr(instruction, 11)
                   + ", [" + disassemble_gpr(instruction, 6) + "]";
    return disassemble;
}

static std::string disassemble_unary(word instruction)
{
    const char *name;
    switch (bitfield_unsigned<0, 4>(instruction))
    {
    case Encoding::kUnaryId_sxtb:
        name = "sxtb";
        break;
    case Encoding::kUnaryId_sxth:
        name = "sxth";
        break;
    case Encoding::kUnaryId_uxtb:
        name = "uxtb";
        break;
    case Encoding::kUnaryId_uxth:
        name = "uxth";
        break;
    case Encoding::kUnaryId_clz:
        name = "clz";
        break;
    case Encoding::kUnaryId_rev:
        name = "rev";
        break;
    case Encoding::kUnaryId_rev16:
        name = "rev16";
        break;
    default:
        return "ERROR: INVALID UNARY OPERATION";
    }
    return std::string(name) + " " + disassemble_gpr(instruction, 17) + ", "
           + disassemble_gpr(instruction, 11);
}

static std::string disassemble_special_instructions(word instruction)
{
    word opsec = bitfield_unsigned<22, 4>(instruction);

    switch (opsec)
    {
    case Encoding::kSpecialOpId_hlt:
        return disassemble_hlt(instruction);
    case Encoding::kSpecialOpId_nop:
        return disassemble_nop(instruction);
    case Encoding::kSpecialOpId_msr:
        return disassemble_msr(instruction);
    case Encoding::kSpecialOpId_mrs:
        return disassemble_mrs(instruction);
    case Encoding::kSpecialOpId_tlbi:
        return disassemble_tlbi(instruction);
    case Encoding::kSpecialOpId_atomic:
        return disassemble_atomic(instruction);
    case Encoding::kSpecialOpId_eret:
        return disassemble_eret(instruction);
    case Encoding::kSpecialOpId_wfi:
        return disassemble_wfi(instruction);
    case Encoding::kSpecialOpId_brk:
        return disassemble_brk(instruction);
    case Encoding::kSpecialOpId_unary:
        return disassemble_unary(instruction);
    default:
        return "ERROR: INVALID SPECOP";
    }
}

static std::string disassemble_add(word instruction)
{
    return disassemble_format_o(instruction, "add", "cmn");
}

static std::string disassemble_sub(word instruction)
{
    return disassemble_format_o(instruction, "sub", "cmp");
}

static std::string disassemble_rsb(word instruction)
{
    return disassemble_format_o(instruction, "rsb");
}

static std::string disassemble_adc(word instruction)
{
    return disassemble_format_o(instruction, "adc");
}

static std::string disassemble_sbc(word instruction)
{
    return disassemble_format_o(instruction, "sbc");
}

static std::string disassemble_rsc(word instruction)
{
    return disassemble_format_o(instruction, "rsc");
}

static std::string disassemble_mul(word instruction)
{
    return disassemble_format_o(instruction, "mul");
}

static std::string disassemble_mull(word instruction)
{
    return disassemble_format_o2(instruction,
                                 test_bit<kLongMulSignedBit>(instruction) ? "smull" : "umull");
}

/// The name of the precision of a floating point instruction: `f32` or `f64`.
static const char *disassemble_precision(word instruction)
{
    return test_bit<25>(instruction) ? "f64" : "f32";
}

static std::string disassemble_fop1(word instruction)
{
    const U8 fn = bitfield_unsigned<0, 5>(instruction);
    const bool dbl = test_bit<25>(instruction);
    const std::string precision = disassemble_precision(instruction);

    std::string name;
    switch (fn)
    {
    case fpu::kUnaryFn_abs:
        name = "fabs." + precision;
        break;
    case fpu::kUnaryFn_neg:
        name = "fneg." + precision;
        break;
    case fpu::kUnaryFn_sqrt:
        name = "fsqrt." + precision;
        break;
    case fpu::kUnaryFn_rint:
        name = "frint." + precision;
        break;
    case fpu::kUnaryFn_rintz:
        name = "frintz." + precision;
        break;
    case fpu::kUnaryFn_rintm:
        name = "frintm." + precision;
        break;
    case fpu::kUnaryFn_rintp:
        name = "frintp." + precision;
        break;
    case fpu::kUnaryFn_rinta:
        name = "frinta." + precision;
        break;
    case fpu::kUnaryFn_tos32:
        name = "fcvt.s32." + precision;
        break;
    case fpu::kUnaryFn_tou32:
        name = "fcvt.u32." + precision;
        break;
    case fpu::kUnaryFn_tos32r:
        name = "fcvtr.s32." + precision;
        break;
    case fpu::kUnaryFn_tou32r:
        name = "fcvtr.u32." + precision;
        break;
    case fpu::kUnaryFn_froms32:
        name = "fcvt." + precision + ".s32";
        break;
    case fpu::kUnaryFn_fromu32:
        name = "fcvt." + precision + ".u32";
        break;
    case fpu::kUnaryFn_fcvt:
        name = dbl ? "fcvt.f32.f64" : "fcvt.f64.f32";
        break;
    default:
        return "ERROR: INVALID FLOATING POINT FUNCTION";
    }
    return name + " " + disassemble_gpr(instruction, 20) + ", " + disassemble_gpr(instruction, 15);
}

static std::string disassemble_fop2(word instruction)
{
    static const char *const kNames[fpu::kBinaryFn_count] = {"fadd", "fsub", "fmul",
                                                             "fdiv", "fmin", "fmax"};
    const U8 fn = bitfield_unsigned<0, 5>(instruction);
    if (fn >= fpu::kBinaryFn_count)
    {
        return "ERROR: INVALID FLOATING POINT FUNCTION";
    }
    return std::string(kNames[fn]) + "." + disassemble_precision(instruction) + " "
           + disassemble_gpr(instruction, 20) + ", " + disassemble_gpr(instruction, 15) + ", "
           + disassemble_gpr(instruction, 9);
}

static std::string disassemble_fop3(word instruction)
{
    static const char *const kNames[fpu::kFmaFn_count] = {"fmadd", "fmsub", "fnmadd", "fnmsub"};
    const U8 fn = bitfield_unsigned<0, 4>(instruction);
    if (fn >= fpu::kFmaFn_count)
    {
        return "ERROR: INVALID FLOATING POINT FUNCTION";
    }
    return std::string(kNames[fn]) + "." + disassemble_precision(instruction) + " "
           + disassemble_gpr(instruction, 20) + ", " + disassemble_gpr(instruction, 15) + ", "
           + disassemble_gpr(instruction, 9) + ", " + disassemble_gpr(instruction, 4);
}

static std::string disassemble_fcmp(word instruction)
{
    return std::string(test_bit<24>(instruction) ? "fcmpe." : "fcmp.")
           + disassemble_precision(instruction) + " " + disassemble_gpr(instruction, 15) + ", "
           + disassemble_gpr(instruction, 9);
}

static std::string disassemble_and(word instruction)
{
    return disassemble_format_o(instruction, "and", "tst");
}

static std::string disassemble_orr(word instruction)
{
    return disassemble_format_o(instruction, "orr");
}

static std::string disassemble_eor(word instruction)
{
    return disassemble_format_o(instruction, "eor", "teq");
}

static std::string disassemble_bic(word instruction)
{
    return disassemble_format_o(instruction, "bic");
}

static std::string disassemble_shift(word instruction)
{
    static const char *const names[] = {"lsl", "lsr", "asr", "ror"};
    return disassemble_format_o1(instruction, names[bitfield_unsigned<7, 2>(instruction)]);
}

static std::string disassemble_mov(word instruction)
{
    return disassemble_format_o3(instruction, "mov");
}

static std::string disassemble_mvn(word instruction)
{
    return disassemble_format_o3(instruction, "mvn");
}

static std::string disassemble_ldr(word instruction)
{
    return disassemble_format_m(instruction, "ldr");
}

static std::string disassemble_str(word instruction)
{
    return disassemble_format_m(instruction, "str");
}

static std::string disassemble_ldrb(word instruction)
{
    return disassemble_format_m(instruction, "ldrb", false);
}

static std::string disassemble_strb(word instruction)
{
    return disassemble_format_m(instruction, "strb", false);
}

static std::string disassemble_ldrh(word instruction)
{
    return disassemble_format_m(instruction, "ldrh");
}

static std::string disassemble_strh(word instruction)
{
    return disassemble_format_m(instruction, "strh");
}

static std::string disassemble_b(word instruction)
{
    return disassemble_format_b1(instruction, "b");
}

static std::string disassemble_bl(word instruction)
{
    return disassemble_format_b1(instruction, "bl");
}

static std::string disassemble_bx(word instruction)
{
    return disassemble_format_b2(instruction, test_bit<kBranchLinkBit>(instruction) ? "blx" : "bx");
}

/// The 22 bit field is a number, not an offset.
static std::string disassemble_swi(word instruction)
{
    std::string disassemble = "swi";
    const ConditionCode condition = (ConditionCode) bitfield_unsigned<22, 4>(instruction);
    if (condition != ConditionCode::AL)
    {
        disassemble += "." + disassemble_condition(condition);
    }
    return disassemble + " " + std::to_string(bitfield_unsigned<0, 22>(instruction));
}

static std::string disassemble_udiv(word instruction)
{
    return disassemble_format_o(instruction, "udiv");
}

static std::string disassemble_sdiv(word instruction)
{
    return disassemble_format_o(instruction, "sdiv");
}

/// The aliases are shown the way they are written: `cset x0, lt` is `csinc x0, xzr, xzr, ge`.
static std::string disassemble_csel(word instruction)
{
    const U8 cond_bits = bitfield_unsigned<22, 4>(instruction);
    const U8 xn = bitfield_unsigned<11, 5>(instruction);
    const U8 xm = bitfield_unsigned<6, 5>(instruction);
    const word variant = bitfield_unsigned<4, 2>(instruction);
    const bool zero_pair = xn == register_to_U8(Register::XZR) && xm == xn;
    const bool same_pair = xn == xm && !zero_pair;
    const bool invertible = cond_bits < U8(ConditionCode::AL);
    const ConditionCode inverse = ConditionCode(cond_bits ^ 1);
    const std::string xd = disassemble_gpr(instruction, 17);

    if (invertible && variant != Encoding::kCselId_csel && zero_pair
        && variant != Encoding::kCselId_csneg)
    {
        return std::string(variant == Encoding::kCselId_csinc ? "cset " : "csetm ") + xd + ", "
               + disassemble_condition(inverse);
    }
    if (invertible && variant != Encoding::kCselId_csel && same_pair)
    {
        const char *name = variant == Encoding::kCselId_csinc   ? "cinc "
                           : variant == Encoding::kCselId_csinv ? "cinv "
                                                                     : "cneg ";
        return std::string(name) + xd + ", " + disassemble_gpr(instruction, 11) + ", "
               + disassemble_condition(inverse);
    }

    const char *name = variant == Encoding::kCselId_csel    ? "csel "
                       : variant == Encoding::kCselId_csinc ? "csinc "
                       : variant == Encoding::kCselId_csinv ? "csinv "
                                                                 : "csneg ";
    return std::string(name) + xd + ", " + disassemble_gpr(instruction, 11) + ", "
           + disassemble_gpr(instruction, 6) + ", "
           + disassemble_condition(ConditionCode(cond_bits));
}

static std::string disassemble_adrp(word instruction)
{
    return disassemble_format_m1(instruction, "adrp");
}

static std::string disassemble_adr(word instruction)
{
    return disassemble_format_m1(instruction, "adr");
}

std::string disassembler::disassemble(const word instr)
{
    switch (bitfield_unsigned<26, 6>(instr))
    {
#define AEMU_DISASSEMBLE_CASE(name, opcode)                                                        \
    case Encoding::_op_##name:                                                                     \
        return disassemble_##name(instr);
        AEMU_OPCODES(AEMU_DISASSEMBLE_CASE)
#undef AEMU_DISASSEMBLE_CASE
    default:
        return disassemble_nop(instr);
    }
}
