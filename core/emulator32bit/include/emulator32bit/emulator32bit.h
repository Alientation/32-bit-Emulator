#pragma once

#include "emulator32bit/alu.h"
#include "emulator32bit/disk.h"
#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/memory.h"
#include "emulator32bit/opcodes.h"
#include "emulator32bit/system_bus.h"

#include <iostream>
#include <memory>
#include <string>

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

    /// The emulator owns the memories and the disk.
    Emulator32bit(RAM *ram, ROM *rom, Disk *disk);
    ~Emulator32bit();

    Emulator32bit(const Emulator32bit &) = delete;
    Emulator32bit &operator=(const Emulator32bit &) = delete;

    /// Why an instruction did not complete.
    enum class InterruptType : U8
    {
        BAD_REG,       ///< A register that does not exist.
        BAD_INSTR,     ///< An instruction that does not exist, or is not implemented.
        HALT_INSTR,    ///< hlt: the program is done, this is not a fault.
        FAILED_ASSERT, ///< An assertion of the program (emu_assert...) did not hold.
        PROGRAM_ERROR, ///< The program reported an error and stopped (emu_error).
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

    const std::unique_ptr<SystemBus> system_bus;

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

    /// Prints the registers and the flags to the output.
    void print();

    /// Where the output of the program (the emulator calls that print or log) and of print ()
    /// goes. std::cout by default.
    void set_output(std::ostream &out);

    /// Where the errors that a program reports go. std::cerr by default.
    void set_error_output(std::ostream &err);

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

    /// The value of a register. xzr is always 0, and so is a register that does not exist.
    inline word read_reg(Register reg)
    {
        return m_x[register_to_U8(reg)];
    }

    inline word read_reg(U8 reg)
    {
        return LIKELY(reg < kNumReg) ? m_x[reg] : 0;
    }

    /// Writes a register. Writes to xzr, and to a register that does not exist, are discarded.
    inline void write_reg(Register reg, word val)
    {
        write_reg(register_to_U8(reg), val);
    }

    inline void write_reg(U8 reg, word val)
    {
        if (LIKELY(reg < kNumReg && reg != register_to_U8(Register::XZR)))
        {
            m_x[reg] = val;
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
    ///                     The value of xzr is 0, it is never written.
    ///
    word m_x[kNumReg];

    /// @brief              Program counter.
    word m_pc;

    /// @brief              Program state. Bits 0-3 are NZCV flags. Rest are TODO
    word m_pstate;

    using InstructionFunction = void (Emulator32bit::*)(word);
    InstructionFunction m_instruction_handler[kMaxInstructions];

    void fill_out_instructions();

    /// Fetches the instruction at the PC (a virtual address). Faults when the PC is not word
    /// aligned, not mapped, not executable or not in RAM.
    word fetch_instruction();

    /// The address that a load or a store (format M) accesses, and what the base register is after
    /// it. The base register is only written once the access happened, with write_back_base ().
    struct MemOperand
    {
        word address;
        U8 base;          ///< Register that the address is relative to.
        word base_after;
        bool writes_back; ///< Pre and post indexed accesses.
    };

    MemOperand decode_mem_operand(word instr);
    void write_back_base(const MemOperand &operand);

    inline void execute(word instr)
    {
        (this->*m_instruction_handler[bitfield_unsigned(instr, 26, 6)])(instr);
    }

    // Instruction handling. For every row of AEMU_OPCODES (opcodes.h): the handler _<name> and
    // the opcode constant _op_<name>.
#define AEMU_DECLARE_OPCODE(name, opcode)                                                          \
  private:                                                                                         \
    void _##name(word instr);                                                                      \
                                                                                                   \
  public:                                                                                          \
    static constexpr word _op_##name = opcode;

    AEMU_OPCODES(AEMU_DECLARE_OPCODE)
#undef AEMU_DECLARE_OPCODE

    // Operations of the special instruction, and the fallback of unused opcodes.
    void _hlt(const word instr);
    /// Handler of every opcode that is not an instruction. Faults with BAD_INSTR.
    void _bad_opcode(const word instr);
    void _nop(const word instr);
    void _msr(const word instr);
    void _mrs(const word instr);
    void _tlbi(const word instr);
    void _atomic(const word instr);

    enum class AtomicOperation
    {
        SWP,
        LDADD,
        LDCLR,
        LDSET
    };

    void _atomic_rmw(const word instr, AtomicOperation operation);

    std::ostream *m_out = &std::cout;
    std::ostream *m_err = &std::cerr;

    // Software interrupt handling.
    U8 _emu_register_arg(word reg_id);
    word _emu_read_value(word mem_addr, U8 size, bool little_endian);
    std::string _emu_read_string(word address);
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
