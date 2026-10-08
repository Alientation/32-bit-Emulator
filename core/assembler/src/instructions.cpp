#include <assembler/assembler.h>

#include "util/logger.h"
#include <util/common.h>

#include <string>

using basm::Token;
using basm::TokenType;

U8 Assembler::parse_sysreg()
{
    const Token &sysreg = expect(TokenType::SYMBOL, "expected a system register");
    if (sysreg.text == "PSTATE")
    {
        return Emulator32bit::kSysregId_pstate;
    }

    fail(sysreg, "invalid system register '" + sysreg.str() + "'");
}

// Register tokens are ordered x0-x29, sp, xzr, so the offset from x0 is the register number.
static_assert(basm::register_index(TokenType::REGISTER_X29) == 29);
static_assert(basm::register_index(TokenType::REGISTER_SP) == 30);
static_assert(basm::register_index(TokenType::REGISTER_XZR) == 31);

U8 Assembler::parse_register()
{
    const Token &reg = m_cursor.peek();
    if (!basm::is_register(reg.type))
    {
        fail(reg, "expected a register, got " + basm::describe(reg));
    }
    m_cursor.next();

    return basm::register_index(reg.type);
}

void Assembler::parse_shift(ShiftType &shift, int &shift_amt)
{
    const Token &kind = m_cursor.peek();
    switch (kind.type)
    {
    case TokenType::INSTRUCTION_LSL:
        shift = ShiftType::SHIFT_LSL;
        break;
    case TokenType::INSTRUCTION_LSR:
        shift = ShiftType::SHIFT_LSR;
        break;
    case TokenType::INSTRUCTION_ASR:
        shift = ShiftType::SHIFT_ASR;
        break;
    case TokenType::INSTRUCTION_ROR:
        shift = ShiftType::SHIFT_ROR;
        break;
    default:
        fail(kind, "expected lsl, lsr, asr or ror, got " + basm::describe(kind));
    }
    m_cursor.next();

    // Note, in future, we could change this to create relocation record instead.
    shift_amt = parse_expression();

    check(word(shift_amt) < (1ULL << 5),
          "shift amount must fit in 5 bits, expected < 32, got " + std::to_string(shift_amt));
}

ConditionCode get_cond_code(TokenType type)
{
    switch (type)
    {
    case TokenType::CONDITION_EQ:
        return ConditionCode::EQ;
    case TokenType::CONDITION_NE:
        return ConditionCode::NE;
    case TokenType::CONDITION_CS:
        return ConditionCode::CS;
    case TokenType::CONDITION_HS:
        return ConditionCode::HS;
    case TokenType::CONDITION_CC:
        return ConditionCode::CC;
    case TokenType::CONDITION_LO:
        return ConditionCode::LO;
    case TokenType::CONDITION_MI:
        return ConditionCode::MI;
    case TokenType::CONDITION_PL:
        return ConditionCode::PL;
    case TokenType::CONDITION_VS:
        return ConditionCode::VS;
    case TokenType::CONDITION_VC:
        return ConditionCode::VC;
    case TokenType::CONDITION_HI:
        return ConditionCode::HI;
    case TokenType::CONDITION_LS:
        return ConditionCode::LS;
    case TokenType::CONDITION_GE:
        return ConditionCode::GE;
    case TokenType::CONDITION_LT:
        return ConditionCode::LT;
    case TokenType::CONDITION_GT:
        return ConditionCode::GT;
    case TokenType::CONDITION_LE:
        return ConditionCode::LE;
    case TokenType::CONDITION_AL:
        return ConditionCode::AL;
    case TokenType::CONDITION_NV:
        return ConditionCode::NV;
    default:
        AEMU_FATAL("Assembler::get_cond_code() - Unreachable.");
    }
}

word Assembler::parse_format_b1(byte opcode)
{
    m_cursor.next();

    ConditionCode condition = ConditionCode::AL;
    if (m_cursor.accept(TokenType::PERIOD))
    {
        const Token &cond = m_cursor.peek();
        if (!basm::is_condition(cond.type))
        {
            fail(cond, "expected a condition code after '.', got " + basm::describe(cond));
        }
        m_cursor.next();
        condition = get_cond_code(cond.type);
    }

    sword value = 0;
    if (m_cursor.check(TokenType::SYMBOL))
    {
        const std::string symbol = m_cursor.next().str();
        m_obj.add_symbol(symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK);

        m_obj.rel_text.push_back({.offset = word(m_obj.text_section.size() * 4),
                                  .symbol = m_obj.string_table[symbol],
                                  .type = ObjectFile::RelocationEntry::Type::R_EMU32_B_OFFSET22,
                                  // TODO: Support shift in future.
                                  .shift = 0,
                                  .token = m_cursor.position()});
    }
    else
    {
        const word imm = parse_expression();
        check(imm < (1ULL << 24), "branch offset must fit in 24 bits");
        check((imm & 0b11) == 0, "branch offset must be 4 byte aligned");
        value = bitfield_signed(imm, 0, 24) >> 2;
    }

    return Emulator32bit::asm_format_b1(opcode, condition, value);
}

word Assembler::parse_format_b2(byte opcode)
{
    m_cursor.next();

    ConditionCode condition = ConditionCode::AL;
    if (m_cursor.accept(TokenType::PERIOD))
    {
        const Token &cond = m_cursor.peek();
        if (!basm::is_condition(cond.type))
        {
            fail(cond, "expected a condition code after '.', got " + basm::describe(cond));
        }
        m_cursor.next();
        condition = get_cond_code(cond.type);
    }

    const byte reg = parse_register();
    return Emulator32bit::asm_format_b2(opcode, condition, reg);
}

word Assembler::parse_format_m1(byte opcode)
{
    m_cursor.next();
    const byte reg = parse_register();
    expect(TokenType::COMMA, "expected ',' and a symbol");

    // Implicitly assume :hi20:
    m_cursor.accept(TokenType::RELOCATION_EMU32_ADRP_HI20);

    const std::string symbol = expect(TokenType::SYMBOL, "expected a symbol").str();
    m_obj.add_symbol(symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK);

    m_obj.rel_text.push_back({
        .offset = word(m_obj.text_section.size() * 4),
        .symbol = m_obj.string_table[symbol],
        .type = ObjectFile::RelocationEntry::Type::R_EMU32_ADRP_HI20,
        // TODO: Support shift in future.
        .shift = 0,
        .token = m_cursor.position(),
    });

    return Emulator32bit::asm_format_m1(opcode, reg, 0);
}

word Assembler::parse_format_m(byte opcode)
{
    // Whether the value to be loaded/stored should be interpreted as signed.
    const bool sign = m_cursor.next().has(basm::SIGN_EXTEND);

    // Target register. For reads, stores read value; for writes, stores write value.
    byte reg_t = parse_register();

    expect(TokenType::COMMA, "expected ',' and a memory address");
    expect(TokenType::OPEN_BRACKET, "expected '[' to start the memory address");

    // Register that contains memory address.
    byte reg_a = parse_register();

    // Parse the address mode.
    Emulator32bit::AddrType addressing_mode;
    bool parsed_addressing_mode = false;

    // Post indexed, offset is applied to value at register after accessing.
    if (m_cursor.accept(TokenType::CLOSE_BRACKET))
    {
        // A bare `[xn]` is a plain access with no offset. Only `[xn], <offset>` is post indexed.
        if (!m_cursor.check(TokenType::COMMA))
        {
            return Emulator32bit::asm_format_m(opcode, sign, reg_t, reg_a, 0,
                                               Emulator32bit::AddrType::ADDR_OFFSET);
        }

        addressing_mode = Emulator32bit::AddrType::ADDR_POST_INC;
        parsed_addressing_mode = true;
    }

    // Check for offset.
    if (m_cursor.accept(TokenType::COMMA))
    {
        if (!basm::is_register(m_cursor.peek().type))
        {
            // The offset is a signed 12 bit value (sign extended by the emulator). A leading '-'
            // negates the whole expression.
            const bool negative = m_cursor.accept(TokenType::OPERATOR_SUBTRACTION);
            const dword magnitude = parse_expression();
            check(negative ? magnitude <= (1ULL << 11) : magnitude < (1ULL << 11),
                  "offset must be a signed 12 bit value (-2048 to 2047)");
            const int offset = negative ? -int(magnitude) : int(magnitude);

            // Post indexed (`[xn], offset`) already consumed the close bracket.
            if (!parsed_addressing_mode)
            {
                expect(TokenType::CLOSE_BRACKET, "expected ']' after the offset");

                // Preindexed, offset is applied to value at register before accessing. Otherwise a
                // simple offset.
                addressing_mode = m_cursor.accept(TokenType::OPERATOR_LOGICAL_NOT)
                                      ? Emulator32bit::AddrType::ADDR_PRE_INC
                                      : Emulator32bit::AddrType::ADDR_OFFSET;
            }

            return Emulator32bit::asm_format_m(opcode, sign, reg_t, reg_a, offset, addressing_mode);
        }
        else
        {
            // Since there is a comma, there is another argument that is not the above checked offset.
            const byte reg_b = parse_register();

            // Shift argument.
            ShiftType shift = ShiftType::SHIFT_LSL;
            int shift_amount = 0;
            if (m_cursor.accept(TokenType::COMMA))
            {
                parse_shift(shift, shift_amount);
            }

            if (!parsed_addressing_mode)
            {
                expect(TokenType::CLOSE_BRACKET, "expected ']' after the offset register");

                addressing_mode = m_cursor.accept(TokenType::OPERATOR_LOGICAL_NOT)
                                      ? Emulator32bit::AddrType::ADDR_PRE_INC
                                      : Emulator32bit::AddrType::ADDR_OFFSET;
            }

            return Emulator32bit::asm_format_m(opcode, sign, reg_t, reg_a, reg_b, shift,
                                               shift_amount, addressing_mode);
        }
    }

    // Check for invalid addressing mode.
    check(parsed_addressing_mode, "invalid addressing mode");
    return Emulator32bit::asm_format_m(opcode, sign, reg_t, reg_a, 0, addressing_mode);
}

word Assembler::parse_format_o3(byte opcode)
{
    // TODO: make sure to handle relocation.
    const bool s = m_cursor.next().has(basm::SETS_FLAGS);
    const byte reg1 = parse_register();
    expect(TokenType::COMMA, "expected ',' after the destination register");

    // TODO: In future, support relocation for immediate value.
    if (basm::is_register(m_cursor.peek().type))
    {
        const byte operand_reg = parse_register();

        word value = 0;
        if (m_cursor.accept(TokenType::COMMA))
        {
            value = parse_expression();
            check(value < (1ULL << 14), "immediate must be a 14 bit value");
        }

        return Emulator32bit::asm_format_o3(opcode, s, reg1, operand_reg, value);
    }
    else
    {
        if (m_cursor.check_any(
                {TokenType::RELOCATION_EMU32_MOV_HI13, TokenType::RELOCATION_EMU32_MOV_LO19}))
        {
            const TokenType relocation = m_cursor.next().type;
            const std::string symbol =
                expect(TokenType::SYMBOL, "expected a symbol to follow the relocation").str();
            m_obj.add_symbol(symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK);

            m_obj.rel_text.push_back(
                {.offset = word(m_obj.text_section.size() * 4),
                 .symbol = m_obj.string_table[symbol],
                 .type = (relocation == TokenType::RELOCATION_EMU32_MOV_HI13
                              ? ObjectFile::RelocationEntry::Type::R_EMU32_MOV_HI13
                              : ObjectFile::RelocationEntry::Type::R_EMU32_MOV_LO19),

                 // TODO: Support shift in future.
                 .shift = 0,
                 .token = m_cursor.position()});

            return Emulator32bit::asm_format_o3(opcode, s, reg1, 0);
        }
        else
        {
            const word imm = parse_expression();

            check(imm < (1ULL << 14), "immediate value must be a 14 bit number");
            return Emulator32bit::asm_format_o3(opcode, s, reg1, imm);
        }
    }

    return 0;
}

word Assembler::parse_format_o2(byte opcode)
{
    bool s = m_cursor.next().has(basm::SETS_FLAGS);

    const byte reg1 = parse_register();
    expect(TokenType::COMMA, "expected ','");

    const byte reg2 = parse_register();
    expect(TokenType::COMMA, "expected ','");

    const byte operand_reg1 = parse_register();
    expect(TokenType::COMMA, "expected ','");

    const byte operand_reg2 = parse_register();
    return Emulator32bit::asm_format_o2(opcode, s, reg1, reg2, operand_reg1, operand_reg2);
}

word Assembler::parse_format_o1(byte opcode)
{
    const bool s = m_cursor.next().has(basm::SETS_FLAGS);

    const byte reg1 = parse_register();
    expect(TokenType::COMMA, "expected ','");

    const byte reg2 = parse_register();
    expect(TokenType::COMMA, "expected ','");

    if (basm::is_register(m_cursor.peek().type))
    {
        const byte operand_reg = parse_register();
        return Emulator32bit::asm_format_o1(opcode, reg1, reg2, false, operand_reg, 0, s);
    }
    else
    {
        const int shift_amt = parse_expression();
        check(word(shift_amt) < (1ULL << 5),
              "shift amount must fit in 5 bits, expected < 32, got " + std::to_string(shift_amt));
        return Emulator32bit::asm_format_o1(opcode, reg1, reg2, true, 0, shift_amt, s);
    }
}

word Assembler::parse_format_o(byte opcode, bool implicit_dest)
{
    const bool s = m_cursor.next().has(basm::SETS_FLAGS);

    byte reg1 = 31; // xzr
    if (!implicit_dest)
    {
        reg1 = parse_register();
        expect(TokenType::COMMA, "expected ','");
    }

    const byte reg2 = parse_register();
    expect(TokenType::COMMA, "expected ','");

    if (basm::is_register(m_cursor.peek().type))
    {
        const byte operand_reg = parse_register();

        // Shift.
        ShiftType shift = ShiftType::SHIFT_LSL;
        int shift_amt = 0;
        if (m_cursor.accept(TokenType::COMMA))
        {
            parse_shift(shift, shift_amt);
        }

        return Emulator32bit::asm_format_o(opcode, s, reg1, reg2, operand_reg, shift, shift_amt);
    }
    else
    {
        word operand = 0;
        if (m_cursor.accept(TokenType::RELOCATION_EMU32_O_LO12))
        {
            const std::string symbol =
                expect(TokenType::SYMBOL, "expected a symbol to follow the relocation").str();
            m_obj.add_symbol(symbol, 0, ObjectFile::SymbolTableEntry::BindingInfo::WEAK);

            m_obj.rel_text.push_back({
                .offset = word(m_obj.text_section.size() * 4),
                .symbol = m_obj.string_table[symbol],
                .type = ObjectFile::RelocationEntry::Type::R_EMU32_O_LO12,

                // TODO: Support shift in future.
                .shift = 0,
                .token = m_cursor.position(),
            });
        }
        else if (basm::is_integer_literal(m_cursor.peek().type)
                 || m_cursor.check(TokenType::LITERAL_CHAR))
        {
            operand = parse_expression();
            check(operand < (1ULL << 14), "immediate must be a 14 bit value");
        }
        else
        {
            fail(m_cursor.peek(), "expected a register, a number or a relocation, got "
                                      + basm::describe(m_cursor.peek()));
        }

        return Emulator32bit::asm_format_o(opcode, s, reg1, reg2, operand);
    }
}

word Assembler::parse_format_atomic(byte width, byte atopcode)
{
    m_cursor.next();

    const byte xt = parse_register();
    expect(TokenType::COMMA, "expected ','");

    const byte xn = parse_register();
    expect(TokenType::COMMA, "expected ','");

    expect(TokenType::OPEN_BRACKET, "expected '[' to start the memory address");

    const byte xm = parse_register();
    expect(TokenType::CLOSE_BRACKET, "expected ']' to end the memory address");

    return Emulator32bit::asm_atomic(xt, xn, xm, width, atopcode);
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
void Assembler::_add()
{
    const word instruction = parse_format_o(Emulator32bit::_op_add);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_sub()
{
    const word instruction = parse_format_o(Emulator32bit::_op_sub);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_rsb()
{
    const word instruction = parse_format_o(Emulator32bit::_op_rsb);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_adc()
{
    const word instruction = parse_format_o(Emulator32bit::_op_adc);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_sbc()
{
    const word instruction = parse_format_o(Emulator32bit::_op_sbc);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_rsc()
{
    const word instruction = parse_format_o(Emulator32bit::_op_rsc);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_mul()
{
    const word instruction = parse_format_o(Emulator32bit::_op_mul);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_umull()
{
    const word instruction = parse_format_o2(Emulator32bit::_op_umull);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_smull()
{
    const word instruction = parse_format_o2(Emulator32bit::_op_smull);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_vabs()
{
    fail(*m_statement, "vabs.f32 is not implemented yet");
}

void Assembler::_vneg()
{
    fail(*m_statement, "vneg.f32 is not implemented yet");
}

void Assembler::_vsqrt()
{
    fail(*m_statement, "vsqrt.f32 is not implemented yet");
}

void Assembler::_vadd()
{
    fail(*m_statement, "vadd.f32 is not implemented yet");
}

void Assembler::_vsub()
{
    fail(*m_statement, "vsub.f32 is not implemented yet");
}

void Assembler::_vdiv()
{
    fail(*m_statement, "vdiv.f32 is not implemented yet");
}

void Assembler::_vmul()
{
    fail(*m_statement, "vmul.f32 is not implemented yet");
}

void Assembler::_vcmp()
{
    fail(*m_statement, "vcmp.f32 is not implemented yet");
}

void Assembler::_vsel()
{
    fail(*m_statement, "vsel.f32 is not implemented yet");
}

void Assembler::_vcint()
{
    fail(*m_statement, "vcint is not implemented yet");
}

void Assembler::_vcflo()
{
    fail(*m_statement, "vcflo is not implemented yet");
}

void Assembler::_vmov()
{
    fail(*m_statement, "vmov.f32 is not implemented yet");
}

void Assembler::_and()
{
    const word instruction = parse_format_o(Emulator32bit::_op_and);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_orr()
{
    const word instruction = parse_format_o(Emulator32bit::_op_orr);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_eor()
{
    const word instruction = parse_format_o(Emulator32bit::_op_eor);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_bic()
{
    const word instruction = parse_format_o(Emulator32bit::_op_bic);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_lsl()
{
    const word instruction = parse_format_o1(Emulator32bit::_op_lsl);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_lsr()
{
    const word instruction = parse_format_o1(Emulator32bit::_op_lsr);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_asr()
{
    const word instruction = parse_format_o1(Emulator32bit::_op_asr);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_ror()
{
    const word instruction = parse_format_o1(Emulator32bit::_op_ror);
    m_obj.text_section.push_back(instruction);
}

// cmp, cmn, tst and teq are the ALU operations without a destination, so they are written as
// `cmp xn, <operand>` and encoded with xzr as the destination.
void Assembler::_cmp()
{
    // TODO: Alias subs.
    const word instruction = parse_format_o(Emulator32bit::_op_cmp, true);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_cmn()
{
    // TODO: Alias adds.
    const word instruction = parse_format_o(Emulator32bit::_op_cmn, true);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_tst()
{
    // TODO: Alias ands.
    const word instruction = parse_format_o(Emulator32bit::_op_tst, true);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_teq()
{
    // TODO: Alias eors.
    const word instruction = parse_format_o(Emulator32bit::_op_teq, true);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_mov()
{
    const word instruction = parse_format_o3(Emulator32bit::_op_mov);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_mvn()
{
    const word instruction = parse_format_o3(Emulator32bit::_op_mvn);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_ldr()
{
    const word instruction = parse_format_m(Emulator32bit::_op_ldr);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_str()
{
    const word instruction = parse_format_m(Emulator32bit::_op_str);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_ldrb()
{
    const word instruction = parse_format_m(Emulator32bit::_op_ldrb);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_strb()
{
    const word instruction = parse_format_m(Emulator32bit::_op_strb);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_ldrh()
{
    const word instruction = parse_format_m(Emulator32bit::_op_ldrh);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_strh()
{
    const word instruction = parse_format_m(Emulator32bit::_op_strh);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_hlt()
{
    m_cursor.next();
    m_obj.text_section.push_back(Emulator32bit::asm_hlt());
}

void Assembler::_nop()
{
    m_cursor.next();
    m_obj.text_section.push_back(Emulator32bit::asm_nop());
}

void Assembler::_msr()
{
    m_cursor.next();

    const word sysreg = parse_sysreg();
    expect(TokenType::COMMA, "expected ',' and a register or an immediate");

    word instruction;
    if (basm::is_register(m_cursor.peek().type))
    {
        const byte xn = parse_register();
        instruction = Emulator32bit::asm_msr(sysreg, false, xn);
    }
    else
    {
        const word imm16 = parse_expression();
        check(imm16 < (1ULL << 16), "immediate must be a 16 bit value");

        instruction = Emulator32bit::asm_msr(sysreg, true, imm16);
    }
    m_obj.text_section.push_back(instruction);
}

void Assembler::_mrs()
{
    m_cursor.next();

    const byte xn = parse_register();
    expect(TokenType::COMMA, "expected ',' and a system register");

    const byte sysreg = parse_sysreg();

    const word instruction = Emulator32bit::asm_mrs(xn, sysreg);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_tlbi()
{
    m_cursor.next();

    fail(*m_statement, "tlbi is not implemented yet");
}

void Assembler::_swp()
{
    const word instruction =
        parse_format_atomic(Emulator32bit::kAtomicWidth_word, Emulator32bit::kAtomicId_swp);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_swpb()
{
    const word instruction =
        parse_format_atomic(Emulator32bit::kAtomicWidth_byte, Emulator32bit::kAtomicId_swp);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_swph()
{
    const word instruction =
        parse_format_atomic(Emulator32bit::kAtomicWidth_hword, Emulator32bit::kAtomicId_swp);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_ldadd()
{
    const word instruction =
        parse_format_atomic(Emulator32bit::kAtomicWidth_word, Emulator32bit::kAtomicId_ldadd);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_ldaddb()
{
    const word instruction =
        parse_format_atomic(Emulator32bit::kAtomicWidth_byte, Emulator32bit::kAtomicId_ldadd);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_ldaddh()
{
    const word instruction =
        parse_format_atomic(Emulator32bit::kAtomicWidth_hword, Emulator32bit::kAtomicId_ldadd);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_ldclr()
{
    const word instruction =
        parse_format_atomic(Emulator32bit::kAtomicWidth_word, Emulator32bit::kAtomicId_ldclr);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_ldclrb()
{
    const word instruction =
        parse_format_atomic(Emulator32bit::kAtomicWidth_byte, Emulator32bit::kAtomicId_ldclr);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_ldclrh()
{
    const word instruction =
        parse_format_atomic(Emulator32bit::kAtomicWidth_hword, Emulator32bit::kAtomicId_ldclr);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_ldset()
{
    const word instruction =
        parse_format_atomic(Emulator32bit::kAtomicWidth_word, Emulator32bit::kAtomicId_ldset);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_ldsetb()
{
    const word instruction =
        parse_format_atomic(Emulator32bit::kAtomicWidth_byte, Emulator32bit::kAtomicId_ldset);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_ldseth()
{
    const word instruction =
        parse_format_atomic(Emulator32bit::kAtomicWidth_hword, Emulator32bit::kAtomicId_ldset);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_b()
{
    const word instruction = parse_format_b1(Emulator32bit::_op_b);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_bl()
{
    const word instruction = parse_format_b1(Emulator32bit::_op_bl);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_bx()
{
    const word instruction = parse_format_b2(Emulator32bit::_op_bx);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_blx()
{
    const word instruction = parse_format_b2(Emulator32bit::_op_blx);
    m_obj.text_section.push_back(instruction);
}

void Assembler::_swi()
{
    const word instruction = parse_format_b1(Emulator32bit::_op_swi);
    m_obj.text_section.push_back(instruction);
}

/// `ret` is `bx x29`, x29 being the link register.
void Assembler::_ret()
{
    m_cursor.next();

    constexpr byte kLinkRegister = 29;
    m_obj.text_section.push_back(
        Emulator32bit::asm_format_b2(Emulator32bit::_op_bx, ConditionCode::AL, kLinkRegister));
}

void Assembler::_adrp()
{
    const word instruction = parse_format_m1(Emulator32bit::_op_adrp);
    m_obj.text_section.push_back(instruction);
}
