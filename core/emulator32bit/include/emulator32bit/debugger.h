#pragma once

#include "emulator32bit/emulator32bit.h"
#include "emulator32bit/symbols.h"

#include <iosfwd>
#include <optional>
#include <string>
#include <vector>

/// Parses decimal, hex (0x), octal (leading 0) and binary (0b) numbers.
std::optional<U64> parse_number(const std::string &str);

/// A watchpoint as `<address|symbol>[:length][:r|w|rw]` (length default 1, kind default w), the
/// syntax of `--watch`. Nothing if any part is malformed.
struct WatchSpec
{
    word address;
    word length;
    Emulator32bit::WatchKind kind;
};

std::optional<WatchSpec> parse_watch_spec(const SymbolMap &symbols, const std::string &text);

/// A register watch as `<reg>[=<value>]` (x0-x29 or sp), the syntax of `--watch-reg`.
struct RegisterWatchSpec
{
    U8 reg;
    std::optional<word> value;
};

std::optional<RegisterWatchSpec> parse_register_watch_spec(const std::string &text);

/// Parses a register name: x0-x29, sp, xzr, or a bare register number.
std::optional<U8> parse_register_name(const std::string &str);

/// An address given as a number, a symbol, or `symbol+number`.
std::optional<word> resolve_address(const SymbolMap &symbols, const std::string &text);

/// An interactive debugger for an emulator: a prompt that steps, continues, sets breakpoints and
/// shows registers, memory and code. It reads commands from a stream so that it can be tested, and
/// `emu32 --debug` runs it on stdin. See docs/debugging.md for the commands.
class Debugger
{
  public:
    /// Turns the instruction history on (if it is off) so a fault can show how it came about.
    /// The emulator and the symbols must outlive the debugger.
    Debugger(Emulator32bit &emu, const SymbolMap &symbols, std::istream &in, std::ostream &out);

    /// Reads and executes commands until `quit` or the end of the input.
    void run();

    /// Executes one command line. False when the debugger should end.
    bool execute(const std::string &line);

    /// How the program ended (halt or fault), or nothing while it can still run.
    const std::optional<Emulator32bit::RunResult> &finished() const
    {
        return m_finished;
    }

    /// Instructions executed by the commands so far.
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
    void command_backtrace();
    void command_set(const std::vector<std::string> &args);

    std::optional<word> resolve_address(const std::string &text) const;

    /// Runs and reports why it stopped.
    void run_and_report(U64 instructions);
    void print_location();
    void print_history(size_t count);
    std::string describe(word address) const;
    std::string disassemble_at(word address) const;
    bool refuse_if_finished();
};
