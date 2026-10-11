#pragma once

#include "emulator32bit/emulator32bit.h"
#include "emulator32bit/symbols.h"

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

/// Parses decimal, hex (0x), octal (leading 0) and binary (0b) numbers.
///
/// @param str The text of the number.
/// @return The number, or nothing if the text is not one.
std::optional<U64> parse_number(const std::string &str);

/// A watchpoint as written in `--watch`: `<address|symbol>[:length][:r|w|rw][:p][:<op><number>]`.
/// The length defaults to 1 and the kind to w. `p` makes the address physical (it has to be a
/// number). `<op><number>` is a condition on the value of the access, with `<op>` one of `==`,
/// `!=`, `<`, `<=`, `>`, `>=` (unsigned): `0x1000:4:w:==5`. The parts after the address can come in
/// any order.
struct WatchSpec
{
    word address;
    word length;
    Emulator32bit::WatchKind kind;
    bool physical = false;
    Emulator32bit::WatchCompare compare = Emulator32bit::WatchCompare::NONE;
    word compare_value = 0;
};

/// The condition of a watchpoint as text: "" without one, "==0x5" with one.
///
/// @param compare The comparison.
/// @param value The number the value of the access is compared with.
/// @return The text.
std::string describe_watch_condition(Emulator32bit::WatchCompare compare, word value);

/// Parses the argument of `--watch`.
///
/// @param symbols The symbols that an address can be named by.
/// @param text The argument.
/// @return The watchpoint, or nothing if any part is malformed.
std::optional<WatchSpec> parse_watch_spec(const SymbolMap &symbols, const std::string &text);

/// A register watch as written in `--watch-reg`: `<reg>[=<value>]` (x0-x29, fp, lr or sp).
struct RegisterWatchSpec
{
    U8 reg;
    bool has_value; ///< Only a change to `value` stops.
    word value;
};

/// Parses the argument of `--watch-reg`.
///
/// @param text The argument.
/// @return The register watch, or nothing if the text is malformed.
std::optional<RegisterWatchSpec> parse_register_watch_spec(const std::string &text);

/// Parses a register name: x0-x29, fp, lr, sp, xzr, or a bare register number.
///
/// @param str The text of the register.
/// @return The number of the register, or nothing if the text is not one.
std::optional<U8> parse_register_name(const std::string &str);

/// Resolves an address given as a number, a symbol, or `symbol+number`.
///
/// @param symbols The symbols to look the name up in.
/// @param text The address.
/// @return The address, or nothing if it cannot be resolved.
std::optional<word> resolve_address(const SymbolMap &symbols, const std::string &text);

/// An interactive debugger for an emulator: a prompt that steps, continues, sets breakpoints and
/// shows registers, memory and code. It reads commands from a stream so that it can be tested, and
/// `emu32 --debug` runs it on stdin. See docs/debugging.md for the commands.
class Debugger
{
  public:
    /// Turns the instruction history on (if it is off) so a fault can show how it came about.
    ///
    /// @param emu The emulator to debug, which must outlive the debugger.
    /// @param symbols The names of addresses, which must outlive the debugger.
    /// @param in Where the commands are read from.
    /// @param out Where the output goes.
    Debugger(Emulator32bit &emu, const SymbolMap &symbols, std::istream &in, std::ostream &out);

    /// Reads and executes commands until `quit` or the end of the input.
    void run();

    /// Executes one command line.
    ///
    /// @param line The command and its arguments.
    /// @return `false` when the debugger should end.
    bool execute(const std::string &line);

    /// @return Whether the program ended (halted or faulted). It can run again after the pc is set.
    bool finished() const
    {
        return m_finished;
    }

    /// @return How the program ended. Only meaningful while `finished ()` is true.
    const Emulator32bit::RunResult &result() const
    {
        return m_result;
    }

    /// @return The instructions executed by the commands so far.
    U64 instructions() const
    {
        return m_instructions;
    }

  private:
    Emulator32bit &m_emu;
    const SymbolMap &m_symbols;
    std::istream &m_in;
    std::ostream &m_out;
    bool m_finished = false;
    Emulator32bit::RunResult m_result = {.status = Emulator32bit::RunResult::Status::LIMIT_REACHED,
                                         .instructions_ran = 0,
                                         .message = ""};
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
    ///
    /// @param text A number, a symbol, or `symbol+number`.
    /// @return The address, or nothing if the text is none of those.
    std::optional<word> resolve_address(const std::string &text) const;

    /// Runs and reports why it stopped.
    ///
    /// @param instructions The number of instructions to run, 0 for no limit.
    void run_and_report(U64 instructions);

    /// Prints the pc and the instruction there.
    void print_location();

    /// Prints the last instructions that were executed, the newest last.
    ///
    /// @param count How many to print at most.
    /// @return How many were printed.
    size_t print_history(size_t count);

    /// Prints a line of the backtrace.
    ///
    /// @param index The number of the frame, which is one more afterwards.
    /// @param address The address of the frame.
    void print_frame(size_t &index, word address);

    /// @param address An address.
    /// @return The address as hex, followed by `<symbol+offset>` if a symbol is at or below it.
    std::string describe(word address) const;

    /// @param address A virtual address.
    /// @return The disassembled instruction there, or `<unreadable>`.
    std::string disassemble_at(word address) const;

    /// For the commands that need a program that can still run.
    ///
    /// @return Whether the program has ended, in which case it said so.
    bool refuse_if_finished();
};
