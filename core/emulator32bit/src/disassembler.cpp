#include "emulator32bit/emulator32bit.h"
#include "util/common.h"
#include "util/logger.h"

static std::string disassemble_gpr (word instruction, U8 offset)
{
    const U8 gpr = bitfield_unsigned (instruction, offset, 5);
    if (gpr == Emulator32bit::register_to_U8 (Emulator32bit::Register::SP))
    {
        return "sp";
    }
    else if (gpr == Emulator32bit::register_to_U8 (Emulator32bit::Register::XZR))
    {
        return "xzr";
    }
    else
    {
        return "x" + std::to_string (gpr);
    }
}

static std::string disassemble_shift (word instruction)
{
    std::string disassemble;
    switch (static_cast<Emulator32bit::ShiftType> (bitfield_unsigned (instruction, 7, 2)))
    {
    case Emulator32bit::ShiftType::SHIFT_LSL:
        disassemble = "lsl ";
        break;
    case Emulator32bit::ShiftType::SHIFT_LSR:
        disassemble = "lsr ";
        break;
    case Emulator32bit::ShiftType::SHIFT_ASR:
        disassemble = "asr ";
        break;
    case Emulator32bit::ShiftType::SHIFT_ROR:
        disassemble = "ror ";
        break;
    default:
        disassemble = "ERROR_SHIFT";
        break;
    }

    disassemble += std::to_string (bitfield_unsigned (instruction, 2, 5));
    return disassemble;
}

static std::string disassemble_condition (Emulator32bit::ConditionCode condition)
{
    switch (condition)
    {
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
    return "ERROR_CONDITION";
}

static std::string disassemble_format_b2 (word instruction, std::string op)
{
    std::string disassemble = op;
    Emulator32bit::ConditionCode condition =
        (Emulator32bit::ConditionCode) bitfield_unsigned (instruction, 22, 4);
    if (condition != Emulator32bit::ConditionCode::AL)
    {
        disassemble += "." + disassemble_condition (condition);
    }

    if (bitfield_unsigned (instruction, 17, 5) == 29)
    {
        disassemble = "ret";
    }
    else
    {
        disassemble += " " + disassemble_gpr (instruction, 17);
    }

    return disassemble;
}

static std::string disassemble_format_b1 (word instruction, std::string op)
{
    std::string disassemble = op;
    Emulator32bit::ConditionCode condition =
        (Emulator32bit::ConditionCode) bitfield_unsigned (instruction, 22, 4);
    if (condition != Emulator32bit::ConditionCode::AL)
    {
        disassemble += "." + disassemble_condition (condition);
    }
    disassemble += " " + std::to_string (bitfield_signed (instruction, 0, 22));
    return disassemble;
}

static std::string disassemble_format_m1 (word instruction, std::string op)
{
    std::string disassemble = op + " ";
    disassemble += disassemble_gpr (instruction, 20);
    disassemble += ", ";

    int32_t offset = bitfield_unsigned (instruction, 0, 20);
    if (test_bit (instruction, Emulator32bit::kInstructionUpdateFlagBit))
    {
        offset -= 1 << 20;
    }
    disassemble += std::to_string (offset);
    return disassemble;
}

static std::string disassemble_format_m (word instruction, std::string op)
{
    std::string disassemble = op;

    if (test_bit (instruction, 25))
    {
        disassemble.insert (disassemble.begin () + 3, 's');
    }
    disassemble += " ";

    disassemble += disassemble_gpr (instruction, 20);
    disassemble += ", ";

    disassemble += "[";
    disassemble += disassemble_gpr (instruction, 15);
    U8 adr_mode = bitfield_unsigned (instruction, 0, 2);
    if (adr_mode != U8 (Emulator32bit::AddrType::ADDR_PRE_INC)
        && adr_mode != U8 (Emulator32bit::AddrType::ADDR_OFFSET)
        && adr_mode != U8 (Emulator32bit::AddrType::ADDR_POST_INC))
    {
        ERROR ("disassemble_format_m() - Invalid addressing mode "
               "in the disassembly of instruction (%s) %u",
               op.c_str (), instruction);
    }

    if (test_bit (instruction, 14))
    {
        int simm12 = bitfield_signed (instruction, 2, 12);
        if (simm12 == 0)
        {
            disassemble += "]";
        }
        else if (adr_mode == U8 (Emulator32bit::AddrType::ADDR_PRE_INC))
        {
            disassemble += ", " + std::to_string (simm12) + "]!";
        }
        else if (adr_mode == U8 (Emulator32bit::AddrType::ADDR_OFFSET))
        {
            disassemble += ", " + std::to_string (simm12) + "]";
        }
        else if (adr_mode == U8 (Emulator32bit::AddrType::ADDR_POST_INC))
        {
            disassemble += "], " + std::to_string (simm12);
        }
    }
    else
    {
        const std::string reg = disassemble_gpr (instruction, 9);
        std::string shift = "";
        if (bitfield_unsigned (instruction, 2, 5) > 0)
        {
            shift = ", " + disassemble_shift (instruction);
        }

        if (adr_mode == U8 (Emulator32bit::AddrType::ADDR_PRE_INC))
        {
            disassemble += ", " + reg + ", " + shift + "]!";
        }
        else if (adr_mode == U8 (Emulator32bit::AddrType::ADDR_OFFSET))
        {
            disassemble += ", " + reg + ", " + shift + "]";
        }
        else if (adr_mode == U8 (Emulator32bit::AddrType::ADDR_POST_INC))
        {
            disassemble += "], " + reg + ", " + shift;
        }
    }
    return disassemble;
}

static std::string disassemble_format_o3 (word instruction, std::string op)
{
    std::string disassemble = op;
    if (test_bit (instruction, 25))
    {
        disassemble += "s";
    }
    disassemble += " ";

    disassemble += disassemble_gpr (instruction, 20);
    disassemble += ", ";

    if (test_bit (instruction, 19))
    {
        disassemble += std::to_string (bitfield_unsigned (instruction, 0, 19));
    }
    else
    {
        disassemble += disassemble_gpr (instruction, 14);
        if (bitfield_unsigned (instruction, 0, 14) > 0)
        {
            disassemble += " " + std::to_string (bitfield_unsigned (instruction, 0, 14));
        }
    }
    return disassemble;
}

static std::string disassemble_format_o2 (word instruction, std::string op)
{
    std::string disassemble = op;
    if (test_bit (instruction, 25))
    {
        disassemble += "s";
    }
    disassemble += " ";

    disassemble += disassemble_gpr (instruction, 20);
    disassemble += ", ";

    disassemble += disassemble_gpr (instruction, 15);
    disassemble += ", ";

    disassemble += disassemble_gpr (instruction, 9);
    disassemble += ", ";

    disassemble += disassemble_gpr (instruction, 4);
    disassemble += ", ";

    return disassemble;
}

static std::string disassemble_format_o1 (word instruction, std::string op)
{
    std::string disassemble = op + " ";

    disassemble += disassemble_gpr (instruction, 20);
    disassemble += ", ";

    disassemble += disassemble_gpr (instruction, 15);
    disassemble += ", ";

    if (test_bit (instruction, 14))
    {
        disassemble += std::to_string (bitfield_unsigned (instruction, 0, 14));
    }
    else
    {
        disassemble += disassemble_gpr (instruction, 9);
    }
    return disassemble;
}

static std::string disassemble_format_o (word instruction, std::string op)
{
    std::string disassemble = op;
    if (test_bit (instruction, 25))
    {
        disassemble += "s";
    }
    disassemble += " ";

    disassemble += disassemble_gpr (instruction, 20);
    disassemble += ", ";

    disassemble += disassemble_gpr (instruction, 15);
    disassemble += ", ";

    if (test_bit (instruction, 14))
    {
        disassemble += std::to_string (bitfield_unsigned (instruction, 0, 14));
    }
    else
    {
        disassemble += disassemble_gpr (instruction, 9);

        if (bitfield_unsigned (instruction, 2, 5) > 0)
        {
            disassemble += ", " + disassemble_shift (instruction);
        }
    }
    return disassemble;
}

static std::string disassemble_hlt (word instruction)
{
    UNUSED (instruction);
    return "hlt";
}

static std::string disassemble_nop (word instruction)
{
    UNUSED (instruction);
    return "nop";
}

static std::string disassemble_msr (word instruction)
{
    const U8 sysreg = bitfield_unsigned (instruction, 17, 5);
    const bool imm = test_bit (instruction, 16);
    if (imm)
    {
        const word val = bitfield_unsigned (instruction, 0, 16);
        return "msr sysreg" + std::to_string (sysreg) + " " + std::to_string (val);
    }
    else
    {
        return "msr sysreg" + std::to_string (sysreg) + " " + disassemble_gpr (instruction, 11);
    }
}

static std::string disassemble_mrs (word instruction)
{
    const U8 sysreg = bitfield_unsigned (instruction, 11, 5);
    return "mrs " + disassemble_gpr (instruction, 17) + " sysreg " + std::to_string (sysreg);
}

static std::string disassemble_tlbi (word instruction)
{
    const bool isxt = test_bit (instruction, 16);
    const word imm16 = bitfield_unsigned (instruction, 0, 16);

    return "tlbi " + std::to_string (imm16)
           + (isxt ? +" " + disassemble_gpr (instruction, 17) : "");
}

static std::string disassemble_atomic (word instruction)
{
    word atop = bitfield_unsigned (instruction, 0, 4);
    const byte width = bitfield_unsigned (instruction, 0, 4);

    std::string disassemble;
    switch (atop)
    {
    case Emulator32bit::kAtomicId_swp:
        disassemble = "swp";
        break;
    case Emulator32bit::kAtomicId_ldadd:
        disassemble = "ldadd";
        break;
    case Emulator32bit::kAtomicId_ldclr:
        disassemble = "ldclr";
        break;
    case Emulator32bit::kAtomicId_ldset:
        disassemble = "ldset";
        break;
    default:
        return "ERROR: INVALID ATOMIC";
    }

    switch (width)
    {
    case Emulator32bit::kAtomicWidth_word:
        break;
    case Emulator32bit::kAtomicWidth_byte:
        disassemble += "b";
        break;
    case Emulator32bit::kAtomicWidth_hword:
        disassemble += "h";
        break;
    default:
        return "ERROR: INVALID ATOMIC WIDTH";
    }

    disassemble += " ";
    disassemble += disassemble_gpr (instruction, 17) + ", " + disassemble_gpr (instruction, 11)
                   + ", [" + disassemble_gpr (instruction, 6) + "]";
    return disassemble;
}

static std::string disassemble_special_instructions (word instruction)
{
    word opsec = bitfield_unsigned (instruction, 22, 4);

    switch (opsec)
    {
    case Emulator32bit::kSpecialOpId_hlt:
        return disassemble_hlt (instruction);
    case Emulator32bit::kSpecialOpId_nop:
        return disassemble_nop (instruction);
    case Emulator32bit::kSpecialOpId_msr:
        return disassemble_msr (instruction);
    case Emulator32bit::kSpecialOpId_mrs:
        return disassemble_mrs (instruction);
    case Emulator32bit::kSpecialOpId_tlbi:
        return disassemble_tlbi (instruction);
    case Emulator32bit::kSpecialOpId_atomic:
        return disassemble_atomic (instruction);
    default:
        return "ERROR: INVALID SPECOP";
    }
}

static std::string disassemble_add (word instruction)
{
    return disassemble_format_o (instruction, "add");
}

static std::string disassemble_sub (word instruction)
{
    return disassemble_format_o (instruction, "sub");
}

static std::string disassemble_rsb (word instruction)
{
    return disassemble_format_o (instruction, "rsb");
}

static std::string disassemble_adc (word instruction)
{
    return disassemble_format_o (instruction, "adc");
}

static std::string disassemble_sbc (word instruction)
{
    return disassemble_format_o (instruction, "sbc");
}

static std::string disassemble_rsc (word instruction)
{
    return disassemble_format_o (instruction, "rsc");
}

static std::string disassemble_mul (word instruction)
{
    return disassemble_format_o (instruction, "mul");
}

static std::string disassemble_umull (word instruction)
{
    return disassemble_format_o2 (instruction, "umull");
}

static std::string disassemble_smull (word instruction)
{
    return disassemble_format_o2 (instruction, "smull");
}

static std::string disassemble_vabs (word instruction)
{
    UNUSED (instruction);
    return "UNIMPLEMENTED";
}

static std::string disassemble_vneg (word instruction)
{
    UNUSED (instruction);
    return "UNIMPLEMENTED";
}

static std::string disassemble_vsqrt (word instruction)
{
    UNUSED (instruction);
    return "UNIMPLEMENTED";
}

static std::string disassemble_vadd (word instruction)
{
    UNUSED (instruction);
    return "UNIMPLEMENTED";
}

static std::string disassemble_vsub (word instruction)
{
    UNUSED (instruction);
    return "UNIMPLEMENTED";
}

static std::string disassemble_vdiv (word instruction)
{
    UNUSED (instruction);
    return "UNIMPLEMENTED";
}

static std::string disassemble_vmul (word instruction)
{
    UNUSED (instruction);
    return "UNIMPLEMENTED";
}

static std::string disassemble_vcmp (word instruction)
{
    UNUSED (instruction);
    return "UNIMPLEMENTED";
}

static std::string disassemble_vsel (word instruction)
{
    UNUSED (instruction);
    return "UNIMPLEMENTED";
}

static std::string disassemble_vcint (word instruction)
{
    UNUSED (instruction);
    return "UNIMPLEMENTED";
}

static std::string disassemble_vcflo (word instruction)
{
    UNUSED (instruction);
    return "UNIMPLEMENTED";
}

static std::string disassemble_vmov (word instruction)
{
    UNUSED (instruction);
    return "UNIMPLEMENTED";
}

static std::string disassemble_and (word instruction)
{
    return disassemble_format_o (instruction, "and");
}

static std::string disassemble_orr (word instruction)
{
    return disassemble_format_o (instruction, "orr");
}

static std::string disassemble_eor (word instruction)
{
    return disassemble_format_o (instruction, "eor");
}

static std::string disassemble_bic (word instruction)
{
    return disassemble_format_o (instruction, "bic");
}

static std::string disassemble_lsl (word instruction)
{
    return disassemble_format_o1 (instruction, "lsl");
}

static std::string disassemble_lsr (word instruction)
{
    return disassemble_format_o1 (instruction, "lsr");
}

static std::string disassemble_asr (word instruction)
{
    return disassemble_format_o1 (instruction, "asr");
}

static std::string disassemble_ror (word instruction)
{
    return disassemble_format_o1 (instruction, "ror");
}

static std::string disassemble_cmp (word instruction)
{
    std::string disassemble = disassemble_format_o (instruction, "cmp");
    return "cmp" + disassemble.substr (disassemble.find_first_of ("xzr") + 4);
}

static std::string disassemble_cmn (word instruction)
{
    std::string disassemble = disassemble_format_o (instruction, "cmn");
    return "cmn" + disassemble.substr (disassemble.find_first_of ("xzr") + 4);
}

static std::string disassemble_tst (word instruction)
{
    std::string disassemble = disassemble_format_o (instruction, "tst");
    return "tst" + disassemble.substr (disassemble.find_first_of ("xzr") + 4);
}

static std::string disassemble_teq (word instruction)
{
    std::string disassemble = disassemble_format_o (instruction, "teq");
    return "teq" + disassemble.substr (disassemble.find_first_of ("xzr") + 4);
}

static std::string disassemble_mov (word instruction)
{
    return disassemble_format_o3 (instruction, "mov");
}

static std::string disassemble_mvn (word instruction)
{
    return disassemble_format_o3 (instruction, "mvn");
}

static std::string disassemble_ldr (word instruction)
{
    return disassemble_format_m (instruction, "ldr");
}

static std::string disassemble_str (word instruction)
{
    return disassemble_format_m (instruction, "str");
}

static std::string disassemble_ldrb (word instruction)
{
    return disassemble_format_m (instruction, "ldrb");
}

static std::string disassemble_strb (word instruction)
{
    return disassemble_format_m (instruction, "strb");
}

static std::string disassemble_ldrh (word instruction)
{
    return disassemble_format_m (instruction, "ldrh");
}

static std::string disassemble_strh (word instruction)
{
    return disassemble_format_m (instruction, "strh");
}

static std::string disassemble_b (word instruction)
{
    return disassemble_format_b1 (instruction, "b");
}

static std::string disassemble_bl (word instruction)
{
    return disassemble_format_b1 (instruction, "bl");
}

static std::string disassemble_bx (word instruction)
{
    return disassemble_format_b2 (instruction, "bx");
}

static std::string disassemble_blx (word instruction)
{
    return disassemble_format_b2 (instruction, "blx");
}

static std::string disassemble_swi (word instruction)
{
    return disassemble_format_b1 (instruction, "swi");
}

static std::string disassemble_adrp (word instruction)
{
    return disassemble_format_m1 (instruction, "adrp");
}

// Construct disassembler instruction mapping.
using DisassemblerFunction = std::string (*) (word);
static DisassemblerFunction _disassembler_instructions[Emulator32bit::kMaxInstructions];

static void disassembler_init ()
{
    static bool init = false;
    if (init)
    {
        return;
    }
    init = true;

    for (U8 i = 0; i < Emulator32bit::kMaxInstructions; i++)
    {
        _disassembler_instructions[i] = disassemble_nop;
    }

#define _INSTR(op) _disassembler_instructions[Emulator32bit::_op_##op] = disassemble_##op;

    _INSTR (special_instructions)

    _INSTR (add)
    _INSTR (sub)
    _INSTR (rsb)
    _INSTR (adc)
    _INSTR (sbc)
    _INSTR (rsc)
    _INSTR (mul)
    _INSTR (umull)
    _INSTR (smull)
    _INSTR (vabs)
    _INSTR (vneg)
    _INSTR (vsqrt)
    _INSTR (vadd)
    _INSTR (vsub)
    _INSTR (vdiv)
    _INSTR (vmul)
    _INSTR (vcmp)
    _INSTR (vsel)
    _INSTR (vcint)
    _INSTR (vcflo)
    _INSTR (vmov)
    _INSTR (and)
    _INSTR (orr)
    _INSTR (eor)
    _INSTR (bic)
    _INSTR (lsl)
    _INSTR (lsr)
    _INSTR (asr)
    _INSTR (ror)
    _INSTR (cmp)
    _INSTR (cmn)
    _INSTR (tst)
    _INSTR (teq)
    _INSTR (mov)
    _INSTR (mvn)
    _INSTR (ldr)
    _INSTR (ldrb)
    _INSTR (ldrh)
    _INSTR (str)
    _INSTR (strb)
    _INSTR (strh)
    _INSTR (b)
    _INSTR (bl)
    _INSTR (bx)
    _INSTR (blx)
    _INSTR (swi)
    _INSTR (adrp)
}

std::string Emulator32bit::disassemble_instr (word instr)
{
    disassembler_init ();
    return (*_disassembler_instructions[bitfield_unsigned (instr, 26, 6)]) (instr);
}