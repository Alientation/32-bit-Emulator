#pragma once

#include "emulator32bit/emulator32bit.h"
#include "emulator32bit/symbols.h"

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

/// Parses decimal, hex (0x), octal (leading 0) and binary (0b) numbers.
///
/// @param str the text of the number
/// @return the number, or nothing if the text is not one
std::optional<U64> parse_number(const std::string &str);

/// A watchpoint as written in `--watch`: `<address|symbol>[:length][:r|w|rw]` (length default 1,
/// kind default w).
struct WatchSpec
{
    word address;
    word length;
    Emulator32bit::WatchKind kind;
};

/// Parses the argument of `--watch`.
///
/// @param symbols the symbols that an address can be named by
/// @param text the argument
/// @return the watchpoint, or nothing if any part is malformed
std::optional<WatchSpec> parse_watch_spec(const SymbolMap &symbols, const std::string &text);

/// A register watch as written in `--watch-reg`: `<reg>[=<value>]` (x0-x29 or sp).
struct RegisterWatchSpec
{
    U8 reg;
    std::optional<word> value;
};

/// Parses the argument of `--watch-reg`.
///
/// @param text the argument
/// @return the register watch, or nothing if the text is malformed
std::optional<RegisterWatchSpec> parse_register_watch_spec(const std::string &text);

/// Parses a register name: x0-x29, sp, xzr, or a bare register number.
///
/// @param str the text of the register
/// @return the number of the register, or nothing if the text is not one
std::optional<U8> parse_register_name(const std::string &str);

/// Resolves an address given as a number, a symbol, or `symbol+number`.
///
/// @param symbols the symbols to look the name up in
/// @param text the address
/// @return the address, or nothing if it cannot be resolved
std::optional<word> resolve_address(const SymbolMap &symbols, const std::string &text);

/// An interactive debugger for an emulator: a prompt that steps, continues, sets breakpoints and
/// shows registers, memory and code. It reads commands from a stream so that it can be tested, and
/// `emu32 --debug` runs it on stdin. See docs/debugging.md for the commands.
class Debugger
{
  public:
    /// Turns the instruction history on (if it is off) so a fault can show how it came about.
    ///
    /// @param emu the emulator to debug, which must outlive the debugger
    /// @param symbols the names of addresses, which must outlive the debugger
    /// @param in where the commands are read from
    /// @param out where the output goes
    Debugger(Emulator32bit &emu, const SymbolMap &symbols, std::istream &in, std::ostream &out);

    /// Reads and executes commands until `quit` or the end of the input.
    void run();

    /// Executes one command line.
    ///
    /// @param line the command and its arguments
    /// @return false when the debugger should end
    bool execute(const std::string &line);

    /// @return how the program ended (halt or fault), or nothing while it can still run
    const std::optional<Emulator32bit::RunResult> &finished() const
    {
        return m_finished;
    }

    /// @return the instructions executed by the commands so far
    U64 instructions() const
    {
        return m_instructions;
    }

  private:
    Emulator32bit &m_emu;
    const SymbolMap &m_symbols;
    std::istream &m_in;
    std::ostream &m_out;
    std::optional<Emulator32bit::RunResult> m_finished;
    U64 m_instructions = 0;

    // The commands, one for each command of the prompt. `args` are the words after its name.
    void command_help();
    void command_step(const std::vector<std::string> &args);
    void command_continue(const std::vector<std::string> &args);
    void command_break(const std::vector<std::string> &args);
    void command_delete(const std::vector<std::string> &args);
    void command_watch(const std::vector<std::string> &args);
    void command_unwatch(const std::vector<std::string> &args);
    void command_watches();
    void command_watchreg(const std::vector<std::string> &args);
    void command_unwatchreg(const std::vector<std::string> &args);
    void command_regs();
    void command_mem(const std::vector<std::string> &args);
    void command_disasm(const std::vector<std::string> &args);
    void command_history();
    /// Frame 0 is the pc. Every function keeps a frame record at x28 (docs/abi.md): the caller's
    /// x28 in the lower word and the return address in the upper one. The record of a function does
    /// not exist yet at its first instruction, where the return address is still in the link
    /// register.
    void command_backtrace();
    void command_set(const std::vector<std::string> &args);

    /// Same as the free resolve_address(), with the symbols of this debugger.
    std::optional<word> resolve_address(const std::string &text) const;

    /// Runs and reports why it stopped.
    ///
    /// @param instructions the number of instructions to run, 0 for no limit
    void run_and_report(U64 instructions);

    /// Prints the pc and the instruction there.
    void print_location();

    /// Prints the last instructions that were executed, the newest last.
    ///
    /// @param count how many to print at most
    void print_history(size_t count);

    /// @param address an address
    /// @return the address as hex, followed by `<symbol+offset>` if a symbol is at or below it
    std::string describe(word address) const;

    /// @param address a virtual address
    /// @return the disassembled instruction there, or `<unreadable>`
    std::string disassemble_at(word address) const;

    /// For the commands that need a program that can still run.
    ///
    /// @return whether the program has ended, in which case it said so
    bool refuse_if_finished();
};
