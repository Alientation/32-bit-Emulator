#pragma once

#include "emulator32bit/alu.h"
#include "emulator32bit/disk.h"
#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/memory.h"
#include "emulator32bit/memory_port.h"
#include "emulator32bit/opcodes.h"
#include "emulator32bit/symbols.h"
#include "emulator32bit/system_bus.h"
#include "emulator32bit/virtual_memory.h"

#include <deque>
#include <iostream>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

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
    Emulator32bit(std::unique_ptr<RAM> ram, std::unique_ptr<ROM> rom, std::unique_ptr<Disk> disk);
    ~Emulator32bit();

    Emulator32bit(const Emulator32bit &) = delete;
    Emulator32bit &operator=(const Emulator32bit &) = delete;

    /// Why an instruction did not complete.
    enum class InterruptType : U8
    {
        BAD_REG,    ///< A register that does not exist. Becomes an undefined instruction exception.
        BAD_INSTR,  ///< An instruction that does not exist, is not implemented or is not allowed.
                    ///< Becomes an undefined instruction exception.
        HALT_INSTR, ///< hlt: the program is done, this is not a fault.
        FAILED_ASSERT, ///< An assertion of the program (emu_assert...) did not hold.
        PROGRAM_ERROR, ///< The program reported an error and stopped (emu_error).
        BREAK_INSTR,   ///< brk with a debugger attached: run () stops with Status::BREAKPOINT.
        DOUBLE_FAULT,  ///< An exception was raised again before any instruction ran.
    };

    /// The exceptions the CPU raises, see docs/exceptions.md. The value is the number of the vector
    /// (the handler is at VBAR + 16 * vector) and the EC field of ESR.
    enum class ExceptionClass : U8
    {
        UNDEFINED_INSTRUCTION = 1,
        SUPERVISOR_CALL = 2,
        INSTRUCTION_ABORT = 3,
        DATA_ABORT = 4,
        BREAKPOINT = 5,
        IRQ = 6,
    };

    /// ISS of an undefined instruction exception (and of the Exception that causes it).
    static constexpr word kUndefinedIss_opcode = 0; ///< An opcode that is not assigned.
    static constexpr word kUndefinedIss_ext_op =
        1; ///< An extended op or encoding that is not assigned.
    static constexpr word kUndefinedIss_privileged = 2; ///< A privileged instruction in user mode.
    static constexpr word kUndefinedIss_unimplemented = 3;
    static constexpr word kUndefinedIss_sysreg = 4;     ///< A system register that does not exist.

    /// ISS of an instruction or data abort: the fault type, and for a data abort bit 3 is set for
    /// a write.
    static constexpr word kAbortIss_translation = 1; ///< Not mapped.
    static constexpr word kAbortIss_permission =
        2; ///< Write to read-only, execute of data, kernel only.
    static constexpr word kAbortIss_alignment = 3; ///< A pc that is not a multiple of 4.
    static constexpr word kAbortIss_bus = 4;       ///< No memory at the physical address.
    static constexpr word kAbortIss_write = 1 << 3;

    class Exception : public std::exception
    {
      private:
        InterruptType type;
        std::string message;
        word iss;

      public:
        /// `iss` is the syndrome of the exception that this becomes when the CPU takes it (an
        /// undefined instruction exception for BAD_INSTR and BAD_REG), kUndefinedIss_*.
        Exception(InterruptType type, const std::string &msg, word iss = 0);
        const char *what() const noexcept override;
        InterruptType get_type() const noexcept;
        word get_iss() const noexcept;
    };

    enum class AddrType : U8
    {
        ADDR_OFFSET,
        ADDR_PRE_INC,
        ADDR_POST_INC
    };

    /// The machine: the physical memory and the devices, the MMU that translates the addresses of
    /// a program onto them, and the port that the CPU reaches memory through. In this order, each
    /// one is made from the ones before it.
    const std::unique_ptr<SystemBus> system_bus;
    const std::unique_ptr<VirtualMemory> mmu;
    MemoryPort memory;

    /// @brief              Why a call to run () stopped.
    struct RunResult
    {
        enum class Status : U8
        {
            HALTED,        ///< Executed a HLT instruction.
            LIMIT_REACHED, ///< Executed the requested number of instructions.
            FAULT,         ///< Any other emulator or system bus exception.
            BREAKPOINT,    ///< The PC is at a breakpoint, the instruction there did not run.
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
        EXIT_BREAKPOINT = 4,
    };

    /// An instruction that was executed (or tried to), for the history.
    struct ExecutedInstruction
    {
        word pc;
        word instruction;
    };

    // Debugging. Single stepping is run (1). None of this costs anything while it is not used.

    /// Makes run () stop, with Status::BREAKPOINT, before the instruction at the address (a
    /// virtual address, like the PC) executes. The first instruction of a run () is never stopped
    /// at, so a run () that starts at a breakpoint goes on.
    void add_breakpoint(word pc);
    bool remove_breakpoint(word pc);
    void clear_breakpoints();
    const std::set<word> &breakpoints() const;

    /// What a watchpoint reacts to.
    enum class WatchKind : U8
    {
        READ = 1,
        WRITE = 2,
        ACCESS = 3, ///< either
    };

    /// A range of virtual addresses (the address the instruction computed, before translation).
    struct Watchpoint
    {
        word address;
        word length; ///< at least 1
        WatchKind kind;
    };

    /// Makes run () stop, with Status::BREAKPOINT, right after an instruction that read or wrote
    /// (per `kind`) any byte of [address, address + length). The pc is then the next instruction.
    /// Only loads, stores and atomics count (not instruction fetches, the page table walker, or
    /// the debugger's own reads); an atomic is a read and a write. An access that faults does not
    /// count. A watchpoint on an address that is already watched replaces it.
    void add_watchpoint(word address, word length = 1, WatchKind kind = WatchKind::WRITE);
    /// Removes the watchpoint that starts at the address.
    bool remove_watchpoint(word address);
    void clear_watchpoints();
    const std::vector<Watchpoint> &watchpoints() const;

    /// Makes run () stop, with Status::BREAKPOINT, once an instruction (or exception entry) has
    /// changed the register (0-29 or sp, the one of the current mode). With `value`, only a change
    /// to that value stops. A write of the value it already has is not a change. The pc is then at
    /// the next instruction. Costs nothing while no register is watched. One watch per register.
    void add_register_watch(U8 reg, std::optional<word> value = std::nullopt);
    bool remove_register_watch(U8 reg);
    void clear_register_watches();

    struct RegisterWatch
    {
        U8 reg;
        std::optional<word> value;
        word last; ///< the value when the watch was last checked
    };

    const std::vector<RegisterWatch> &register_watches() const;

    /// Writes a line per instruction to the stream before the next run (), or nothing for nullptr:
    /// `pc <symbol>: instruction | what changed`, or the reason when the instruction faulted. The
    /// stream must outlive its use.
    void set_trace(std::ostream *out);

    /// Keeps the last `count` instructions that were executed (0 turns it off and clears it).
    /// Includes the instruction that faulted, which is the newest one.
    void set_history_size(size_t count);
    size_t history_size() const;
    std::vector<ExecutedInstruction> history() const;

    /// Names for the trace, the symbols must outlive their use. nullptr for none.
    void set_symbols(const SymbolMap *symbols);

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

    /// PSTATE: NZCV, the mode (kUserModeBit) and the IRQ mask (kIrqMaskBit).
    inline word get_pstate() const
    {
        return m_pstate;
    }

    inline bool user_mode() const
    {
        return test_bit(m_pstate, kUserModeBit);
    }

    /// The system registers the way the kernel sees them (kSysregId_*), without the checks that
    /// MRS and MSR make of the mode. Throws Exception (BAD_REG) for a register that does not exist.
    /// Writing PSTATE changes the mode (and which stack pointer is in use) like ERET does.
    word read_sysreg(U8 id) const;
    void write_sysreg(U8 id, word value);

    /// The name of a system register as written in assembly (lower case), nullptr if there is no
    /// register with the number.
    static const char *sysreg_name(U8 id);

    /// The number of a system register from its name, in any case.
    static std::optional<U8> sysreg_id(const std::string &name);

    /// Whether `swi 1`, the emulator calls (print, assert, ...), is allowed. On by default. If it
    /// is off, the instruction is an undefined instruction.
    void set_semihosting(bool enabled);

    /// With a debugger attached `brk` stops run () (Status::BREAKPOINT, the pc is the next
    /// instruction) instead of raising the breakpoint exception. Off by default.
    void set_brk_stops(bool stops);

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

    /// @brief              Program state. Bits 0-3 are the NZCV flags, bit 4 the mode, bit 5 masks
    ///                     IRQs.
    word m_pstate;

    /// The stack pointer of the other mode: the user one while in kernel mode (what the USP system
    /// register reads and writes) and the kernel one while in user mode. m_x[sp] is the one in use.
    word m_sp_other = 0;

    // System registers, see docs/exceptions.md.
    word m_elr = 0;   ///< Where ERET goes back to.
    word m_spsr = 0;  ///< PSTATE of the code that was interrupted.
    word m_esr = 0;   ///< Class (bits 26-31) and syndrome of the last exception.
    word m_far = 0;   ///< Address of the last abort.
    word m_vbar = 0;  ///< Vector table. 0: there is none and nothing is raised, see below.
    word m_ptbr = 0;  ///< Physical address of the first level page table (a page).
    word m_sctlr = 0; ///< System control: bit 0 turns on translation by the page tables.

    /// Set by what writes the pc itself (ERET, taking an exception), run () then does not add 4.
    bool m_pc_written = false;

    /// Whether an instruction completed since an exception was taken. An exception that is raised
    /// when none has is a double fault.
    bool m_retired_since_entry = true;

    bool m_semihosting = true;
    bool m_brk_stops = false;

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

    /// Runs the handler of the opcode of `instr` (a switch generated from AEMU_OPCODES).
    void execute(word instr);

    /// execute () that writes the trace line of the instruction.
    void execute_traced(word instr);

    /// Raises an exception: saves the state, goes to kernel mode and jumps to the vector. A
    /// double fault is thrown instead if no instruction completed since the last one.
    /// `elr` is where ERET returns to, `far` the address of an abort.
    void enter_exception(ExceptionClass cls, word iss, word far, word elr);

    /// If the exception that stopped an instruction (`fetching`: while fetching it, else `instr`
    /// is the instruction that was executing) is one the CPU raises, and a vector table is
    /// installed, raises it and returns true. Otherwise false and the caller reports the fault.
    bool deliver_exception(const std::exception &error, bool fetching, word instr = 0);

    /// The address that the load, store or atomic `instr` accesses, for FAR when it faults; 0 for
    /// any other instruction. It is worked out from the registers, which is right because such an
    /// instruction changes none until its access succeeded, and it costs nothing in the
    /// instructions that do not fault.
    word data_address_of(word instr);

    /// Switches to user or kernel mode, and to the stack pointer of that mode.
    void set_user_mode(bool user);

    /// Throws the undefined instruction exception of a privileged instruction in user mode.
    void require_kernel();

    std::set<word> m_breakpoints;
    std::vector<Watchpoint> m_watchpoints;
    std::vector<RegisterWatch> m_register_watches;
    /// The first watchpoint the running instruction hit, as the message run () reports.
    std::string m_watch_hit;

    /// Called by the memory instructions after a successful access of `length` bytes. `value` is
    /// what was loaded or stored.
    void watch_access(word address, word length, bool write, word value);
    std::ostream *m_trace = nullptr;
    const SymbolMap *m_symbols = nullptr;
    size_t m_history_size = 0;
    std::deque<ExecutedInstruction> m_history;

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
    void _eret(const word instr);
    void _wfi(const word instr);
    void _brk(const word instr);
    void _unary(const word instr);
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
    static word asm_eret();
    static word asm_wfi();
    static word asm_brk(word imm22);
    static word asm_unary(word op, word xd, word xn);
    static word asm_csel(word variant, ConditionCode cond, word xd, word xn, word xm);
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

    /// The immediate of `swi` that is an emulator call, see _swi. Every other one is a system call
    /// of the operating system.
    static constexpr word kSwiSemihosting = 1;
};
