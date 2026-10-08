#include "emulator32bit/debugger.h"

#include <algorithm>
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
    if (str == "sp")
    {
        return static_cast<U8>(Register::SP);
    }
    if (str == "xzr")
    {
        return static_cast<U8>(Register::XZR);
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
          << (m_emu.get_flag(kVFlagBit) ? 'V' : 'v') << "\n";
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
                const byte value = m_emu.system_bus->read_byte(*address + i);
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

// Frame 0 is the pc. Every function keeps a frame record at x28 (docs/abi.md): the caller's x28 in
// the lower word and the return address in the upper one. The record of a function does not exist
// yet at its first instruction, where the return address is still in the link register.
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
            caller_fp = m_emu.system_bus->read_word(fp);
            return_address = m_emu.system_bus->read_word(fp + 4);
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
        m_out << "Usage: set <x0..x29|sp|pc> <value>\n";
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
        return Emulator32bit::disassemble_instr(m_emu.system_bus->read_word(address));
    }
    catch (const std::exception &)
    {
        return "<unreadable>";
    }
}
