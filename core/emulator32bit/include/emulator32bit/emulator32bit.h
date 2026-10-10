#pragma once

#include "emulator32bit/alu.h"
#include "emulator32bit/disk.h"
#include "emulator32bit/encoding.h"
#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/memory.h"
#include "emulator32bit/memory_port.h"
#include "emulator32bit/opcodes.h"
#include "emulator32bit/symbols.h"
#include "emulator32bit/system_bus.h"
#include "emulator32bit/virtual_memory.h"

#include <deque>
#include <iosfwd>
#include <memory>
#include <optional>
#include <set>
#include <string>
#include <vector>

/// A software simulated 32 bit processor, modeled off of the ARM architecture with many
/// simplifications.
class Emulator32bit : public Encoding
{
  public:
    /// Makes an emulator with a RAM, a ROM that holds the given image and a MockDisk.
    ///
    /// @param ram_npages the number of pages of RAM
    /// @param ram_start_page the page number of the first page of the RAM
    /// @param rom_data the image of the ROM, of `rom_npages` pages
    /// @param rom_npages the number of pages of ROM
    /// @param rom_start_page the page number of the first page of the ROM
    Emulator32bit(word ram_npages, word ram_start_page, const byte rom_data[], word rom_npages,
                  word rom_start_page);

    /// The emulator owns the memories and the disk.
    ///
    /// @param ram the RAM, which holds the frames that the MMU pages in
    /// @param rom the ROM
    /// @param disk the disk that backs the swapping
    Emulator32bit(std::unique_ptr<RAM> ram, std::unique_ptr<ROM> rom, std::unique_ptr<Disk> disk);
    ~Emulator32bit();

    Emulator32bit(const Emulator32bit &) = delete;
    Emulator32bit &operator=(const Emulator32bit &) = delete;

    /// Why an instruction did not complete.
    enum class InterruptType : U8
    {
        /// A register that does not exist. Becomes an undefined instruction exception.
        BAD_REG,
        /// An instruction that does not exist, is not implemented or is not allowed. Becomes an
        /// undefined instruction exception.
        BAD_INSTR,
        /// hlt: the program is done, this is not a fault.
        HALT_INSTR,
        /// An assertion of the program (emu_assert...) did not hold.
        FAILED_ASSERT,
        /// The program reported an error and stopped (emu_error).
        PROGRAM_ERROR,
        /// brk with a debugger attached: run () stops with Status::BREAKPOINT.
        BREAK_INSTR,
        /// An exception was raised again before any instruction ran.
        DOUBLE_FAULT,
        /// A word or half-word load or store (not ldur, stur) at an address that is not a multiple
        /// of its size, becomes a data abort with the alignment syndrome. A pc that is not a
        /// multiple of 4 is this too, and becomes an instruction abort.
        MISALIGNED,
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

    /// @return the name of the class ("data abort"), or "?" for a number that is not one
    static const char *exception_class_name(ExceptionClass cls);

    /// @return how many exceptions have been taken and not returned from (ERET), at most 255:
    ///         non zero in a handler. ELR, ESR and FAR describe the last one.
    U8 exception_depth() const
    {
        return m_exception_depth;
    }

    /// ISS of an undefined instruction exception (and of the Exception that causes it).
    /// An opcode that is not assigned.
    static constexpr word kUndefinedIss_opcode = 0;
    /// An extended op or encoding that is not assigned.
    static constexpr word kUndefinedIss_ext_op = 1;
    /// A privileged instruction in user mode.
    static constexpr word kUndefinedIss_privileged = 2;
    /// An instruction that is assigned but not available: a `swi` that has no vector table to go
    /// to and no emulator calls to serve it.
    static constexpr word kUndefinedIss_unimplemented = 3;
    /// A system register that does not exist.
    static constexpr word kUndefinedIss_sysreg = 4;
    /// A floating point instruction that names a double in a register that cannot start a pair
    /// (x29, sp, xzr).
    static constexpr word kUndefinedIss_fp_operand = 5;

    /// ISS of an instruction or data abort: one fault type, and for a data abort `kAbortIss_write`
    /// is or'd in when the access was a write.
    /// Not mapped.
    static constexpr word kAbortIss_translation = 1;
    /// Write to read-only, execute of data, kernel only.
    static constexpr word kAbortIss_permission = 2;
    /// A pc that is not a multiple of 4, or a data access (ldr, str, ldrh, strh) that is not
    /// aligned to its size.
    static constexpr word kAbortIss_alignment = 3;
    /// No memory at the physical address.
    static constexpr word kAbortIss_bus = 4;
    /// Bit 3: the access was a write.
    static constexpr word kAbortIss_write = 1 << 3;

    /// What stops an instruction from completing: a fault of the program, a `hlt` or a `brk`.
    class Exception : public std::exception
    {
      private:
        InterruptType type;
        std::string message;
        word iss;

      public:
        /// `iss` is the syndrome of the exception that this becomes when the CPU takes it. For
        /// BAD_INSTR and BAD_REG that is an undefined instruction exception, so pass a
        /// kUndefinedIss_* value; the other types ignore it.
        ///
        /// @param type why the instruction did not complete
        /// @param msg the message that `what` returns
        /// @param iss the syndrome of the exception
        Exception(InterruptType type, const std::string &msg, word iss = 0);
        const char *what() const noexcept override;

        /// @return why the instruction did not complete
        InterruptType get_type() const noexcept;

        /// @return the syndrome of the exception that this becomes
        word get_iss() const noexcept;
    };

    /// The machine: the physical memory and the devices, the MMU that translates the addresses of
    /// a program onto them, and the port that the CPU reaches memory through. In this order, each
    /// one is made from the ones before it.
    const std::unique_ptr<SystemBus> system_bus;
    const std::unique_ptr<VirtualMemory> mmu;
    MemoryPort memory;

    /// Why a call to run () stopped.
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

    /// Exit code of the emulator CLI interface.
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
    ///
    /// @param pc the virtual address of the instruction
    void add_breakpoint(word pc);

    /// @param pc the virtual address of a breakpoint
    /// @return whether there was a breakpoint at the address
    bool remove_breakpoint(word pc);

    /// Removes every breakpoint.
    void clear_breakpoints();

    /// @return the addresses of the breakpoints
    const std::set<word> &breakpoints() const;

    /// What a watchpoint reacts to.
    enum class WatchKind : U8
    {
        READ = 1,
        WRITE = 2,
        ACCESS = 3, ///< either
    };

    /// How the value of an access is compared with the number of a watchpoint (unsigned).
    enum class WatchCompare : U8
    {
        NONE, ///< every access counts
        EQ,
        NE,
        LT,
        LE,
        GT,
        GE,
    };

    /// A range of virtual addresses (the address the instruction computed, before translation), or
    /// of physical ones.
    struct Watchpoint
    {
        word address;
        word length; ///< at least 1
        WatchKind kind;
        bool physical = false; ///< `address` is where the access ends up after the translation
        WatchCompare compare = WatchCompare::NONE; ///< only an access whose value ...
        word compare_value = 0;                    ///< ... compares like this with this one counts
    };

    /// @param compare how to compare
    /// @param value what an access loaded or stored (the bytes of it, zero extended)
    /// @param against the number to compare with
    /// @return whether the comparison holds
    static bool watch_compare_holds(WatchCompare compare, word value, word against);

    /// Makes run () stop, with Status::BREAKPOINT, right after an instruction that read or wrote
    /// (per `kind`) any byte of [address, address + length). The pc is then the next instruction.
    /// Only loads, stores and atomics count (not instruction fetches, the page table walker, or
    /// the debugger's own reads); an atomic is a read and a write. An access that faults does not
    /// count. A watchpoint on an address that is already watched (the same kind of address)
    /// replaces it.
    ///
    /// With a `compare`, only an access whose value (what it loaded or stored) compares like that
    /// with `compare_value` counts: `==` 5 stops when a 5 is written. The message of a write says
    /// what was in the memory before.
    ///
    /// @param address the address of the first byte to watch
    /// @param length the number of bytes, at least 1
    /// @param kind which accesses stop the run
    /// @param physical whether the address is physical (a watch that follows the page tables, or
    ///        the same memory under another virtual address)
    /// @param compare the condition on the value of the access
    /// @param compare_value the number the value is compared with
    void add_watchpoint(word address, word length = 1, WatchKind kind = WatchKind::WRITE,
                        bool physical = false, WatchCompare compare = WatchCompare::NONE,
                        word compare_value = 0);

    /// Removes the watchpoint that starts at the address.
    ///
    /// @param address the address the watchpoint starts at
    /// @param physical whether it is a watchpoint on physical addresses
    /// @return whether there was one
    bool remove_watchpoint(word address, bool physical = false);

    /// Removes every watchpoint.
    void clear_watchpoints();

    /// @return the watchpoints
    const std::vector<Watchpoint> &watchpoints() const;

    /// What the last watchpoint that stopped a run was hit by.
    struct WatchHit
    {
        word address;   ///< the first byte of the access
        WatchKind kind; ///< the kind of the watchpoint
    };

    /// @return the last hit, valid after a run that ended in Status::BREAKPOINT because of a
    ///         watchpoint
    const WatchHit &last_watch_hit() const
    {
        return m_last_watch_hit;
    }

    /// Makes run () stop, with Status::BREAKPOINT, once an instruction (or exception entry) has
    /// changed the register (0-29 or sp, the one of the current mode). With `value`, only a change
    /// to that value stops. A write of the value it already has is not a change. The pc is then at
    /// the next instruction. Costs nothing while no register is watched. One watch per register.
    ///
    /// @param reg the number of the register
    /// @param value the value that stops the run, or nothing for any change
    void add_register_watch(U8 reg, std::optional<word> value = std::nullopt);

    /// @param reg the number of a watched register
    /// @return whether the register was watched
    bool remove_register_watch(U8 reg);

    /// Removes every register watch.
    void clear_register_watches();

    struct RegisterWatch
    {
        U8 reg;
        std::optional<word> value;
        word last; ///< the value when the watch was last checked
    };

    /// @return the register watches
    const std::vector<RegisterWatch> &register_watches() const;

    /// Writes a line per instruction to the stream before the next run (), or nothing for nullptr:
    /// `pc <symbol>: instruction | what changed`, or the reason when the instruction faulted.
    ///
    /// @param out the stream, which must outlive its use
    void set_trace(std::ostream *out);

    /// Keeps the last `count` instructions that were executed (0 turns it off and clears it).
    /// Includes the instruction that faulted, which is the newest one.
    ///
    /// @param count the number of instructions to keep
    void set_history_size(size_t count);

    /// @return the number of instructions that the history keeps
    size_t history_size() const;

    /// @return the last instructions that were executed, the oldest first
    std::vector<ExecutedInstruction> history() const;

    /// Names for the trace.
    ///
    /// @param symbols the symbols, which must outlive their use, or nullptr for none
    void set_symbols(const SymbolMap *symbols);

    /// Runs the program from the pc.
    ///
    /// @param instructions the number of instructions to run, or 0 to run until something stops
    ///     the program (a `hlt`, a fault or a breakpoint)
    /// @return why execution stopped and how many instructions ran
    RunResult run(U64 instructions);

    /// Prints the registers and the flags to the output.
    void print();

    /// Where the output of the program (the emulator calls that print or log) and of print ()
    /// goes. std::cout by default.
    ///
    /// @param out the stream, which must outlive its use
    void set_output(std::ostream &out);

    /// Where the errors that a program reports go. std::cerr by default.
    ///
    /// @param err the stream, which must outlive its use
    void set_error_output(std::ostream &err);

    /// Resets the processor state: registers, PSTATE (kernel mode, IRQs masked), the system
    /// registers and the bus.
    void reset();

    /// @param pc the virtual address of the next instruction
    inline void set_pc(word pc)
    {
        m_pc = pc;
    }

    /// @return the virtual address of the next instruction
    inline word get_pc()
    {
        return m_pc;
    }

    /// The value of a register. xzr is always 0, and so is a register that does not exist.
    ///
    /// @param reg the register
    /// @return the value of the register
    inline word read_reg(Register reg)
    {
        return m_x[register_to_U8(reg)];
    }

    /// Same as above, for the number of a register.
    inline word read_reg(U8 reg)
    {
        return LIKELY(reg < kNumReg) ? m_x[reg] : 0;
    }

    /// Writes a register. Writes to xzr, and to a register that does not exist, are discarded.
    ///
    /// @param reg the register
    /// @param val the value to store
    inline void write_reg(Register reg, word val)
    {
        write_reg(register_to_U8(reg), val);
    }

    /// Same as above, for the number of a register.
    inline void write_reg(U8 reg, word val)
    {
        if (LIKELY(reg < kNumReg && reg != register_to_U8(Register::XZR)))
        {
            m_x[reg] = val;
        }
    }

    /// Sets or clears one bit of PSTATE.
    ///
    /// @param flag the bit number, e.g. kZFlagBit
    /// @param value the new value of the bit
    inline void set_flag(U8 flag, bool value)
    {
        m_pstate = set_bit(m_pstate, flag, value);
    }

    /// @param flag the bit number of PSTATE, e.g. kZFlagBit
    /// @return whether the bit is set
    inline bool get_flag(U8 flag)
    {
        return test_bit(m_pstate, flag);
    }

    /// Sets the NZCV flags of PSTATE and leaves the other bits.
    ///
    /// @param N the negative flag
    /// @param Z the zero flag
    /// @param C the carry flag
    /// @param V the overflow flag
    inline void set_NZCV(bool N, bool Z, bool C, bool V)
    {
        m_pstate = (m_pstate & ~word(0xF)) | (word(N) << kNFlagBit) | (word(Z) << kZFlagBit)
                   | (word(C) << kCFlagBit) | (word(V) << kVFlagBit);
    }

    /// Same as above, with the flags in a struct.
    inline void set_NZCV(NZCVFlags flags)
    {
        set_NZCV(flags.n, flags.z, flags.c, flags.v);
    }

    /// @return the NZCV flags of PSTATE
    inline NZCVFlags get_NZCV()
    {
        return {
            .n = test_bit<kNFlagBit>(m_pstate),
            .z = test_bit<kZFlagBit>(m_pstate),
            .c = test_bit<kCFlagBit>(m_pstate),
            .v = test_bit<kVFlagBit>(m_pstate),
        };
    }

    /// @return PSTATE: NZCV, the mode (kUserModeBit) and the IRQ mask (kIrqMaskBit)
    inline word get_pstate() const
    {
        return m_pstate;
    }

    /// @return whether the CPU is in user mode, otherwise it is in kernel mode
    inline bool user_mode() const
    {
        return test_bit<kUserModeBit>(m_pstate);
    }

    /// The system registers the way the kernel sees them (kSysregId_*), without the checks that
    /// MRS and MSR make of the mode. Throws Exception (BAD_REG) for a register that does not exist.
    /// Writing PSTATE changes the mode (and which stack pointer is in use) like ERET does.
    ///
    /// @param id the number of the system register (kSysregId_*)
    /// @return the value of the register
    word read_sysreg(U8 id) const;

    /// @param id the number of the system register (kSysregId_*)
    /// @param value the value to store, of which only the bits that the register has are kept
    void write_sysreg(U8 id, word value);

    /// @param name the name of a system register, in any case
    /// @return the number of the register, or nothing if there is none with the name
    static std::optional<U8> sysreg_id(const std::string &name);

    /// Whether `swi 1`, the emulator calls (print, assert, ...), is allowed. On by default. If it
    /// is off, the instruction is an undefined instruction.
    ///
    /// @param enabled whether the emulator calls are allowed
    void set_semihosting(bool enabled);

    /// With a debugger attached `brk` stops run () (Status::BREAKPOINT, the pc is the next
    /// instruction) instead of raising the breakpoint exception. Off by default.
    ///
    /// @param stops whether `brk` stops the run
    void set_brk_stops(bool stops);

    // TODO: determine if fp registers (fpcr, fpsr) are needed.

  private:
    /// General purpose registers x0-x29, sp and xzr. x29 is the link register. xzr is always 0 and
    /// is never written.
    word m_x[kNumReg];

    /// Program counter.
    word m_pc;

    /// Program state. Bits 0-3 are the NZCV flags, bit 4 the mode, bit 5 masks IRQs.
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

    /// Exceptions taken and not returned from, see exception_depth ().
    U8 m_exception_depth = 0;

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
        bool unaligned;   ///< ADDR_UNALIGNED: the address need not be aligned to the access size.
    };

    /// Works out the address of a load or store and what the base register becomes, without
    /// changing any register.
    ///
    /// @tparam kHasUnalignedForm whether the instruction has the ADDR_UNALIGNED form (word and
    ///         half-word accesses). For a byte access that mode is an undefined instruction.
    /// @param instr the instruction, of format M
    /// @return the address and the new base
    template<bool kHasUnalignedForm>
    MemOperand decode_mem_operand(word instr);

    /// Writes the new base register of a pre or post indexed access, after the access succeeded.
    ///
    /// @param operand what decode_mem_operand returned for the instruction
    void write_back_base(const MemOperand &operand);

    /// Runs the handler of the opcode of `instr` (a switch generated from AEMU_OPCODES).
    void execute(word instr);

    /// execute () that writes the trace line of the instruction:
    /// `0x00000010 <main+0x4>: add x0, x1, 4 ; x0=0x1->0x5 NZCV=nzcv->nzCv`, with the reason in
    /// place of the changes when the instruction does not complete. A taken branch shows
    /// `pc=target`.
    ///
    /// @param instr the instruction to execute
    void execute_traced(word instr);

    /// Raises an exception: saves the state, goes to kernel mode and jumps to the vector. A
    /// double fault is thrown instead if no instruction completed since the last one.
    ///
    /// @param cls the class of the exception, which is also the vector
    /// @param iss the syndrome
    /// @param far the address of an abort
    /// @param elr where ERET returns to
    void enter_exception(ExceptionClass cls, word iss, word far, word elr);

    /// If the exception that stopped an instruction (`fetching`: while fetching it, else `instr`
    /// is the instruction that was executing) is one the CPU raises, and a vector table is
    /// installed, raises it and returns true. Otherwise false and the caller reports the fault.
    ///
    /// @param error what stopped the instruction
    /// @param fetching whether it happened while fetching the instruction
    /// @param instr the instruction that was executing, if not fetching
    /// @return whether an exception was raised
    bool deliver_exception(const std::exception &error, bool fetching, word instr = 0);

    /// The address that the load, store or atomic `instr` accesses, for FAR when it faults; 0 for
    /// any other instruction. It is worked out from the registers, which is right because such an
    /// instruction changes none until its access succeeded, and it costs nothing in the
    /// instructions that do not fault.
    ///
    /// @param instr the instruction
    /// @return the address of the access
    word data_address_of(word instr);

    /// Switches to user or kernel mode, and to the stack pointer of that mode.
    ///
    /// @param user true for user mode
    void set_user_mode(bool user);

    /// Throws the undefined instruction exception of a privileged instruction in user mode.
    void require_kernel();

    std::set<word> m_breakpoints;
    std::vector<Watchpoint> m_watchpoints;
    std::vector<RegisterWatch> m_register_watches;
    /// The first watchpoint the running instruction hit, as the message run () reports.
    std::string m_watch_hit;
    WatchHit m_last_watch_hit{0, WatchKind::WRITE};

    /// Called by the memory instructions after a successful access of `length` bytes. `value` is
    /// what was loaded or stored.
    ///
    /// @param address the virtual address of the access
    /// @param length the number of bytes accessed
    /// @param write whether the access was a store
    /// @param value what was loaded or stored
    /// @param old_value what a store replaced
    void watch_access(word address, word length, bool write, word value, word old_value = 0);

    /// @return the bytes at the virtual address as they are before a store (0 if they cannot be
    ///         read), for watch_access
    word watch_old_value(word address, word length);

    /// What the store that is about to run replaces, read by execute_hooked () for the message of
    /// a watchpoint (the store handlers themselves are as they were without watchpoints).
    word m_watch_old = 0;

    /// execute () for a run where something looks at each instruction: the old value of a store
    /// for the watchpoints, and the trace.
    void execute_hooked(word instr);
    std::ostream *m_trace = nullptr;
    const SymbolMap *m_symbols = nullptr;
    size_t m_history_size = 0;
    std::deque<ExecutedInstruction> m_history;

    // The floating point registers are last, so that adding them did not move the members that
    // every instruction reads (see the benchmarks in CLAUDE.md).
    /// The instruction that run () is executing, for the exception it may raise.
    word m_instr_in_flight = 0;
    word m_fpcr = 0; ///< Floating point control: the rounding mode, bits 1-0 (fpu::kRound*).
    word m_fpsr = 0; ///< Floating point status: the cumulative exception flags, bits 4-0.

    // Instruction handling. For every row of AEMU_OPCODES (opcodes.h): the handler _<name> and
    // the opcode constant _op_<name>.
#define AEMU_DECLARE_OPCODE(name, opcode)                                                          \
  private:                                                                                         \
    void _##name(word instr);

    AEMU_OPCODES(AEMU_DECLARE_OPCODE)
#undef AEMU_DECLARE_OPCODE

    // Operations of the special instruction, and the fallback of unused opcodes. Each handler
    // executes the instruction `instr`.
    void _hlt(const word instr);
    /// Handler of every opcode that is not an instruction. Faults with BAD_INSTR.
    void _bad_opcode(const word instr);
    void _nop(const word instr);
    /// PSTATE is the one register user code may use, and only for the flags.
    void _msr(const word instr);
    void _mrs(const word instr);
    void _tlbi(const word instr);
    /// PC = ELR and PSTATE = SPSR. The mode comes back with SPSR, and with it the stack pointer.
    void _eret(const word instr);
    /// Waits until an interrupt is pending (masked or not, the next instruction then runs, and
    /// takes the interrupt if it is not masked). Nothing else happens while waiting, so the only
    /// things that can raise one are the timer and a block device command: time jumps to the
    /// first of them. With nothing to wait for the program ends like it does with hlt, so that it
    /// does not hang.
    void _wfi(const word instr);
    void _brk(const word instr);
    /// `op xd, xn`: sign and zero extension of the low byte or half-word, count of leading zeros,
    /// and the byte reversals. No flags.
    void _unary(const word instr);
    void _atomic(const word instr);

    enum class AtomicOperation
    {
        SWP,
        LDADD,
        LDCLR,
        LDSET
    };

    /// The read-modify-write atomics: loads the old value into xt and stores the new one.
    ///
    /// @param instr the instruction
    /// @param operation how the new value is made from the old one and xm
    void _atomic_rmw(const word instr, AtomicOperation operation);

    /// std::cout and std::cerr (emulator32bit.cpp), so that this header needs no <iostream>.
    static std::ostream *default_out();
    static std::ostream *default_err();

    std::ostream *m_out = default_out();
    std::ostream *m_err = default_err();

    // Software interrupt handling: the emulator calls of `swi 1` (software_interrupt.cpp).
    U8 _emu_register_arg(word reg_id);
    /// The value of the size bytes at the address, the most significant byte being the last one in
    /// memory for a little endian value and the first one for a big endian value.
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
    /// TODO: raise interrupt so kernel can handle
    void _emu_err(word err);

  public:
    // Helpers to assemble instructions. Each one returns the instruction word.

    /// @return `hlt`
    static word asm_hlt();

    /// @return `nop`
    static word asm_nop();

    /// Encodes `msr sysreg, xn` or `msr sysreg, imm16`.
    ///
    /// @param sysreg the system register number (kSysregId_*)
    /// @param imm whether the source is an immediate, otherwise a register
    /// @param xn_or_imm16 the register number, or the 16 bit immediate
    static word asm_msr(U8 sysreg, bool imm, word xn_or_imm16);

    /// Encodes `mrs xn, sysreg`.
    ///
    /// @param xn the destination register
    /// @param sysreg the system register number (kSysregId_*)
    static word asm_mrs(U8 xn, U8 sysreg);

    /// Encodes `tlbi` (all translations) or `tlbi xt` (the page that holds the address in xt).
    ///
    /// @param xt the register that holds the address
    /// @param isxt whether the form with a register is used
    /// @param imm16 reserved, 0
    static word asm_tlbi(U8 xt, bool isxt, word imm16);

    /// @return `eret`
    static word asm_eret();

    /// @return `wfi`
    static word asm_wfi();

    /// Encodes `brk imm22`.
    ///
    /// @param imm22 the 22 bit number of the breakpoint
    static word asm_brk(word imm22);

    /// Encodes one of the unary operations `op xd, xn` (sxtb, clz, rev, ...).
    ///
    /// @param op the operation (kUnaryId_*)
    /// @param xd the destination register
    /// @param xn the source register
    static word asm_unary(word op, word xd, word xn);

    /// Encodes a unary floating point instruction or a conversion (`fabs`, `fsqrt`, `fcvt`...,
    /// format F1).
    ///
    /// @param fn the function (fpu::kUnaryFn_*)
    /// @param dbl the precision bit, see fpu::unary_dest_is_pair
    /// @param xd the destination register (the first one of a pair for a double)
    /// @param xn the source register
    static word asm_fop1(U8 fn, bool dbl, int xd, int xn);

    /// Encodes a binary floating point instruction `fadd`, ... `xd, xn, xm` (format F2).
    ///
    /// @param fn the function (fpu::kBinaryFn_*)
    /// @param dbl whether the operands and the result are doubles
    static word asm_fop2(U8 fn, bool dbl, int xd, int xn, int xm);

    /// Encodes a fused multiply-add `fmadd`, ... `xd, xn, xm, xa` (format F4).
    ///
    /// @param fn the function (fpu::kFmaFn_*)
    /// @param dbl whether the operands and the result are doubles
    static word asm_fop3(U8 fn, bool dbl, int xd, int xn, int xm, int xa);

    /// Encodes `fcmp` or `fcmpe xn, xm` (format F3).
    ///
    /// @param signaling `fcmpe`: a quiet NaN raises Invalid too
    static word asm_fcmp(bool dbl, bool signaling, int xn, int xm);

    /// Encodes `csel`, `csinc`, `csinv` or `csneg xd, xn, xm, cond`.
    ///
    /// @param variant which one (kCselId_*)
    /// @param cond the condition
    /// @param xd the destination register
    /// @param xn the register that is chosen if the condition holds
    /// @param xm the register that the other value is made from
    static word asm_csel(word variant, ConditionCode cond, word xd, word xn, word xm);

    /// Encodes an atomic read-modify-write.
    ///
    /// @param xt the register that receives the old value
    /// @param xn the register that holds the address
    /// @param xm the register that holds the operand
    /// @param width the access width (kAtomicWidth_*)
    /// @param atop the operation (kAtomicId_*)
    static word asm_atomic(word xt, word xn, word xm, U8 width, U8 atop);

    /// Constructs instructions of format O with an imm14 operand
    ///
    /// @param opcode 6 bit identifier of a format O instruction
    /// @param s whether condition flags are set
    /// @param xd 5 bit destination register identifier
    /// @param xn 5 bit operand register identifier
    /// @param imm14 14 bit immediate value
    /// @return instruction word
    static word asm_format_o(U8 opcode, bool s, int xd, int xn, int imm14);
    /// Constructs instructions of format O with an arg operand
    ///
    /// @param opcode 6 bit identifier of a format O instruction
    /// @param s whether condition flags are set
    /// @param xd 5 bit destination register identifier
    /// @param xn 5 bit operand register identifier
    /// @param xm 5 bit operand register identifier
    /// @param shift shift type to be applied on the value in the xm register
    /// @param imm5 shift amount
    /// @return instruction word
    static word asm_format_o(U8 opcode, bool s, int xd, int xn, int xm, ShiftType shift, int imm5);

    /// Constructs instructions of format O1 (the shifts): the amount is a register or an imm5
    ///
    /// @param type which shift it is (lsl, lsr, asr or ror); they are one opcode, `_op_shift`
    /// @param xd 5 bit destination register identifier
    /// @param xn 5 bit register identifier of the value to shift
    /// @param imm whether the amount is `imm5`, otherwise it is the register `xm`
    /// @param xm 5 bit register identifier of the amount
    /// @param imm5 the amount
    /// @param s whether condition flags are set
    static word asm_format_o1(ShiftType type, int xd, int xn, bool imm, int xm, int imm5,
                              bool s = false);

    /// Constructs instructions of format O2 (the long multiplies), with two destination registers
    ///
    /// @param is_signed smull, otherwise umull; they are one opcode, `_op_mull`
    /// @param s whether condition flags are set
    /// @param xlo 5 bit register identifier that receives the low word
    /// @param xhi 5 bit register identifier that receives the high word
    /// @param xn 5 bit identifier of the first operand register
    /// @param xm 5 bit identifier of the second operand register
    static word asm_format_o2(bool is_signed, bool s, int xlo, int xhi, int xn, int xm);

    /// Constructs instructions of format O3 with an imm19 operand (`mov`, `mvn`)
    ///
    /// @param opcode 6 bit identifier of the instruction
    /// @param s whether condition flags are set
    /// @param xd 5 bit destination register identifier
    /// @param imm19 19 bit immediate value
    static word asm_format_o3(U8 opcode, bool s, int xd, int imm19);

    /// Constructs instructions of format O3 with a register and an imm14 operand
    ///
    /// @param opcode 6 bit identifier of the instruction
    /// @param s whether condition flags are set
    /// @param xd 5 bit destination register identifier
    /// @param xn 5 bit operand register identifier
    /// @param imm14 14 bit immediate value
    static word asm_format_o3(U8 opcode, bool s, int xd, int xn, int imm14);

    /// Constructs a load or store (format M) with a register offset
    ///
    /// @param opcode 6 bit identifier of the instruction
    /// @param sign whether the loaded value is sign extended
    /// @param xt 5 bit register identifier that is loaded or stored
    /// @param xn 5 bit base register identifier
    /// @param xm 5 bit offset register identifier
    /// @param shift shift type to be applied on the value in the xm register
    /// @param imm5 shift amount
    /// @param adr how the offset is applied to the base
    static word asm_format_m(U8 opcode, bool sign, int xt, int xn, int xm, ShiftType shift,
                             int imm5, AddrType adr);

    /// Constructs a load or store (format M) with an immediate offset
    ///
    /// @param opcode 6 bit identifier of the instruction
    /// @param sign whether the loaded value is sign extended
    /// @param xt 5 bit register identifier that is loaded or stored
    /// @param xn 5 bit base register identifier
    /// @param simm12 12 bit signed offset
    /// @param adr how the offset is applied to the base
    static word asm_format_m(U8 opcode, bool sign, int xt, int xn, int simm12, AddrType adr);

    /// Constructs instructions of format M1, a register and an imm20 operand (`adrp`, `adr`)
    ///
    /// @param opcode 6 bit identifier of the instruction
    /// @param xd 5 bit destination register identifier
    /// @param imm20 20 bit immediate value
    static word asm_format_m1(U8 opcode, int xd, int imm20);

    /// Constructs an instruction of format B1: a branch to a pc relative offset (also `swi`)
    ///
    /// @param opcode 6 bit identifier of the instruction
    /// @param cond the condition under which the branch is taken
    /// @param simm22 22 bit signed offset, in instructions
    static word asm_format_b1(U8 opcode, ConditionCode cond, sword simm22);

    /// Constructs a branch (format B2) to the address in a register (`bx`, or `blx` with `link`)
    ///
    /// @param cond the condition under which the branch is taken
    /// @param xd 5 bit register identifier that holds the target
    /// @param link whether x29 gets the address of the next instruction (`blx`)
    static word asm_format_b2(ConditionCode cond, int xd, bool link = false);

    /// @param instr an instruction word
    /// @return the instruction as assembly text
    static std::string disassemble_instr(word instr);
};
