#pragma once

#include "emulator32bit/alu.h"
#include "emulator32bit/disk.h"
#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/encoding.h"
#include "emulator32bit/memory.h"
#include "emulator32bit/memory_port.h"
#include "emulator32bit/opcodes.h"
#include "emulator32bit/symbols.h"
#include "emulator32bit/system_bus.h"
#include "emulator32bit/virtual_memory.h"
#include "util/bitarray.h"

#include <iosfwd>
#include <memory>
#include <optional>
#include <set>
#include <string>

/// The most watchpoints that can be set at once (`add_watchpoint`).
inline constexpr size_t kMaxWatchpoints = 16;

/// The most registers that can be watched at once (`add_register_watch`).
inline constexpr size_t kMaxRegisterWatches = 8;

/// The deepest nesting of exceptions that `exception_depth ()` counts. Deeper ones are not counted.
inline constexpr U8 kMaxExceptionDepth = 255;

/// The most instructions that the history can keep (`set_history_size`).
inline constexpr size_t kMaxHistory = 256;

/// A software simulated 32 bit processor, modeled off of the ARM architecture with many
/// simplifications.
class Emulator32bit
{
  public:
    /// Makes an emulator with a RAM, a ROM that holds the given image and a MockDisk.
    ///
    /// @param[in] ram_npages The number of pages of RAM.
    /// @param[in] ram_start_page The page number of the first page of the RAM.
    /// @param[in] rom_data The image of the ROM, of `rom_npages` pages.
    /// @param[in] rom_npages The number of pages of ROM.
    /// @param[in] rom_start_page The page number of the first page of the ROM.
    Emulator32bit(word ram_npages, word ram_start_page, const byte rom_data[], word rom_npages,
                  word rom_start_page);

    /// The emulator owns the memories and the disk.
    ///
    /// @param[in] ram The RAM, which holds the frames that the MMU pages in.
    /// @param[in] rom The ROM.
    /// @param[in] disk The disk that backs the swapping.
    Emulator32bit(std::unique_ptr<RAM> ram, std::unique_ptr<ROM> rom, std::unique_ptr<Disk> disk);
    ~Emulator32bit();

    Emulator32bit(const Emulator32bit &) = delete;
    Emulator32bit &operator=(const Emulator32bit &) = delete;

    /// Why an instruction did not complete.
    enum class TrapType : U8
    {
        /// A register that does not exist. Becomes an undefined instruction exception.
        BAD_REG,
        /// An instruction that does not exist, is not implemented or is not allowed. Becomes an
        /// undefined instruction exception.
        BAD_INSTR,
        /// `hlt`: the program is done, this is not a fault.
        HALT_INSTR,
        /// An assertion of the program (emu_assert...) did not hold.
        FAILED_ASSERT,
        /// The program reported an error and stopped (emu_error).
        PROGRAM_ERROR,
        /// `brk` with a debugger attached: `run ()` stops with Status::BREAKPOINT.
        BREAK_INSTR,
        /// An exception was raised again before any instruction ran.
        DOUBLE_FAULT,
        /// A word or half-word load or store (not ldur, stur) at an address that is not a multiple
        /// of its size, becomes a data abort with the alignment syndrome. A pc that is not a
        /// multiple of 4 is this too, and becomes an instruction abort.
        MISALIGNED,
    };

    /// The exception class the CPU raises, see docs/exceptions.md. The value is the number of
    /// the vector (the handler is at VBAR + kVectorSize * vector) and the EC field of ESR.
    enum class ExceptionClass : U8
    {
        UNDEFINED_INSTRUCTION = 1,
        SUPERVISOR_CALL = 2,
        INSTRUCTION_ABORT = 3,
        DATA_ABORT = 4,
        BREAKPOINT = 5,
        IRQ = 6,
    };

    /// @param[in] cls The class.
    /// @return The name of the class ("data abort"), or "?" for a number that is not one.
    static const char *exception_class_name(ExceptionClass cls);

    /// @return How many exceptions have been taken and not returned from (ERET), at most
    ///         kMaxExceptionDepth: non zero in a handler. ELR, ESR and FAR describe the last one.
    U8 exception_depth() const
    {
        return m_exception_depth;
    }

    // The ISS (instruction specific syndrome) of an undefined instruction exception..

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

    // The ISS (syndrome) of an instruction or data abort: one fault type, and for a data abort
    // `kAbortIss_write` is or'd in when the access was a write.

    /// The address is not mapped.
    static constexpr word kAbortIss_translation = 1;
    /// A write to read-only memory, an execute of data, or a user access to kernel memory.
    static constexpr word kAbortIss_permission = 2;
    /// A pc that is not a multiple of 4, or a data access (ldr, str, ldrh, strh) that is not
    /// aligned to its size.
    static constexpr word kAbortIss_alignment = 3;
    /// No memory at the physical address.
    static constexpr word kAbortIss_bus = 4;
    /// Bit 3 of the ISS, set when the access was a write.
    static constexpr word kAbortIss_write = 1 << 3;

    /// What stops an instruction from completing: a fault of the program, a `hlt` or a `brk`.
    class Trap : public std::exception
    {
      private:
        TrapType type;
        std::string message;
        word iss;

      public:
        /// `iss` is the instruction specific syndrome of the exception. BAD_INSTR and BAD_REG are
        /// an undefined instruction exception, so pass a kUndefinedIss_* value. For other
        /// exception types, leave unset.
        ///
        /// @param[in] type Why the instruction did not complete.
        /// @param[in] msg The message that `what` returns.
        /// @param[in] iss The syndrome of the exception.
        Trap(TrapType type, const std::string &msg, word iss = 0);
        const char *what() const noexcept override;

        /// @return Why the instruction did not complete.
        TrapType get_type() const noexcept;

        /// @return The syndrome of the exception that this becomes.
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
            HALTED,        /// Executed a HLT instruction.
            LIMIT_REACHED, /// Executed the requested number of instructions.
            FAULT,         /// Any other emulator or system bus exception.
            BREAKPOINT,    /// The PC is at a breakpoint, the instruction there did not run.
            POWER_OFF,     /// The program turned the machine off (the system controller).
        };

        Status status;
        U64 instructions_ran;
        std::string message; /// Message of the trap or fault, empty if the limit was reached.
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

    /// An instruction that was executed or attempted, for the history.
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
    /// @param[in] pc The virtual address of the instruction.
    void add_breakpoint(word pc);

    /// @param[in] pc The virtual address of a breakpoint.
    /// @return Whether there was a breakpoint at the address.
    bool remove_breakpoint(word pc);

    /// Removes every breakpoint.
    void clear_breakpoints();

    /// @return The addresses of the breakpoints.
    const std::set<word> &breakpoints() const;

    /// What a watchpoint reacts to.
    enum class WatchKind : U8
    {
        READ = 1,
        WRITE = 2,
        ACCESS = 3, /// Either.
    };

    /// How the value of an access is compared with watchpoint's value.
    enum class WatchCompare : U8
    {
        NONE, /// Every access counts.
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
        word length;           /// At least 1.
        WatchKind kind;
        bool physical = false; /// `address` is where the access ends up after the translation.
        WatchCompare compare = WatchCompare::NONE; /// Only an access whose value ...
        word compare_value = 0;                    /// ... compares like this with this one counts.
    };

    /// @param[in] compare How to compare.
    /// @param[in] value What an access loaded or stored (the bytes of it, zero extended).
    /// @param[in] against The number to compare with.
    /// @return Whether the comparison holds.
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
    /// @param[in] address The address of the first byte to watch.
    /// @param[in] length The number of bytes, at least 1.
    /// @param[in] kind Which accesses stop the run.
    /// @param[in] physical Whether the address is physical (a watch that follows the page tables,
    ///        or the same memory under another virtual address).
    /// @param[in] compare The condition on the value of the access.
    /// @param[in] compare_value The number the value is compared with.
    /// @return `false` if there are kMaxWatchpoints already and this is not a replacement.
    bool add_watchpoint(word address, word length = 1, WatchKind kind = WatchKind::WRITE,
                        bool physical = false, WatchCompare compare = WatchCompare::NONE,
                        word compare_value = 0);

    /// Removes the watchpoint that starts at the address.
    ///
    /// @param[in] address The address the watchpoint starts at.
    /// @param[in] physical Whether it is a watchpoint on physical addresses.
    /// @return Whether there was one.
    bool remove_watchpoint(word address, bool physical = false);

    /// Removes every watchpoint.
    void clear_watchpoints();

    /// @return The number of watchpoints, at most kMaxWatchpoints.
    size_t watchpoint_count() const
    {
        return m_watchpoints.count;
    }

    /// @param[in] i The index, below watchpoint_count (it changes when one is removed).
    /// @return The watchpoint.
    Watchpoint watchpoint(size_t i) const;

    /// What the last watchpoint that stopped a run was hit by.
    struct WatchHit
    {
        word address;   /// The first byte of the access.
        WatchKind kind; /// The kind of the watchpoint.
    };

    /// @return The last hit, valid after a run that ended in Status::BREAKPOINT because of a
    ///         watchpoint.
    const WatchHit &last_watch_hit() const
    {
        return m_watchpoints.last_hit;
    }

    /// Makes run () stop, with Status::BREAKPOINT, once an instruction (or exception entry) has
    /// changed the register (0-29 or sp, the one of the current mode). With `value`, only a change
    /// to that value stops. A write of the value it already has is not a change. The pc is then at
    /// the next instruction. Costs nothing while no register is watched. One watch per register,
    /// and a watch on a register that is watched already replaces it.
    ///
    /// @param[in] reg The number of the register.
    /// @return `false` if kMaxRegisterWatches registers are watched already and this is not one.
    bool add_register_watch(U8 reg);

    /// A watch that only stops when the register changes to `value`.
    ///
    /// @param[in] reg The number of the register.
    /// @param[in] value The value that stops the run.
    /// @return `false` if kMaxRegisterWatches registers are watched already and this is not one.
    bool add_register_watch(U8 reg, word value);

    /// @param[in] reg The number of a watched register.
    /// @return Whether the register was watched.
    bool remove_register_watch(U8 reg);

    /// Removes every register watch.
    void clear_register_watches();

    /// A watch on a register, see add_register_watch ().
    struct RegisterWatch
    {
        U8 reg;
        bool has_value; /// Only a change to `value` counts.
        word value;
        word last;      /// The value when the watch was last checked.
    };

    /// @return The number of register watches, at most kMaxRegisterWatches.
    size_t register_watch_count() const
    {
        return m_regwatches.count;
    }

    /// @param[in] i The index, below register_watch_count (it changes when one is removed).
    /// @return The register watch.
    RegisterWatch register_watch(size_t i) const;

    /// Writes a line per instruction to the stream before the next run (), or nothing for nullptr:
    /// `pc <symbol>: instruction | what changed`, or the reason when the instruction faulted.
    ///
    /// @param[in] out The stream, which must outlive its use.
    void set_trace(std::ostream *out);

    /// Keeps the last `count` instructions that were executed (0 turns it off and clears it).
    /// Includes the instruction that faulted, which is the newest one. A smaller count than the
    /// history has keeps the newest ones.
    ///
    /// @param[in] count The number of instructions to keep, at most kMaxHistory.
    void set_history_size(size_t count);

    /// @return The number of instructions that the history keeps.
    size_t history_size() const;

    /// Room for the most instructions that the history can keep.
    using History = ExecutedInstruction[kMaxHistory];

    /// @param[out] out Receives the last instructions that were executed, the oldest first.
    /// @return The number of entries of `out` that were written, at most history_size ().
    size_t history(History &out) const;

    /// Names for the trace.
    ///
    /// @param[in] symbols The symbols, which must outlive their use, or nullptr for none.
    void set_symbols(const SymbolMap *symbols);

    /// Runs the program from the pc.
    ///
    /// @param[in] instructions The number of instructions to run, or 0 to run until something stops
    ///     the program (a `hlt`, a fault or a breakpoint).
    /// @return Why execution stopped and how many instructions ran.
    RunResult run(U64 instructions);

    /// Prints the registers and the flags to the output.
    void print();

    /// Where the output of the program (the emulator calls that print or log) and of print ()
    /// goes. std::cout by default.
    ///
    /// @param[in] out The stream, which must outlive its use.
    void set_output(std::ostream &out);

    /// Where the errors that a program reports go. std::cerr by default.
    ///
    /// @param[in] err The stream, which must outlive its use.
    void set_error_output(std::ostream &err);

    /// Resets the processor state: registers, PSTATE (kernel mode, IRQs masked), the system
    /// registers and the bus. The pc becomes the boot address.
    void reset();

    /// Where a reset starts the machine: what a reset by the system controller runs next.
    ///
    /// @param[in] pc The address, 0 until it is set.
    void set_boot_pc(const word pc)
    {
        m_boot_pc = pc;
    }

    /// @param[in] pc The virtual address of the next instruction.
    inline void set_pc(const word pc)
    {
        m_pc = pc;
    }

    /// @return The virtual address of the next instruction.
    inline word get_pc() const
    {
        return m_pc;
    }

    /// The value of a register. xzr is always 0, and so is a register that does not exist.
    ///
    /// @param[in] reg The register.
    /// @return The value of the register.
    inline word read_reg(const Register reg) const
    {
        return m_x[register_to_U8(reg)];
    }

    /// Same as above, for the number of a register.
    ///
    /// @param[in] reg The number of the register.
    /// @return The value, 0 for a number that is not a register.
    inline word read_reg(const U8 reg) const
    {
        return LIKELY(reg < kNumReg) ? m_x[reg] : 0;
    }

    /// Writes a register. Writes to xzr, and to a register that does not exist, are discarded.
    ///
    /// @param[in] reg The register.
    /// @param[in] val The value to store.
    inline void write_reg(const Register reg, const word val)
    {
        write_reg(register_to_U8(reg), val);
    }

    /// Same as above, for the number of a register.
    ///
    /// @param[in] reg The number of the register.
    /// @param[in] val The value to store.
    inline void write_reg(const U8 reg, const word val)
    {
        if (LIKELY(reg < kNumReg && reg != register_to_U8(Register::XZR)))
        {
            m_x[reg] = val;
        }
    }

    /// Sets or clears one bit of PSTATE.
    ///
    /// @param[in] flag The bit number, e.g. kZFlagBit.
    /// @param[in] value The new value of the bit.
    inline void set_flag(const U8 flag, const bool value)
    {
        m_pstate = set_bit(m_pstate, flag, value);
    }

    /// @param[in] flag The bit number of PSTATE, e.g. kZFlagBit.
    /// @return Whether the bit is set.
    inline bool get_flag(const U8 flag) const
    {
        return test_bit(m_pstate, flag);
    }

    /// Sets the NZCV flags of PSTATE and leaves the other bits.
    ///
    /// @param[in] N The negative flag.
    /// @param[in] Z The zero flag.
    /// @param[in] C The carry flag.
    /// @param[in] V The overflow flag.
    inline void set_NZCV(const bool N, const bool Z, const bool C, const bool V)
    {
        m_pstate = (m_pstate & ~word(0xF)) | (word(N) << kNFlagBit) | (word(Z) << kZFlagBit)
                   | (word(C) << kCFlagBit) | (word(V) << kVFlagBit);
    }

    /// Same as above, with the flags in a struct.
    ///
    /// @param[in] flags The new flags.
    inline void set_NZCV(NZCVFlags flags)
    {
        set_NZCV(flags.n, flags.z, flags.c, flags.v);
    }

    /// @return The NZCV flags of PSTATE.
    inline NZCVFlags get_NZCV() const
    {
        return {
            .n = test_bit<kNFlagBit>(m_pstate),
            .z = test_bit<kZFlagBit>(m_pstate),
            .c = test_bit<kCFlagBit>(m_pstate),
            .v = test_bit<kVFlagBit>(m_pstate),
        };
    }

    /// @return PSTATE: NZCV, the mode (kUserModeBit) and the IRQ mask (kIrqMaskBit).
    inline word get_pstate() const
    {
        return m_pstate;
    }

    /// @return Whether the CPU is in user mode, otherwise it is in kernel mode.
    inline bool user_mode() const
    {
        return test_bit<kUserModeBit>(m_pstate);
    }

    /// The system registers the way the kernel sees them (kSysregId_*), without the checks that
    /// MRS and MSR make of the mode. Throws Trap (BAD_REG) for a register that does not exist.
    /// Writing PSTATE changes the mode (and which stack pointer is in use) like ERET does.
    ///
    /// @param[in] id The number of the system register (kSysregId_*).
    /// @return The value of the register.
    word read_sysreg(U8 id) const;

    /// @param[in] id The number of the system register (kSysregId_*).
    /// @param[in] value The value to store, of which only the bits that the register has are kept.
    void write_sysreg(U8 id, word value);

    /// @param[in] name The name of a system register, in any case.
    /// @return The number of the register, or nothing if there is none with the name.
    static std::optional<U8> sysreg_id(const std::string &name);

    /// Whether `swi 1`, the emulator calls (print, assert, ...), is allowed. On by default. If it
    /// is off, the instruction is an undefined instruction.
    ///
    /// @param[in] enabled Whether the emulator calls are allowed.
    void set_semihosting(bool enabled);

    /// With a debugger attached `brk` stops run () (Status::BREAKPOINT, the pc is the next
    /// instruction) instead of raising the breakpoint exception. Off by default.
    ///
    /// @param[in] stops Whether `brk` stops the run.
    void set_brk_stops(bool stops);

  private:
    /// General purpose registers x0-x29, sp and xzr. x28 is the frame pointer and x29 the link
    /// register. xzr is always 0 and is never written.
    word m_x[kNumReg];

    /// Program counter.
    word m_pc;

    /// Program state. Bits 0-3 are the NZCV flags, bit 4 the mode, bit 5 masks IRQs.
    word m_pstate;

    /// The stack pointer of the other mode: the user one while in kernel mode (what the USP system
    /// register reads and writes) and the kernel one while in user mode. m_x[sp] is the one in use.
    word m_sp_other = 0;

    /// System registers, see docs/exceptions.md.
    word m_elr = 0;   /// Where ERET returns back to.
    word m_spsr = 0;  /// PSTATE of the code that was interrupted.
    word m_esr = 0;   /// Class (bits 26-31) and syndrome of the last exception (bits 0-25).
    word m_far = 0;   /// Address of the last abort.
    word m_vbar = 0;  /// Vector table address. 0: Nothing is raised, see deliver_exception().
    word m_ptbr = 0;  /// Physical address of the first level page table (a page).
    word m_sctlr = 0; /// System control: bit 0 turns on translation by the page tables.

    /// Set by a handler that writes the pc itself (a taken branch, ERET, taking an exception).
    /// run () then does not add 4.
    bool m_pc_written = false;

    /// Set when an exception is taken, cleared when an instruction completes. An exception that is
    /// raised while it is set (before any instruction of the handler ran) is a double fault.
    bool m_just_took_exception = false;

    /// Exceptions taken and not returned from, see exception_depth ().
    U8 m_exception_depth = 0;

    bool m_semihosting = true;
    bool m_brk_stops = false;

    /// Fetches the instruction at the PC (a virtual address). Faults when the PC is not word
    /// aligned, not mapped, not executable or not in RAM.
    ///
    /// @return The instruction.
    word fetch_instruction();

    /// The address that a load or a store (format M) accesses, and what the base register is after
    /// it. The base register is only written once the access happened, with write_back_base ().
    struct MemOperand
    {
        word address;
        U8 base;          /// Register that the address is relative to.
        word base_after;
        bool writes_back; /// Pre and post indexed accesses.
        bool unaligned;   /// ADDR_UNALIGNED: the address need not be aligned to the access size.
    };

    /// Works out the address of a load or store and what the base register becomes, without
    /// changing any register.
    ///
    /// @param[in] instr The instruction, of format M.
    /// @param[in] has_unaligned_form Whether the instruction has the ADDR_UNALIGNED form (word and
    ///        half-word accesses). For a byte access that mode is an undefined instruction.
    /// @return The address and the new base.
    MemOperand decode_mem_operand(word instr, bool has_unaligned_form);

    /// Writes the new base register of a pre or post indexed access, after the access succeeded.
    ///
    /// @param[in] operand What decode_mem_operand returned for the instruction.
    void write_back_base(const MemOperand &operand);

    /// Runs the handler of the opcode of `instr` (a switch generated from AEMU_OPCODES).
    /// An opcode that is not listed faults with BAD_INSTR.
    ///
    /// @param[in] instr The instruction.
    void execute(word instr);

    /// `execute ()` that writes the trace line of the instruction:
    /// `0x00000010 <main+0x4>: add x0, x1, 4 ; x0=0x1->0x5 NZCV=nzcv->nzCv`, with the reason in
    /// place of the changes when the instruction does not complete. A taken branch shows
    /// `pc=target`.
    ///
    /// @param[in] instr The instruction to execute.
    void execute_traced(word instr);

    /// Raises an exception: saves the state, goes to kernel mode and jumps to the vector. A
    /// double fault is thrown instead if no instruction completed since an exception was taken.
    ///
    /// @param[in] cls The class of the exception, which is also the vector.
    /// @param[in] iss The syndrome.
    /// @param[in] far The address of an abort.
    /// @param[in] elr Where ERET returns to.
    void raise_exception(ExceptionClass cls, word iss, word far, word elr);

    /// Raises the exception that an error stands for, if the CPU raises one for it and a vector
    /// table is installed. If not, the caller reports the error as a fault.
    ///
    /// @param[in] error What stopped the instruction.
    /// @param[in] fetching Whether it happened while fetching the instruction.
    /// @param[in] instr The instruction that was executing, if not fetching.
    /// @return Whether an exception was raised.
    bool deliver_exception(const std::exception &error, bool fetching, word instr = 0);

    /// The address that the load, store or atomic `instr` accesses, for FAR when it faults; 0 for
    /// any other instruction. It is worked out from the registers, which is right because such an
    /// instruction changes none until its access succeeded, and it costs nothing in the
    /// instructions that do not fault.
    ///
    /// @param[in] instr The instruction.
    /// @return The address of the access.
    word data_address_of(word instr);

    /// Switches to user or kernel mode, and to the stack pointer of that mode.
    ///
    /// @param[in] user `true` for user mode.
    void set_user_mode(bool user);

    /// Throws the undefined instruction exception of a privileged instruction in user mode.
    void require_kernel() const;

    /// Called by the memory instructions after a successful access of `length` bytes. `value` is
    /// what was loaded or stored.
    ///
    /// @param[in] address The virtual address of the access.
    /// @param[in] length The number of bytes accessed.
    /// @param[in] write Whether the access was a store.
    /// @param[in] value What was loaded or stored.
    /// @param[in] old_value What a store replaced.
    void watch_access(word address, word length, bool write, word value, word old_value = 0);

    /// @param[in] address The virtual address of the store.
    /// @param[in] length The number of bytes.
    /// @return The bytes at the virtual address as they are before a store (0 if they cannot be
    ///         read), for watch_access.
    word watch_old_value(word address, word length);

    /// `execute ()` for a run where something looks at each instruction: the old value of a store
    /// for the watchpoints, and the trace.
    ///
    /// @param[in] instr The instruction.
    void execute_hooked(word instr);

    /// Puts the instruction that is about to execute in the history, over the oldest one if full.
    ///
    /// @param[in] instr The instruction.
    void record_history(word instr);

    /// The index that find_watchpoint () and find_register_watch () return for no match.
    static constexpr size_t kNotFound = ~U64(0);

    /// @param[in] address The address.
    /// @param[in] physical Whether the watchpoint is on a physical address.
    /// @return The index of the watchpoint at the address, or kNotFound if there is none.
    size_t find_watchpoint(word address, bool physical) const;

    /// Looks at the watched registers between two instructions, which covers a completed
    /// instruction and an exception entry.
    ///
    /// @param[out] result The result of the run, which becomes a stop at a breakpoint if a watched
    ///        register changed since it was last looked at.
    /// @return Whether a register watch stopped the run.
    bool register_watch_hit(RunResult &result);

    /// Serves the request of the system controller between two instructions: a reset starts the
    /// machine again at its boot address, a power off ends the run.
    ///
    /// @param[out] result The result of the run, which becomes a power off.
    /// @return Whether the run ends.
    bool serve_system_request(RunResult &result);

    /// @param[in] reg The register.
    /// @return The index of the watch on the register, or kNotFound if there is none.
    size_t find_register_watch(U8 reg) const;

    /// Adds a register watch or replaces the one on `reg`. The overloads of add_register_watch ()
    /// call it.
    ///
    /// @param[in] reg The register.
    /// @param[in] has_value Whether the watch only stops on `value`.
    /// @param[in] value The value to stop on.
    /// @return `false` if there is no room for another watch.
    bool set_register_watch(U8 reg, bool has_value, word value);

    // These members are last, so that adding them did not move the members that every instruction
    // reads (see the benchmark notes in docs/internals.md).

    /// The instruction that run () is executing, for the exception it may raise.
    word m_instr_in_flight = 0;
    word m_fpcr = 0;    /// Floating point control: the rounding mode, bits 1-0 (fpu::kRound*).
    word m_fpsr = 0;    /// Floating point status: the cumulative exception flags, bits 4-0.
    word m_boot_pc = 0; /// The pc after a reset.

    // The tables of the debugger are after the members that every instruction reads. Each is a
    // struct of arrays: the fields of entry i are at index i of each array, and [0, count) are the
    // entries. A removed entry is replaced by the last one.

    /// The watchpoints, see Watchpoint.
    struct WatchpointTable
    {
        size_t count = 0;
        word address[kMaxWatchpoints];
        word length[kMaxWatchpoints];
        WatchKind kind[kMaxWatchpoints];
        WatchCompare compare[kMaxWatchpoints];
        word compare_value[kMaxWatchpoints];
        BitArray<kMaxWatchpoints> physical;

        /// The message of the first watchpoint that the running instruction hit, as run () reports.
        std::string hit_message;
        /// The last hit, see last_watch_hit ().
        WatchHit last_hit{.address = 0, .kind = WatchKind::WRITE};
        /// What the store that is about to run replaces, read by execute_hooked () for the message
        /// of a watchpoint (the store handlers themselves are as they were without watchpoints).
        word old_value = 0;
    } m_watchpoints;

    /// The register watches, see RegisterWatch.
    struct RegisterWatchTable
    {
        size_t count = 0;
        U8 reg[kMaxRegisterWatches];
        word value[kMaxRegisterWatches];
        word last[kMaxRegisterWatches];
        BitArray<kMaxRegisterWatches> has_value;
    } m_regwatches;

    /// The history is a ring buffer of `size` entries (at most kMaxHistory), of which the newest
    /// `count` are valid, and the next one goes to `head`.
    struct HistoryRing
    {
        size_t size = 0;
        size_t count = 0;
        size_t head = 0;
        word pc[kMaxHistory];
        word instruction[kMaxHistory];
    } m_history;

    /// The addresses that run () stops before.
    std::set<word> m_breakpoints;

    /// Where the trace of each instruction goes, and the symbols that name its addresses.
    struct Trace
    {
        std::ostream *out = nullptr;
        const SymbolMap *symbols = nullptr;
    } m_trace;

    // Instruction handling. For every row of AEMU_OPCODES (opcodes.h): the handler _<name> and
    // the opcode constant _op_<name>.
#define AEMU_DECLARE_OPCODE(name, opcode)                                                          \
  private:                                                                                         \
    void _##name(word instr);

    AEMU_OPCODES(AEMU_DECLARE_OPCODE)
#undef AEMU_DECLARE_OPCODE

    // The operations of the special instruction (opcode 0), and the fallback for an opcode that is
    // not listed. Each handler executes the instruction `instr`.
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
    /// Waits until an interrupt is pending, masked or not. The next instruction then runs, which
    /// takes the interrupt if it is not masked. Nothing else happens while waiting, so only the
    /// timer and a block device command can raise one: time jumps ahead to the first of their
    /// events. If there is nothing to wait for, the program ends as it does with hlt, so that it
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
    /// @param[in] instr The instruction.
    /// @param[in] operation How the new value is made from the old one and xm.
    void _atomic_rmw(const word instr, AtomicOperation operation);

    /// std::cout and std::cerr (emulator32bit.cpp), so that this header needs no <iostream>.
    ///
    /// @return The stream.
    static std::ostream *default_out();
    static std::ostream *default_err();

    std::ostream *m_out = default_out();
    std::ostream *m_err = default_err();

    // Software interrupt handling: the emulator calls of `swi 1` (software_interrupt.cpp).
    U8 _emu_register_arg(word reg_id);
    /// The value of the size bytes at the address, the most significant byte being the last one in
    /// memory for a little endian value and the first one for a big endian value.
    ///
    /// @param[in] mem_addr The address of the value.
    /// @param[in] size The number of bytes.
    /// @param[in] little_endian Whether the value is stored little endian.
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
    /// Prints the message and stops the run.
    /// TODO: raise an exception so that the kernel can handle it.
    ///
    /// @param[in] err The address of the message.
    void _emu_err(word err);

  public:
    // Helpers to assemble instructions. Each one returns the instruction word.

    /// @return `hlt`.
    static word asm_hlt();

    /// @return `nop`.
    static word asm_nop();

    /// Encodes `msr sysreg, xn` or `msr sysreg, imm16`.
    ///
    /// @param[in] sysreg The system register number (kSysregId_*).
    /// @param[in] imm Whether the source is an immediate, otherwise a register.
    /// @param[in] xn_or_imm16 The register number, or the 16 bit immediate.
    /// @return The instruction word.
    static word asm_msr(U8 sysreg, bool imm, word xn_or_imm16);

    /// Encodes `mrs xn, sysreg`.
    ///
    /// @param[in] xn The destination register.
    /// @param[in] sysreg The system register number (kSysregId_*).
    /// @return The instruction word.
    static word asm_mrs(U8 xn, U8 sysreg);

    /// Encodes `tlbi` (all translations) or `tlbi xt` (the page that holds the address in xt).
    ///
    /// @param[in] xt The register that holds the address.
    /// @param[in] isxt Whether the form with a register is used.
    /// @param[in] imm16 Reserved, 0.
    /// @return The instruction word.
    static word asm_tlbi(U8 xt, bool isxt, word imm16);

    /// @return `eret`.
    static word asm_eret();

    /// @return `wfi`.
    static word asm_wfi();

    /// Encodes `brk imm22`.
    ///
    /// @param[in] imm22 The 22 bit number of the breakpoint.
    /// @return The instruction word.
    static word asm_brk(word imm22);

    /// Encodes one of the unary operations `op xd, xn` (sxtb, clz, rev, ...).
    ///
    /// @param[in] op The operation (kUnaryId_*).
    /// @param[in] xd The destination register.
    /// @param[in] xn The source register.
    /// @return The instruction word.
    static word asm_unary(word op, word xd, word xn);

    /// Encodes a unary floating point instruction or a conversion (`fabs`, `fsqrt`, `fcvt`...,
    /// format F1).
    ///
    /// @param[in] fn The function (fpu::kUnaryFn_*).
    /// @param[in] dbl The precision bit, see fpu::unary_dest_is_pair.
    /// @param[in] xd The destination register (the first one of a pair for a double).
    /// @param[in] xn The source register.
    /// @return The instruction word.
    static word asm_fop1(U8 fn, bool dbl, U8 xd, U8 xn);

    /// Encodes a binary floating point instruction `fadd`, ... `xd, xn, xm` (format F2).
    ///
    /// @param[in] fn The function (fpu::kBinaryFn_*).
    /// @param[in] dbl Whether the operands and the result are doubles.
    /// @param[in] xd The destination register (the first one of a pair for a double).
    /// @param[in] xn The first operand.
    /// @param[in] xm The second operand.
    /// @return The instruction word.
    static word asm_fop2(U8 fn, bool dbl, U8 xd, U8 xn, U8 xm);

    /// Encodes a fused multiply-add `fmadd`, ... `xd, xn, xm, xa` (format F4).
    ///
    /// @param[in] fn The function (fpu::kFmaFn_*).
    /// @param[in] dbl Whether the operands and the result are doubles.
    /// @param[in] xd The destination register (the first one of a pair for a double).
    /// @param[in] xn The first factor.
    /// @param[in] xm The second factor.
    /// @param[in] xa The addend.
    /// @return The instruction word.
    static word asm_fop3(U8 fn, bool dbl, U8 xd, U8 xn, U8 xm, U8 xa);

    /// Encodes `fcmp` or `fcmpe xn, xm` (format F3).
    ///
    /// @param[in] dbl Whether the operands are doubles.
    /// @param[in] signaling `fcmpe`: a quiet NaN raises Invalid too.
    /// @param[in] xn The first operand.
    /// @param[in] xm The second operand.
    /// @return The instruction word.
    static word asm_fcmp(bool dbl, bool signaling, U8 xn, U8 xm);

    /// Encodes `csel`, `csinc`, `csinv` or `csneg xd, xn, xm, cond`.
    ///
    /// @param[in] variant Which one (kCselId_*).
    /// @param[in] cond The condition.
    /// @param[in] xd The destination register.
    /// @param[in] xn The register that is chosen if the condition holds.
    /// @param[in] xm The register that the other value is made from.
    /// @return The instruction word.
    static word asm_csel(word variant, ConditionCode cond, word xd, word xn, word xm);

    /// Encodes an atomic read-modify-write.
    ///
    /// @param[in] xt The register that receives the old value.
    /// @param[in] xn The register that holds the address.
    /// @param[in] xm The register that holds the operand.
    /// @param[in] width The access width (kAtomicWidth_*).
    /// @param[in] atop The operation (kAtomicId_*).
    /// @return The instruction word.
    static word asm_atomic(word xt, word xn, word xm, U8 width, U8 atop);

    /// Constructs instructions of format O with an imm14 operand.
    ///
    /// @param[in] opcode 6 bit identifier of a format O instruction.
    /// @param[in] s Whether condition flags are set.
    /// @param[in] xd 5 bit destination register identifier.
    /// @param[in] xn 5 bit operand register identifier.
    /// @param[in] imm14 14 bit immediate value.
    /// @return Instruction word.
    static word asm_format_o(U8 opcode, bool s, U8 xd, U8 xn, S32 imm14);
    /// Constructs instructions of format O with an arg operand.
    ///
    /// @param[in] opcode 6 bit identifier of a format O instruction.
    /// @param[in] s Whether condition flags are set.
    /// @param[in] xd 5 bit destination register identifier.
    /// @param[in] xn 5 bit operand register identifier.
    /// @param[in] xm 5 bit operand register identifier.
    /// @param[in] shift Shift type to be applied on the value in the xm register.
    /// @param[in] imm5 Shift amount.
    /// @return Instruction word.
    static word asm_format_o(U8 opcode, bool s, U8 xd, U8 xn, U8 xm, ShiftType shift, S32 imm5);

    /// Constructs instructions of format O1 (the shifts): the amount is a register or an imm5.
    ///
    /// @param[in] type Which shift it is (lsl, lsr, asr or ror); they are one opcode, `_op_shift`.
    /// @param[in] xd 5 bit destination register identifier.
    /// @param[in] xn 5 bit register identifier of the value to shift.
    /// @param[in] imm Whether the amount is `imm5`, otherwise it is the register `xm`.
    /// @param[in] xm 5 bit register identifier of the amount.
    /// @param[in] imm5 The amount.
    /// @param[in] s Whether condition flags are set.
    /// @return The instruction word.
    static word asm_format_o1(ShiftType type, U8 xd, U8 xn, bool imm, U8 xm, S32 imm5,
                              bool s = false);

    /// Constructs instructions of format O2 (the long multiplies), with two destination registers.
    ///
    /// @param[in] is_signed `smull`, otherwise `umull`; they are one opcode, `_op_mull`.
    /// @param[in] s Whether condition flags are set.
    /// @param[in] xlo 5 bit register identifier that receives the low word.
    /// @param[in] xhi 5 bit register identifier that receives the high word.
    /// @param[in] xn 5 bit identifier of the first operand register.
    /// @param[in] xm 5 bit identifier of the second operand register.
    /// @return The instruction word.
    static word asm_format_o2(bool is_signed, bool s, U8 xlo, U8 xhi, U8 xn, U8 xm);

    /// Constructs instructions of format O3 with an imm19 operand (`mov`, `mvn`).
    ///
    /// @param[in] opcode 6 bit identifier of the instruction.
    /// @param[in] s Whether condition flags are set.
    /// @param[in] xd 5 bit destination register identifier.
    /// @param[in] imm19 19 bit immediate value.
    /// @return The instruction word.
    static word asm_format_o3(U8 opcode, bool s, U8 xd, S32 imm19);

    /// Constructs instructions of format O3 with a register and an imm14 operand.
    ///
    /// @param[in] opcode 6 bit identifier of the instruction.
    /// @param[in] s Whether condition flags are set.
    /// @param[in] xd 5 bit destination register identifier.
    /// @param[in] xn 5 bit operand register identifier.
    /// @param[in] imm14 14 bit immediate value.
    /// @return The instruction word.
    static word asm_format_o3(U8 opcode, bool s, U8 xd, U8 xn, S32 imm14);

    /// Constructs a load or store (format M) with a register offset.
    ///
    /// @param[in] opcode 6 bit identifier of the instruction.
    /// @param[in] sign Whether the loaded value is sign extended.
    /// @param[in] xt 5 bit register identifier that is loaded or stored.
    /// @param[in] xn 5 bit base register identifier.
    /// @param[in] xm 5 bit offset register identifier.
    /// @param[in] shift Shift type to be applied on the value in the xm register.
    /// @param[in] imm5 Shift amount.
    /// @param[in] adr How the offset is applied to the base.
    /// @return The instruction word.
    static word asm_format_m(U8 opcode, bool sign, U8 xt, U8 xn, U8 xm, ShiftType shift, S32 imm5,
                             encoding::AddrType adr);

    /// Constructs a load or store (format M) with an immediate offset.
    ///
    /// @param[in] opcode 6 bit identifier of the instruction.
    /// @param[in] sign Whether the loaded value is sign extended.
    /// @param[in] xt 5 bit register identifier that is loaded or stored.
    /// @param[in] xn 5 bit base register identifier.
    /// @param[in] simm12 12 bit signed offset.
    /// @param[in] adr How the offset is applied to the base.
    /// @return The instruction word.
    static word asm_format_m(U8 opcode, bool sign, U8 xt, U8 xn, S32 simm12,
                             encoding::AddrType adr);

    /// Constructs instructions of format M1, a register and an imm20 operand (`adrp`, `adr`).
    ///
    /// @param[in] opcode 6 bit identifier of the instruction.
    /// @param[in] xd 5 bit destination register identifier.
    /// @param[in] imm20 20 bit immediate value.
    /// @return The instruction word.
    static word asm_format_m1(U8 opcode, U8 xd, S32 imm20);

    /// Constructs an instruction of format B1: a branch to a pc relative offset (also `swi`).
    ///
    /// @param[in] opcode 6 bit identifier of the instruction.
    /// @param[in] cond The condition under which the branch is taken.
    /// @param[in] simm22 22 bit signed offset, in instructions.
    /// @return The instruction word.
    static word asm_format_b1(U8 opcode, ConditionCode cond, sword simm22);

    /// Constructs a branch (format B2) to the address in a register (`bx`, or `blx` with `link`).
    ///
    /// @param[in] cond The condition under which the branch is taken.
    /// @param[in] xd 5 bit register identifier that holds the target.
    /// @param[in] link Whether x29 gets the address of the next instruction (`blx`).
    /// @return The instruction word.
    static word asm_format_b2(ConditionCode cond, U8 xd, bool link = false);

    /// @param[in] instr An instruction word.
    /// @return The instruction as assembly text.
    static std::string disassemble_instr(word instr);
};
