#pragma once

#include "emulator32bit/alu.h"
#include "emulator32bit/disk.h"
#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/memory.h"
#include "emulator32bit/system_bus.h"

#include <string>

// TODO: I think we can get rid of these forward declarations. They should not
// need to know about this emulator class.
class MMU;
class Timer;

///
/// @brief              32 bit Emulator
/// @paragraph          Modeled off of the ARM architecture with many simplifications.
///                     A software simulated processor.
///
class Emulator32bit
{
  public:
    /// @brief              Default size of RAM memory in pages.
    static constexpr word RAM_NPAGES = 16;

    /// @brief              Default start page of RAM memory.
    static constexpr word RAM_START_PAGE = 0;

    /// @brief              Default size of ROM memory in pages.
    static constexpr word ROM_NPAGES = 16;

    /// @brief              Default start page of ROM memory.
    static constexpr word ROM_START_PAGE = 16;

    /// @brief              Data stored in ROM, should be of the same length specified in
    ///                     @ref ROM_NPAGES.
    static constexpr byte ROM_DATA[ROM_NPAGES << kNumPageOffsetBits] = {};

    Emulator32bit();
    Emulator32bit(word ram_npages, word ram_start_page, const byte rom_data[], word rom_npages,
                  word rom_start_page);
    Emulator32bit(RAM *ram, ROM *rom, Disk *disk);
    ~Emulator32bit();

    // TODO:
    enum class InterruptType : U8
    {
        BAD_REG,
        BAD_INSTR,
        HALT_INSTR,
        FAILED_ASSERT,
        BAD_PAGEDIR,
        PAGEFAULT,
    };

    // TODO:
    struct InterruptFrame
    {
        word saved_reg[kNumReg];
        word saved_px;
        word saved_pstate;
        word saved_pagedir;
    };

    class Exception : public std::exception
    {
      private:
        InterruptType type;
        std::string message;

      public:
        Exception(InterruptType type, const std::string &msg);
        const char *what() const noexcept override;
        InterruptType get_type() const noexcept;
    };

    enum class AddrType : U8
    {
        ADDR_OFFSET,
        ADDR_PRE_INC,
        ADDR_POST_INC
    };

    SystemBus *const system_bus = nullptr;

    Timer *const timer = nullptr;

    /// @brief              Pointer to the page directory for the virtual address space of the
    ///                     process.
    word pagedir;

    /// @brief              Why a call to run () stopped.
    struct RunResult
    {
        enum class Status : U8
        {
            HALTED,        ///< Executed a HLT instruction.
            LIMIT_REACHED, ///< Executed the requested number of instructions.
            FAULT,         ///< Any other emulator or system bus exception.
        };

        Status status;
        U64 instructions_ran;
        std::string message; ///< Exception message, empty if the limit was reached.
    };

    /// @brief              Exit code of the emulator CLI interface.
    enum class EmuCLIExitCode : U8
    {
        EXIT_HALTED = 0,
        EXIT_USAGE_ERROR = 1,
        EXIT_LIMIT_REACHED = 2,
        EXIT_FAULT = 3,
    };

    ///
    /// @brief                  Run the emulator for a given number of instructions.
    ///
    /// @param instructions     Number of instructions to run, if 0 run until HLT instruction or
    ///                         exception is thrown.
    /// @return                 Why execution stopped and how many instructions ran.
    ///
    RunResult run(U64 instructions);

    void print();

    ///
    /// @brief              Resets the processor state.
    ///
    ///
    void reset();

    inline void set_pc(word pc)
    {
        m_pc = pc;
    }

    inline word get_pc()
    {
        return m_pc;
    }

    inline word read_reg(Register reg)
    {
        const U8 r = register_to_U8(reg);
        // The lower 32 bits is the register mask, upper 32 bits contains the value.
        return word(m_x[r]) & word(m_x[r] >> 32);
    }

    inline word read_reg(U8 reg)
    {
        if (LIKELY(reg < kNumReg))
        {
            // The lower 32 bits is the register mask, upper 32 bits contains the value.
            return word(m_x[reg]) & word(m_x[reg] >> 32);
        }
        return 0;
    }

    inline void write_reg(Register reg, word val)
    {
        const U8 r = register_to_U8(reg);
        // The lower 32 bits is the register mask, upper 32 bits contains the value.
        m_x[r] = word(m_x[r]) ^ dword(val) << 32;
    }

    inline void write_reg(U8 reg, word val)
    {
        if (LIKELY(reg < kNumReg))
        {
            // The lower 32 bits is the register mask, upper 32 bits contains the value.
            m_x[reg] = word(m_x[reg]) ^ dword(val) << 32;
        }
    }

    ///
    /// @brief              Sets flags in the process state register.
    ///
    /// @param flag         Bit to set.
    /// @param value        Flag value.
    ///
    inline void set_flag(U8 flag, bool value)
    {
        m_pstate = set_bit(m_pstate, flag, value);
    }

    inline bool get_flag(U8 flag)
    {
        return test_bit(m_pstate, flag);
    }

    ///
    /// @brief              Sets the @ref _pstate NZCV flags.
    ///
    /// @param N            Negative flag.
    /// @param Z            Zero flag.
    /// @param C            Carry flag.
    /// @param V            Overflow flag.
    ///
    inline void set_NZCV(bool N, bool Z, bool C, bool V)
    {
        m_pstate = set_bit(m_pstate, kNFlagBit, N);
        m_pstate = set_bit(m_pstate, kZFlagBit, Z);
        m_pstate = set_bit(m_pstate, kCFlagBit, C);
        m_pstate = set_bit(m_pstate, kVFlagBit, V);
    }

    inline void set_NZCV(NZCVFlags flags)
    {
        set_NZCV(flags.n, flags.z, flags.c, flags.v);
    }

    inline NZCVFlags get_NZCV()
    {
        return {
            .n = test_bit(m_pstate, kNFlagBit),
            .z = test_bit(m_pstate, kZFlagBit),
            .c = test_bit(m_pstate, kCFlagBit),
            .v = test_bit(m_pstate, kVFlagBit),
        };
    }

    /// @todo               TODO: determine if fp registers are needed
    // word fpcr;
    // word fpsr;

  private:
    ///
    /// @brief              General purpose registers, x0-x29, xzr, and SP. x29 is the link register.
    ///
    ///                     Format: top 32 bits register value, bottom 32 bits mask value (for xzr
    ///                     register)
    ///
    dword m_x[kNumReg];

    /// @brief              Program counter.
    word m_pc;

    /// @brief              Program state. Bits 0-3 are NZCV flags. Rest are TODO
    word m_pstate;

    using InstructionFunction = void (Emulator32bit::*)(word);
    InstructionFunction m_instruction_handler[kMaxInstructions];

    void fill_out_instructions();

    word calc_mem_addr(word xn, sword offset, U8 addr_mode);

    inline void execute(word instr)
    {
        (this->*m_instruction_handler[bitfield_unsigned(instr, 26, 6)])(instr);
    }

#define _INSTR(func_name, opcode)                                                                  \
  private:                                                                                         \
    void _##func_name(word instr);                                                                 \
                                                                                                   \
  public:                                                                                          \
    static constexpr word _op_##func_name = opcode;

    // Instruction handling.
    _INSTR(special_instructions, 0b000000)

    void _hlt(const word instr);
    void _nop(const word instr);
    void _msr(const word instr);
    void _mrs(const word instr);
    void _tlbi(const word instr);
    void _atomic(const word instr);

    _INSTR(add, 0b000001)
    _INSTR(sub, 0b000010)
    _INSTR(rsb, 0b000011)
    _INSTR(adc, 0b000100)
    _INSTR(sbc, 0b000101)
    _INSTR(rsc, 0b000110)
    _INSTR(mul, 0b000111)
    _INSTR(umull, 0b001000)
    _INSTR(smull, 0b001001)

    _INSTR(vabs, 0b001010)
    _INSTR(vneg, 0b001011)
    _INSTR(vsqrt, 0b001100)
    _INSTR(vadd, 0b001101)
    _INSTR(vsub, 0b001110)
    _INSTR(vdiv, 0b001111)
    _INSTR(vmul, 0b010000)
    _INSTR(vcmp, 0b010001)
    _INSTR(vsel, 0b010010)
    _INSTR(vcint, 0b010011)
    _INSTR(vcflo, 0b010100)
    _INSTR(vmov, 0b010101)

    _INSTR(and, 0b010110)
    _INSTR(orr, 0b010111)
    _INSTR(eor, 0b011000)
    _INSTR(bic, 0b011001)
    _INSTR(lsl, 0b011010)
    _INSTR(lsr, 0b011011)
    _INSTR(asr, 0b011100)
    _INSTR(ror, 0b011101)

    _INSTR(cmp, 0b011110)
    _INSTR(cmn, 0b011111)
    _INSTR(tst, 0b100000)
    _INSTR(teq, 0b100001)

    _INSTR(mov, 0b100010)
    _INSTR(mvn, 0b100011)

    _INSTR(ldr, 0b100100)
    _INSTR(ldrb, 0b100101)
    _INSTR(ldrh, 0b100110)
    _INSTR(str, 0b100111)
    _INSTR(strb, 0b101000)
    _INSTR(strh, 0b101001)
    // _INSTR(nop, 0b101010)
    // _INSTR(nop, 0b101011)
    // _INSTR(nop, 0b101100)
    _INSTR(b, 0b101101)
    _INSTR(bl, 0b101110)
    _INSTR(bx, 0b101111)
    _INSTR(blx, 0b110000)
    _INSTR(swi, 0b110001)

    _INSTR(adrp, 0b110010)

    // _INSTR(nop_, 0b110100)
    // _INSTR(nop_, 0b110101)
    // _INSTR(nop_, 0b110110)
    // _INSTR(nop_, 0b110111)
    // _INSTR(nop_, 0b111000)
    // _INSTR(nop_, 0b111001)
    // _INSTR(nop_, 0b111010)
    // _INSTR(nop_, 0b111011)
    // _INSTR(nop_, 0b111100)
    // _INSTR(nop_, 0b111101)
    // _INSTR(nop_, 0b111110)

    // _INSTR(nop, 0b111111)

#undef _INSTR

    enum class AtomicOperation
    {
        SWP,
        LDADD,
        LDCLR,
        LDSET
    };

    void _atomic_rmw(const word instr, AtomicOperation operation);

    // Software interrupt handling.
    void _emu_print();
    void _emu_printr(U8 reg_id);
    void _emu_printm(word mem_addr, U8 size, bool little_endian);
    void _emu_printp();
    void _emu_assertr(U8 reg_id, word min_value, word max_value);
    void _emu_assertm(word mem_addr, U8 size, bool little_endian, word min_value, word max_value);
    void _emu_assertp(U8 p_state_id, bool expected_value);
    void _emu_log(word str);
    void _emu_err(word err);

  public:
    // Helpers to assemble instructions.
    static word asm_hlt();
    static word asm_nop();
    static word asm_msr(U8 sysreg, bool imm, word xn_or_imm16);
    static word asm_mrs(U8 xn, U8 sysreg);
    static word asm_tlbi(U8 xt, bool isxt, word imm16);
    static word asm_atomic(word xt, word xn, word xm, U8 width, U8 atop);

    static word asm_format_o(U8 opcode, bool s, int xd, int xn, int imm14);
    static word asm_format_o(U8 opcode, bool s, int xd, int xn, int xm, ShiftType shift, int imm5);
    static word asm_format_o1(U8 opcode, int xd, int xn, bool imm, int xm, int imm5,
                              bool s = false);
    static word asm_format_o2(U8 opcode, bool s, int xlo, int xhi, int xn, int xm);
    static word asm_format_o3(U8 opcode, bool s, int xd, int imm19);
    static word asm_format_o3(U8 opcode, bool s, int xd, int xn, int imm14);
    static word asm_format_m(U8 opcode, bool sign, int xt, int xn, int xm, ShiftType shift,
                             int imm5, AddrType adr);
    static word asm_format_m(U8 opcode, bool sign, int xt, int xn, int simm12, AddrType adr);
    static word asm_format_m1(U8 opcode, int xd, int imm20);
    static word asm_format_b1(U8 opcode, ConditionCode cond, sword simm22);
    static word asm_format_b2(U8 opcode, ConditionCode cond, int xd);

    static std::string disassemble_instr(word instr);

    // Since these operations are encoded under one 'Special' instruction,
    // these are the id for each operation.
    static constexpr word kSpecialOpId_hlt = 0b0000;
    static constexpr word kSpecialOpId_nop = 0b1111;
    static constexpr word kSpecialOpId_msr = 0b0001;
    static constexpr word kSpecialOpId_mrs = 0b0010;
    static constexpr word kSpecialOpId_tlbi = 0b0011;
    static constexpr word kSpecialOpId_atomic = 0b0100;

    static constexpr word kAtomicId_swp = 0b0000;
    static constexpr word kAtomicId_ldadd = 0b0001;
    static constexpr word kAtomicId_ldclr = 0b0010;
    static constexpr word kAtomicId_ldset = 0b0011;

    static constexpr word kAtomicWidth_word = 0b00;
    static constexpr word kAtomicWidth_byte = 0b01;
    static constexpr word kAtomicWidth_hword = 0b10;

    static constexpr word kSysregId_pstate = 1;
};
