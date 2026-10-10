#include "emulator32bit/debugger.h"

#include <algorithm>
#include <cctype>
#include <format>
#include <istream>
#include <ostream>
#include <sstream>

namespace
{

/// How many instructions of the history a fault shows.
constexpr size_t kFaultHistory = 8;

/// How many instructions the history keeps when the debugger turns it on.
constexpr size_t kDebuggerHistory = 32;

constexpr word kDefaultDumpBytes = 16;
constexpr word kDefaultDisassembledInstructions = 8;

const char *kHelp =
    "Commands (an address is a number, a symbol, or symbol+number):\n"
    "  step [n]            (s)    execute n instructions (default 1)\n"
    "  continue [limit]    (c)    run until a breakpoint, a halt or a fault\n"
    "  break [address]     (b)    stop before the instruction at the address (default: the pc)\n"
    "  delete <address|all> (d)   remove a breakpoint\n"
    "  breaks                     list the breakpoints\n"
    "  watch <address> [len] [r|w|rw] [p] [<op> <value>] (w)  stop after a read and/or write "
    "(default w) of the bytes; p: physical address; <op> is == != < <= > >= and the value is "
    "that of the access\n"
    "  unwatch <address|all> [p]  remove a watchpoint\n"
    "  watches                    list the watchpoints and register watches\n"
    "  watchreg <reg> [value]     stop after the register changes (to the value, if given)\n"
    "  unwatchreg <reg|all>       remove a register watch\n"
    "  regs                (r)    show the registers and the flags\n"
    "  mem <address> [len] (x)    show len bytes (default 16)\n"
    "  disasm [address] [n] (dis) show n instructions (default 8) from the address (default: the "
    "pc)\n"
    "  history             (hist) show the last executed instructions\n"
    "  backtrace           (bt)   show the call chain (follows the frame records, see "
    "docs/abi.md)\n"
    "  set <reg|pc> <value>       change a register or the pc\n"
    "  help                (h)    this text\n"
    "  quit                (q)    leave the debugger\n";

} // namespace

std::optional<U64> parse_number(const std::string &str)
{
    try
    {
        size_t consumed = 0;
        U64 value;
        if (str.rfind("0b", 0) == 0 || str.rfind("0B", 0) == 0)
        {
            value = std::stoull(str.substr(2), &consumed, 2);
            consumed += 2;
        }
        else
        {
            value = std::stoull(str, &consumed, 0);
        }

        if (consumed != str.size())
        {
            return std::nullopt;
        }
        return value;
    }
    catch (const std::exception &)
    {
        return std::nullopt;
    }
}

std::optional<U8> parse_register_name(const std::string &str)
{
    // fp, lr, sp and xzr
    for (U8 reg = static_cast<U8>(Register::FP); reg < kNumReg; reg++)
    {
        if (str == register_name(reg))
        {
            return reg;
        }
    }

    const std::string digits = (!str.empty() && str[0] == 'x') ? str.substr(1) : str;
    if (digits.empty() || digits.find_first_not_of("0123456789") != std::string::npos)
    {
        return std::nullopt;
    }

    const std::optional<U64> reg = parse_number(digits);
    if (!reg || *reg >= kNumReg)
    {
        return std::nullopt;
    }
    return static_cast<U8>(*reg);
}

Debugger::Debugger(Emulator32bit &emu, const SymbolMap &symbols, std::istream &in,
                   std::ostream &out) :
    m_emu(emu),
    m_symbols(symbols),
    m_in(in),
    m_out(out)
{
    if (m_emu.history_size() == 0)
    {
        m_emu.set_history_size(kDebuggerHistory);
    }
    m_emu.set_symbols(&m_symbols);
    m_emu.set_brk_stops(true);
}

void Debugger::run()
{
    print_location();
    std::string line;
    while (true)
    {
        m_out << "(dbg) " << std::flush;
        if (!std::getline(m_in, line))
        {
            m_out << "\n";
            return;
        }
        if (!execute(line))
        {
            return;
        }
    }
}

bool Debugger::execute(const std::string &line)
{
    std::istringstream words(line);
    std::string command;
    if (!(words >> command))
    {
        return true;
    }
    std::vector<std::string> args;
    for (std::string arg; words >> arg;)
    {
        args.push_back(arg);
    }

    if (command == "quit" || command == "q")
    {
        return false;
    }
    else if (command == "help" || command == "h")
    {
        command_help();
    }
    else if (command == "step" || command == "s")
    {
        command_step(args);
    }
    else if (command == "continue" || command == "c")
    {
        command_continue(args);
    }
    else if (command == "break" || command == "b")
    {
        command_break(args);
    }
    else if (command == "delete" || command == "d")
    {
        command_delete(args);
    }
    else if (command == "breaks")
    {
        if (m_emu.breakpoints().empty())
        {
            m_out << "No breakpoints.\n";
        }
        for (const word pc : m_emu.breakpoints())
        {
            m_out << "  " << describe(pc) << "\n";
        }
    }
    else if (command == "watch" || command == "w")
    {
        command_watch(args);
    }
    else if (command == "unwatch")
    {
        command_unwatch(args);
    }
    else if (command == "watches")
    {
        command_watches();
    }
    else if (command == "watchreg")
    {
        command_watchreg(args);
    }
    else if (command == "unwatchreg")
    {
        command_unwatchreg(args);
    }
    else if (command == "regs" || command == "r")
    {
        command_regs();
    }
    else if (command == "mem" || command == "x")
    {
        command_mem(args);
    }
    else if (command == "disasm" || command == "dis")
    {
        command_disasm(args);
    }
    else if (command == "history" || command == "hist")
    {
        command_history();
    }
    else if (command == "backtrace" || command == "bt")
    {
        command_backtrace();
    }
    else if (command == "set")
    {
        command_set(args);
    }
    else
    {
        m_out << "Unknown command '" << command << "', try 'help'.\n";
    }
    return true;
}

void Debugger::command_help()
{
    m_out << kHelp;
}

bool Debugger::refuse_if_finished()
{
    if (!m_finished)
    {
        return false;
    }
    m_out << "The program has ended (" << m_finished->message
          << "). Change the pc with 'set pc <address>' to run it again, or 'quit'.\n";
    return true;
}

void Debugger::command_step(const std::vector<std::string> &args)
{
    U64 count = 1;
    if (!args.empty())
    {
        const std::optional<U64> parsed = parse_number(args[0]);
        if (!parsed || *parsed == 0)
        {
            m_out << "Expected a number of instructions greater than 0, got '" << args[0] << "'.\n";
            return;
        }
        count = *parsed;
    }

    if (!refuse_if_finished())
    {
        run_and_report(count);
    }
}

void Debugger::command_continue(const std::vector<std::string> &args)
{
    U64 limit = 0;
    if (!args.empty())
    {
        const std::optional<U64> parsed = parse_number(args[0]);
        if (!parsed)
        {
            m_out << "Expected a limit, got '" << args[0] << "'.\n";
            return;
        }
        limit = *parsed;
    }

    if (!refuse_if_finished())
    {
        run_and_report(limit);
    }
}

void Debugger::command_break(const std::vector<std::string> &args)
{
    word address = m_emu.get_pc();
    if (!args.empty())
    {
        const std::optional<word> resolved = resolve_address(args[0]);
        if (!resolved)
        {
            m_out << "'" << args[0] << "' is not an address or a known symbol.\n";
            return;
        }
        address = *resolved;
    }

    m_emu.add_breakpoint(address);
    m_out << "Breakpoint at " << describe(address) << ".\n";
}

void Debugger::command_delete(const std::vector<std::string> &args)
{
    if (args.empty())
    {
        m_out << "Expected an address or 'all'.\n";
        return;
    }
    if (args[0] == "all")
    {
        m_emu.clear_breakpoints();
        m_out << "All breakpoints removed.\n";
        return;
    }

    const std::optional<word> address = resolve_address(args[0]);
    if (!address || !m_emu.remove_breakpoint(*address))
    {
        m_out << "There is no breakpoint at '" << args[0] << "'.\n";
        return;
    }
    m_out << "Breakpoint at " << describe(*address) << " removed.\n";
}

static const char *watch_kind_name(const Emulator32bit::WatchKind kind)
{
    switch (kind)
    {
    case Emulator32bit::WatchKind::READ:
        return "read";
    case Emulator32bit::WatchKind::WRITE:
        return "write";
    case Emulator32bit::WatchKind::ACCESS:
        return "read/write";
    }
    return "?";
}

void Debugger::command_watch(const std::vector<std::string> &args)
{
    if (args.empty() || args.size() > 6)
    {
        m_out << "Usage: watch <address> [length] [r|w|rw] [p] [<op> <value>]\n";
        return;
    }

    // "== 5" in two words is the same as "==5".
    std::string spec;
    bool joined = false;
    for (const std::string &arg : args)
    {
        const bool is_operator = arg == "==" || arg == "!=" || arg == "<" || arg == "<="
                                 || arg == ">" || arg == ">=";
        spec += (spec.empty() || joined ? "" : ":") + arg;
        joined = is_operator;
    }
    const std::optional<WatchSpec> watch = parse_watch_spec(m_symbols, spec);
    if (!watch)
    {
        m_out << "Expected <address> [length greater than 0] [r|w|rw] [p] [<op> <value>], got '"
              << spec << "'.\n";
        return;
    }

    if (!m_emu.add_watchpoint(watch->address, watch->length, watch->kind, watch->physical,
                              watch->compare, watch->compare_value))
    {
        m_out << "There are " << kMaxWatchpoints << " watchpoints already; remove one with unwatch.\n";
        return;
    }
    m_out << "Watching " << watch->length << " byte" << (watch->length == 1 ? "" : "s") << " at "
          << (watch->physical ? std::format("physical {:#010x}", watch->address)
                              : describe(watch->address))
          << " for " << watch_kind_name(watch->kind);
    if (watch->compare != Emulator32bit::WatchCompare::NONE)
    {
        m_out << " when the value is " << describe_watch_condition(watch->compare, watch->compare_value);
    }
    m_out << ".\n";
}

void Debugger::command_unwatch(const std::vector<std::string> &args)
{
    if (args.empty() || args.size() > 2 || (args.size() == 2 && args[1] != "p"))
    {
        m_out << "Expected an address or 'all', and p for a physical address.\n";
        return;
    }
    if (args[0] == "all")
    {
        m_emu.clear_watchpoints();
        m_out << "All watchpoints removed.\n";
        return;
    }

    const bool physical = args.size() == 2;
    const std::optional<word> address = resolve_address(args[0]);
    if (!address || !m_emu.remove_watchpoint(*address, physical))
    {
        m_out << "There is no watchpoint at '" << args[0] << "'" << (physical ? " (physical)" : "")
              << ".\n";
        return;
    }
    m_out << "Watchpoint at " << describe(*address) << " removed.\n";
}

void Debugger::command_watches()
{
    if (m_emu.watchpoint_count() == 0 && m_emu.register_watch_count() == 0)
    {
        m_out << "No watchpoints.\n";
    }
    for (unsigned i = 0; i < m_emu.register_watch_count(); i++)
    {
        const Emulator32bit::RegisterWatch watch = m_emu.register_watch(i);
        m_out << "  " << register_name(watch.reg) << ", changes"
              << (watch.has_value ? std::format(" to {:#x}", watch.value) : "") << "\n";
    }
    for (unsigned i = 0; i < m_emu.watchpoint_count(); i++)
    {
        const Emulator32bit::Watchpoint watch = m_emu.watchpoint(i);
        m_out << "  "
              << (watch.physical ? std::format("physical {:#010x}", watch.address)
                                 : describe(watch.address))
              << ", " << watch.length << " byte" << (watch.length == 1 ? "" : "s") << ", "
              << watch_kind_name(watch.kind);
        if (watch.compare != Emulator32bit::WatchCompare::NONE)
        {
            m_out << ", value " << describe_watch_condition(watch.compare, watch.compare_value);
        }
        m_out << "\n";
    }
}

void Debugger::command_watchreg(const std::vector<std::string> &args)
{
    if (args.empty() || args.size() > 2)
    {
        m_out << "Usage: watchreg <x0..x29|fp|lr|sp> [value]\n";
        return;
    }
    const std::optional<RegisterWatchSpec> watch =
        parse_register_watch_spec(args.size() == 2 ? args[0] + "=" + args[1] : args[0]);
    if (!watch)
    {
        m_out << "Expected <x0..x29|fp|lr|sp> [value], got '" << args[0] << "'.\n";
        return;
    }

    if (!(watch->has_value ? m_emu.add_register_watch(watch->reg, watch->value)
                           : m_emu.add_register_watch(watch->reg)))
    {
        m_out << kMaxRegisterWatches << " registers are watched already; remove one with unwatchreg.\n";
        return;
    }
    m_out << "Watching " << register_name(watch->reg) << " for a change"
          << (watch->has_value ? std::format(" to {:#x}", watch->value) : "") << ".\n";
}

void Debugger::command_unwatchreg(const std::vector<std::string> &args)
{
    if (args.empty())
    {
        m_out << "Expected a register or 'all'.\n";
        return;
    }
    if (args[0] == "all")
    {
        m_emu.clear_register_watches();
        m_out << "All register watches removed.\n";
        return;
    }

    const std::optional<U8> reg = parse_register_name(args[0]);
    if (!reg || !m_emu.remove_register_watch(*reg))
    {
        m_out << "There is no watch on '" << args[0] << "'.\n";
        return;
    }
    m_out << "Watch on " << register_name(*reg) << " removed.\n";
}

void Debugger::command_regs()
{
    m_out << std::format("  pc {:#010x}", m_emu.get_pc());
    const std::string name = m_symbols.describe(m_emu.get_pc());
    m_out << (name.empty() ? "" : " <" + name + ">") << "\n";

    for (U8 reg = 0; reg < static_cast<U8>(Register::SP); reg++)
    {
        m_out << std::format(" {:>3} {:#010x}", "x" + std::to_string(reg), m_emu.read_reg(reg));
        m_out << (reg % 4 == 3 ? "\n" : " ");
    }
    m_out << "\n" << std::format("  sp {:#010x}\n", m_emu.read_reg(Register::SP));

    m_out << "  NZCV " << (m_emu.get_flag(kNFlagBit) ? 'N' : 'n')
          << (m_emu.get_flag(kZFlagBit) ? 'Z' : 'z') << (m_emu.get_flag(kCFlagBit) ? 'C' : 'c')
          << (m_emu.get_flag(kVFlagBit) ? 'V' : 'v') << "  "
          << (m_emu.user_mode() ? "user mode" : "kernel mode")
          << (m_emu.get_flag(kIrqMaskBit) ? ", IRQs masked" : "") << "\n";

    // The exception state, once a vector table is installed.
    if (m_emu.read_sysreg(Emulator32bit::kSysregId_vbar) != 0)
    {
        for (const U8 id : {Emulator32bit::kSysregId_vbar, Emulator32bit::kSysregId_elr,
                            Emulator32bit::kSysregId_spsr, Emulator32bit::kSysregId_esr,
                            Emulator32bit::kSysregId_far, Emulator32bit::kSysregId_usp})
        {
            m_out << std::format("  {:<5}{:#010x}\n", Emulator32bit::sysreg_name(id),
                                 m_emu.read_sysreg(id));
        }
    }
}

void Debugger::command_mem(const std::vector<std::string> &args)
{
    if (args.empty())
    {
        m_out << "Expected an address.\n";
        return;
    }
    const std::optional<word> address = resolve_address(args[0]);
    if (!address)
    {
        m_out << "'" << args[0] << "' is not an address or a known symbol.\n";
        return;
    }
    word length = kDefaultDumpBytes;
    if (args.size() > 1)
    {
        const std::optional<U64> parsed = parse_number(args[1]);
        if (!parsed || *parsed == 0)
        {
            m_out << "Expected a length greater than 0, got '" << args[1] << "'.\n";
            return;
        }
        length = static_cast<word>(*parsed);
    }

    for (word offset = 0; offset < length; offset += 16)
    {
        m_out << std::format("{:#010x}:", *address + offset);
        std::string text;
        for (word i = offset; i < std::min<word>(offset + 16, length); i++)
        {
            try
            {
                const byte value = m_emu.memory.read_byte(*address + i);
                m_out << std::format(" {:02x}", value);
                text += (value >= 0x20 && value < 0x7F) ? char(value) : '.';
            }
            catch (const std::exception &)
            {
                m_out << " ??";
                text += '?';
            }
        }
        m_out << "  " << text << "\n";
    }
}

void Debugger::command_disasm(const std::vector<std::string> &args)
{
    word address = m_emu.get_pc();
    if (!args.empty())
    {
        const std::optional<word> resolved = resolve_address(args[0]);
        if (!resolved)
        {
            m_out << "'" << args[0] << "' is not an address or a known symbol.\n";
            return;
        }
        address = *resolved;
    }
    word count = kDefaultDisassembledInstructions;
    if (args.size() > 1)
    {
        const std::optional<U64> parsed = parse_number(args[1]);
        if (!parsed || *parsed == 0)
        {
            m_out << "Expected a number of instructions greater than 0, got '" << args[1] << "'.\n";
            return;
        }
        count = static_cast<word>(*parsed);
    }

    for (word i = 0; i < count; i++, address += 4)
    {
        m_out << (address == m_emu.get_pc() ? "=> " : "   ") << describe(address) << ": "
              << disassemble_at(address) << "\n";
    }
}

void Debugger::command_history()
{
    print_history(m_emu.history().size());
    if (m_emu.history().empty())
    {
        m_out << "No instructions executed yet.\n";
    }
}

void Debugger::command_backtrace()
{
    constexpr size_t kMaxFrames = 64;

    size_t index = 0;
    const auto print_frame = [&](const word address)
    { m_out << std::format("#{:<3}{}\n", index++, describe(address)); };

    const word pc = m_emu.get_pc();
    print_frame(pc);
    if (m_symbols.has_symbol_at(pc))
    {
        print_frame(m_emu.read_reg(Register::LR));
    }

    // In a handler the frame records go on with the code that was interrupted (a handler that
    // does not make a record of its own leaves x28 as it was), but the interrupted function is not
    // among them: its record gives its caller. ELR is where it was, so it is shown here.
    if (m_emu.exception_depth() != 0)
    {
        const word esr = m_emu.read_sysreg(Emulator32bit::kSysregId_esr);
        const auto cls = static_cast<Emulator32bit::ExceptionClass>(esr >> 26);
        const bool is_abort = cls == Emulator32bit::ExceptionClass::INSTRUCTION_ABORT
                              || cls == Emulator32bit::ExceptionClass::DATA_ABORT;
        m_out << std::format("    in the handler of a {}: ESR={:#x}{}{}, the code that was "
                             "interrupted:\n",
                             Emulator32bit::exception_class_name(cls), esr,
                             is_abort ? std::format(", FAR={:#010x}",
                                                    m_emu.read_sysreg(Emulator32bit::kSysregId_far))
                                      : "",
                             m_emu.exception_depth() > 1 ? " (the last of several)" : "");
        print_frame(m_emu.read_sysreg(Emulator32bit::kSysregId_elr));
    }

    word fp = m_emu.read_reg(Register::FP);
    while (fp != 0 && index < kMaxFrames)
    {
        if (fp & 0b11)
        {
            m_out << std::format("(stopped: the frame pointer {:#x} is not word aligned)\n", fp);
            return;
        }

        word caller_fp;
        word return_address;
        try
        {
            caller_fp = m_emu.memory.read_word(fp);
            return_address = m_emu.memory.read_word(fp + 4);
        }
        catch (const std::exception &)
        {
            m_out << std::format("(stopped: cannot read the frame record at {:#x})\n", fp);
            return;
        }

        print_frame(return_address);
        // The stack grows down, so the record of a caller is at a higher address.
        if (caller_fp != 0 && caller_fp <= fp)
        {
            m_out << std::format("(stopped: the next frame pointer {:#x} is not above {:#x})\n",
                                 caller_fp, fp);
            return;
        }
        fp = caller_fp;
    }
    if (fp != 0)
    {
        m_out << "(more frames not shown)\n";
    }
}

void Debugger::command_set(const std::vector<std::string> &args)
{
    if (args.size() != 2)
    {
        m_out << "Usage: set <x0..x29|fp|lr|sp|pc> <value>\n";
        return;
    }
    const std::optional<U64> value = parse_number(args[1]);
    if (!value)
    {
        m_out << "'" << args[1] << "' is not a number.\n";
        return;
    }

    if (args[0] == "pc")
    {
        if (*value & 0b11)
        {
            m_out << "The pc has to be a multiple of 4.\n";
            return;
        }
        m_emu.set_pc(static_cast<word>(*value));
        m_finished.reset();
        print_location();
        return;
    }

    const std::optional<U8> reg = parse_register_name(args[0]);
    if (!reg || *reg == static_cast<U8>(Register::XZR))
    {
        m_out << "'" << args[0] << "' is not a register that can be set.\n";
        return;
    }
    m_emu.write_reg(*reg, static_cast<word>(*value));
}

std::optional<word> Debugger::resolve_address(const std::string &text) const
{
    return ::resolve_address(m_symbols, text);
}

std::optional<RegisterWatchSpec> parse_register_watch_spec(const std::string &text)
{
    const size_t equals = text.find('=');
    const std::optional<U8> reg = parse_register_name(text.substr(0, equals));
    // xzr never changes, and a bare number would be a register number in parse_register_name.
    if (!reg || *reg > static_cast<U8>(Register::SP)
        || std::isdigit(static_cast<unsigned char>(text[0])))
    {
        return std::nullopt;
    }

    RegisterWatchSpec spec{.reg = *reg, .has_value = false, .value = 0};
    if (equals != std::string::npos)
    {
        const std::optional<U64> value = parse_number(text.substr(equals + 1));
        if (!value || *value > 0xFFFFFFFFu)
        {
            return std::nullopt;
        }
        spec.has_value = true;
        spec.value = static_cast<word>(*value);
    }
    return spec;
}

namespace
{
struct ConditionName
{
    const char *text;
    Emulator32bit::WatchCompare compare;
};

// The longer operators first, so that "<=" is not read as "<" and "=5".
constexpr ConditionName kConditionNames[] = {
    {"==", Emulator32bit::WatchCompare::EQ}, {"!=", Emulator32bit::WatchCompare::NE},
    {"<=", Emulator32bit::WatchCompare::LE}, {">=", Emulator32bit::WatchCompare::GE},
    {"<", Emulator32bit::WatchCompare::LT},  {">", Emulator32bit::WatchCompare::GT},
};

/// "==5", ">=0x10": an operator and a number.
std::optional<std::pair<Emulator32bit::WatchCompare, word>>
parse_watch_condition(const std::string &text)
{
    for (const ConditionName &name : kConditionNames)
    {
        if (text.rfind(name.text, 0) != 0)
        {
            continue;
        }
        const std::optional<U64> value = parse_number(text.substr(std::string(name.text).size()));
        if (!value || *value > 0xFFFFFFFFu)
        {
            return std::nullopt;
        }
        return std::pair{name.compare, static_cast<word>(*value)};
    }
    return std::nullopt;
}
} // namespace

std::string describe_watch_condition(const Emulator32bit::WatchCompare compare, const word value)
{
    for (const ConditionName &name : kConditionNames)
    {
        if (name.compare == compare)
        {
            return std::format("{}{:#x}", name.text, value);
        }
    }
    return "";
}

std::optional<WatchSpec> parse_watch_spec(const SymbolMap &symbols, const std::string &text)
{
    std::vector<std::string> parts;
    std::istringstream stream(text);
    for (std::string part; std::getline(stream, part, ':');)
    {
        parts.push_back(part);
    }
    if (parts.empty() || parts.size() > 5)
    {
        return std::nullopt;
    }

    const std::optional<word> address = resolve_address(symbols, parts[0]);
    if (!address)
    {
        return std::nullopt;
    }

    WatchSpec spec{.address = *address, .length = 1, .kind = Emulator32bit::WatchKind::WRITE};
    bool has_length = false;
    bool has_kind = false;
    for (size_t i = 1; i < parts.size(); i++)
    {
        const std::string &part = parts[i];
        if (part == "p")
        {
            if (spec.physical)
            {
                return std::nullopt;
            }
            spec.physical = true;
        }
        else if (part == "r" || part == "w" || part == "rw")
        {
            if (has_kind)
            {
                return std::nullopt;
            }
            has_kind = true;
            spec.kind = part == "r"   ? Emulator32bit::WatchKind::READ
                        : part == "w" ? Emulator32bit::WatchKind::WRITE
                                      : Emulator32bit::WatchKind::ACCESS;
        }
        else if (const auto condition = parse_watch_condition(part))
        {
            if (spec.compare != Emulator32bit::WatchCompare::NONE)
            {
                return std::nullopt;
            }
            spec.compare = condition->first;
            spec.compare_value = condition->second;
        }
        else
        {
            const std::optional<U64> length = parse_number(part);
            if (has_length || !length || *length == 0 || *length > 0xFFFFFFFFu)
            {
                return std::nullopt;
            }
            has_length = true;
            spec.length = static_cast<word>(*length);
        }
    }

    // A symbol is a virtual address.
    if (spec.physical && !parse_number(parts[0]))
    {
        return std::nullopt;
    }
    return spec;
}

std::optional<word> resolve_address(const SymbolMap &symbols, const std::string &text)
{
    if (const std::optional<U64> number = parse_number(text))
    {
        return static_cast<word>(*number);
    }
    if (const std::optional<word> symbol = symbols.find(text))
    {
        return *symbol;
    }

    const size_t plus = text.find('+');
    if (plus != std::string::npos)
    {
        const std::optional<word> base = symbols.find(text.substr(0, plus));
        const std::optional<U64> offset = parse_number(text.substr(plus + 1));
        if (base && offset)
        {
            return static_cast<word>(*base + *offset);
        }
    }
    return std::nullopt;
}

void Debugger::run_and_report(const U64 instructions)
{
    const Emulator32bit::RunResult result = m_emu.run(instructions);
    m_instructions += result.instructions_ran;

    using Status = Emulator32bit::RunResult::Status;
    switch (result.status)
    {
    case Status::HALTED:
        m_finished = result;
        m_out << "Program halted after " << m_instructions << " instructions.\n";
        break;
    case Status::FAULT:
        m_finished = result;
        m_out << "Fault: " << result.message << "\n";
        print_history(kFaultHistory);
        break;
    case Status::BREAKPOINT:
        m_out << result.message << "\n";
        break;
    case Status::LIMIT_REACHED:
        break;
    }
    print_location();
}

void Debugger::print_location()
{
    m_out << "=> " << describe(m_emu.get_pc()) << ": " << disassemble_at(m_emu.get_pc()) << "\n";
}

void Debugger::print_history(const size_t count)
{
    const std::vector<Emulator32bit::ExecutedInstruction> history = m_emu.history();
    const size_t first = history.size() > count ? history.size() - count : 0;
    for (size_t i = first; i < history.size(); i++)
    {
        m_out << "   " << describe(history[i].pc) << ": "
              << Emulator32bit::disassemble_instr(history[i].instruction) << "\n";
    }
}

std::string Debugger::describe(const word address) const
{
    const std::string name = m_symbols.describe(address);
    return std::format("{:#010x}", address) + (name.empty() ? "" : " <" + name + ">");
}

std::string Debugger::disassemble_at(const word address) const
{
    try
    {
        return Emulator32bit::disassemble_instr(m_emu.memory.read_word(address));
    }
    catch (const std::exception &)
    {
        return "<unreadable>";
    }
}
