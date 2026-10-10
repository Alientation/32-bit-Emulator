#pragma once

#include "emulator32bit/opcodes.h"
#include "util/types.h"

/// The numbers that name things in an instruction word: the opcodes, the operations of the
/// special instructions, how a memory access uses its offset, the system registers. They are the
/// encoding of the instruction set (docs/isa.md) and nothing else, so what only needs to read or
/// write instructions (the disassembler, the assembler) does not need the machine.
///
/// `Emulator32bit` derives from it, so every name here is also `Emulator32bit::name`.
struct Encoding
{
    /// How a load or a store uses its offset: added to the base, added before the base is updated
    /// (pre-index) or after the access (post-index).
    ///
    /// ADDR_UNALIGNED is the offset form of `ldur`, `ldurh`, `stur` and `sturh`: the same as
    /// ADDR_OFFSET, but the address does not have to be aligned to the size of the access. It
    /// exists for the word and half-word accesses only (a byte is always aligned), and has no
    /// pre or post indexed form.
    enum class AddrType : U8
    {
        ADDR_OFFSET,
        ADDR_PRE_INC,
        ADDR_POST_INC,
        ADDR_UNALIGNED,
    };

    // The opcode constants `_op_<name>`, one for each row of AEMU_OPCODES (opcodes.h).
#define AEMU_DECLARE_OPCODE_CONSTANT(name, opcode) static constexpr word _op_##name = opcode;
    AEMU_OPCODES(AEMU_DECLARE_OPCODE_CONSTANT)
#undef AEMU_DECLARE_OPCODE_CONSTANT

    // Since these operations are encoded under one 'Special' instruction,
    // these are the id for each operation.
    static constexpr word kSpecialOpId_hlt = 0b0000;
    static constexpr word kSpecialOpId_nop = 0b1111;
    static constexpr word kSpecialOpId_msr = 0b0001;
    static constexpr word kSpecialOpId_mrs = 0b0010;
    static constexpr word kSpecialOpId_tlbi = 0b0011;
    static constexpr word kSpecialOpId_atomic = 0b0100;
    static constexpr word kSpecialOpId_eret = 0b0101;
    static constexpr word kSpecialOpId_wfi = 0b0110;
    static constexpr word kSpecialOpId_brk = 0b0111;
    static constexpr word kSpecialOpId_unary = 0b1000;

    // The operations of the unary special instruction (`op xd, xn`, bits 0-3).
    static constexpr word kUnaryId_sxtb = 0b0000;
    static constexpr word kUnaryId_sxth = 0b0001;
    static constexpr word kUnaryId_uxtb = 0b0010;
    static constexpr word kUnaryId_uxth = 0b0011;
    static constexpr word kUnaryId_clz = 0b0100;
    static constexpr word kUnaryId_rev = 0b0101;
    static constexpr word kUnaryId_rev16 = 0b0110;

    // The variants of CSEL (bits 4-5): what is written when the condition is false.
    static constexpr word kCselId_csel = 0b00;  ///< xm
    static constexpr word kCselId_csinc = 0b01; ///< xm + 1
    static constexpr word kCselId_csinv = 0b10; ///< ~xm
    static constexpr word kCselId_csneg = 0b11; ///< -xm

    static constexpr word kAtomicId_swp = 0b0000;
    static constexpr word kAtomicId_ldadd = 0b0001;
    static constexpr word kAtomicId_ldclr = 0b0010;
    static constexpr word kAtomicId_ldset = 0b0011;

    static constexpr word kAtomicWidth_word = 0b00;
    static constexpr word kAtomicWidth_byte = 0b01;
    static constexpr word kAtomicWidth_hword = 0b10;

    // The system registers, the 5 bit number of MSR and MRS. 0 reads 0 and ignores writes.
    static constexpr word kSctlrMmuEnable = 1; ///< Bit 0 of SCTLR.
    static constexpr word kSysregId_pstate = 1;
    static constexpr word kSysregId_elr = 2;
    static constexpr word kSysregId_spsr = 3;
    static constexpr word kSysregId_esr = 4;
    static constexpr word kSysregId_far = 5;
    static constexpr word kSysregId_vbar = 6;
    static constexpr word kSysregId_usp = 7;
    static constexpr word kSysregId_ptbr = 8;
    static constexpr word kSysregId_sctlr = 9;
    static constexpr word kSysregId_fpcr = 10;
    static constexpr word kSysregId_fpsr = 11;

    /// The immediate of `swi` that is an emulator call, see _swi. Every other one is a system call
    /// of the operating system.
    static constexpr word kSwiSemihosting = 1;

    /// @param id the number of a system register
    /// @return the name of the register as written in assembly (lower case), nullptr if there is
    ///     no register with the number
    static const char *sysreg_name(const U8 id)
    {
        for (const SysregName &sysreg : kSysregNames)
        {
            if (sysreg.id == id)
            {
                return sysreg.name;
            }
        }
        return nullptr;
    }

    /// A system register and its name in assembly.
    struct SysregName
    {
        U8 id;
        const char *name;
    };

    /// The system registers by number.
    static constexpr SysregName kSysregNames[] = {
        {kSysregId_pstate, "pstate"}, {kSysregId_elr, "elr"},     {kSysregId_spsr, "spsr"},
        {kSysregId_esr, "esr"},       {kSysregId_far, "far"},     {kSysregId_vbar, "vbar"},
        {kSysregId_usp, "usp"},       {kSysregId_ptbr, "ptbr"},   {kSysregId_sctlr, "sctlr"},
        {kSysregId_fpcr, "fpcr"},     {kSysregId_fpsr, "fpsr"},
    };
};
