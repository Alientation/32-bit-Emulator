#include <emulator32bit/emulator32bit.h>

#include "util/logger.h"
#include <util/common.h>

#include <string>

/**
 * @internal
 * @brief                     Useful macros to extract information from instruction bits
 * @hideinitializer
 *
 */
#define _X1(instr) (bitfield_unsigned(instr, 20, 5))  /* bits 20 to 24 */
#define _X2(instr) (bitfield_unsigned(instr, 15, 5))  /* bits 15 to 19 */
#define _X3(instr) (bitfield_unsigned(instr, 9, 5))   /* bits 9 to 13 */
#define _X4(instr) (bitfield_unsigned(instr, 4, 5))   /* bits 4 to 8 */

#define _SX1(instr) (bitfield_unsigned(instr, 17, 5)) /* bits 17 to 21 */
#define _SX2(instr) (bitfield_unsigned(instr, 11, 5)) /* bits 11 to 15 */
#define _SX3(instr) (bitfield_unsigned(instr, 6, 5))  /* bits 6 to 10 */

/**
 * @internal
 * @brief                   Parse the value of the argument for instruction format O
 * @details                 Also used to parse value of argument for some other instruction format
 *                          like format M which conveniently has a similar structure
 * @param cpu Emulator context.
 * @param instr 32 bit instruction to extract the value of the argument.
 */
static word get_format_o_arg(Emulator32bit &cpu, const word instr)
{
    if (test_bit(instr, 14))
    {
        return bitfield_unsigned(instr, 0, 14);
    }

    const word value = cpu.read_reg(_X3(instr));
    const auto type = ShiftType(bitfield_unsigned(instr, 7, 2));
    const U8 amount = bitfield_unsigned(instr, 2, 5);

    return alu_shift(value, type, amount, {}).result;
}

/**
 * @internal
 * @brief                    A sequence of bits to add to a @ref Joiner
 *
 */
struct JPart
{
    JPart(const int bits, const word val = 0) :
        bits(bits),
        val(val)
    {
    }

    const int bits; /* Number of bits stored in this part */
    const word
        val; /* Contents of the bits stored in this part, stored with the first bit in the most significant bit */
};

/**
 * @internal
 * @brief                    A value that is formed by joining @ref JPart
 *
 */
class Joiner
{
  public:
    word val = 0; /* Content stored so far */

    /**
         * @internal
         * @brief            Add a new @ref JPart
         *
         * @param            p: @ref JPart to add
         * @return             Reference to this object
         */
    Joiner &operator<<(const JPart &p)
    {
        val <<= p.bits;
        val += p.val;
        return *this;
    }

    /**
         * @internal
         * @brief            Add filler bits all set to 0
         *
         * @param             bits: Number of bits to add
         * @return             Reference to this object
         */
    Joiner &operator<<(const int bits)
    {
        val <<= bits;
        return *this;
    }

    /**
         * @internal
         * @brief             Extract the value of this object
         *
         * @return             word
         */
    operator word() const
    {
        return val;
    }
};

/**
 * @brief                    Constructs instructions of format O with an imm14 operand
 *
 * @param                     opcode: 6 bit identifier of a format O instruction
 * @param                     s: whether condition flags are set
 * @param                     xd: 5 bit destination register identifier
 * @param                     xn: 5 bit operand register identifier
 * @param                     imm14: 14 bit immediate value
 * @return                     instruction word
 */
word Emulator32bit::asm_format_o(const U8 opcode, const bool s, const int xd, const int xn,
                                 const int imm14)
{
    return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xd) << JPart(5, xn)
                    << JPart(1, 1) << JPart(14, imm14);
}

/**
 * @brief                     Constructs instructions of format O with an arg operand
 *
 * @param                     opcode: 6 bit identifier of a format O instruction
 * @param                     s: whether condition flags are set
 * @param                     xd: 5 bit destination register identifier
 * @param                     xn: 5 bit operand register identifier
 * @param                     xm: 5 bit operand register identifier
 * @param                     shift: shift type to be applied on the value in the xm register
 * @param                     imm5: shift amount
 * @return                     instruction word
 */
word Emulator32bit::asm_format_o(const U8 opcode, const bool s, const int xd, const int xn,
                                 const int xm, const ShiftType shift, const int imm5)
{
    return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xd) << JPart(5, xn) << 1
                    << JPart(5, xm) << JPart(2, U8(shift)) << JPart(5, imm5) << 2;
}

word Emulator32bit::asm_format_o1(const U8 opcode, const int xd, const int xn, const bool imm,
                                  const int xm, const int imm5, const bool s)
{
    return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xd) << JPart(5, xn)
                    << JPart(1, imm) << JPart(5, xm) << 2 << JPart(5, imm5) << 2;
}

word Emulator32bit::asm_format_o2(const U8 opcode, const bool s, const int xlo, const int xhi,
                                  const int xn, const int xm)
{
    return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xlo) << JPart(5, xhi) << 1
                    << JPart(5, xn) << JPart(5, xm) << 4;
}

word Emulator32bit::asm_format_o3(const U8 opcode, const bool s, const int xd, const int imm19)
{
    return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xd) << JPart(1, 1)
                    << JPart(19, imm19);
}

word Emulator32bit::asm_format_o3(const U8 opcode, const bool s, const int xd, const int xn,
                                  const int imm14)
{
    return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xd) << JPart(1, 0)
                    << JPart(5, xn) << JPart(14, imm14);
}

word Emulator32bit::asm_format_m(const U8 opcode, const bool sign, const int xt, const int xn,
                                 const int xm, const ShiftType shift, const int imm5,
                                 const AddrType adr)
{
    return Joiner() << JPart(6, opcode) << JPart(1, sign) << JPart(5, xt) << JPart(5, xn) << 1
                    << JPart(5, xm) << JPart(2, U8(shift)) << JPart(5, imm5) << JPart(2, U8(adr));
}

word Emulator32bit::asm_format_m(const U8 opcode, const bool sign, const int xt, const int xn,
                                 const int simm12, const AddrType adr)
{
    return Joiner() << JPart(6, opcode) << JPart(1, sign) << JPart(5, xt) << JPart(5, xn)
                    << JPart(1, 1) << JPart(12, bitfield_unsigned(simm12, 0, 12))
                    << JPart(2, U8(adr));
}

word Emulator32bit::asm_format_m1(const U8 opcode, const int xd, const int imm20)
{
    return Joiner() << JPart(6, opcode) << 1 << JPart(5, xd) << JPart(20, imm20);
}

word Emulator32bit::asm_format_b1(const U8 opcode, const ConditionCode cond, const sword simm22)
{
    return Joiner() << JPart(6, opcode) << JPart(4, word(cond))
                    << JPart(22, bitfield_unsigned(simm22, 0, 22));
}

word Emulator32bit::asm_format_b2(const U8 opcode, const ConditionCode cond, const int xd)
{
    return Joiner() << JPart(6, opcode) << JPart(4, word(cond)) << JPart(5, xd) << 17;
}

void Emulator32bit::_special_instructions(const word instr)
{
    word opspec = bitfield_unsigned(instr, 22, 4);

    switch (opspec)
    {
    case kSpecialOpId_hlt:
        _hlt(instr);
        break;
    case kSpecialOpId_nop:
        _nop(instr);
        break;
    case kSpecialOpId_msr:
        _msr(instr);
        break;
    case kSpecialOpId_mrs:
        _mrs(instr);
        break;
    case kSpecialOpId_tlbi:
        _tlbi(instr);
        break;
    case kSpecialOpId_atomic:
        _atomic(instr);
        break;
    default:
        throw Exception(Emulator32bit::InterruptType::BAD_INSTR,
                        "Bad OPSPEC specifier " + std::to_string(opspec));
    }
}

void Emulator32bit::_hlt(const word instr)
{
    UNUSED(instr);
    throw Exception(InterruptType::HALT_INSTR, "HLT Exception");
}

word Emulator32bit::asm_hlt()
{
    return Joiner() << JPart(6, _op_special_instructions) << JPart(4, kSpecialOpId_hlt) << 22;
}

void Emulator32bit::_nop(const word instr)
{
    UNUSED(instr);
    return; // do nothing
}

word Emulator32bit::asm_nop()
{
    return Joiner() << JPart(6, _op_special_instructions) << JPart(4, kSpecialOpId_nop) << 22;
}

void Emulator32bit::_msr(const word instr)
{
    const word sysreg = _SX1(instr);
    const bool imm = test_bit(instr, 16);
    word val = imm ? bitfield_unsigned(instr, 0, 16) : read_reg(_SX2(instr));

    (void) (val);

    // TODO
    switch (sysreg)
    {
    default:
        throw Exception(Emulator32bit::InterruptType::BAD_REG,
                        "System register " + std::to_string(sysreg) + " unimplemented.");
    }
}

word Emulator32bit::asm_msr(U8 sysreg, bool imm, word xn_or_imm16)
{
    if (imm)
    {
        return Joiner() << JPart(6, _op_special_instructions) << JPart(4, kSpecialOpId_msr)
                        << JPart(5, sysreg) << JPart(1, imm) << JPart(16, xn_or_imm16);
    }
    else
    {
        return Joiner() << JPart(6, _op_special_instructions) << JPart(4, kSpecialOpId_msr)
                        << JPart(5, sysreg) << JPart(1, imm) << JPart(5, xn_or_imm16) << 11;
    }
}

void Emulator32bit::_mrs(const word instr)
{
    word xn = _SX1(instr);
    word sysreg = _SX2(instr);

    // todo
    (void) (xn);
    (void) (sysreg);

    throw Exception(Emulator32bit::InterruptType::BAD_INSTR, "MRS unimplemented.");
}

word Emulator32bit::asm_mrs(U8 xn, U8 sysreg)
{
    return Joiner() << JPart(6, _op_special_instructions) << JPart(4, kSpecialOpId_mrs)
                    << JPart(5, xn) << 1 << JPart(5, sysreg) << 11;
}

void Emulator32bit::_tlbi(const word instr)
{
    word xt = _SX1(instr);
    bool isxt = test_bit(instr, 16);
    word imm16 = bitfield_unsigned(instr, 0, 16);

    // todo
    (void) (xt);
    (void) (isxt);
    (void) (imm16);

    throw Exception(Emulator32bit::InterruptType::BAD_INSTR, "TLBI unimplemented.");
}

word Emulator32bit::asm_tlbi(U8 xt, bool isxt, word imm16)
{
    return Joiner() << JPart(6, _op_special_instructions) << JPart(4, kSpecialOpId_tlbi)
                    << JPart(5, xt) << JPart(1, isxt) << JPart(16, imm16);
}

void Emulator32bit::_atomic_rmw(const word instr, const AtomicOperation operation)
{
    const U8 xt = _SX1(instr);
    const U8 xn = _SX2(instr);
    const U8 xm = _SX3(instr);

    const word mem_adr = read_reg(xm);
    const U8 width = bitfield_unsigned(instr, 4, 2);

    const word val_reg = read_reg(xn);

    word val_mem;
    word new_val;

    switch (width)
    {
    case kAtomicWidth_word:
        val_mem = system_bus->read_word(mem_adr);
        break;

    case kAtomicWidth_byte:
        val_mem = system_bus->read_byte(mem_adr);
        break;

    case kAtomicWidth_hword:
        val_mem = system_bus->read_hword(mem_adr);
        break;

    default:
        AEMU_FATAL("Invalid ATOMIC_WIDTH: {}", width);
        return;
    }

    // The value in the source register is truncated to the
    // width of the atomic operation.
    const word operand = width == kAtomicWidth_word    ? val_reg
                         : width == kAtomicWidth_hword ? val_reg & 0xFFFF
                                                       : val_reg & 0xFF;

    switch (operation)
    {
    case AtomicOperation::SWP:
        new_val = operand;
        break;

    case AtomicOperation::LDADD:
        new_val = val_mem + operand;
        break;

    case AtomicOperation::LDCLR:
        new_val = val_mem & ~operand;
        break;

    case AtomicOperation::LDSET:
        new_val = val_mem | operand;
        break;

    default:
        AEMU_FATAL("Invalid atomic operation: {}", static_cast<int>(operation));
        return;
    }

    // Return the original memory value in Xt.
    write_reg(xt, val_mem);

    switch (width)
    {
    case kAtomicWidth_word:
        system_bus->write_word(mem_adr, new_val);
        break;

    case kAtomicWidth_byte:
        system_bus->write_byte(mem_adr, new_val);
        break;

    case kAtomicWidth_hword:
        system_bus->write_hword(mem_adr, new_val);
        break;

    default:
        // Already validated above.
        break;
    }
}

void Emulator32bit::_atomic(const word instr)
{
    const U8 atop = bitfield_unsigned(instr, 0, 4);

    switch (atop)
    {
    case kAtomicId_swp:
        _atomic_rmw(instr, AtomicOperation::SWP);
        break;
    case kAtomicId_ldadd:
        _atomic_rmw(instr, AtomicOperation::LDADD);
        break;
    case kAtomicId_ldclr:
        _atomic_rmw(instr, AtomicOperation::LDCLR);
        break;
    case kAtomicId_ldset:
        _atomic_rmw(instr, AtomicOperation::LDSET);
        break;
    default:
        throw Exception(Emulator32bit::InterruptType::BAD_INSTR,
                        "Atomic op " + std::to_string(atop) + " unimplemented.");
    }
}

word Emulator32bit::asm_atomic(word xt, word xn, word xm, U8 width, U8 atop)
{
    return Joiner() << JPart(6, _op_special_instructions) << JPart(4, kSpecialOpId_atomic)
                    << JPart(5, xt) << 1 << JPart(5, xn) << JPart(5, xm) << JPart(2, width)
                    << JPart(4, atop);
}

// Format O data-processing instructions: xd = f(xn, op2). `expr` computes the AluResult from `rn`
// (the value of xn) and `op2` (the immediate or shifted register operand). Flags are only written
// back when the S bit is set.
#define ALU_OP(name, expr)                                                                         \
    void Emulator32bit::name(const word instr)                                                     \
    {                                                                                              \
        const word rn = read_reg(_X2(instr));                                                      \
        const word op2 = get_format_o_arg(*this, instr);                                           \
        const AluResult result = expr;                                                             \
        if (test_bit(instr, kInstructionUpdateFlagBit)) set_NZCV(result.flags);                    \
        write_reg(_X1(instr), word(result.result));                                                \
    }

// Compare/test instructions: same operands as ALU_OP, but only the flags are written, always.
#define FLAGS_ONLY_OP(name, expr)                                                                  \
    void Emulator32bit::name(const word instr)                                                     \
    {                                                                                              \
        const word rn = read_reg(_X2(instr));                                                      \
        const word op2 = get_format_o_arg(*this, instr);                                           \
        set_NZCV((expr).flags);                                                                    \
    }

// Shift instructions: the amount is an imm5 or the low 5 bits of xm.
#define SHIFT_OP(name, shift_type)                                                                 \
    void Emulator32bit::name(const word instr)                                                     \
    {                                                                                              \
        const unsigned amount =                                                                    \
            test_bit(instr, 14) ? bitfield_unsigned(instr, 2, 5) : (read_reg(_X3(instr)) & 0x1F);  \
        const AluResult result = alu_shift(read_reg(_X2(instr)), shift_type, amount, get_NZCV());  \
        if (test_bit(instr, kInstructionUpdateFlagBit)) set_NZCV(result.flags);                    \
        write_reg(_X1(instr), result.result);                                                      \
    }

// Long multiplies: {xhi:xlo} = xn * xm, where xlo is _X1 and xhi is _X2.
#define LONG_MUL_OP(name, alu_fn)                                                                  \
    void Emulator32bit::name(const word instr)                                                     \
    {                                                                                              \
        const AluResult result = alu_fn(read_reg(_X3(instr)), read_reg(_X4(instr)), get_NZCV());   \
        if (test_bit(instr, kInstructionUpdateFlagBit)) set_NZCV(result.flags);                    \
        write_reg(_X1(instr), word(result.result));                                                \
        write_reg(_X2(instr), word(result.result >> 32));                                          \
    }

ALU_OP(_add, alu_add(rn, op2, false))
ALU_OP(_sub, alu_sub(rn, op2, true))
ALU_OP(_rsb, alu_sub(op2, rn, true))
ALU_OP(_adc, alu_add(rn, op2, get_flag(kCFlagBit)))
ALU_OP(_sbc, alu_sub(rn, op2, get_flag(kCFlagBit)))
ALU_OP(_rsc, alu_sub(op2, rn, get_flag(kCFlagBit)))
ALU_OP(_mul, alu_mul(rn, op2, get_NZCV()))

// N and Z are set from the result. C and V are left unchanged (the shifter carry-out of the second
// operand is ignored).
// https://developer.arm.com/documentation/dui0489/h/arm-and-thumb-instructions/and--orr--eor--bic--and-orn
ALU_OP(_and, alu_and(rn, op2, get_NZCV()))
ALU_OP(_orr, alu_orr(rn, op2, get_NZCV()))
ALU_OP(_eor, alu_eor(rn, op2, get_NZCV()))
ALU_OP(_bic, alu_bic(rn, op2, get_NZCV()))

LONG_MUL_OP(_umull, alu_umull)
LONG_MUL_OP(_smull, alu_smull)

SHIFT_OP(_lsl, ShiftType::SHIFT_LSL)
SHIFT_OP(_lsr, ShiftType::SHIFT_LSR)
SHIFT_OP(_asr, ShiftType::SHIFT_ASR)
SHIFT_OP(_ror, ShiftType::SHIFT_ROR)

FLAGS_ONLY_OP(_cmp, alu_sub(rn, op2, true))       // alias to subs
FLAGS_ONLY_OP(_cmn, alu_add(rn, op2, false))      // alias to adds
FLAGS_ONLY_OP(_tst, alu_and(rn, op2, get_NZCV())) // alias to ands
FLAGS_ONLY_OP(_teq, alu_eor(rn, op2, get_NZCV())) // alias to eors

#undef ALU_OP
#undef FLAGS_ONLY_OP
#undef SHIFT_OP
#undef LONG_MUL_OP

// todo WILL DO LATER JUST NOT NOW
void Emulator32bit::_vabs(const word instr)
{
    UNUSED(instr);
}

void Emulator32bit::_vneg(const word instr)
{
    UNUSED(instr);
}

void Emulator32bit::_vsqrt(const word instr)
{
    UNUSED(instr);
}

void Emulator32bit::_vadd(const word instr)
{
    UNUSED(instr);
}

void Emulator32bit::_vsub(const word instr)
{
    UNUSED(instr);
}

void Emulator32bit::_vdiv(const word instr)
{
    UNUSED(instr);
}

void Emulator32bit::_vmul(const word instr)
{
    UNUSED(instr);
}

void Emulator32bit::_vcmp(const word instr)
{
    UNUSED(instr);
}

void Emulator32bit::_vsel(const word instr)
{
    UNUSED(instr);
}

void Emulator32bit::_vcint(const word instr)
{
    UNUSED(instr);
}

void Emulator32bit::_vcflo(const word instr)
{
    UNUSED(instr);
}

void Emulator32bit::_vmov(const word instr)
{
    UNUSED(instr);
}

static word get_mov_arg(Emulator32bit &cpu, const word instr)
{
    if (test_bit(instr, 19)) return bitfield_unsigned(instr, 0, 19);
    return bitfield_unsigned(instr, 0, 14) + cpu.read_reg(bitfield_unsigned(instr, 14, 5));
}

// Moves: xd = f(operand), where the operand is an imm19 or xn + imm14.
#define MOVE_OP(name, alu_fn)                                                                      \
    void Emulator32bit::name(const word instr)                                                     \
    {                                                                                              \
        const AluResult result = alu_fn(get_mov_arg(*this, instr), get_NZCV());                    \
        if (test_bit(instr, kInstructionUpdateFlagBit)) set_NZCV(result.flags);                    \
        write_reg(_X1(instr), result.result);                                                      \
    }

MOVE_OP(_mov, alu_mov)
MOVE_OP(_mvn, alu_mvn)
#undef MOVE_OP

word Emulator32bit::calc_mem_addr(word xn, sword offset, U8 addr_mode)
{
    word mem_addr = 0;
    const word xn_val = read_reg(xn);
    if (addr_mode == 0)
    {
        mem_addr = xn_val + offset;
    }
    else if (addr_mode == 1)
    {
        mem_addr = xn_val + offset;
        write_reg(xn, mem_addr);
    }
    else if (addr_mode == 2)
    {
        mem_addr = xn_val;
        write_reg(xn, xn_val + offset);
    }
    else
    {
        throw Exception(InterruptType::BAD_INSTR,
                        "Bad memory address mode " + std::to_string(addr_mode));
    }
    return mem_addr;
}

void Emulator32bit::_ldr(const word instr)
{
    const U8 xt = _X1(instr);
    const U8 xn = _X2(instr);
    const bool simm = test_bit(instr, 14);
    sword offset = simm ? bitfield_signed(instr, 2, 12) : get_format_o_arg(*this, instr);

    const U8 address_mode = bitfield_unsigned(instr, 0, 2);
    const word mem_addr = calc_mem_addr(xn, offset, address_mode);
    const word read_val = system_bus->read_word(mem_addr);
    write_reg(xt, read_val);
}

void Emulator32bit::_ldrb(const word instr)
{
    const bool sign = test_bit(instr, 25);
    const U8 xt = _X1(instr);
    const U8 xn = _X2(instr);
    const bool simm = test_bit(instr, 14);
    sword offset = simm ? bitfield_signed(instr, 2, 12) : get_format_o_arg(*this, instr);

    const U8 address_mode = bitfield_unsigned(instr, 0, 2);
    const word mem_addr = calc_mem_addr(xn, offset, address_mode);
    word read_val = system_bus->read_byte(mem_addr);
    if (sign)
    {
        read_val = sword(S8(read_val));
    }
    write_reg(xt, read_val);
}

void Emulator32bit::_ldrh(const word instr)
{
    const bool sign = test_bit(instr, 25);
    const U8 xt = _X1(instr);
    const U8 xn = _X2(instr);
    const bool simm = test_bit(instr, 14);
    sword offset = simm ? bitfield_signed(instr, 2, 12) : get_format_o_arg(*this, instr);

    const U8 address_mode = bitfield_unsigned(instr, 0, 2);
    const word mem_addr = calc_mem_addr(xn, offset, address_mode);
    word read_val = system_bus->read_hword(mem_addr);
    if (sign)
    {
        read_val = sword(S16(read_val));
    }
    write_reg(xt, read_val);
}

void Emulator32bit::_str(const word instr)
{
    const U8 xt = _X1(instr);
    const U8 xn = _X2(instr);
    const bool simm = test_bit(instr, 14);
    sword offset = simm ? bitfield_signed(instr, 2, 12) : get_format_o_arg(*this, instr);

    const U8 address_mode = bitfield_unsigned(instr, 0, 2);
    const word mem_addr = calc_mem_addr(xn, offset, address_mode);
    const word write_val = read_reg(xt);
    system_bus->write_word(mem_addr, write_val);
}

void Emulator32bit::_strb(const word instr)
{
    const bool sign = test_bit(instr, 25);
    const U8 xt = _X1(instr);
    const U8 xn = _X2(instr);
    const bool simm = test_bit(instr, 14);
    sword offset = simm ? bitfield_signed(instr, 2, 12) : get_format_o_arg(*this, instr);

    const U8 address_mode = bitfield_unsigned(instr, 0, 2);
    const word mem_addr = calc_mem_addr(xn, offset, address_mode);
    word write_val = read_reg(xt);
    if (sign)
    {
        write_val = sword(S8(write_val));
    }
    system_bus->write_byte(mem_addr, write_val);
}

void Emulator32bit::_strh(const word instr)
{
    const bool sign = test_bit(instr, 25);
    const U8 xt = _X1(instr);
    const U8 xn = _X2(instr);
    const bool simm = test_bit(instr, 14);
    sword offset = simm ? bitfield_signed(instr, 2, 12) : get_format_o_arg(*this, instr);

    const U8 address_mode = bitfield_unsigned(instr, 0, 2);
    const word mem_addr = calc_mem_addr(xn, offset, address_mode);
    word write_val = read_reg(xt);
    if (sign)
    {
        write_val = sword(S16(write_val));
    }
    system_bus->write_hword(mem_addr, write_val);
}

void Emulator32bit::_b(const word instr)
{
    const U8 cond = bitfield_unsigned(instr, 22, 4);
    if (check_cond(m_pstate, cond))
    {
        m_pc += (bitfield_signed(instr, 0, 22) << 2)
                - 4; /* account for execution loop incrementing _pc by 4 */
    }
}

void Emulator32bit::_bl(const word instr)
{
    const U8 cond = bitfield_unsigned(instr, 22, 4);
    if (check_cond(m_pstate, cond))
    {
        write_reg(Register::LR, m_pc + 4);
        m_pc += (bitfield_signed(instr, 0, 22) << 2) - 4;
    }
}

void Emulator32bit::_bx(const word instr)
{
    const U8 cond = bitfield_unsigned(instr, 22, 4);
    const U8 reg = bitfield_unsigned(instr, 17, 5);
    if (check_cond(m_pstate, cond))
    {
        m_pc = sword(read_reg(reg)) - 4;
    }
}

void Emulator32bit::_blx(const word instr)
{
    const U8 cond = bitfield_unsigned(instr, 22, 4);
    const U8 reg = bitfield_unsigned(instr, 17, 5);
    if (check_cond(m_pstate, cond))
    {
        write_reg(Register::LR, m_pc + 4);
        m_pc = sword(read_reg(reg)) - 4;
    }
}

void Emulator32bit::_adrp(const word instr)
{
    const U8 xd = _X1(instr);
    const word imm20 = bitfield_unsigned(instr, 0, 20);

    signed int simm21 = imm20;
    if (test_bit(instr, kInstructionUpdateFlagBit))
    {
        simm21 -= (1 << 20);
    }

    word val = mask_0(m_pc, 0, 12) + (simm21 << 12);
    write_reg(xd, val);
}