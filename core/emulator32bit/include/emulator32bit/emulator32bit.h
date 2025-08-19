#pragma once

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

    Emulator32bit ();
    Emulator32bit (word ram_npages, word ram_start_page, const byte rom_data[], word rom_npages,
                   word rom_start_page);
    Emulator32bit (RAM *ram, ROM *rom, Disk *disk);
    ~Emulator32bit ();

    enum class Register : U8
    {
        X0 = 0,
        X1 = 1,
        X2 = 2,
        X3 = 3,
        X4 = 4,
        X5 = 5,
        X6 = 6,
        X7 = 7,
        X8 = 8,
        SYSCALL = 8,
        X9 = 9,
        X10 = 10,
        X11 = 11,
        X12 = 12,
        X13 = 13,
        X14 = 14,
        X15 = 15,
        X16 = 16,
        X17 = 17,
        X18 = 18,
        X19 = 19,
        X20 = 20,
        X21 = 21,
        X22 = 22,
        X23 = 23,
        X24 = 24,
        X25 = 25,
        X26 = 26,
        X27 = 27,
        X28 = 28,
        FP = 28,
        X29 = 29,
        LR = 29,
        SP = 30,
        XZR = 31,
    };

    ///
    /// @brief                  IDs for special registers
    ///
    /// Stack grows downwards
    /// <--------STACK_TOP-------->
    ///          Saved FP
    ///          Saved LR                <--- fp
    ///  ---STACK_FRAME_BORDER---
    ///      local variables
    ///          <...>
    ///          <...>
    ///          <...>
    ///      local variables             <---- sp
    ///
    /// Link register stores the previous pc, the next instruction is what will be
    /// executed.
    ///
    /// Register Conventions
    ///  - x0-x17: Caller Saved
    ///     - x0-x7: Parameter Registers
    ///     - x0: Return value
    ///     - x8: Syscall Number
    ///  - x19-27: Callee Saved
    ///  - x28: Frame Register
    ///  - x29: Link Register
    ///

    /// @brief              Number of general purpose stack registers.
    static constexpr U8 kNumReg = 32;
    static_assert (U8 (Register::XZR) + 1 == kNumReg);

    ///
    /// @brief              Flag bit locations in the _pstate register.
    ///

    /// @brief              Negative Flag.
    static constexpr U8 kNFlagBit = 0;

    /// @brief              Zero Flag.
    static constexpr U8 kZFlagBit = 1;

    /// @brief              Carry Flag.
    static constexpr U8 kCFlagBit = 2;

    /// @brief              Overflow Flag.
    static constexpr U8 kVFlagBit = 3;

    /// @brief              User mode flag.
    static constexpr U8 kUserModeFlagBit = 8;

    /// @brief              Real memory mode flag.
    static constexpr U8 kRealModeFlagBit = 9;

    /// @brief              Which bit of the instruction determines whether flags will be updated.
    static constexpr U8 kInstructionUpdateFlagBit = 25;

    /// @brief              Max supported instructions. 6 bits are used to represent the opcode for easy
    ///                     look-up table translations.
    static constexpr U8 kMaxInstructions = 64;

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
        Exception (InterruptType type, const std::string &msg);
        const char *what () const noexcept override;
    };

    enum class ConditionCode : U8
    {
        /// @brief          Equal                           : Z==1
        EQ = 0,

        /// @brief          Not Equal                       : Z==0
        NE = 1,

        /// @brief          Unsigned higher or same         : C==1
        CS = 2,
        HS = 2,

        /// @brief          Unsigned lower                  : C==0
        CC = 3,
        LO = 3,

        /// @brief          Negative                        : N==1
        MI = 4,

        /// @brief          Nonnegative                     : N==0
        PL = 5,

        /// @brief          Signed overflow                 : V==1
        VS = 6,

        /// @brief          No signed overflow              : V==0
        VC = 7,

        /// @brief          Unsigned higher                 : C==1 && Z==0
        HI = 8,

        /// @brief          Unsigned lower or same          : C==0 || Z==0
        LS = 9,

        /// @brief          Signed greater than or equal    : N==V
        GE = 10,

        /// @brief          Signed less than                : N!=V
        LT = 11,

        /// @brief          Signed greater than             : Z==0 && N==V
        GT = 12,

        /// @brief          Signed less than or equal       : Z==1 || N!=V
        LE = 13,

        /// @brief          Always executed                 : NONE
        AL = 14,

        /// @brief          Never executed                  : NONE
        NV = 15,
    };

    enum class ShiftType : U8
    {
        SHIFT_LSL,
        SHIFT_LSR,
        SHIFT_ASR,
        SHIFT_ROR
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

    ///
    /// @brief                  Run the emulator for a given number of instructions.
    ///
    /// @param instructions     Number of instructions to run, if 0 run until HLT instruction or
    ///                         exception is thrown.
    /// @throws                 Exception
    ///
    void run (U64 instructions);

    void print ();

    ///
    /// @brief              Resets the processor state.
    ///
    ///
    void reset ();

    inline void set_pc (word pc)
    {
        m_pc = pc;
    }

    inline word get_pc ()
    {
        return m_pc;
    }

    inline word read_reg (U8 reg)
    {
        return word (m_x[reg]) & word (m_x[reg] >> 32);
    }

    inline void write_reg (U8 reg, word val)
    {
        m_x[reg] = word (m_x[reg]) ^ dword (val) << 32;
    }

    ///
    /// @brief              Sets the @ref _pstate NZCV flags.
    ///
    /// @param N            Negative flag.
    /// @param Z            Zero flag.
    /// @param C            Carry flag.
    /// @param V            Overflow flag.
    ///
    inline void set_NZCV (bool N, bool Z, bool C, bool V)
    {
        m_pstate = set_bit (m_pstate, kNFlagBit, N);
        m_pstate = set_bit (m_pstate, kZFlagBit, Z);
        m_pstate = set_bit (m_pstate, kCFlagBit, C);
        m_pstate = set_bit (m_pstate, kVFlagBit, V);
    }

    ///
    /// @brief              Sets flags in the process state register.
    ///
    /// @param flag         Bit to set.
    /// @param value        Flag value.
    ///
    inline void set_flag (U8 flag, bool value)
    {
        m_pstate = set_bit (m_pstate, flag, value);
    }

    inline bool get_flag (U8 flag)
    {
        return test_bit (m_pstate, flag);
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

    using InstructionFunction = void (Emulator32bit::*) (word);
    InstructionFunction m_instruction_handler[kMaxInstructions];

    void fill_out_instructions ();

    word calc_mem_addr (word xn, sword offset, U8 addr_mode);

    inline void execute (word instr)
    {
        (this->*m_instruction_handler[bitfield_unsigned (instr, 26, 6)]) (instr);
    }

    inline bool check_cond (word pstate, U8 cond)
    {
        bool N = test_bit (pstate, kNFlagBit);
        bool Z = test_bit (pstate, kZFlagBit);
        bool C = test_bit (pstate, kCFlagBit);
        bool V = test_bit (pstate, kVFlagBit);

        switch ((ConditionCode) cond)
        {
        case ConditionCode::EQ:
            return Z == 1;
        case ConditionCode::NE:
            return Z == 0;
        case ConditionCode::CS:
            return C == 1;
        case ConditionCode::CC:
            return C == 0;
        case ConditionCode::MI:
            return N == 1;
        case ConditionCode::PL:
            return N == 0;
        case ConditionCode::VS:
            return V == 1;
        case ConditionCode::VC:
            return V == 0;
        case ConditionCode::HI:
            return C == 1 && Z == 0;
        case ConditionCode::LS:
            return C == 0 || Z == 1;
        case ConditionCode::GE:
            return N == V;
        case ConditionCode::LT:
            return N != V;
        case ConditionCode::GT:
            return Z == 0 && (N == V);
        case ConditionCode::LE:
            return Z == 1 || (N != V);
        case ConditionCode::AL:
            return true;
        case ConditionCode::NV:
            return false;
        }

        // Shouldn't ever reach this.
        // TODO: figure out how to report this better?
        return false;
    }

#define _INSTR(func_name, opcode)                                                                  \
  private:                                                                                         \
    void _##func_name (word instr);                                                                \
                                                                                                   \
  public:                                                                                          \
    static constexpr word _op_##func_name = opcode;

    // Instruction handling.
    _INSTR (special_instructions, 0b000000)

    void _hlt (const word instr);
    void _nop (const word instr);
    void _msr (const word instr);
    void _mrs (const word instr);
    void _tlbi (const word instr);
    void _atomic (const word instr);
    void _swp (const word instr);
    void _ldadd (const word instr);
    void _ldclr (const word instr);
    void _ldset (const word instr);

    _INSTR (add, 0b000001)
    _INSTR (sub, 0b000010)
    _INSTR (rsb, 0b000011)
    _INSTR (adc, 0b000100)
    _INSTR (sbc, 0b000101)
    _INSTR (rsc, 0b000110)
    _INSTR (mul, 0b000111)
    _INSTR (umull, 0b001000)
    _INSTR (smull, 0b001001)

    _INSTR (vabs, 0b001010)
    _INSTR (vneg, 0b001011)
    _INSTR (vsqrt, 0b001100)
    _INSTR (vadd, 0b001101)
    _INSTR (vsub, 0b001110)
    _INSTR (vdiv, 0b001111)
    _INSTR (vmul, 0b010000)
    _INSTR (vcmp, 0b010001)
    _INSTR (vsel, 0b010010)
    _INSTR (vcint, 0b010011)
    _INSTR (vcflo, 0b010100)
    _INSTR (vmov, 0b010101)

    _INSTR (and, 0b010110)
    _INSTR (orr, 0b010111)
    _INSTR (eor, 0b011000)
    _INSTR (bic, 0b011001)
    _INSTR (lsl, 0b011010)
    _INSTR (lsr, 0b011011)
    _INSTR (asr, 0b011100)
    _INSTR (ror, 0b011101)

    _INSTR (cmp, 0b011110)
    _INSTR (cmn, 0b011111)
    _INSTR (tst, 0b100000)
    _INSTR (teq, 0b100001)

    _INSTR (mov, 0b100010)
    _INSTR (mvn, 0b100011)

    _INSTR (ldr, 0b100100)
    _INSTR (ldrb, 0b100101)
    _INSTR (ldrh, 0b100110)
    _INSTR (str, 0b100111)
    _INSTR (strb, 0b101000)
    _INSTR (strh, 0b101001)
    // _INSTR(nop, 0b101010)
    // _INSTR(nop, 0b101011)
    // _INSTR(nop, 0b101100)
    _INSTR (b, 0b101101)
    _INSTR (bl, 0b101110)
    _INSTR (bx, 0b101111)
    _INSTR (blx, 0b110000)
    _INSTR (swi, 0b110001)

    _INSTR (adrp, 0b110010)

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

    // Software interrupt handling.
    void _emu_print ();
    void _emu_printr (U8 reg_id);
    void _emu_printm (word mem_addr, U8 size, bool little_endian);
    void _emu_printp ();
    void _emu_assertr (U8 reg_id, word min_value, word max_value);
    void _emu_assertm (word mem_addr, U8 size, bool little_endian, word min_value, word max_value);
    void _emu_assertp (U8 p_state_id, bool expected_value);
    void _emu_log (word str);
    void _emu_err (word err);

  public:
    // Helpers to assemble instructions.
    static word asm_hlt ();
    static word asm_nop ();
    static word asm_msr (U8 sysreg, bool imm, word xn_or_imm16);
    static word asm_mrs (U8 xn, U8 sysreg);
    static word asm_tlbi (U8 xt, bool isxt, word imm16);
    static word asm_atomic (word xt, word xn, word xm, U8 width, U8 atop);

    static word asm_format_o (U8 opcode, bool s, int xd, int xn, int imm14);
    static word asm_format_o (U8 opcode, bool s, int xd, int xn, int xm, ShiftType shift, int imm5);
    static word asm_format_o1 (U8 opcode, int xd, int xn, bool imm, int xm, int imm5);
    static word asm_format_o2 (U8 opcode, bool s, int xlo, int xhi, int xn, int xm);
    static word asm_format_o3 (U8 opcode, bool s, int xd, int imm19);
    static word asm_format_o3 (U8 opcode, bool s, int xd, int xn, int imm14);
    static word asm_format_m (U8 opcode, bool sign, int xt, int xn, int xm, ShiftType shift,
                              int imm5, AddrType adr);
    static word asm_format_m (U8 opcode, bool sign, int xt, int xn, int simm12, AddrType adr);
    static word asm_format_m1 (U8 opcode, int xd, int imm20);
    static word asm_format_b1 (U8 opcode, ConditionCode cond, sword simm22);
    static word asm_format_b2 (U8 opcode, ConditionCode cond, int xd);

    static std::string disassemble_instr (word instr);

    static constexpr word _opspec_hlt = 0b0000;
    static constexpr word _opspec_nop = 0b1111;
    static constexpr word _opspec_msr = 0b0001;
    static constexpr word _opspec_mrs = 0b0010;
    static constexpr word _opspec_tlbi = 0b0011;
    static constexpr word _opspec_atomic = 0b0100;

    static constexpr int ATOMIC_SWP = 0b0000;
    static constexpr int ATOMIC_LDADD = 0b0001;
    static constexpr int ATOMIC_LDCLR = 0b0010;
    static constexpr int ATOMIC_LDSET = 0b0011;

    static constexpr int ATOMIC_WIDTH_WORD = 0b00;
    static constexpr int ATOMIC_WIDTH_BYTE = 0b01;
    static constexpr int ATOMIC_WIDTH_HWORD = 0b10;

    static constexpr word SYSREG_PSTATE = 1;
};
