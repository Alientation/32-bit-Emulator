#include <assembler/assembler.h>

#include "util/logger.h"
#include <util/common.h>

#include <iterator>
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
    // A symbol is the label to branch to, unless it names a constant: then it is a distance.
    if (m_cursor.check(TokenType::SYMBOL) && !is_constant(m_cursor.peek().str()))
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
        // The offset in bytes from this instruction, as a signed number: `b 8` is two
        // instructions ahead, `b -4` the one before. The field holds 22 bits of words.
        const sdword offset = parse_signed_expression();
        check((offset & 0b11) == 0, "branch offset must be 4 byte aligned");
        check(offset >= -(sdword(1) << 23) && offset < (sdword(1) << 23),
              "branch offset must be between -8388608 and 8388604 bytes, got "
                  + std::to_string(offset));
        value = sword(offset >> 2);
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

    // Pre/post-indexing writes the base register back, which is pointless or surprising in a few
    // cases. `zero_offset` is true when the offset is known to be 0.
    const bool is_load = opcode == Emulator32bit::_op_ldr || opcode == Emulator32bit::_op_ldrb
                         || opcode == Emulator32bit::_op_ldrh;
    auto warn_writeback = [&](const Emulator32bit::AddrType mode, const bool zero_offset)
    {
        if (mode == Emulator32bit::AddrType::ADDR_OFFSET) return;

        const std::string kind =
            mode == Emulator32bit::AddrType::ADDR_PRE_INC ? "pre-index" : "post-index";
        const std::string base = "x" + std::to_string(reg_a);
        if (reg_a == U8(Register::XZR))
        {
            warn(*m_statement, "the " + kind + " writeback to xzr is discarded");
        }
        else if (reg_t == reg_a && is_load)
        {
            // Two results for one register: the loaded value wins.
            warn(*m_statement, "the loaded value overwrites the base register '" + base
                                   + "', so the " + kind + " writeback is lost");
        }
        else if (reg_t == reg_a)
        {
            // Defined, but easy to misread: the value stored is the one from before the writeback.
            warn(*m_statement, "'" + base
                                   + "' is both the stored value and the base register; the "
                                     "value from before the "
                                   + kind + " writeback is stored");
        }
        else if (zero_offset)
        {
            warn(*m_statement, "the " + kind + " offset is zero, so the writeback to '" + base
                                   + "' has no effect");
        }
    };

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
            // The offset is a signed 12 bit value (sign extended by the emulator).
            const sdword value = parse_signed_expression();
            check(value >= -(sdword(1) << 11) && value < (sdword(1) << 11),
                  "offset must be a signed 12 bit value (-2048 to 2047)");
            const int offset = int(value);

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

            warn_writeback(addressing_mode, offset == 0);
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

            warn_writeback(addressing_mode, reg_b == U8(Register::XZR));
            return Emulator32bit::asm_format_m(opcode, sign, reg_t, reg_a, reg_b, shift,
                                               shift_amount, addressing_mode);
        }
    }

    // Check for invalid addressing mode.
    check(parsed_addressing_mode, "invalid addressing mode");
    warn_writeback(addressing_mode, true);
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
        else if (at_expression())
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

// The instructions that are encoded the same way as others of their format are described by a
// row of instruction_list.h. The table is built from it.
namespace basm
{
namespace
{

#define BASM_INSTRUCTION_SPEC(X, NAME, text, allows_s, format, a, b)                               \
    {TokenType::INSTRUCTION_##NAME, text, InstructionFormat::format, byte(a), byte(b)},
constexpr InstructionSpec kInstructionSpecs[] = {BASM_INSTRUCTION_LIST(BASM_INSTRUCTION_SPEC, )};
#undef BASM_INSTRUCTION_SPEC

/// The table is indexed by token type, so its rows must follow the order of the token types.
constexpr bool specs_follow_the_token_order()
{
    constexpr size_t first = size_t(TokenType::INSTRUCTION_HLT);
    constexpr size_t last = size_t(TokenType::INSTRUCTION_RET);
    if (std::size(kInstructionSpecs) != last - first + 1)
    {
        return false;
    }
    for (size_t i = 0; i < std::size(kInstructionSpecs); i++)
    {
        if (size_t(kInstructionSpecs[i].token) != first + i)
        {
            return false;
        }
    }
    return true;
}

static_assert(specs_follow_the_token_order());

} // namespace

const InstructionSpec &instruction_spec(TokenType type)
{
    return kInstructionSpecs[size_t(type) - size_t(TokenType::INSTRUCTION_HLT)];
}

} // namespace basm

///
/// Assembles the instruction at the cursor and appends it to .text.
///
/// add x1, x2, x3
/// add x1, x2, #40
/// add x1, x2, x3, lsl 4
/// add x1, x2, :lo12:symbol
/// NOT SUPPORTED -- add x1, x2, :lo12:symbol + 4
///
/// cmp, cmn, tst and teq are the ALU operations without a destination, so they are written as
/// `cmp xn, <operand>` and encoded with xzr as the destination.
///
void Assembler::assemble_instruction(const basm::InstructionSpec &spec)
{
    using Format = basm::InstructionFormat;

    word instruction;
    switch (spec.format)
    {
    case Format::O:
        instruction = parse_format_o(spec.a);
        break;
    case Format::O_NO_DEST:
        instruction = parse_format_o(spec.a, true);
        break;
    case Format::O1:
        instruction = parse_format_o1(spec.a);
        break;
    case Format::O2:
        instruction = parse_format_o2(spec.a);
        break;
    case Format::O3:
        instruction = parse_format_o3(spec.a);
        break;
    case Format::M:
        instruction = parse_format_m(spec.a);
        break;
    case Format::M1:
        instruction = parse_format_m1(spec.a);
        break;
    case Format::B1:
        instruction = parse_format_b1(spec.a);
        break;
    case Format::B2:
        instruction = parse_format_b2(spec.a);
        break;
    case Format::ATOMIC:
        instruction = parse_format_atomic(spec.a, spec.b);
        break;
    case Format::HLT:
        _hlt();
        return;
    case Format::NOP:
        _nop();
        return;
    case Format::MSR:
        _msr();
        return;
    case Format::MRS:
        _mrs();
        return;
    case Format::RET:
        _ret();
        return;
    case Format::UNIMPLEMENTED:
        fail(*m_statement, std::string(spec.text) + " is not implemented yet");
    }
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

/// `ret` is `bx x29`, x29 being the link register.
void Assembler::_ret()
{
    m_cursor.next();

    constexpr byte kLinkRegister = 29;
    m_obj.text_section.push_back(
        Emulator32bit::asm_format_b2(Emulator32bit::_op_bx, ConditionCode::AL, kLinkRegister));
}
