#include <emulator32bit/emulator32bit.h>

#include <util/common.h>
#define AEMU_ONLY_CRITICAL_LOG
#include <util/logger.h>

#include <string>

/**
 * @internal
 * @brief                     Useful macros to extract information from instruction bits
 * @hideinitializer
 *
 */
#define _X1(instr) (bitfield_unsigned (instr, 20, 5))  /* bits 20 to 24 */
#define _X2(instr) (bitfield_unsigned (instr, 15, 5))  /* bits 15 to 19 */
#define _X3(instr) (bitfield_unsigned (instr, 9, 5))   /* bits 9 to 13 */
#define _X4(instr) (bitfield_unsigned (instr, 4, 5))   /* bits 4 to 8 */

#define _SX1(instr) (bitfield_unsigned (instr, 17, 5)) /* bits 17 to 21 */
#define _SX2(instr) (bitfield_unsigned (instr, 11, 5)) /* bits 11 to 15 */
#define _SX3(instr) (bitfield_unsigned (instr, 6, 5))  /* bits 6 to 10 */

/**
 * @internal
 * @brief                     Calculates the new value after applying the specified shift
 *
 * @param                     val: value to shift
 * @param                     shift_type: shift specified by the instruction
 * @param                     imm5: shift amount
 * @return                     shifted value
 *
 */
static word calc_shift (word val, const Emulator32bit::ShiftType shift_type, const U8 imm5)
{
    switch (shift_type)
    {
    case Emulator32bit::ShiftType::SHIFT_LSL:
        DEBUG ("LSL %u", word (imm5));
        val <<= imm5;
        break;
    case Emulator32bit::ShiftType::SHIFT_LSR:
        DEBUG ("LSR %u", word (imm5));
        val >>= imm5;
        break;
    case Emulator32bit::ShiftType::SHIFT_ASR:
        DEBUG ("ASR %u", word (imm5));
        val = S32 (val) >> imm5;
        break;
    case Emulator32bit::ShiftType::SHIFT_ROR:
    {
        DEBUG ("ROR %u", word (imm5));
        word rot_bits = val & ((1 << imm5) - 1);
        rot_bits <<= (kNumWordBits - imm5);
        val >>= imm5;
        val &=
            (1 << (kNumWordBits - imm5)) - 1; /* to be safe and remove bits that will be replaced */
        val |= rot_bits;
        break;
    }
    default: /* Invalid shift */
        ERROR ("Invalid shift: " + val);
    }
    return val;
}

/**
 * @internal
 * @brief                   Get the carry flag after adding two values
 * @details                 yoinked from https://github.com/unicorn-engine/ because I could not
 *                          figure out carry/overflow for subtraction
 *
 * @param[in]                op1: operand 1
 * @param[in]                op2: operand 2
 * @return                     carry flag
 *
 */
static bool get_c_flag_add (const word op1, const word op2)
{
    return op1 + op2 < op1;
}

/**
 * @internal
 * @brief                   Get the overflow flag after adding two values
 * @details                 yoinked from https://github.com/unicorn-engine/ because I could not
 *                          figure out carry/overflow for subtraction
 *
 * @param[in]                op1: operand 1
 * @param[in]                op2: operand 2
 * @return                     overflow flag
 *
 */
static bool get_v_flag_add (const word op1, const word op2)
{
    return (op1 ^ op2 ^ -1) & (op1 ^ (op1 + op2)) & (1U << 31);
}

/**
 * @internal
 * @brief                   Get the carry flag after subtracting two values
 * @details                 yoinked from https://github.com/unicorn-engine/ because I could not
 *                          figure out carry/overflow for subtraction
 *
 * @param[in]                op1: operand 1
 * @param[in]                op2: operand 2
 * @return                     carry flag
 *
 */
static bool get_c_flag_sub (const word op1, const word op2)
{
    return (((~op1 & op2) | ((op1 - op2) & (~op1 | op2))) & (1U << 31));
}

/**
 * @internal
 * @brief                   Get the overflow flag after subtracting two values
 * @details                 yoinked from https://github.com/unicorn-engine/ because I could not
 *                          figure out carry/overflow for subtraction
 *
 * @param[in]                op1: operand 1
 * @param[in]                op2: operand 2
 * @return                     overflow flag
 *
 */
static bool get_v_flag_sub (const word op1, const word op2)
{
    return (((op1 ^ op2) & (op1 ^ (op1 - op2))) & (1U << 31));
}

/**
 * @internal
 * @brief                   Parse the value of the argument for instruction format O
 * @details                 Also used to parse value of argument for some other instruction format
 *                          like format M which conveniently has a similar structure
 * @hideinitializer
 *
 */
#define FORMAT_O__get_arg(instr)                                                                   \
    (test_bit (instr, 14)                                                                          \
         ? bitfield_unsigned (instr, 0, 14)                                                        \
         : calc_shift (read_reg (_X3 (instr)),                                                     \
                       (Emulator32bit::ShiftType) bitfield_unsigned (instr, 7, 2),                 \
                       bitfield_unsigned (instr, 2, 2)))

/**
 * @internal
 * @brief                    A sequence of bits to add to a @ref Joiner
 *
 */
struct JPart
{
    JPart (const int bits, const word val = 0) :
        bits (bits),
        val (val)
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
    Joiner &operator<< (const JPart &p)
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
    Joiner &operator<< (const int bits)
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
    operator word () const
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
word Emulator32bit::asm_format_o (const U8 opcode, const bool s, const int xd, const int xn,
                                  const int imm14)
{
    return Joiner () << JPart (6, opcode) << JPart (1, s) << JPart (5, xd) << JPart (5, xn)
                     << JPart (1, 1) << JPart (14, imm14);
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
word Emulator32bit::asm_format_o (const U8 opcode, const bool s, const int xd, const int xn,
                                  const int xm, const ShiftType shift, const int imm5)
{
    return Joiner () << JPart (6, opcode) << JPart (1, s) << JPart (5, xd) << JPart (5, xn) << 1
                     << JPart (5, xm) << JPart (2, U8 (shift)) << JPart (5, imm5) << 2;
}

word Emulator32bit::asm_format_o1 (const U8 opcode, const int xd, const int xn, const bool imm,
                                   const int xm, const int imm5)
{
    return Joiner () << JPart (6, opcode) << 1 << JPart (5, xd) << JPart (5, xn) << JPart (1, imm)
                     << JPart (5, xm) << 2 << JPart (5, imm5) << 2;
}

word Emulator32bit::asm_format_o2 (const U8 opcode, const bool s, const int xlo, const int xhi,
                                   const int xn, const int xm)
{
    return Joiner () << JPart (6, opcode) << JPart (1, s) << JPart (5, xlo) << JPart (5, xhi) << 1
                     << JPart (5, xn) << JPart (5, xm) << 4;
}

word Emulator32bit::asm_format_o3 (const U8 opcode, const bool s, const int xd, const int imm19)
{
    return Joiner () << JPart (6, opcode) << JPart (1, s) << JPart (5, xd) << JPart (1, 1)
                     << JPart (19, imm19);
}

word Emulator32bit::asm_format_o3 (const U8 opcode, const bool s, const int xd, const int xn,
                                   const int imm14)
{
    return Joiner () << JPart (6, opcode) << JPart (1, s) << JPart (5, xd) << 0 << JPart (5, xn)
                     << JPart (14, imm14);
}

word Emulator32bit::asm_format_m (const U8 opcode, const bool sign, const int xt, const int xn,
                                  const int xm, const ShiftType shift, const int imm5,
                                  const AddrType adr)
{
    return Joiner () << JPart (6, opcode) << JPart (1, sign) << JPart (5, xt) << JPart (5, xn) << 1
                     << JPart (5, xm) << JPart (2, U8 (shift)) << JPart (5, imm5)
                     << JPart (2, U8 (adr));
}

word Emulator32bit::asm_format_m (const U8 opcode, const bool sign, const int xt, const int xn,
                                  const int simm12, const AddrType adr)
{
    return Joiner () << JPart (6, opcode) << JPart (1, sign) << JPart (5, xt) << JPart (5, xn)
                     << JPart (1, 1) << JPart (12, bitfield_unsigned (simm12, 0, 12))
                     << JPart (2, U8 (adr));
}

word Emulator32bit::asm_format_m1 (const U8 opcode, const int xd, const int imm20)
{
    return Joiner () << JPart (6, opcode) << 1 << JPart (5, xd) << JPart (20, imm20);
}

word Emulator32bit::asm_format_b1 (const U8 opcode, const ConditionCode cond, const sword simm22)
{
    return Joiner () << JPart (6, opcode) << JPart (4, word (cond))
                     << JPart (22, bitfield_unsigned (simm22, 0, 22));
}

word Emulator32bit::asm_format_b2 (const U8 opcode, const ConditionCode cond, const int xd)
{
    return Joiner () << JPart (6, opcode) << JPart (4, word (cond)) << JPart (5, xd) << 17;
}

void Emulator32bit::_special_instructions (const word instr)
{
    word opspec = bitfield_unsigned (instr, 22, 4);

    switch (opspec)
    {
    case _opspec_hlt:
        _hlt (instr);
        break;
    case _opspec_nop:
        _nop (instr);
        break;
    case _opspec_msr:
        _msr (instr);
        break;
    case _opspec_mrs:
        _mrs (instr);
        break;
    case _opspec_tlbi:
        _tlbi (instr);
        break;
    case _opspec_atomic:
        _atomic (instr);
        break;
    default:
        throw Exception (Emulator32bit::InterruptType::BAD_INSTR,
                         "Bad OPSPEC specifier " + std::to_string (opspec));
    }
}

void Emulator32bit::_hlt (const word instr)
{
    UNUSED (instr);
    throw Exception (InterruptType::HALT_INSTR, "HLT Exception");
}

word Emulator32bit::asm_hlt ()
{
    return Joiner () << JPart (6, _op_special_instructions) << JPart (4, _opspec_hlt) << 22;
}

void Emulator32bit::_nop (const word instr)
{
    UNUSED (instr);
    return; // do nothing
}

word Emulator32bit::asm_nop ()
{
    return Joiner () << JPart (6, _op_special_instructions) << JPart (4, _opspec_nop) << 22;
}

void Emulator32bit::_msr (const word instr)
{
    const word sysreg = _SX1 (instr);
    const bool imm = test_bit (instr, 16);
    word val = imm ? bitfield_unsigned (instr, 0, 16) : read_reg (_SX2 (instr));

    (void) (val);

    // TODO
    switch (sysreg)
    {
    default:
        throw Exception (Emulator32bit::InterruptType::BAD_REG,
                         "System register " + std::to_string (sysreg) + " unimplemented.");
    }
}

word Emulator32bit::asm_msr (U8 sysreg, bool imm, word xn_or_imm16)
{
    if (imm)
    {
        return Joiner () << JPart (6, _op_special_instructions) << JPart (4, _opspec_msr)
                         << JPart (5, sysreg) << JPart (1, imm) << JPart (16, xn_or_imm16);
    }
    else
    {
        return Joiner () << JPart (6, _op_special_instructions) << JPart (4, _opspec_msr)
                         << JPart (5, sysreg) << JPart (1, imm) << JPart (5, xn_or_imm16) << 11;
    }
}

void Emulator32bit::_mrs (const word instr)
{
    word xn = _SX1 (instr);
    word sysreg = _SX2 (instr);

    // todo
    (void) (xn);
    (void) (sysreg);

    throw Exception (Emulator32bit::InterruptType::BAD_INSTR, "MRS unimplemented.");
}

word Emulator32bit::asm_mrs (U8 xn, U8 sysreg)
{
    return Joiner () << JPart (6, _op_special_instructions) << JPart (4, _opspec_mrs)
                     << JPart (5, xn) << 1 << JPart (5, sysreg) << 11;
}

void Emulator32bit::_tlbi (const word instr)
{
    word xt = _SX1 (instr);
    bool isxt = test_bit (instr, 16);
    word imm16 = bitfield_unsigned (instr, 0, 16);

    // todo
    (void) (xt);
    (void) (isxt);
    (void) (imm16);

    throw Exception (Emulator32bit::InterruptType::BAD_INSTR, "TLBI unimplemented.");
}

word Emulator32bit::asm_tlbi (U8 xt, bool isxt, word imm16)
{
    return Joiner () << JPart (6, _op_special_instructions) << JPart (4, _opspec_mrs)
                     << JPart (5, xt) << JPart (1, isxt) << JPart (16, imm16);
}

void Emulator32bit::_atomic (const word instr)
{
    U8 atop = bitfield_unsigned (instr, 0, 4);

    switch (atop)
    {
    case ATOMIC_SWP:
        _swp (instr);
        break;
    case ATOMIC_LDADD:
        _ldadd (instr);
        break;
    case ATOMIC_LDCLR:
        _ldclr (instr);
        break;
    case ATOMIC_LDSET:
        _ldset (instr);
        break;
    default:
        throw Exception (Emulator32bit::InterruptType::BAD_INSTR,
                         "Atomic op " + std::to_string (atop) + " unimplemented.");
    }
}

void Emulator32bit::_swp (const word instr)
{
    const U8 xt = _SX1 (instr);
    const U8 xn = _SX2 (instr);
    const U8 xm = _SX3 (instr);
    const word mem_adr = read_reg (xm);
    const U8 width = bitfield_unsigned (instr, 0, 4);

    if (width == ATOMIC_WIDTH_WORD)
    {
        DEBUG_SS (std::stringstream ()
                  << "swp x" << std::to_string (xt) << ", x" << std::to_string (xn) << ", [x"
                  << std::to_string (xm) << "]");
    }
    else if (width == ATOMIC_WIDTH_BYTE)
    {
        DEBUG_SS (std::stringstream ()
                  << "swpb x" << std::to_string (xt) << ", x" << std::to_string (xn) << ", [x"
                  << std::to_string (xm) << "]");
    }
    else if (width == ATOMIC_WIDTH_HWORD)
    {
        DEBUG_SS (std::stringstream ()
                  << "swph x" << std::to_string (xt) << ", x" << std::to_string (xn) << ", [x"
                  << std::to_string (xm) << "]");
    }

    if (width == ATOMIC_WIDTH_WORD)
    {
        const word val_reg = read_reg (xn);
        const word val_mem = system_bus->read_word (mem_adr);
        write_reg (xt, val_mem);
        system_bus->write_word (mem_adr, val_reg);
    }
    else if (width == ATOMIC_WIDTH_BYTE)
    {
        const word val_reg = read_reg (xn) & 0xFF;
        const word val_mem = system_bus->read_byte (mem_adr);
        write_reg (xt, (val_reg & ~(0xFF)) + val_mem);
        system_bus->write_byte (mem_adr, val_reg);
    }
    else if (width == ATOMIC_WIDTH_HWORD)
    {
        const word val_reg = read_reg (xn) & 0xFFFF;
        const word val_mem = system_bus->read_hword (mem_adr);
        write_reg (xt, (val_reg & ~(0xFFFF)) + val_mem);
        system_bus->write_hword (mem_adr, val_reg);
    }
    else
    {
        ERROR ("Invalid ATOMIC_WIDTH");
    }
}

void Emulator32bit::_ldadd (const word instr)
{
    const U8 xt = _SX1 (instr);
    const U8 xn = _SX2 (instr);
    const U8 xm = _SX3 (instr);
    const word mem_adr = read_reg (xm);
    const U8 width = bitfield_unsigned (instr, 0, 4);

    if (width == ATOMIC_WIDTH_WORD)
    {
        DEBUG_SS (std::stringstream ()
                  << "ldadd x" << std::to_string (xt) << ", x" << std::to_string (xn) << ", [x"
                  << std::to_string (xm) << "]");
    }
    else if (width == ATOMIC_WIDTH_BYTE)
    {
        DEBUG_SS (std::stringstream ()
                  << "ldaddb x" << std::to_string (xt) << ", x" << std::to_string (xn) << ", [x"
                  << std::to_string (xm) << "]");
    }
    else if (width == ATOMIC_WIDTH_HWORD)
    {
        DEBUG_SS (std::stringstream ()
                  << "ldaddh x" << std::to_string (xt) << ", x" << std::to_string (xn) << ", [x"
                  << std::to_string (xm) << "]");
    }

    if (width == ATOMIC_WIDTH_WORD)
    {
        const word val_reg = read_reg (xn);
        const word val_mem = system_bus->read_word (mem_adr);
        write_reg (xt, val_mem);
        system_bus->write_word (mem_adr, val_mem + val_reg);
    }
    else if (width == ATOMIC_WIDTH_BYTE)
    {
        const word val_reg = read_reg (xn) & 0xFF;
        const word val_mem = system_bus->read_byte (mem_adr);
        write_reg (xt, (val_reg & ~(0xFF)) + val_mem);
        system_bus->write_byte (mem_adr, val_mem + val_reg);
    }
    else if (width == ATOMIC_WIDTH_HWORD)
    {
        const word val_reg = read_reg (xn) & 0xFFFF;
        const word val_mem = system_bus->read_hword (mem_adr);
        write_reg (xt, (val_reg & ~(0xFFFF)) + val_mem);
        system_bus->write_hword (mem_adr, val_mem + val_reg);
    }
    else
    {
        ERROR ("Invalid ATOMIC_WIDTH");
    }
}

void Emulator32bit::_ldclr (const word instr)
{
    const U8 xt = _SX1 (instr);
    const U8 xn = _SX2 (instr);
    const U8 xm = _SX3 (instr);
    const word mem_adr = read_reg (xm);
    const U8 width = bitfield_unsigned (instr, 0, 4);

    if (width == ATOMIC_WIDTH_WORD)
    {
        DEBUG_SS (std::stringstream ()
                  << "ldclr x" << std::to_string (xt) << ", x" << std::to_string (xn) << ", [x"
                  << std::to_string (xm) << "]");
    }
    else if (width == ATOMIC_WIDTH_BYTE)
    {
        DEBUG_SS (std::stringstream ()
                  << "ldclrb x" << std::to_string (xt) << ", x" << std::to_string (xn) << ", [x"
                  << std::to_string (xm) << "]");
    }
    else if (width == ATOMIC_WIDTH_HWORD)
    {
        DEBUG_SS (std::stringstream ()
                  << "ldclrh x" << std::to_string (xt) << ", x" << std::to_string (xn) << ", [x"
                  << std::to_string (xm) << "]");
    }

    if (width == ATOMIC_WIDTH_WORD)
    {
        const word val_reg = read_reg (xn);
        const word val_mem = system_bus->read_word (mem_adr);
        write_reg (xt, val_mem);
        system_bus->write_word (mem_adr, val_mem & (~val_reg));
    }
    else if (width == ATOMIC_WIDTH_BYTE)
    {
        const word val_reg = read_reg (xn) & 0xFF;
        const word val_mem = system_bus->read_byte (mem_adr);
        write_reg (xt, (val_reg & ~(0xFF)) + val_mem);
        system_bus->write_byte (mem_adr, val_mem & (~val_reg));
    }
    else if (width == ATOMIC_WIDTH_HWORD)
    {
        const word val_reg = read_reg (xn) & 0xFFFF;
        const word val_mem = system_bus->read_hword (mem_adr);
        write_reg (xt, (val_reg & ~(0xFFFF)) + val_mem);
        system_bus->write_hword (mem_adr, val_mem & (~val_reg));
    }
    else
    {
        ERROR ("Invalid ATOMIC_WIDTH");
    }
}

void Emulator32bit::_ldset (const word instr)
{
    const U8 xt = _SX1 (instr);
    const U8 xn = _SX2 (instr);
    const U8 xm = _SX3 (instr);
    const word mem_adr = read_reg (xm);
    const U8 width = bitfield_unsigned (instr, 0, 4);

    if (width == ATOMIC_WIDTH_WORD)
    {
        DEBUG_SS (std::stringstream ()
                  << "ldset x" << std::to_string (xt) << ", x" << std::to_string (xn) << ", [x"
                  << std::to_string (xm) << "]");
    }
    else if (width == ATOMIC_WIDTH_BYTE)
    {
        DEBUG_SS (std::stringstream ()
                  << "lsetb x" << std::to_string (xt) << ", x" << std::to_string (xn) << ", [x"
                  << std::to_string (xm) << "]");
    }
    else if (width == ATOMIC_WIDTH_HWORD)
    {
        DEBUG_SS (std::stringstream ()
                  << "ldseth x" << std::to_string (xt) << ", x" << std::to_string (xn) << ", [x"
                  << std::to_string (xm) << "]");
    }

    if (width == ATOMIC_WIDTH_WORD)
    {
        const word val_reg = read_reg (xn);
        const word val_mem = system_bus->read_word (mem_adr);
        write_reg (xt, val_mem);
        system_bus->write_word (mem_adr, val_mem | val_reg);
    }
    else if (width == ATOMIC_WIDTH_BYTE)
    {
        const word val_reg = read_reg (xn) & 0xFF;
        const word val_mem = system_bus->read_byte (mem_adr);
        write_reg (xt, (val_reg & ~(0xFF)) + val_mem);
        system_bus->write_byte (mem_adr, val_mem | val_reg);
    }
    else if (width == ATOMIC_WIDTH_HWORD)
    {
        const word val_reg = read_reg (xn) & 0xFFFF;
        const word val_mem = system_bus->read_hword (mem_adr);
        write_reg (xt, (val_reg & ~(0xFFFF)) + val_mem);
        system_bus->write_hword (mem_adr, val_mem | val_reg);
    }
    else
    {
        ERROR ("Invalid ATOMIC_WIDTH");
    }
}

word Emulator32bit::asm_atomic (word xt, word xn, word xm, U8 width, U8 atop)
{
    return Joiner () << JPart (6, _op_special_instructions) << JPart (4, _opspec_atomic)
                     << JPart (5, xt) << 1 << JPart (5, xn) << JPart (5, xm) << JPart (2, width)
                     << JPart (4, atop);
}

void Emulator32bit::_add (const word instr)
{
    const U8 xd = _X1 (instr);
    const word xn_val = read_reg (_X2 (instr));
    const word add_val = FORMAT_O__get_arg (instr);
    const word dst_val = add_val + xn_val;

    // check to update NZCV
    if (test_bit (instr, kInstructionUpdateFlagBit))
    {
        set_NZCV (test_bit (dst_val, 31), dst_val == 0, get_c_flag_add (xn_val, add_val),
                  get_v_flag_add (xn_val, add_val));
    }

    DEBUG_SS (std::stringstream () << "add " << std::to_string (add_val) << " "
                                   << std::to_string (xn_val) << " = " << std::to_string (dst_val));
    write_reg (xd, dst_val);
}

void Emulator32bit::_sub (const word instr)
{
    const U8 xd = _X1 (instr);
    const word xn_val = read_reg (_X2 (instr));
    const word sub_val = FORMAT_O__get_arg (instr);
    const word dst_val = xn_val - sub_val;

    // check to update NZCV
    if (test_bit (instr, kInstructionUpdateFlagBit))
    {
        set_NZCV (test_bit (dst_val, 31), dst_val == 0, get_c_flag_sub (xn_val, sub_val),
                  get_v_flag_sub (xn_val, sub_val));
    }

    DEBUG_SS (std::stringstream () << "sub " << std::to_string (sub_val) << " "
                                   << std::to_string (xn_val) << " = " << std::to_string (dst_val));
    write_reg (xd, dst_val);
}

void Emulator32bit::_rsb (const word instr)
{
    const U8 xd = _X1 (instr);
    const word sub_val = read_reg (_X2 (instr));
    const word xn_val = FORMAT_O__get_arg (instr);
    const word dst_val = xn_val - sub_val;

    // check to update NZCV
    if (test_bit (instr, kInstructionUpdateFlagBit))
    {
        set_NZCV (test_bit (dst_val, 31), dst_val == 0, get_c_flag_sub (xn_val, sub_val),
                  get_v_flag_sub (xn_val, sub_val));
    }

    DEBUG_SS (std::stringstream ()
              << "rsb " << std::to_string (xn_val) << " " << std::to_string (sub_val) << " = "
              << std::to_string (dst_val));
    write_reg (xd, dst_val);
}

void Emulator32bit::_adc (const word instr)
{
    const bool c = test_bit (m_pstate, kCFlagBit);
    const U8 xd = _X1 (instr);
    const word xn_val = read_reg (_X2 (instr));
    const word add_val = FORMAT_O__get_arg (instr);
    const word dst_val = add_val + xn_val + c;

    // check to update NZCV
    if (test_bit (instr, kInstructionUpdateFlagBit))
    {
        set_NZCV (test_bit (dst_val, 31), dst_val == 0,
                  get_c_flag_add (xn_val + c, add_val) | get_c_flag_add (xn_val, c),
                  get_v_flag_add (xn_val + c, add_val) | get_v_flag_add (xn_val, c));
    }

    DEBUG_SS (std::stringstream () << "adc " << std::to_string (add_val) << " "
                                   << std::to_string (xn_val) << " = " << std::to_string (dst_val));
    write_reg (xd, dst_val);
}

void Emulator32bit::_sbc (const word instr)
{
    const bool borrow = test_bit (m_pstate, kCFlagBit);
    const U8 xd = _X1 (instr);
    const word xn_val = read_reg (_X2 (instr));
    const word sub_val = FORMAT_O__get_arg (instr);
    const word dst_val = xn_val - sub_val - borrow;

    // check to update NZCV
    if (test_bit (instr, kInstructionUpdateFlagBit))
    {
        set_NZCV (test_bit (dst_val, 31), dst_val == 0,
                  get_c_flag_sub (xn_val - borrow, sub_val) | get_c_flag_sub (xn_val, borrow),
                  get_v_flag_sub (xn_val - borrow, sub_val) | get_v_flag_sub (xn_val, borrow));
    }

    DEBUG_SS (std::stringstream () << "sbc " << std::to_string (sub_val) << " "
                                   << std::to_string (xn_val) << " = " << std::to_string (dst_val));
    write_reg (xd, dst_val);
}

void Emulator32bit::_rsc (const word instr)
{
    const bool borrow = test_bit (m_pstate, kCFlagBit);
    const U8 xd = _X1 (instr);
    const word sub_val = read_reg (_X2 (instr));
    const word xn_val = FORMAT_O__get_arg (instr);
    const word dst_val = xn_val - sub_val - borrow;

    // check to update NZCV
    if (test_bit (instr, kInstructionUpdateFlagBit))
    {
        set_NZCV (test_bit (dst_val, 31), dst_val == 0,
                  get_c_flag_sub (xn_val - borrow, sub_val) | get_c_flag_sub (xn_val, borrow),
                  get_v_flag_sub (xn_val - borrow, sub_val) | get_v_flag_sub (xn_val, borrow));
    }

    DEBUG_SS (std::stringstream ()
              << "rsc " << std::to_string (xn_val) << " " << std::to_string (sub_val) << " = "
              << std::to_string (dst_val));
    write_reg (xd, dst_val);
}

void Emulator32bit::_mul (const word instr)
{
    const U8 xd = _X1 (instr);
    dword xn_val = read_reg (_X2 (instr));
    dword xm_val = FORMAT_O__get_arg (instr);
    dword dst_val = xn_val * xm_val;

    // check to update NZCV
    if (test_bit (instr, kInstructionUpdateFlagBit))
    {
        // according to https://developer.arm.com/documentation/dui0473/m/arm-and-thumb-instructions/smull
        // arm's MUL instruction does not set carry or overflow flags
        set_NZCV (test_bit (dst_val, 31), dst_val == 0, test_bit (m_pstate, kCFlagBit),
                  test_bit (m_pstate, kVFlagBit));
    }

    DEBUG_SS (std::stringstream () << "mul " << std::to_string (xn_val) << " "
                                   << std::to_string (xm_val) << " = " << std::to_string (dst_val));

    write_reg (xd, word (dst_val));
}

void Emulator32bit::_umull (const word instr)
{
    const U8 xlo = _X1 (instr);
    const U8 xhi = _X2 (instr);
    dword xn_val = read_reg (_X3 (instr));
    dword xm_val = read_reg (_X4 (instr));
    dword dst_val = xn_val * xm_val;

    // check to update NZCV
    if (test_bit (instr, kInstructionUpdateFlagBit))
    {
        // according to https://developer.arm.com/documentation/dui0473/m/arm-and-thumb-instructions/umull
        // arm's UMULL instruction does not set carry or overflow flags
        set_NZCV (test_bit (dst_val, 63), dst_val == 0, test_bit (m_pstate, kCFlagBit),
                  test_bit (m_pstate, kVFlagBit));
    }

    DEBUG_SS (std::stringstream () << "mul " << std::to_string (xn_val) << " "
                                   << std::to_string (xm_val) << " = " << std::to_string (dst_val));

    write_reg (xlo, word (dst_val));
    write_reg (xhi, word (dst_val >> 32));
}

void Emulator32bit::_smull (const word instr)
{
    const U8 xlo = _X1 (instr);
    const U8 xhi = _X2 (instr);
    const signed long long xn_val = S64 (read_reg (_X3 (instr))) << 32 >> 32;
    const signed long long xm_val = S64 (read_reg (_X4 (instr))) << 32 >> 32;
    const signed long long dst_val = xn_val * xm_val;

    // check to update NZCV
    if (test_bit (instr, kInstructionUpdateFlagBit))
    {
        // according to https://developer.arm.com/documentation/dui0489/c/arm-and-thumb-instructions/multiply-instructions/mul--mla--and-mls
        // arm's UMULL instruction does not set carry or overflow flags
        set_NZCV (test_bit (dst_val, 63), dst_val == 0, test_bit (m_pstate, kCFlagBit),
                  test_bit (m_pstate, kVFlagBit));
    }

    DEBUG_SS (std::stringstream () << "mul " << std::to_string (xn_val) << " "
                                   << std::to_string (xm_val) << " = " << std::to_string (dst_val));

    write_reg (xlo, word (dst_val));
    write_reg (xhi, word (dst_val >> 32));
}

// todo WILL DO LATER JUST NOT NOW
void Emulator32bit::_vabs (const word instr)
{
    UNUSED (instr);
}

void Emulator32bit::_vneg (const word instr)
{
    UNUSED (instr);
}

void Emulator32bit::_vsqrt (const word instr)
{
    UNUSED (instr);
}

void Emulator32bit::_vadd (const word instr)
{
    UNUSED (instr);
}

void Emulator32bit::_vsub (const word instr)
{
    UNUSED (instr);
}

void Emulator32bit::_vdiv (const word instr)
{
    UNUSED (instr);
}

void Emulator32bit::_vmul (const word instr)
{
    UNUSED (instr);
}

void Emulator32bit::_vcmp (const word instr)
{
    UNUSED (instr);
}

void Emulator32bit::_vsel (const word instr)
{
    UNUSED (instr);
}

void Emulator32bit::_vcint (const word instr)
{
    UNUSED (instr);
}

void Emulator32bit::_vcflo (const word instr)
{
    UNUSED (instr);
}

void Emulator32bit::_vmov (const word instr)
{
    UNUSED (instr);
}

void Emulator32bit::_and (const word instr)
{
    const U8 xd = _X1 (instr);
    const word xn_val = read_reg (_X2 (instr));
    const word and_val = FORMAT_O__get_arg (instr);
    const word dst_val = and_val & xn_val;

    // check to update NZCV
    if (test_bit (instr, kInstructionUpdateFlagBit))
    {
        // https://developer.arm.com/documentation/dui0489/h/arm-and-thumb-instructions/and--orr--eor--bic--and-orn
        // N and Z flags are set based of the result, C flag may be set based of the calculation for the second operand
        // but will ignore for now
        set_NZCV (test_bit (dst_val, 31), dst_val == 0, test_bit (m_pstate, kCFlagBit),
                  test_bit (m_pstate, kVFlagBit));
    }

    DEBUG_SS (std::stringstream () << "and " << std::to_string (and_val) << " "
                                   << std::to_string (xn_val) << " = " << std::to_string (dst_val));
    write_reg (xd, dst_val);
}

void Emulator32bit::_orr (const word instr)
{
    const U8 xd = _X1 (instr);
    const word xn_val = read_reg (_X2 (instr));
    const word or_val = FORMAT_O__get_arg (instr);
    const word dst_val = or_val | xn_val;

    // check to update NZCV
    if (test_bit (instr, kInstructionUpdateFlagBit))
    {
        // https://developer.arm.com/documentation/dui0489/h/arm-and-thumb-instructions/and--orr--eor--bic--and-orn
        // N and Z flags are set based of the result, C flag may be set based of the calculation for the second operand
        // but will ignore for now
        set_NZCV (test_bit (dst_val, 31), dst_val == 0, test_bit (m_pstate, kCFlagBit),
                  test_bit (m_pstate, kVFlagBit));
    }

    DEBUG_SS (std::stringstream () << "orr " << std::to_string (or_val) << " "
                                   << std::to_string (xn_val) << " = " << std::to_string (dst_val));
    write_reg (xd, dst_val);
}

void Emulator32bit::_eor (const word instr)
{
    const U8 xd = _X1 (instr);
    const word xn_val = read_reg (_X2 (instr));
    const word eor_val = FORMAT_O__get_arg (instr);
    const word dst_val = eor_val ^ xn_val;

    // check to update NZCV
    if (test_bit (instr, kInstructionUpdateFlagBit))
    {
        // https://developer.arm.com/documentation/dui0489/h/arm-and-thumb-instructions/and--orr--eor--bic--and-orn
        // N and Z flags are set based of the result, C flag may be set based of the calculation for the second operand
        // but will ignore for now
        set_NZCV (test_bit (dst_val, 31), dst_val == 0, test_bit (m_pstate, kCFlagBit),
                  test_bit (m_pstate, kVFlagBit));
    }

    DEBUG_SS (std::stringstream () << "eor " << std::to_string (eor_val) << " "
                                   << std::to_string (xn_val) << " = " << std::to_string (dst_val));
    write_reg (xd, dst_val);
}

void Emulator32bit::_bic (const word instr)
{
    const U8 xd = _X1 (instr);
    const word xn_val = read_reg (_X2 (instr));
    const word bic_val = FORMAT_O__get_arg (instr);
    const word dst_val = (~bic_val) & xn_val;

    // check to update NZCV
    if (test_bit (instr, kInstructionUpdateFlagBit))
    {
        // https://developer.arm.com/documentation/dui0489/h/arm-and-thumb-instructions/and--orr--eor--bic--and-orn
        // N and Z flags are set based of the result, C flag may be set based of the calculation for the second operand
        // but will ignore for now
        set_NZCV (test_bit (dst_val, 31), dst_val == 0, test_bit (m_pstate, kCFlagBit),
                  test_bit (m_pstate, kVFlagBit));
    }

    DEBUG_SS (std::stringstream () << "bic " << std::to_string (bic_val) << " "
                                   << std::to_string (xn_val) << " = " << std::to_string (dst_val));
    write_reg (xd, dst_val);
}

void Emulator32bit::_lsl (const word instr)
{
    const U8 xd = _X1 (instr);
    const word xn_val = read_reg (_X2 (instr));
    const word lsl_val =
        test_bit (instr, 14) ? bitfield_unsigned (instr, 2, 5) : 0xFF & read_reg (_X3 (instr));
    const word dst_val = xn_val << lsl_val;

    DEBUG_SS (std::stringstream () << "lsl " << std::to_string (lsl_val) << " "
                                   << std::to_string (xn_val) << " = " << std::to_string (dst_val));
    write_reg (xd, dst_val);
}

void Emulator32bit::_lsr (const word instr)
{
    const U8 xd = _X1 (instr);
    const word xn_val = read_reg (_X2 (instr));
    const word lsl_val =
        test_bit (instr, 14) ? bitfield_unsigned (instr, 2, 5) : 0xFF & read_reg (_X3 (instr));
    const word dst_val = xn_val >> lsl_val;

    DEBUG_SS (std::stringstream () << "lsr " << std::to_string (lsl_val) << " "
                                   << std::to_string (xn_val) << " = " << std::to_string (dst_val));
    write_reg (xd, dst_val);
}

void Emulator32bit::_asr (const word instr)
{
    const U8 xd = _X1 (instr);
    const word xn_val = read_reg (_X2 (instr));
    const word lsl_val =
        test_bit (instr, 14) ? bitfield_unsigned (instr, 2, 5) : 0xFF & read_reg (_X3 (instr));
    const word dst_val = sword (xn_val) >> lsl_val;

    DEBUG_SS (std::stringstream () << "asr " << std::to_string (lsl_val) << " "
                                   << std::to_string (xn_val) << " = " << std::to_string (dst_val));
    write_reg (xd, dst_val);
}

void Emulator32bit::_ror (const word instr)
{
    const U8 xd = _X1 (instr);
    const word xn_val = read_reg (_X2 (instr));
    const word lsl_val =
        test_bit (instr, 14) ? bitfield_unsigned (instr, 2, 5) : 0xFF & read_reg (_X3 (instr));
    const word dst_val =
        (xn_val >> lsl_val) | (bitfield_unsigned (xn_val, 0, lsl_val) << (32 - lsl_val));

    DEBUG_SS (std::stringstream () << "ror " << std::to_string (lsl_val) << " "
                                   << std::to_string (xn_val) << " = " << std::to_string (dst_val));
    write_reg (xd, dst_val);
}

// alias to subs
void Emulator32bit::_cmp (const word instr)
{
    const word xn_val = read_reg (_X2 (instr));
    const word cmp_val = FORMAT_O__get_arg (instr);
    const word dst_val = xn_val - cmp_val;

    set_NZCV (test_bit (dst_val, 31), dst_val == 0, get_c_flag_sub (xn_val, cmp_val),
              get_v_flag_sub (xn_val, cmp_val));

    DEBUG_SS (std::stringstream () << "cmp " << std::to_string (cmp_val) << " "
                                   << std::to_string (xn_val) << " = " << std::to_string (dst_val));
}

// alias to adds
void Emulator32bit::_cmn (const word instr)
{
    const word xn_val = read_reg (_X2 (instr));
    const word cmn_val = FORMAT_O__get_arg (instr);
    const word dst_val = cmn_val + xn_val;

    set_NZCV (test_bit (dst_val, 31), dst_val == 0, get_c_flag_add (xn_val, cmn_val),
              get_v_flag_add (xn_val, cmn_val));

    DEBUG_SS (std::stringstream () << "cmn " << std::to_string (cmn_val) << " "
                                   << std::to_string (xn_val) << " = " << std::to_string (dst_val));
}

// alias to ands
void Emulator32bit::_tst (const word instr)
{
    const word xn_val = read_reg (_X2 (instr));
    const word tst_val = FORMAT_O__get_arg (instr);
    const word dst_val = tst_val & xn_val;

    set_NZCV (test_bit (dst_val, 31), dst_val == 0, test_bit (m_pstate, kCFlagBit),
              test_bit (m_pstate, kVFlagBit));

    DEBUG_SS (std::stringstream () << "tst " << std::to_string (tst_val) << " "
                                   << std::to_string (xn_val) << " = " << std::to_string (dst_val));
}

// alias to eors
void Emulator32bit::_teq (const word instr)
{
    const word xn_val = read_reg (_X2 (instr));
    const word teq_val = FORMAT_O__get_arg (instr);
    const word dst_val = teq_val ^ xn_val;

    set_NZCV (test_bit (dst_val, 31), dst_val == 0, test_bit (m_pstate, kCFlagBit),
              test_bit (m_pstate, kVFlagBit));

    DEBUG_SS (std::stringstream () << "teq " << std::to_string (teq_val) << " "
                                   << std::to_string (xn_val) << " = " << std::to_string (dst_val));
}

void Emulator32bit::_mov (const word instr)
{
    const U8 xd = _X1 (instr);
    word mov_val = 0;
    if (test_bit (instr, 19))
    {
        mov_val = bitfield_unsigned (instr, 0, 19);
    }
    else
    {
        mov_val = bitfield_unsigned (instr, 0, 14) + read_reg (bitfield_unsigned (instr, 14, 5));
    }

    // check to update NZCV
    if (test_bit (instr, kInstructionUpdateFlagBit))
    {
        set_NZCV (test_bit (mov_val, 31), mov_val == 0, test_bit (m_pstate, kCFlagBit),
                  test_bit (m_pstate, kVFlagBit));
    }

    DEBUG_SS (std::stringstream ()
              << "mov " << std::to_string (xd) << " " << std::to_string (mov_val));
    write_reg (xd, mov_val);
}

void Emulator32bit::_mvn (const word instr)
{
    const U8 xd = _X1 (instr);
    word mvn_val = 0;
    if (test_bit (instr, 19))
    {
        mvn_val = bitfield_unsigned (instr, 0, 19);
    }
    else
    {
        mvn_val = bitfield_unsigned (instr, 0, 14) + read_reg (bitfield_unsigned (instr, 14, 5));
    }

    const word dst_val = ~mvn_val;

    // check to update NZCV
    if (test_bit (instr, kInstructionUpdateFlagBit))
    {
        set_NZCV (test_bit (dst_val, 31), dst_val == 0, test_bit (m_pstate, kCFlagBit),
                  test_bit (m_pstate, kVFlagBit));
    }

    DEBUG_SS (std::stringstream ()
              << "mvn " << std::to_string (xd) << " " << std::to_string (mvn_val) << " = "
              << std::to_string (dst_val));
    write_reg (xd, dst_val);
}

word Emulator32bit::calc_mem_addr (word xn, sword offset, U8 addr_mode)
{
    word mem_addr = 0;
    const word xn_val = read_reg (xn);
    if (addr_mode == 0)
    {
        mem_addr = xn_val + offset;
    }
    else if (addr_mode == 1)
    {
        mem_addr = xn_val + offset;
        write_reg (xn, mem_addr);
    }
    else if (addr_mode == 2)
    {
        mem_addr = xn_val;
        write_reg (xn, xn_val + offset);
    }
    else
    {
        throw Exception (InterruptType::BAD_INSTR,
                         "Bad memory address mode " + std::to_string (addr_mode));
    }
    return mem_addr;
}

void Emulator32bit::_ldr (const word instr)
{
    const U8 xt = _X1 (instr);
    const U8 xn = _X2 (instr);
    const bool simm = test_bit (instr, 14);
    sword offset = simm ? bitfield_signed (instr, 2, 12) : FORMAT_O__get_arg (instr);
    printf ("OFFSET %d\n", offset);

    const U8 address_mode = bitfield_unsigned (instr, 0, 2);
    const word mem_addr = calc_mem_addr (xn, offset, address_mode);
    printf ("MEMORY ADDRESS %u\n", mem_addr);
    const word read_val = system_bus->read_word (mem_addr);

    if (address_mode == 0)
    {
        DEBUG_SS (std::stringstream ()
                  << "ldr x" << std::to_string (xt) << ", [x" << std::to_string (xn) << ", #"
                  << std::to_string (offset) << "] (" << std::to_string (mem_addr)
                  << ") = " << std::to_string (read_val));
    }
    else if (address_mode == 1)
    {
        DEBUG_SS (std::stringstream ()
                  << "ldr x" << std::to_string (xt) << ", [x" << std::to_string (xn) << ", #"
                  << std::to_string (offset) << "]! (" << std::to_string (mem_addr)
                  << ") = " << std::to_string (read_val));
    }
    else
    {
        DEBUG_SS (std::stringstream ()
                  << "ldr x" << std::to_string (xt) << ", [x" << std::to_string (xn) << "], #"
                  << std::to_string (offset) << " (" << std::to_string (mem_addr)
                  << ") = " << std::to_string (read_val));
    }
    write_reg (xt, read_val);
}

void Emulator32bit::_ldrb (const word instr)
{
    const bool sign = test_bit (instr, 25);
    const U8 xt = _X1 (instr);
    const U8 xn = _X2 (instr);
    const bool simm = test_bit (instr, 14);
    sword offset = simm ? bitfield_signed (instr, 2, 12) : FORMAT_O__get_arg (instr);

    const U8 address_mode = bitfield_unsigned (instr, 0, 2);
    const word mem_addr = calc_mem_addr (xn, offset, address_mode);
    word read_val = system_bus->read_byte (mem_addr);
    if (sign)
    {
        read_val = sword (byte (read_val));
    }

    if (address_mode == 0)
    {
        DEBUG_SS (std::stringstream ()
                  << "ldr" << (sign ? "sb " : "b ") << std::to_string (xt) << ", ["
                  << std::to_string (xn) << ", " << offset << "] [" << std::to_string (mem_addr)
                  << "] = " << std::to_string (read_val));
    }
    else if (address_mode == 1)
    {
        DEBUG_SS (std::stringstream ()
                  << "ldr" << (sign ? "sb " : "b ") << ", [" << std::to_string (xn) << ", "
                  << offset << "]! [" << std::to_string (mem_addr)
                  << "] = " << std::to_string (read_val));
    }
    else
    {
        DEBUG_SS (std::stringstream ()
                  << "ldr" << (sign ? "sb " : "b ") << ", [" << std::to_string (xn) << "], "
                  << offset << " [" << std::to_string (mem_addr)
                  << "] = " << std::to_string (read_val));
    }
    write_reg (xt, read_val);
}

void Emulator32bit::_ldrh (const word instr)
{
    const bool sign = test_bit (instr, 25);
    const U8 xt = _X1 (instr);
    const U8 xn = _X2 (instr);
    const bool simm = test_bit (instr, 14);
    sword offset = simm ? bitfield_signed (instr, 2, 12) : FORMAT_O__get_arg (instr);

    const U8 address_mode = bitfield_unsigned (instr, 0, 2);
    const word mem_addr = calc_mem_addr (xn, offset, address_mode);
    word read_val = system_bus->read_hword (mem_addr);
    if (sign)
    {
        read_val = sword (hword (read_val));
    }

    if (address_mode == 0)
    {
        DEBUG_SS (std::stringstream ()
                  << "ldr" << (sign ? "sh " : "h ") << std::to_string (xt) << ", ["
                  << std::to_string (xn) << ", " << offset << "] [" << std::to_string (mem_addr)
                  << "] = " << std::to_string (read_val));
    }
    else if (address_mode == 1)
    {
        DEBUG_SS (std::stringstream ()
                  << "ldr" << (sign ? "sh " : "h ") << ", [" << std::to_string (xn) << ", "
                  << offset << "]! [" << std::to_string (mem_addr)
                  << "] = " << std::to_string (read_val));
    }
    else
    {
        DEBUG_SS (std::stringstream ()
                  << "ldr" << (sign ? "sh " : "h ") << ", [" << std::to_string (xn) << "], "
                  << offset << " [" << std::to_string (mem_addr)
                  << "] = " << std::to_string (read_val));
    }
    write_reg (xt, read_val);
}

void Emulator32bit::_str (const word instr)
{
    const U8 xt = _X1 (instr);
    const U8 xn = _X2 (instr);
    const bool simm = test_bit (instr, 14);
    sword offset = simm ? bitfield_signed (instr, 2, 12) : FORMAT_O__get_arg (instr);

    const U8 address_mode = bitfield_unsigned (instr, 0, 2);
    const word mem_addr = calc_mem_addr (xn, offset, address_mode);
    const word write_val = read_reg (xt);

    if (address_mode == 0)
    {
        DEBUG_SS (std::stringstream ()
                  << "str x" << std::to_string (xt) << ", [x" << std::to_string (xn) << ", #"
                  << offset << "] (" << std::to_string (mem_addr)
                  << ") = " << std::to_string (write_val));
    }
    else if (address_mode == 1)
    {
        DEBUG_SS (std::stringstream ()
                  << "str x" << std::to_string (xt) << ", [x" << std::to_string (xn) << ", #"
                  << offset << "]! (" << std::to_string (mem_addr)
                  << ") = " << std::to_string (write_val));
    }
    else
    {
        DEBUG_SS (std::stringstream ()
                  << "str x" << std::to_string (xt) << ", [x" << std::to_string (xn) << "], #"
                  << offset << " (" << std::to_string (mem_addr)
                  << ") = " << std::to_string (write_val));
    }
    system_bus->write_word (mem_addr, write_val);
}

void Emulator32bit::_strb (const word instr)
{
    const bool sign = test_bit (instr, 25);
    const U8 xt = _X1 (instr);
    const U8 xn = _X2 (instr);
    const bool simm = test_bit (instr, 14);
    sword offset = simm ? bitfield_signed (instr, 2, 12) : FORMAT_O__get_arg (instr);

    const U8 address_mode = bitfield_unsigned (instr, 0, 2);
    const word mem_addr = calc_mem_addr (xn, offset, address_mode);
    word write_val = read_reg (xt);
    if (sign)
    {
        write_val = sword (byte (write_val));
    }

    if (address_mode == 0)
    {
        DEBUG_SS (std::stringstream ()
                  << "str" << (sign ? "sb " : "b ") << std::to_string (xt) << ", ["
                  << std::to_string (xn) << ", " << offset << "] [" << std::to_string (mem_addr)
                  << "] = " << std::to_string (write_val));
    }
    else if (address_mode == 1)
    {
        DEBUG_SS (std::stringstream ()
                  << "str" << (sign ? "sb " : "b ") << ", [" << std::to_string (xn) << ", "
                  << offset << "]! [" << std::to_string (mem_addr)
                  << "] = " << std::to_string (write_val));
    }
    else
    {
        DEBUG_SS (std::stringstream ()
                  << "str" << (sign ? "sb " : "b ") << ", [" << std::to_string (xn) << "], "
                  << offset << " [" << std::to_string (mem_addr)
                  << "] = " << std::to_string (write_val));
    }
    system_bus->write_byte (mem_addr, write_val);
}

void Emulator32bit::_strh (const word instr)
{
    const bool sign = test_bit (instr, 25);
    const U8 xt = _X1 (instr);
    const U8 xn = _X2 (instr);
    const bool simm = test_bit (instr, 14);
    sword offset = simm ? bitfield_signed (instr, 2, 12) : FORMAT_O__get_arg (instr);

    const U8 address_mode = bitfield_unsigned (instr, 0, 2);
    const word mem_addr = calc_mem_addr (xn, offset, address_mode);
    word write_val = read_reg (xt);
    if (sign)
    {
        write_val = sword (hword (write_val));
    }

    if (address_mode == 0)
    {
        DEBUG_SS (std::stringstream ()
                  << "str" << (sign ? "sh " : "h ") << std::to_string (xt) << ", ["
                  << std::to_string (xn) << ", " << offset << "] [" << std::to_string (mem_addr)
                  << "] = " << std::to_string (write_val));
    }
    else if (address_mode == 1)
    {
        DEBUG_SS (std::stringstream ()
                  << "str" << (sign ? "sh " : "h ") << ", [" << std::to_string (xn) << ", "
                  << offset << "]! [" << std::to_string (mem_addr)
                  << "] = " << std::to_string (write_val));
    }
    else
    {
        DEBUG_SS (std::stringstream ()
                  << "str" << (sign ? "sh " : "h ") << ", [" << std::to_string (xn) << "], "
                  << offset << " [" << std::to_string (mem_addr)
                  << "] = " << std::to_string (write_val));
    }
    system_bus->write_hword (mem_addr, write_val);
}

void Emulator32bit::_b (const word instr)
{
    const U8 cond = bitfield_unsigned (instr, 22, 4);
    if (check_cond (m_pstate, cond))
    {
        m_pc += (bitfield_signed (instr, 0, 22) << 2)
                - 4; /* account for execution loop incrementing _pc by 4 */
    }
    DEBUG_SS (std::stringstream () << "b " << std::to_string (cond));
}

void Emulator32bit::_bl (const word instr)
{
    const U8 cond = bitfield_unsigned (instr, 22, 4);
    if (check_cond (m_pstate, cond))
    {
        write_reg (U8 (Register::LR), m_pc + 4);
        m_pc += (bitfield_signed (instr, 0, 22) << 2) - 4;
    }
    DEBUG_SS (std::stringstream () << "bl " << std::to_string (cond));
}

void Emulator32bit::_bx (const word instr)
{
    const U8 cond = bitfield_unsigned (instr, 22, 4);
    const U8 reg = bitfield_unsigned (instr, 17, 5);
    if (check_cond (m_pstate, cond))
    {
        m_pc = sword (read_reg (reg)) - 4;
    }
    DEBUG_SS (std::stringstream ()
              << "bx " << std::to_string (reg) << " (" << std::to_string (cond) << ")");
}

void Emulator32bit::_blx (const word instr)
{
    const U8 cond = bitfield_unsigned (instr, 22, 4);
    const U8 reg = bitfield_unsigned (instr, 17, 5);
    if (check_cond (m_pstate, cond))
    {
        write_reg (U8 (Emulator32bit::Register::LR), m_pc + 4);
        m_pc = sword (read_reg (reg)) - 4;
    }
    DEBUG_SS (std::stringstream ()
              << "blx " << std::to_string (reg) << "(" << std::to_string (cond) << ")");
}

void Emulator32bit::_adrp (const word instr)
{
    const U8 xd = _X1 (instr);
    const word imm20 = bitfield_unsigned (instr, 0, 20) << 12;

    signed int simm21 = imm20;
    if (test_bit (instr, kInstructionUpdateFlagBit))
    {
        simm21 -= (1 << 20);
    }

    word val = mask_0 (m_pc, 0, 12) + simm21;
    write_reg (xd, val);
    DEBUG_SS (std::stringstream ()
              << "adrp " << std::to_string (xd) << " " << std::to_string (simm21));
}