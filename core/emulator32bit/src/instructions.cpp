#include "emulator32bit/emulator32bit.h"
#include "util/logger.h"
#include "util/common.h"

#include <bit>
#include <format>
#include <string>

/// Useful macros to extract information from instruction bits
#define _X1(instr) (bitfield_unsigned<20, 5>(instr)) // bits 20 to 24
#define _X2(instr) (bitfield_unsigned<15, 5>(instr)) // bits 15 to 19
#define _X3(instr) (bitfield_unsigned<9, 5>(instr)) // bits 9 to 13
#define _X4(instr) (bitfield_unsigned<4, 5>(instr)) // bits 4 to 8

#define _SX1(instr) (bitfield_unsigned<17, 5>(instr)) // bits 17 to 21
#define _SX2(instr) (bitfield_unsigned<11, 5>(instr)) // bits 11 to 15
#define _SX3(instr) (bitfield_unsigned<6, 5>(instr)) // bits 6 to 10

/// Parse the value of the argument for instruction format O
///
/// Also used to parse value of argument for some other instruction format
/// like format M which conveniently has a similar structure
///
/// @param cpu Emulator context.
/// @param instr 32 bit instruction to extract the value of the argument.
static word get_format_o_arg(Emulator32bit &cpu, const word instr)
{
    if (test_bit<14>(instr))
    {
        return bitfield_unsigned<0, 14>(instr);
    }

    const word value = cpu.read_reg(_X3(instr));
    const auto type = ShiftType(bitfield_unsigned<7, 2>(instr));
    const U8 amount = bitfield_unsigned<2, 5>(instr);

    return alu_shift(value, type, amount, {}).result;
}

/// A sequence of bits to add to a @ref Joiner
struct JPart
{
    // A value that does not fit in its bits would spill into the fields next to it, and the
    // instruction would silently be another one.
    JPart(const int bits, const word val = 0) :
        bits(bits),
        val(val)
    {
        AEMU_CHECK(bits >= int(sizeof(word) * 8) || val < (word(1) << bits),
                   "Emulator32bit - The value {} does not fit in {} bits of an instruction.", val,
                   bits);
    }

    /// Number of bits stored in this part.
    const int bits;

    /// Contents of the bits stored in this part, with the first bit in the most significant bit.
    const word val;
};

/// Bits of an instruction that are 0 (unused, or a flag that is off)
struct Zeros
{
    explicit Zeros(const int bits) :
        bits(bits)
    {
    }

    const int bits;
};

/// A value that is formed by joining @ref JPart
class Joiner
{
  public:
    word val = 0; // Content stored so far

    /// Add a new @ref JPart
    ///
    /// @param p @ref JPart to add
    /// @return Reference to this object
    Joiner &operator<<(const JPart &p)
    {
        val <<= p.bits;
        val |= p.val;
        return *this;
    }

    /// Add filler bits all set to 0
    ///
    /// @param zeros The bits to add
    /// @return Reference to this object
    Joiner &operator<<(const Zeros &zeros)
    {
        val <<= zeros.bits;
        return *this;
    }

    /// Extract the value of this object
    ///
    /// @return word
    operator word() const
    {
        return val;
    }
};

word Emulator32bit::asm_format_o(const U8 opcode, const bool s, const int xd, const int xn,
                                 const int imm14)
{
    return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xd) << JPart(5, xn)
                    << JPart(1, 1) << JPart(14, imm14);
}

word Emulator32bit::asm_format_o(const U8 opcode, const bool s, const int xd, const int xn,
                                 const int xm, const ShiftType shift, const int imm5)
{
    return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xd) << JPart(5, xn) << Zeros(1)
                    << JPart(5, xm) << JPart(2, U8(shift)) << JPart(5, imm5) << Zeros(2);
}

word Emulator32bit::asm_format_o1(const U8 opcode, const int xd, const int xn, const bool imm,
                                  const int xm, const int imm5, const bool s)
{
    return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xd) << JPart(5, xn)
                    << JPart(1, imm) << JPart(5, xm) << Zeros(2) << JPart(5, imm5) << Zeros(2);
}

word Emulator32bit::asm_format_o2(const U8 opcode, const bool s, const int xlo, const int xhi,
                                  const int xn, const int xm)
{
    return Joiner() << JPart(6, opcode) << JPart(1, s) << JPart(5, xlo) << JPart(5, xhi) << Zeros(1)
                    << JPart(5, xn) << JPart(5, xm) << Zeros(4);
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
    return Joiner() << JPart(6, opcode) << JPart(1, sign) << JPart(5, xt) << JPart(5, xn)
                    << Zeros(1) << JPart(5, xm) << JPart(2, U8(shift)) << JPart(5, imm5)
                    << JPart(2, U8(adr));
}

word Emulator32bit::asm_format_m(const U8 opcode, const bool sign, const int xt, const int xn,
                                 const int simm12, const AddrType adr)
{
    return Joiner() << JPart(6, opcode) << JPart(1, sign) << JPart(5, xt) << JPart(5, xn)
                    << JPart(1, 1) << JPart(12, bitfield_unsigned<0, 12>(simm12))
                    << JPart(2, U8(adr));
}

word Emulator32bit::asm_format_m1(const U8 opcode, const int xd, const int imm20)
{
    return Joiner() << JPart(6, opcode) << Zeros(1) << JPart(5, xd) << JPart(20, imm20);
}

word Emulator32bit::asm_format_b1(const U8 opcode, const ConditionCode cond, const sword simm22)
{
    return Joiner() << JPart(6, opcode) << JPart(4, word(cond))
                    << JPart(22, bitfield_unsigned<0, 22>(simm22));
}

word Emulator32bit::asm_format_b2(const U8 opcode, const ConditionCode cond, const int xd)
{
    return Joiner() << JPart(6, opcode) << JPart(4, word(cond)) << JPart(5, xd) << Zeros(17);
}

void Emulator32bit::_special_instructions(const word instr)
{
    word opspec = bitfield_unsigned<22, 4>(instr);

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
    case kSpecialOpId_eret:
        _eret(instr);
        break;
    case kSpecialOpId_wfi:
        _wfi(instr);
        break;
    case kSpecialOpId_brk:
        _brk(instr);
        break;
    case kSpecialOpId_unary:
        _unary(instr);
        break;
    default:
        throw Exception(Emulator32bit::InterruptType::BAD_INSTR,
                        "Bad OPSPEC specifier " + std::to_string(opspec), kUndefinedIss_ext_op);
    }
}

void Emulator32bit::_hlt(const word instr)
{
    UNUSED(instr);
    require_kernel();
    throw Exception(InterruptType::HALT_INSTR, "HLT Exception");
}

void Emulator32bit::_bad_opcode(const word instr)
{
    throw Exception(InterruptType::BAD_INSTR,
                    "Bad opcode " + std::to_string(bitfield_unsigned<26, 6>(instr)),
                    kUndefinedIss_opcode);
}

word Emulator32bit::asm_hlt()
{
    return Joiner() << JPart(6, _op_special_instructions) << JPart(4, kSpecialOpId_hlt)
                    << Zeros(22);
}

void Emulator32bit::_nop(const word instr)
{
    UNUSED(instr);
    return; // do nothing
}

word Emulator32bit::asm_nop()
{
    return Joiner() << JPart(6, _op_special_instructions) << JPart(4, kSpecialOpId_nop)
                    << Zeros(22);
}

void Emulator32bit::_msr(const word instr)
{
    const U8 sysreg = _SX1(instr);
    const bool imm = test_bit<16>(instr);
    const word val = imm ? bitfield_unsigned<0, 16>(instr) : read_reg(_SX2(instr));

    if (sysreg == kSysregId_pstate && user_mode())
    {
        constexpr word kNZCV = 0b1111;
        m_pstate = (m_pstate & ~kNZCV) | (val & kNZCV);
        return;
    }

    require_kernel();
    write_sysreg(sysreg, val);
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
                        << JPart(5, sysreg) << JPart(1, imm) << JPart(5, xn_or_imm16) << Zeros(11);
    }
}

void Emulator32bit::_mrs(const word instr)
{
    const U8 xn = _SX1(instr);
    const U8 sysreg = _SX2(instr);

    if (sysreg == kSysregId_pstate && user_mode())
    {
        write_reg(xn, m_pstate & 0b1111);
        return;
    }

    require_kernel();
    write_reg(xn, read_sysreg(sysreg));
}

word Emulator32bit::asm_mrs(U8 xn, U8 sysreg)
{
    return Joiner() << JPart(6, _op_special_instructions) << JPart(4, kSpecialOpId_mrs)
                    << JPart(5, xn) << Zeros(1) << JPart(5, sysreg) << Zeros(11);
}

void Emulator32bit::_tlbi(const word instr)
{
    require_kernel();

    // With `?xt` the translation of the page that holds the address in xt, otherwise all of them.
    // imm16 is reserved (0).
    if (test_bit<16>(instr))
    {
        mmu->invalidate_translation(read_reg(_SX1(instr)));
    }
    else
    {
        mmu->invalidate_translations();
    }
}

word Emulator32bit::asm_tlbi(U8 xt, bool isxt, word imm16)
{
    return Joiner() << JPart(6, _op_special_instructions) << JPart(4, kSpecialOpId_tlbi)
                    << JPart(5, xt) << JPart(1, isxt) << JPart(16, imm16);
}

void Emulator32bit::_eret(const word instr)
{
    UNUSED(instr);
    require_kernel();

    set_user_mode(test_bit<kUserModeBit>(m_spsr));
    m_pstate = m_spsr & kPstateMask;
    m_pc = m_elr;
    m_pc_written = true;
}

word Emulator32bit::asm_eret()
{
    return Joiner() << JPart(6, _op_special_instructions) << JPart(4, kSpecialOpId_eret)
                    << Zeros(22);
}

void Emulator32bit::_wfi(const word instr)
{
    UNUSED(instr);
    require_kernel();
    SystemBus &bus = *system_bus;
    if (bus.intc.has_pending())
    {
        return;
    }

    // Time jumps to the first event, whichever device it is.
    const std::optional<word> timer = bus.timer.cycles_until_event();
    const std::optional<word> block = bus.block.cycles_until_event();
    if (timer || block)
    {
        const word wait = std::min(timer.value_or(~word(0)), block.value_or(~word(0)));
        bus.timer.advance(wait);
        bus.block.advance(wait);
        if (bus.intc.has_pending())
        {
            return;
        }
    }
    throw Exception(InterruptType::HALT_INSTR, "WFI with no interrupt source");
}

word Emulator32bit::asm_wfi()
{
    return Joiner() << JPart(6, _op_special_instructions) << JPart(4, kSpecialOpId_wfi)
                    << Zeros(22);
}

void Emulator32bit::_brk(const word instr)
{
    const word imm = bitfield_unsigned<0, 22>(instr);

    if (m_brk_stops)
    {
        throw Exception(InterruptType::BREAK_INSTR, std::format("brk {} at {:#010x}", imm, m_pc));
    }
    if (m_vbar != 0)
    {
        enter_exception(ExceptionClass::BREAKPOINT, imm, 0, m_pc);
        return;
    }
    throw Exception(
        InterruptType::PROGRAM_ERROR,
        std::format("brk {} executed at {:#010x}, there is no vector table and no debugger", imm,
                    m_pc));
}

word Emulator32bit::asm_brk(const word imm22)
{
    return Joiner() << JPart(6, _op_special_instructions) << JPart(4, kSpecialOpId_brk)
                    << JPart(22, imm22);
}

void Emulator32bit::_unary(const word instr)
{
    const word op = bitfield_unsigned<0, 4>(instr);
    const word xn = read_reg(_SX2(instr));

    word result;
    switch (op)
    {
    case kUnaryId_sxtb:
        result = word(sword(S8(xn)));
        break;
    case kUnaryId_sxth:
        result = word(sword(S16(xn)));
        break;
    case kUnaryId_uxtb:
        result = xn & 0xFF;
        break;
    case kUnaryId_uxth:
        result = xn & 0xFFFF;
        break;
    case kUnaryId_clz:
        result = word(std::countl_zero(xn));
        break;
    case kUnaryId_rev:
        result = (xn << 24) | ((xn & 0xFF00) << 8) | ((xn >> 8) & 0xFF00) | (xn >> 24);
        break;
    case kUnaryId_rev16:
        result = ((xn & 0x00FF00FF) << 8) | ((xn >> 8) & 0x00FF00FF);
        break;
    default:
        throw Exception(InterruptType::BAD_INSTR, "Bad unary operation " + std::to_string(op),
                        kUndefinedIss_ext_op);
    }
    write_reg(_SX1(instr), result);
}

word Emulator32bit::asm_unary(const word op, const word xd, const word xn)
{
    return Joiner() << JPart(6, _op_special_instructions) << JPart(4, kSpecialOpId_unary)
                    << JPart(5, xd) << Zeros(1) << JPart(5, xn) << Zeros(7) << JPart(4, op);
}

/// Conditional select: xd = cond ? xn : f(xm), where f depends on the variant. The flags are
/// not changed. `cset` and the other aliases are these with the zero register and the opposite
/// condition.
void Emulator32bit::_csel(const word instr)
{
    const U8 cond = bitfield_unsigned<22, 4>(instr);
    const word variant = bitfield_unsigned<4, 2>(instr);
    const word xn = read_reg(_SX2(instr));
    const word xm = read_reg(bitfield_unsigned<6, 5>(instr));

    word result = xn;
    if (!check_cond(m_pstate, cond))
    {
        switch (variant)
        {
        case kCselId_csel:
            result = xm;
            break;
        case kCselId_csinc:
            result = xm + 1;
            break;
        case kCselId_csinv:
            result = ~xm;
            break;
        default:
            result = word(0) - xm;
            break;
        }
    }
    write_reg(_SX1(instr), result);
}

word Emulator32bit::asm_csel(const word variant, const ConditionCode cond, const word xd,
                             const word xn, const word xm)
{
    return Joiner() << JPart(6, _op_csel) << JPart(4, word(cond)) << JPart(5, xd) << JPart(1, 0)
                    << JPart(5, xn) << JPart(5, xm) << JPart(2, variant) << Zeros(4);
}

void Emulator32bit::_atomic_rmw(const word instr, const AtomicOperation operation)
{
    const U8 xt = _SX1(instr);
    const U8 xn = _SX2(instr);
    const U8 xm = _SX3(instr);

    const word mem_adr = read_reg(xm);
    const U8 width = bitfield_unsigned<4, 2>(instr);

    const word val_reg = read_reg(xn);

    word val_mem;
    word new_val;

    switch (width)
    {
    case kAtomicWidth_word:
        val_mem = memory.read_word(mem_adr);
        break;

    case kAtomicWidth_byte:
        val_mem = memory.read_byte(mem_adr);
        break;

    case kAtomicWidth_hword:
        val_mem = memory.read_hword(mem_adr);
        break;

    default:
        throw Exception(InterruptType::BAD_INSTR, "Invalid atomic width " + std::to_string(width),
                        kUndefinedIss_ext_op);
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
        throw Exception(InterruptType::BAD_INSTR,
                        "Invalid atomic operation " + std::to_string(static_cast<int>(operation)),
                        kUndefinedIss_ext_op);
    }

    switch (width)
    {
    case kAtomicWidth_word:
        memory.write_word(mem_adr, new_val);
        break;

    case kAtomicWidth_byte:
        memory.write_byte(mem_adr, new_val);
        break;

    case kAtomicWidth_hword:
        memory.write_hword(mem_adr, new_val);
        break;

    default:
        // Already validated above.
        break;
    }

    if (UNLIKELY(!m_watchpoints.empty()))
    {
        const word length = width == kAtomicWidth_word ? 4 : width == kAtomicWidth_hword ? 2 : 1;
        // The write is reported first: it is the more interesting half of a read-modify-write.
        watch_access(mem_adr, length, true, new_val);
        watch_access(mem_adr, length, false, val_mem);
    }

    // Return the original memory value in Xt. Only once the memory is written, a write that
    // faults leaves the registers as they were.
    write_reg(xt, val_mem);
}

void Emulator32bit::_atomic(const word instr)
{
    const U8 atop = bitfield_unsigned<0, 4>(instr);

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
                        "Atomic op " + std::to_string(atop) + " unimplemented.",
                        kUndefinedIss_ext_op);
    }
}

word Emulator32bit::asm_atomic(word xt, word xn, word xm, U8 width, U8 atop)
{
    return Joiner() << JPart(6, _op_special_instructions) << JPart(4, kSpecialOpId_atomic)
                    << JPart(5, xt) << Zeros(1) << JPart(5, xn) << JPart(5, xm) << JPart(2, width)
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
        if (test_bit<kInstructionUpdateFlagBit>(instr)) set_NZCV(result.flags);                    \
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
            test_bit<14>(instr) ? bitfield_unsigned<2, 5>(instr) : (read_reg(_X3(instr)) & 0x1F);  \
        const AluResult result = alu_shift(read_reg(_X2(instr)), shift_type, amount, get_NZCV());  \
        if (test_bit<kInstructionUpdateFlagBit>(instr)) set_NZCV(result.flags);                    \
        write_reg(_X1(instr), result.result);                                                      \
    }

// Long multiplies: {xhi:xlo} = xn * xm, where xlo is _X1 and xhi is _X2.
#define LONG_MUL_OP(name, alu_fn)                                                                  \
    void Emulator32bit::name(const word instr)                                                     \
    {                                                                                              \
        const AluResult result = alu_fn(read_reg(_X3(instr)), read_reg(_X4(instr)), get_NZCV());   \
        if (test_bit<kInstructionUpdateFlagBit>(instr)) set_NZCV(result.flags);                    \
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
ALU_OP(_udiv, alu_udiv(rn, op2, get_NZCV()))
ALU_OP(_sdiv, alu_sdiv(rn, op2, get_NZCV()))

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

// Vector/floating point instructions are not implemented. Executing one is a fault instead of a
// silent no-op.
#define UNIMPLEMENTED_OP(name, mnemonic)                                                           \
    void Emulator32bit::name(const word instr)                                                     \
    {                                                                                              \
        UNUSED(instr);                                                                             \
        throw Exception(InterruptType::BAD_INSTR, mnemonic " is not implemented.",                 \
                        kUndefinedIss_unimplemented);                                              \
    }

UNIMPLEMENTED_OP(_vabs, "vabs")
UNIMPLEMENTED_OP(_vneg, "vneg")
UNIMPLEMENTED_OP(_vsqrt, "vsqrt")
UNIMPLEMENTED_OP(_vadd, "vadd")
UNIMPLEMENTED_OP(_vsub, "vsub")
UNIMPLEMENTED_OP(_vdiv, "vdiv")
UNIMPLEMENTED_OP(_vmul, "vmul")
UNIMPLEMENTED_OP(_vcmp, "vcmp")
UNIMPLEMENTED_OP(_vsel, "vsel")
UNIMPLEMENTED_OP(_vcint, "vcint")
UNIMPLEMENTED_OP(_vcflo, "vcflo")
UNIMPLEMENTED_OP(_vmov, "vmov")
#undef UNIMPLEMENTED_OP

static word get_mov_arg(Emulator32bit &cpu, const word instr)
{
    if (test_bit<19>(instr)) return bitfield_unsigned<0, 19>(instr);
    return bitfield_unsigned<0, 14>(instr) + cpu.read_reg(bitfield_unsigned<14, 5>(instr));
}

// Moves: xd = f(operand), where the operand is an imm19 or xn + imm14.
#define MOVE_OP(name, alu_fn)                                                                      \
    void Emulator32bit::name(const word instr)                                                     \
    {                                                                                              \
        const AluResult result = alu_fn(get_mov_arg(*this, instr), get_NZCV());                    \
        if (test_bit<kInstructionUpdateFlagBit>(instr)) set_NZCV(result.flags);                    \
        write_reg(_X1(instr), result.result);                                                      \
    }

MOVE_OP(_mov, alu_mov)
MOVE_OP(_mvn, alu_mvn)
#undef MOVE_OP

[[gnu::always_inline]] inline Emulator32bit::MemOperand
Emulator32bit::decode_mem_operand(const word instr)
{
    const U8 xn = _X2(instr);
    const bool simm = test_bit<14>(instr);
    const sword offset = simm ? bitfield_signed<2, 12>(instr) : get_format_o_arg(*this, instr);
    const U8 addr_mode = bitfield_unsigned<0, 2>(instr);

    const word base = read_reg(xn);
    switch (AddrType(addr_mode))
    {
    case AddrType::ADDR_OFFSET:
        return {.address = base + offset, .base = xn, .base_after = base, .writes_back = false};
    case AddrType::ADDR_PRE_INC:
        return {
            .address = base + offset, .base = xn, .base_after = base + offset, .writes_back = true};
    case AddrType::ADDR_POST_INC:
        return {.address = base, .base = xn, .base_after = base + offset, .writes_back = true};
    }

    throw Exception(InterruptType::BAD_INSTR,
                    "Bad memory address mode " + std::to_string(addr_mode), kUndefinedIss_ext_op);
}

static_assert(Emulator32bit::_op_ldr < Emulator32bit::_op_strh
              && Emulator32bit::_op_strh - Emulator32bit::_op_ldr == 5,
              "data_address_of treats ldr..strh as the range of the format M accesses");

word Emulator32bit::data_address_of(const word instr)
{
    const word opcode = bitfield_unsigned<26, 6>(instr);
    if (opcode >= _op_ldr && opcode <= _op_strh)
    {
        return decode_mem_operand(instr).address;
    }
    if (opcode == _op_special_instructions && bitfield_unsigned<22, 4>(instr) == kSpecialOpId_atomic)
    {
        return read_reg(_SX3(instr));
    }
    return 0;
}

void Emulator32bit::write_back_base(const MemOperand &operand)
{
    if (operand.writes_back)
    {
        write_reg(operand.base, operand.base_after);
    }
}

/// The base register is written back after the access, so an access that faults changes nothing.
/// A load writes the loaded value after that, which is why it wins when xt is the base register.
/// A store has read xt before the write back, so `str x1, [x1, 8]!` stores the old x1.
void Emulator32bit::_ldr(const word instr)
{
    const MemOperand mem = decode_mem_operand(instr);
    const word read_val = memory.read_word(mem.address);
    if (UNLIKELY(!m_watchpoints.empty()))
    {
        watch_access(mem.address, 4, false, read_val);
    }
    write_back_base(mem);
    write_reg(_X1(instr), read_val);
}

void Emulator32bit::_ldrb(const word instr)
{
    const bool sign = test_bit<25>(instr);
    const MemOperand mem = decode_mem_operand(instr);
    word read_val = memory.read_byte(mem.address);
    if (UNLIKELY(!m_watchpoints.empty()))
    {
        watch_access(mem.address, 1, false, read_val);
    }
    if (sign)
    {
        read_val = sword(S8(read_val));
    }
    write_back_base(mem);
    write_reg(_X1(instr), read_val);
}

void Emulator32bit::_ldrh(const word instr)
{
    const bool sign = test_bit<25>(instr);
    const MemOperand mem = decode_mem_operand(instr);
    word read_val = memory.read_hword(mem.address);
    if (UNLIKELY(!m_watchpoints.empty()))
    {
        watch_access(mem.address, 2, false, read_val);
    }
    if (sign)
    {
        read_val = sword(S16(read_val));
    }
    write_back_base(mem);
    write_reg(_X1(instr), read_val);
}

/// There is no sign bit in a store, the bytes that are stored are the low ones of xt.
void Emulator32bit::_str(const word instr)
{
    const MemOperand mem = decode_mem_operand(instr);
    const word value = read_reg(_X1(instr));
    memory.write_word(mem.address, value);
    if (UNLIKELY(!m_watchpoints.empty()))
    {
        watch_access(mem.address, 4, true, value);
    }
    write_back_base(mem);
}

void Emulator32bit::_strb(const word instr)
{
    const MemOperand mem = decode_mem_operand(instr);
    const word value = read_reg(_X1(instr));
    memory.write_byte(mem.address, value);
    if (UNLIKELY(!m_watchpoints.empty()))
    {
        watch_access(mem.address, 1, true, value & 0xFF);
    }
    write_back_base(mem);
}

void Emulator32bit::_strh(const word instr)
{
    const MemOperand mem = decode_mem_operand(instr);
    const word value = read_reg(_X1(instr));
    memory.write_hword(mem.address, value);
    if (UNLIKELY(!m_watchpoints.empty()))
    {
        watch_access(mem.address, 2, true, value & 0xFFFF);
    }
    write_back_base(mem);
}

void Emulator32bit::_b(const word instr)
{
    const U8 cond = bitfield_unsigned<22, 4>(instr);
    if (check_cond(m_pstate, cond))
    {
        m_pc += (bitfield_signed<0, 22>(instr) << 2)
                - 4; // account for execution loop incrementing _pc by 4
    }
}

void Emulator32bit::_bl(const word instr)
{
    const U8 cond = bitfield_unsigned<22, 4>(instr);
    if (check_cond(m_pstate, cond))
    {
        write_reg(Register::LR, m_pc + 4);
        m_pc += (bitfield_signed<0, 22>(instr) << 2) - 4;
    }
}

void Emulator32bit::_bx(const word instr)
{
    const U8 cond = bitfield_unsigned<22, 4>(instr);
    const U8 reg = bitfield_unsigned<17, 5>(instr);
    if (check_cond(m_pstate, cond))
    {
        m_pc = sword(read_reg(reg)) - 4;
    }
}

void Emulator32bit::_blx(const word instr)
{
    const U8 cond = bitfield_unsigned<22, 4>(instr);
    const U8 reg = bitfield_unsigned<17, 5>(instr);
    if (check_cond(m_pstate, cond))
    {
        write_reg(Register::LR, m_pc + 4);
        m_pc = sword(read_reg(reg)) - 4;
    }
}

void Emulator32bit::_adrp(const word instr)
{
    const U8 xd = _X1(instr);
    const word imm20 = bitfield_unsigned<0, 20>(instr);

    signed int simm21 = imm20;
    if (test_bit<kInstructionUpdateFlagBit>(instr))
    {
        simm21 -= (1 << 20);
    }

    word val = zero_bits<0, 12>(m_pc) + (simm21 << 12);
    write_reg(xd, val);
}

/// Same format as adrp, but the signed 21 bit number is a distance in bytes from this instruction.
void Emulator32bit::_adr(const word instr)
{
    const U8 xd = _X1(instr);
    sword distance = sword(bitfield_unsigned<0, 20>(instr));
    if (test_bit<kInstructionUpdateFlagBit>(instr))
    {
        distance -= (1 << 20);
    }

    write_reg(xd, m_pc + word(distance));
}