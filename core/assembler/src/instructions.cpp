#include <assembler/assembler.h>

#include <util/common.h>
#include <util/logger.h>

#include <string>

byte Assembler::parse_sysreg ()
{
    const std::string sysreg = m_tokenizer.consume ().value;
    if (sysreg == "PSTATE")
    {
        return Emulator32bit::SYSREG_PSTATE;
    }

    ERROR ("Assembler::parse_sysreg() - Invalid System Register %s.", sysreg.c_str ());
    return 0;
}

constexpr bool check_sequential_register_enum ()
{
    // register order is assumed to be x0-x29, sp, xzr.
    constexpr Tokenizer::Type kRegisterSequence[] = {
        Tokenizer::Type::REGISTER_X0,  Tokenizer::Type::REGISTER_X1,  Tokenizer::Type::REGISTER_X2,
        Tokenizer::Type::REGISTER_X3,  Tokenizer::Type::REGISTER_X4,  Tokenizer::Type::REGISTER_X5,
        Tokenizer::Type::REGISTER_X6,  Tokenizer::Type::REGISTER_X7,  Tokenizer::Type::REGISTER_X8,
        Tokenizer::Type::REGISTER_X9,  Tokenizer::Type::REGISTER_X10, Tokenizer::Type::REGISTER_X11,
        Tokenizer::Type::REGISTER_X12, Tokenizer::Type::REGISTER_X13, Tokenizer::Type::REGISTER_X14,
        Tokenizer::Type::REGISTER_X15, Tokenizer::Type::REGISTER_X16, Tokenizer::Type::REGISTER_X17,
        Tokenizer::Type::REGISTER_X18, Tokenizer::Type::REGISTER_X19, Tokenizer::Type::REGISTER_X20,
        Tokenizer::Type::REGISTER_X21, Tokenizer::Type::REGISTER_X22, Tokenizer::Type::REGISTER_X23,
        Tokenizer::Type::REGISTER_X24, Tokenizer::Type::REGISTER_X25, Tokenizer::Type::REGISTER_X26,
        Tokenizer::Type::REGISTER_X27, Tokenizer::Type::REGISTER_X28, Tokenizer::Type::REGISTER_X29,
        Tokenizer::Type::REGISTER_SP,  Tokenizer::Type::REGISTER_XZR,
    };

    for (size_t i = 0; i + 1 < std::size (kRegisterSequence); i++)
    {
        if (static_cast<U32> (kRegisterSequence[i]) + 1
            != static_cast<U32> (kRegisterSequence[i + 1]))
        {
            return false;
        }
    }
    return true;
}

byte Assembler::parse_register ()
{
    const Tokenizer::Type type =
        m_tokenizer
            .consume (Tokenizer::REGISTERS,
                      "Assembler::parse_register() - Expected register identifier. Got "
                          + m_tokenizer.get_token ().value)
            .type;

    static_assert (check_sequential_register_enum ());

    return byte (static_cast<U32> (type) - static_cast<U32> (Tokenizer::Type::REGISTER_X0));
}

void Assembler::parse_shift (Emulator32bit::ShiftType &shift, int &shift_amt)
{
    m_tokenizer.expect_next ({Tokenizer::INSTRUCTION_LSL, Tokenizer::INSTRUCTION_LSR,
                              Tokenizer::INSTRUCTION_ASR, Tokenizer::INSTRUCTION_ROR},
                             "Assembler::parse_shift() - Expected shift.");

    switch (m_tokenizer.consume ().type)
    {
    case Tokenizer::INSTRUCTION_LSL:
        shift = Emulator32bit::SHIFT_LSL;
        break;
    case Tokenizer::INSTRUCTION_LSR:
        shift = Emulator32bit::SHIFT_LSR;
        break;
    case Tokenizer::INSTRUCTION_ASR:
        shift = Emulator32bit::SHIFT_ASR;
        break;
    case Tokenizer::INSTRUCTION_ROR:
        shift = Emulator32bit::SHIFT_ROR;
        break;
    default:
        ERROR ("Assembler::parse_shift() - Unreachable.");
    }

    // Note, in future, we could change this to create relocation record instead.
    shift_amt = parse_expression ();

    EXPECT_TRUE (
        word (shift_amt) < (1ULL << 5),
        "Assembler::parse_shift() - Shift amount must fit in 5 bits. Expected < 32, Got: %d. "
        "Error in line %llu.",
        shift_amt, m_tokenizer.get_linei ());
}

Emulator32bit::ConditionCode get_cond_code (Tokenizer::Type type)
{
    switch (type)
    {
    case Tokenizer::Type::CONDITION_EQ:
        return Emulator32bit::ConditionCode::EQ;
    case Tokenizer::Type::CONDITION_NE:
        return Emulator32bit::ConditionCode::NE;
    case Tokenizer::Type::CONDITION_CS:
        return Emulator32bit::ConditionCode::CS;
    case Tokenizer::Type::CONDITION_HS:
        return Emulator32bit::ConditionCode::HS;
    case Tokenizer::Type::CONDITION_CC:
        return Emulator32bit::ConditionCode::CC;
    case Tokenizer::Type::CONDITION_LO:
        return Emulator32bit::ConditionCode::LO;
    case Tokenizer::Type::CONDITION_MI:
        return Emulator32bit::ConditionCode::MI;
    case Tokenizer::Type::CONDITION_PL:
        return Emulator32bit::ConditionCode::PL;
    case Tokenizer::Type::CONDITION_VS:
        return Emulator32bit::ConditionCode::VS;
    case Tokenizer::Type::CONDITION_VC:
        return Emulator32bit::ConditionCode::VC;
    case Tokenizer::Type::CONDITION_HI:
        return Emulator32bit::ConditionCode::HI;
    case Tokenizer::Type::CONDITION_LS:
        return Emulator32bit::ConditionCode::LS;
    case Tokenizer::Type::CONDITION_GE:
        return Emulator32bit::ConditionCode::GE;
    case Tokenizer::Type::CONDITION_LT:
        return Emulator32bit::ConditionCode::LT;
    case Tokenizer::Type::CONDITION_GT:
        return Emulator32bit::ConditionCode::GT;
    case Tokenizer::Type::CONDITION_LE:
        return Emulator32bit::ConditionCode::LE;
    case Tokenizer::Type::CONDITION_AL:
        return Emulator32bit::ConditionCode::AL;
    case Tokenizer::Type::CONDITION_NV:
        return Emulator32bit::ConditionCode::NV;
    default:
        ERROR ("Assembler::get_cond_code() - Unreachable.");
        return Emulator32bit::ConditionCode::NV;
    }
}

word Assembler::parse_format_b1 (byte opcode)
{
    m_tokenizer.consume ();

    Emulator32bit::ConditionCode condition = Emulator32bit::ConditionCode::AL;
    if (m_tokenizer.is_next (Tokenizer::PERIOD))
    {
        m_tokenizer.consume ();
        condition = get_cond_code (
            m_tokenizer
                .consume (
                    Tokenizer::CONDITIONS,
                    "Assembler::parse_format_b1() - Expected condition code to follow period.")
                .type);
    }

    sword value = 0;
    if (m_tokenizer.is_next (Tokenizer::SYMBOL))
    {
        const std::string &symbol = m_tokenizer.consume ().value;
        m_obj.add_symbol (symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK);

        m_obj.rel_text.push_back ((ObjectFile::RelocationEntry) {
            .offset = word (m_obj.text_section.size () * 4),
            .symbol = m_obj.string_table[symbol],
            .type = ObjectFile::RelocationEntry::Type::R_EMU32_B_OFFSET22,
            // TODO: Support shift in future.
            .shift = 0,
            .token = m_tokenizer.get_toki ()});
    }
    else
    {
        const word imm = parse_expression ();
        EXPECT_TRUE (imm < (1ULL << 24),
                     "Assembler::parse_format_b1() - Expected immediate to be 24 bits. "
                     "Error at %s in line %llu.",
                     Emulator32bit::disassemble_instr (word (opcode) << 26).c_str (),
                     m_tokenizer.get_linei ());
        EXPECT_TRUE ((imm & 0b11) == 0,
                     "Assembler::parse_format_b1() - Expected immediate to be 4 byte aligned. "
                     "Error at %s in line %llu.",
                     Emulator32bit::Emulator32bit::disassemble_instr (word (opcode) << 26).c_str (),
                     m_tokenizer.get_linei ());
        value = bitfield_signed (imm, 0, 24) >> 2;
    }

    return Emulator32bit::asm_format_b1 (opcode, condition, value);
}

word Assembler::parse_format_b2 (byte opcode)
{
    m_tokenizer.consume ();

    Emulator32bit::ConditionCode condition = Emulator32bit::ConditionCode::AL;
    if (m_tokenizer.is_next (Tokenizer::PERIOD))
    {
        m_tokenizer.consume ();
        condition = (Emulator32bit::ConditionCode) get_cond_code (
            m_tokenizer
                .consume (
                    Tokenizer::CONDITIONS,
                    "Assembler::parse_format_b1() - Expected condition code to follow period.")
                .type);
    }

    const byte reg = parse_register ();
    return Emulator32bit::asm_format_b2 (opcode, condition, reg);
}

word Assembler::parse_format_m1 (byte opcode)
{
    m_tokenizer.consume ();
    const byte reg = parse_register ();
    m_tokenizer.consume (Tokenizer::COMMA,
                         "Assembler::parse_format_m2() - Expected second argument.");

    // Implicitly assume :hi20:
    if (m_tokenizer.is_next (Tokenizer::RELOCATION_EMU32_ADRP_HI20))
    {
        m_tokenizer.consume ();
    }

    const std::string &symbol =
        m_tokenizer.consume (Tokenizer::SYMBOL, "Assembler::parse_format_m2() - Expected symbol.")
            .value;
    m_obj.add_symbol (symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK);

    m_obj.rel_text.push_back ((ObjectFile::RelocationEntry) {
        .offset = word (m_obj.text_section.size () * 4),
        .symbol = m_obj.string_table[symbol],
        .type = ObjectFile::RelocationEntry::Type::R_EMU32_ADRP_HI20,
        // TODO: Support shift in future.
        .shift = 0,
        .token = m_tokenizer.get_toki (),
    });

    return Emulator32bit::asm_format_m1 (opcode, reg, 0);
}

word Assembler::parse_format_m (byte opcode)
{
    const std::string &op = m_tokenizer.consume ().value;

    // Whether the value to be loaded/stored should be interpreted as signed.
    const bool sign = op.size () > 3 ? op.at (3) == 's' : false;

    // Target register. For reads, stores read value; for writes, stores write value.
    byte reg_t = parse_register ();

    m_tokenizer.consume (Tokenizer::COMMA,
                         "Assembler::parse_format_m() - Expected second argument.");
    m_tokenizer.consume (Tokenizer::OPEN_BRACKET,
                         "Assembler::parse_format_m() - Expected open bracket");

    // Register that contains memory address.
    byte reg_a = parse_register ();

    // Parse the address mode.
    Emulator32bit::AddrType addressing_mode;
    bool parsed_addressing_mode = false;

    // Post indexed, offset is applied to value at register after accessing.
    if (m_tokenizer.is_next (Tokenizer::CLOSE_BRACKET))
    {
        m_tokenizer.consume ();
        addressing_mode = Emulator32bit::ADDR_POST_INC;
        parsed_addressing_mode = true;
    }

    // Check for offset.
    if (m_tokenizer.is_next (Tokenizer::COMMA))
    {
        m_tokenizer.consume ();

        // Offset begins with the '#' symbol. Checked by parse expression. TODO: IS THIS TRUE??
        if (!m_tokenizer.is_next (Tokenizer::REGISTERS))
        {
            const word offset = parse_expression ();
            EXPECT_TRUE (offset < (1ULL << 12),
                         "Assembler::parse_format_m() - Offset must be 12 bit value.");

            m_tokenizer.consume (Tokenizer::CLOSE_BRACKET,
                                 "Assembler::parse_format_m() - Expected close bracket.");

            // Only update addressing mode if not yet determined. Only ADDR_POST_INC mode has been
            // checked thus far. This reduces code repetition, since the way offsets are calculated
            // are the same for all addressing modes, but differ soley in arrangement.
            if (!parsed_addressing_mode)
            {
                if (m_tokenizer.is_next (Tokenizer::OPERATOR_LOGICAL_NOT))
                {
                    // Preindexed, offset is applied to value at register before accessing.
                    m_tokenizer.consume ();
                    addressing_mode = Emulator32bit::ADDR_PRE_INC;
                    parsed_addressing_mode = true;
                }
                else
                {
                    // Simple offset.
                    addressing_mode = Emulator32bit::ADDR_OFFSET;
                    parsed_addressing_mode = true;
                }
            }

            return Emulator32bit::asm_format_m (opcode, sign, reg_t, reg_a, offset,
                                                addressing_mode);
        }
        else
        {
            // Since there is a comma, there is another argument that is not the above checked offset.
            const byte reg_b = parse_register ();

            // Shift argument.
            Emulator32bit::ShiftType shift = Emulator32bit::SHIFT_LSL;
            int shift_amount = 0;
            if (m_tokenizer.is_next (Tokenizer::COMMA))
            {
                m_tokenizer.consume ();
                parse_shift (shift, shift_amount);
            }

            m_tokenizer.consume (Tokenizer::CLOSE_BRACKET,
                                 "Assembler::parse_format_m() - Expected close bracket.");

            // Same logic as above, only update addressing mode if not yet determined.
            if (!parsed_addressing_mode)
            {
                if (m_tokenizer.is_next (Tokenizer::OPERATOR_LOGICAL_NOT))
                {
                    // Preindexed.
                    m_tokenizer.consume ();
                    addressing_mode = Emulator32bit::ADDR_PRE_INC;
                    parsed_addressing_mode = true;
                }
                else
                {
                    // Simple offset.
                    addressing_mode = Emulator32bit::ADDR_OFFSET;
                    parsed_addressing_mode = true;
                }
            }

            return Emulator32bit::asm_format_m (opcode, sign, reg_t, reg_a, reg_b, shift,
                                                shift_amount, addressing_mode);
        }
    }

    // Check for invalid addressing mode.
    EXPECT_FALSE (parsed_addressing_mode, "Assembler::parse_format_m() - Invalid addressing mode.");
    return Emulator32bit::asm_format_m (opcode, sign, reg_t, reg_a, 0, addressing_mode);
}

word Assembler::parse_format_o3 (byte opcode)
{
    // TODO: make sure to handle relocation.
    const bool s = m_tokenizer.consume ().value.back () == 's';
    const byte reg1 = parse_register ();
    m_tokenizer.consume (Tokenizer::COMMA, "Assembler::parse_format_o3() - Expected comma.");

    // TODO: In future, support relocation for immediate value.
    if (m_tokenizer.is_next (Tokenizer::REGISTERS))
    {
        const byte operand_reg = parse_register ();

        word value = 0;
        if (m_tokenizer.is_next (Tokenizer::COMMA))
        {
            m_tokenizer.consume ();
            value = parse_expression ();
            EXPECT_TRUE (value < (1ULL << 14),
                         "Assembler::parse_format_o3() - Immediate must be a 14 bit value.");
        }

        return Emulator32bit::asm_format_o3 (opcode, s, reg1, operand_reg, value);
    }
    else
    {
        if (m_tokenizer.is_next (
                {Tokenizer::RELOCATION_EMU32_MOV_HI13, Tokenizer::RELOCATION_EMU32_MOV_LO19}))
        {
            const Tokenizer::Type relocation = m_tokenizer.consume ().type;
            const std::string symbol =
                m_tokenizer
                    .consume (
                        Tokenizer::SYMBOL,
                        "Assembler::parse_format_o3() - Expected symbol to follow relocation.")
                    .value;
            m_obj.add_symbol (symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK);

            m_obj.rel_text.push_back ((ObjectFile::RelocationEntry) {
                .offset = word (m_obj.text_section.size () * 4),
                .symbol = m_obj.string_table[symbol],
                .type = (relocation == Tokenizer::RELOCATION_EMU32_MOV_HI13
                             ? ObjectFile::RelocationEntry::Type::R_EMU32_MOV_HI13
                             : ObjectFile::RelocationEntry::Type::R_EMU32_MOV_LO19),

                // TODO: Support shift in future.
                .shift = 0,
                .token = m_tokenizer.get_toki ()});

            return Emulator32bit::asm_format_o3 (opcode, s, reg1, 0);
        }
        else
        {
            const word imm = parse_expression ();

            EXPECT_TRUE (imm < (1ULL << 14),
                         "Assembler::parse_format_o3() - Immediate value must be a 14 bit number. "
                         "Error at %s in line %llu.",
                         Emulator32bit::disassemble_instr (word (opcode) << 26).c_str (),
                         m_tokenizer.get_linei ());
            return Emulator32bit::asm_format_o3 (opcode, s, reg1, imm);
        }
    }

    return 0;
}

word Assembler::parse_format_o2 (byte opcode)
{
    bool s = m_tokenizer.consume ().value.back () == 's';

    const byte reg1 = parse_register ();
    m_tokenizer.consume (Tokenizer::COMMA, "Assembler::parse_format_o2() - Expected comma.");

    const byte reg2 = parse_register ();
    m_tokenizer.consume (Tokenizer::COMMA, "Assembler::parse_format_o2() - Expected comma.");

    const byte operand_reg1 = parse_register ();
    m_tokenizer.consume (Tokenizer::COMMA, "Assembler::parse_format_o2() - Expected comma.");

    const byte operand_reg2 = parse_register ();
    return Emulator32bit::asm_format_o2 (opcode, s, reg1, reg2, operand_reg1, operand_reg2);
}

word Assembler::parse_format_o1 (byte opcode)
{
    m_tokenizer.consume ();

    const byte reg1 = parse_register ();
    m_tokenizer.consume (Tokenizer::COMMA, "Assembler::parse_format_o1() - Expected comma.");

    const byte reg2 = parse_register ();
    m_tokenizer.consume (Tokenizer::COMMA, "Assembler::parse_format_o1() - Expected comma.");

    if (m_tokenizer.is_next (Tokenizer::REGISTERS))
    {
        const byte operand_reg = parse_register ();
        return Emulator32bit::asm_format_o1 (opcode, reg1, reg2, false, operand_reg, 0);
    }
    else
    {
        const int shift_amt = parse_expression ();
        EXPECT_TRUE (word (shift_amt) < (1ULL << 5),
                     "Assembler::parse_format_o1() - Shift amount must fit in 5 bits. Expected < "
                     "32, Got: %d. "
                     "Error at %s in line %llu.",
                     shift_amt, Emulator32bit::disassemble_instr (word (opcode) << 26).c_str (),
                     m_tokenizer.get_linei ());
        return Emulator32bit::asm_format_o1 (opcode, reg1, reg2, true, 0, shift_amt);
    }
}

word Assembler::parse_format_o (byte opcode)
{
    const bool s = m_tokenizer.consume ().value.back () == 's';

    const byte reg1 = parse_register ();
    m_tokenizer.consume (Tokenizer::COMMA, "Assembler::parse_format_o() - Expected comma.");

    const byte reg2 = parse_register ();
    m_tokenizer.consume (Tokenizer::COMMA, "Assembler::parse_format_o() - Expected comma.");

    if (m_tokenizer.is_next (Tokenizer::REGISTERS))
    {
        const byte operand_reg = parse_register ();

        // Shift.
        Emulator32bit::ShiftType shift = Emulator32bit::SHIFT_LSL;
        int shift_amt = 0;
        if (m_tokenizer.is_next (Tokenizer::COMMA))
        {
            m_tokenizer.consume ();
            parse_shift (shift, shift_amt);
        }

        return Emulator32bit::asm_format_o (opcode, s, reg1, reg2, operand_reg, shift, shift_amt);
    }
    else
    {
        word operand = 0;
        if (m_tokenizer.is_next (Tokenizer::RELOCATION_EMU32_O_LO12))
        {
            m_tokenizer.consume ();
            const std::string symbol =
                m_tokenizer
                    .consume (Tokenizer::SYMBOL,
                              "Assembler::parse_format_o() - Expected symbol to follow relocation.")
                    .value;
            m_obj.add_symbol (symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK);

            m_obj.rel_text.push_back ((ObjectFile::RelocationEntry) {
                .offset = word (m_obj.text_section.size () * 4),
                .symbol = m_obj.string_table[symbol],
                .type = ObjectFile::RelocationEntry::Type::R_EMU32_O_LO12,

                // TODO: Support shift in future.
                .shift = 0,
                .token = m_tokenizer.get_toki (),
            });
        }
        else if (m_tokenizer.is_next (Tokenizer::LITERAL_NUMBERS))
        {
            operand = parse_expression ();
            EXPECT_TRUE (operand < (1ULL << 14),
                         "Assembler::parse_format_o() - Immediate must be a 14 bit value.");
        }
        else
        {
            m_state = Assembler::State::ASSEMBLER_ERROR;
            ERROR ("Assembler::parse_format_o() - Could not parse token. Error at %s in line %d.",
                   m_tokenizer.get_token ().to_string ().c_str (), m_tokenizer.get_linei ());
        }

        EXPECT_TRUE (
            operand < (1ULL << 14),
            "Assembler::parse_format_o() - Expected numeric argument to be a 14 bit value. "
            "Error at %s in line %llu.",
            Emulator32bit::disassemble_instr (word (opcode) << 26).c_str (),
            m_tokenizer.get_linei ());

        return Emulator32bit::asm_format_o (opcode, s, reg1, reg2, operand);
    }
}

word Assembler::parse_format_atomic (byte width, byte atopcode)
{
    m_tokenizer.consume ();

    const byte xt = parse_register ();
    m_tokenizer.consume (Tokenizer::COMMA, "Assembler::parse_format_atomic() - Expected comma.");

    const byte xn = parse_register ();
    m_tokenizer.consume (Tokenizer::COMMA, "Assembler::parse_format_atomic() - Expected comma.");

    m_tokenizer.consume (
        Tokenizer::OPEN_BRACKET,
        "Assembler::parse_format_atomic() - Expected opening '[' for memory address.");

    const byte xm = parse_register ();
    m_tokenizer.consume (
        Tokenizer::CLOSE_BRACKET,
        "Assembler::parse_format_atomic() - Expected closing ']' for memory address.");

    return Emulator32bit::asm_atomic (xt, xn, xm, width, atopcode);
}

///
/// @brief
///
/// add x1, x2, x3
/// add x1, x2, #40
/// add x1, x2, x3, lsl 4
/// add x1, x2, :lo12:symbol
/// NOT SUPPORTED -- add x1, x2, :lo12:symbol + 4
///
void Assembler::_add ()
{
    const word instruction = parse_format_o (Emulator32bit::_op_add);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_sub ()
{
    const word instruction = parse_format_o (Emulator32bit::_op_sub);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_rsb ()
{
    const word instruction = parse_format_o (Emulator32bit::_op_rsb);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_adc ()
{
    const word instruction = parse_format_o (Emulator32bit::_op_adc);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_sbc ()
{
    const word instruction = parse_format_o (Emulator32bit::_op_sbc);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_rsc ()
{
    const word instruction = parse_format_o (Emulator32bit::_op_rsc);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_mul ()
{
    const word instruction = parse_format_o (Emulator32bit::_op_mul);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_umull ()
{
    const word instruction = parse_format_o2 (Emulator32bit::_op_umull);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_smull ()
{
    const word instruction = parse_format_o2 (Emulator32bit::_op_smull);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_vabs ()
{
    ERROR ("Assembler::_vabs() - Instruction not implemented yet.");
}

void Assembler::_vneg ()
{
    ERROR ("Assembler::_vneg() - Instruction not implemented yet.");
}

void Assembler::_vsqrt ()
{
    ERROR ("Assembler::_vsqrt() - Instruction not implemented yet.");
}

void Assembler::_vadd ()
{
    ERROR ("Assembler::_vadd() - Instruction not implemented yet.");
}

void Assembler::_vsub ()
{
    ERROR ("Assembler::_vsub() - Instruction not implemented yet.");
}

void Assembler::_vdiv ()
{
    ERROR ("Assembler::_vdiv() - Instruction not implemented yet.");
}

void Assembler::_vmul ()
{
    ERROR ("Assembler::_vmul() - Instruction not implemented yet.");
}

void Assembler::_vcmp ()
{
    ERROR ("Assembler::_vcmp() - Instruction not implemented yet.");
}

void Assembler::_vsel ()
{
    ERROR ("Assembler::_vsel() - Instruction not implemented yet.");
}

void Assembler::_vcint ()
{
    ERROR ("Assembler::_vcint() - Instruction not implemented yet.");
}

void Assembler::_vcflo ()
{
    ERROR ("Assembler::_vcflo() - Instruction not implemented yet.");
}

void Assembler::_vmov ()
{
    ERROR ("Assembler::_vmov() - Instruction not implemented yet.");
}

void Assembler::_and ()
{
    const word instruction = parse_format_o (Emulator32bit::_op_and);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_orr ()
{
    const word instruction = parse_format_o (Emulator32bit::_op_orr);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_eor ()
{
    const word instruction = parse_format_o (Emulator32bit::_op_eor);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_bic ()
{
    const word instruction = parse_format_o (Emulator32bit::_op_bic);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_lsl ()
{
    const word instruction = parse_format_o1 (Emulator32bit::_op_lsl);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_lsr ()
{
    const word instruction = parse_format_o1 (Emulator32bit::_op_lsr);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_asr ()
{
    const word instruction = parse_format_o1 (Emulator32bit::_op_asr);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_ror ()
{
    const word instruction = parse_format_o1 (Emulator32bit::_op_ror);
    m_obj.text_section.push_back (instruction);
}

void insert_xzr (Tokenizer &tokenizer)
{
    const std::vector<Tokenizer::Token> insert = {
        Tokenizer::Token (Tokenizer::Type::WHITESPACE_SPACE, " "),
        Tokenizer::Token (Tokenizer::Type::REGISTER_XZR, "xzr"),
        Tokenizer::Token (Tokenizer::Type::COMMA, ","),
    };

    tokenizer.insert_tokens (insert, tokenizer.get_toki () + 1);
}

void Assembler::_cmp ()
{
    insert_xzr (m_tokenizer);

    // TODO: Alias subs.
    const word instruction = parse_format_o (Emulator32bit::_op_cmp);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_cmn ()
{
    insert_xzr (m_tokenizer);

    // TODO: Alias adds.
    const word instruction = parse_format_o (Emulator32bit::_op_cmn);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_tst ()
{
    insert_xzr (m_tokenizer);

    // TODO: Alias ands.
    const word instruction = parse_format_o (Emulator32bit::_op_tst);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_teq ()
{
    insert_xzr (m_tokenizer);

    // TODO: Alias eors.
    const word instruction = parse_format_o (Emulator32bit::_op_teq);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_mov ()
{
    const word instruction = parse_format_o3 (Emulator32bit::_op_mov);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_mvn ()
{
    const word instruction = parse_format_o3 (Emulator32bit::_op_mvn);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_ldr ()
{
    const word instruction = parse_format_m (Emulator32bit::_op_ldr);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_str ()
{
    const word instruction = parse_format_m (Emulator32bit::_op_str);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_ldrb ()
{
    const word instruction = parse_format_m (Emulator32bit::_op_ldrb);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_strb ()
{
    const word instruction = parse_format_m (Emulator32bit::_op_strb);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_ldrh ()
{
    const word instruction = parse_format_m (Emulator32bit::_op_ldrh);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_strh ()
{
    const word instruction = parse_format_m (Emulator32bit::_op_strh);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_hlt ()
{
    m_tokenizer.consume ();
    m_obj.text_section.push_back (Emulator32bit::asm_hlt ());
}

void Assembler::_nop ()
{
    m_tokenizer.consume ();
    m_obj.text_section.push_back (Emulator32bit::asm_nop ());
}

void Assembler::_msr ()
{
    m_tokenizer.consume ();

    const word sysreg = parse_sysreg ();
    m_tokenizer.consume (Tokenizer::COMMA, "Assembler::_msr() - Expected second argument.");

    word instruction;
    if (m_tokenizer.is_next (Tokenizer::REGISTERS))
    {
        const byte xn = parse_register ();
        instruction = Emulator32bit::asm_msr (sysreg, false, xn);
    }
    else
    {
        const word imm16 = parse_expression ();
        EXPECT_TRUE (imm16 < (1ULL << 16), "Assembler::_msr() - Immediate must be a 16 bit value.");

        instruction = Emulator32bit::asm_msr (sysreg, true, imm16);
    }
    m_obj.text_section.push_back (instruction);
}

void Assembler::_mrs ()
{
    m_tokenizer.consume ();

    const byte xn = parse_register ();
    m_tokenizer.consume (Tokenizer::COMMA, "Assembler::_mrs() - Expected second argument.");

    const byte sysreg = parse_sysreg ();

    const word instruction = Emulator32bit::asm_mrs (xn, sysreg);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_tlbi ()
{
    m_tokenizer.consume ();

    ERROR ("Assembler::_tlbi() - Unimplemented instruction.");
}

void Assembler::_swp ()
{
    const word instruction =
        parse_format_atomic (Emulator32bit::ATOMIC_WIDTH_WORD, Emulator32bit::ATOMIC_SWP);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_swpb ()
{
    const word instruction =
        parse_format_atomic (Emulator32bit::ATOMIC_WIDTH_BYTE, Emulator32bit::ATOMIC_SWP);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_swph ()
{
    const word instruction =
        parse_format_atomic (Emulator32bit::ATOMIC_WIDTH_HWORD, Emulator32bit::ATOMIC_SWP);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_ldadd ()
{
    const word instruction =
        parse_format_atomic (Emulator32bit::ATOMIC_WIDTH_WORD, Emulator32bit::ATOMIC_LDADD);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_ldaddb ()
{
    const word instruction =
        parse_format_atomic (Emulator32bit::ATOMIC_WIDTH_BYTE, Emulator32bit::ATOMIC_LDADD);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_ldaddh ()
{
    const word instruction =
        parse_format_atomic (Emulator32bit::ATOMIC_WIDTH_HWORD, Emulator32bit::ATOMIC_LDADD);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_ldclr ()
{
    const word instruction =
        parse_format_atomic (Emulator32bit::ATOMIC_WIDTH_WORD, Emulator32bit::ATOMIC_LDCLR);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_ldclrb ()
{
    const word instruction =
        parse_format_atomic (Emulator32bit::ATOMIC_WIDTH_BYTE, Emulator32bit::ATOMIC_LDCLR);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_ldclrh ()
{
    const word instruction =
        parse_format_atomic (Emulator32bit::ATOMIC_WIDTH_HWORD, Emulator32bit::ATOMIC_LDCLR);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_ldset ()
{
    const word instruction =
        parse_format_atomic (Emulator32bit::ATOMIC_WIDTH_WORD, Emulator32bit::ATOMIC_LDSET);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_ldsetb ()
{
    const word instruction =
        parse_format_atomic (Emulator32bit::ATOMIC_WIDTH_BYTE, Emulator32bit::ATOMIC_LDSET);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_ldseth ()
{
    const word instruction =
        parse_format_atomic (Emulator32bit::ATOMIC_WIDTH_HWORD, Emulator32bit::ATOMIC_LDSET);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_b ()
{
    const word instruction = parse_format_b1 (Emulator32bit::_op_b);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_bl ()
{
    const word instruction = parse_format_b1 (Emulator32bit::_op_bl);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_bx ()
{
    const word instruction = parse_format_b2 (Emulator32bit::_op_bx);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_blx ()
{
    const word instruction = parse_format_b2 (Emulator32bit::_op_blx);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_swi ()
{
    const word instruction = parse_format_b1 (Emulator32bit::_op_swi);
    m_obj.text_section.push_back (instruction);
}

void Assembler::_ret ()
{
    m_tokenizer.consume ();

    const std::vector<Tokenizer::Token> insert = {
        Tokenizer::Token (Tokenizer::Type::INSTRUCTION_BX, "bx"),
        Tokenizer::Token (Tokenizer::Type::WHITESPACE_SPACE, " "),
        Tokenizer::Token (Tokenizer::Type::REGISTER_X29, "x29"),
    };

    m_tokenizer.insert_tokens (insert, m_tokenizer.get_toki ());
}

void Assembler::_adrp ()
{
    const word instruction = parse_format_m1 (Emulator32bit::_op_adrp);
    m_obj.text_section.push_back (instruction);
}
