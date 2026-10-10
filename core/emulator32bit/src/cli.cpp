#include "emulator32bit/debugger.h"
#include "emulator32bit/emulator32bit.h"
#include "emulator32bit/host_input.h"
#include "assembler/load_executable.h"
#include "assembler/object_file.h"
#include "util/file.h"
#include "util/logger.h"

#include <cxxopts.hpp>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <sstream>
#include <string>
#include <thread>
#include <vector>

namespace
{

const char *status_name(Emulator32bit::RunResult::Status status)
{
    switch (status)
    {
    case Emulator32bit::RunResult::Status::HALTED:
        return "halted";
    case Emulator32bit::RunResult::Status::LIMIT_REACHED:
        return "limit";
    case Emulator32bit::RunResult::Status::BREAKPOINT:
        return "breakpoint";
    case Emulator32bit::RunResult::Status::FAULT:
        return "fault";
    default:
        AEMU_FATAL("Unknown Status: {}", static_cast<U8>(status));
        return "error";
    }
}

std::string hex(word value)
{
    char buf[16];
    std::snprintf(buf, sizeof(buf), "0x%08x", value);
    return buf;
}

/// Machine readable state dump, one "key=value" pair per line.
void print_plain(std::ostream &out, Emulator32bit &emu, const Emulator32bit::RunResult &result,
                 const std::vector<std::pair<word, word>> &mem_ranges)
{
    out << "status=" << status_name(result.status) << "\n";
    out << "instructions=" << result.instructions_ran << "\n";
    if (!result.message.empty()) out << "message=" << result.message << "\n";

    out << "pc=" << hex(emu.get_pc()) << "\n";
    for (U8 reg = 0; reg < static_cast<U8>(Register::SP); reg++)
    {
        out << "x" << int(reg) << "=" << hex(emu.read_reg(reg)) << "\n";
    }
    out << "sp=" << hex(emu.read_reg(Register::SP)) << "\n";

    out << "N=" << emu.get_flag(kNFlagBit) << "\n";
    out << "Z=" << emu.get_flag(kZFlagBit) << "\n";
    out << "C=" << emu.get_flag(kCFlagBit) << "\n";
    out << "V=" << emu.get_flag(kVFlagBit) << "\n";

    // The mode and the state of the exceptions (docs/exceptions.md).
    out << "mode=" << (emu.user_mode() ? "user" : "kernel") << "\n";
    for (const U8 id : {Emulator32bit::kSysregId_pstate, Emulator32bit::kSysregId_elr,
                        Emulator32bit::kSysregId_spsr, Emulator32bit::kSysregId_esr,
                        Emulator32bit::kSysregId_far, Emulator32bit::kSysregId_vbar,
                        Emulator32bit::kSysregId_fpcr, Emulator32bit::kSysregId_fpsr})
    {
        out << Emulator32bit::sysreg_name(id) << "=" << hex(emu.read_sysreg(id)) << "\n";
    }

    // mem[<addr>]=<hex bytes, space separated>
    for (const auto &[addr, len] : mem_ranges)
    {
        out << "mem[" << hex(addr) << "]=";
        for (word i = 0; i < len; i++)
        {
            char buf[4];
            try
            {
                std::snprintf(buf, sizeof(buf), "%02x", emu.memory.read_byte(addr + i));
            }
            catch (const std::exception &)
            {
                std::snprintf(buf, sizeof(buf), "??");
            }
            out << (i == 0 ? "" : " ") << buf;
        }
        out << "\n";
    }
}

/// The last executed instructions, oldest first: `history[0]=<pc> <instruction word> <assembly>`.
/// The newest one is the instruction that faulted, if the run ended in a fault.
void print_history(std::ostream &out, const Emulator32bit &emu, const SymbolMap &symbols)
{
    const std::vector<Emulator32bit::ExecutedInstruction> history = emu.history();
    for (size_t i = 0; i < history.size(); i++)
    {
        const std::string name = symbols.describe(history[i].pc);
        out << "history[" << i << "]=" << hex(history[i].pc) << " " << hex(history[i].instruction)
            << " " << Emulator32bit::disassemble_instr(history[i].instruction)
            << (name.empty() ? "" : " <" + name + ">") << "\n";
    }
}

/// The names of the symbols that the linker put in an executable, for the trace and the debugger.
SymbolMap read_symbols(const std::string &exe_path)
{
    SymbolMap symbols;
    ObjectFile obj{File(exe_path)};
    for (const auto &[name_index, symbol] : obj.symbol_table)
    {
        if (symbol.section != U32(-1))
        {
            // The assembler names a local symbol `name:LOCAL:<scope>`, only the name is of use here.
            const std::string name = obj.get_symbol_name(name_index);
            symbols.add(name.substr(0, name.find(":LOCAL:")), symbol.symbol_value);
        }
    }
    return symbols;
}

} // namespace

/// The declaration of the options. Kept apart so that the rest of the file does not depend on
/// how they are written.
static cxxopts::Options make_options()
{
    cxxopts::Options options("emu32", "32 bit cpu emulator");

    // clang-format off
    options.add_options ("Memory")
        ("ram-start", "Starting RAM page", cxxopts::value<std::string> ()->default_value ("0"))
        ("ram-pages", "Number of RAM pages", cxxopts::value<std::string> ()->default_value ("32"))
        ("rom-start", "Starting ROM page", cxxopts::value<std::string> ()->default_value ("32"))
        ("rom-pages", "Number of ROM pages", cxxopts::value<std::string> ()->default_value ("32"))
        ("rom-file", "File containing ROM data", cxxopts::value<std::string> ())
        ("disk-start", "Starting Disk page", cxxopts::value<std::string> ())
        ("disk-pages", "Number of Disk pages", cxxopts::value<std::string> ())
        ("disk-file", "File containing Disk data. Without one, an in-memory disk is used",
            cxxopts::value<std::string> ());

    options.add_options ("Program")
        ("e,exe", "Executable (.bexe) to load. Sets the PC to its _start symbol",
            cxxopts::value<std::string> ());

    options.add_options ("Register and State")
        ("pc", "Initial PC (ignored with --exe)", cxxopts::value<std::string> ()->default_value ("0"))
        ("l,limit", "Number of instructions to execute, 0 for no limit",
            cxxopts::value<std::string> ()->default_value ("0"))
        ("f,flags", "Initial NZCV flags (e.g. 0b0100)", cxxopts::value<std::string> ()->default_value ("0b0100"))
        ("r,reg", "Initial register values (e.g. x0=10,x1=0x20,sp=0x1000)",
            cxxopts::value<std::vector<std::string>> ());

    options.add_options ("Output")
        ("format", "State dump format: pretty or plain (key=value lines)",
            cxxopts::value<std::string> ()->default_value ("pretty"))
        ("o,output", "Write the plain state dump to this file instead of stdout", cxxopts::value<std::string> ())
        ("m,mem", "Memory ranges to include in a plain dump (e.g. 0x1000:16,0x2000:4)",
            cxxopts::value<std::vector<std::string>> ())
        ("h,help", "Show help");

    options.add_options ("Debugging")
        ("debug", "Start an interactive debugger on stdin/stdout (type 'help'). Ignores --limit")
        ("t,trace", "Write every executed instruction and what it changed to this file (- for stdout)",
            cxxopts::value<std::string> ())
        ("console-input", "File whose bytes the console device receives (see docs/devices.md)",
            cxxopts::value<std::string> ())
        ("console-stdin", "The console device receives what is typed (standard input) while the machine runs")
        ("block-file", "File that holds the block device's disk (512 byte sectors); made if it does not exist",
            cxxopts::value<std::string> ())
        ("block-sectors", "Number of sectors of the block device (at least the size of --block-file)",
            cxxopts::value<std::string> ())
        ("no-semihosting", "Make 'swi 1', the emulator calls (print, assert, ...), an undefined instruction")
        ("break", "Stop before executing the instruction at these addresses or symbols (exit code 4)",
            cxxopts::value<std::vector<std::string>> ())
        ("watch", "Stop after an access: ADDR|SYMBOL[:LENGTH][:r|w|rw][:p][:OP VALUE], default 1 byte, write; p: ADDR is physical; OP VALUE (==5, !=0, <=0x10...) only when the value matches (exit code 4)",
            cxxopts::value<std::vector<std::string>> ())
        ("watch-reg", "Stop after a register changes: REG[=VALUE] (x0-x29, sp), only to VALUE if given (exit code 4)",
            cxxopts::value<std::vector<std::string>> ())
        ("history", "Keep the last N executed instructions and print them after the run",
            cxxopts::value<std::string> ()->default_value ("0"));
    // clang-format on
    return options;
}

/// What the command line asked for, with the numbers and names resolved.
struct CliArgs
{
    word ram_npages = 0;
    word ram_start_page = 0;
    word rom_npages = 0;
    word rom_start_page = 0;
    word pc = 0;
    word limit = 0;
    word flags = 0;
    word history_size = 0;
    word disk_npages = 0;
    word disk_start_page = 0;
    word block_sectors = 0;
    bool plain = false;
    std::vector<std::pair<word, word>> mem_ranges;
    std::vector<std::pair<U8, word>> reg_values;
    SymbolMap symbols; // of the executable, which the breakpoints and watchpoints can name
    std::vector<word> breakpoints;
    std::vector<WatchSpec> watchpoints;
    std::vector<RegisterWatchSpec> register_watches;
    std::ofstream trace_file;
    std::ostream *trace = nullptr;
};

/// Reads the options into `args`. Every mistake is reported, not just the first.
///
/// @return false if there was one
static bool parse_args(const cxxopts::ParseResult &result, CliArgs &args)
{
    bool parse_error = false;
    auto number_option = [&](const char *option) -> word
    {
        const std::string value = result[option].as<std::string>();
        const std::optional<U64> number = parse_number(value);
        if (!number)
        {
            std::cerr << "ERROR: Invalid number for --" << option << ": " << value << "\n";
            parse_error = true;
            return 0;
        }
        return static_cast<word>(*number);
    };

    args.ram_npages = number_option("ram-pages");
    args.ram_start_page = number_option("ram-start");
    args.rom_npages = number_option("rom-pages");
    args.rom_start_page = number_option("rom-start");
    args.pc = number_option("pc");
    args.limit = number_option("limit");
    args.flags = number_option("flags");
    args.history_size = number_option("history");

    const std::string format = result["format"].as<std::string>();
    if (format != "pretty" && format != "plain")
    {
        std::cerr << "ERROR: --format must be 'pretty' or 'plain'\n";
        parse_error = true;
    }
    args.plain = format == "plain";

    if (result.count("mem"))
    {
        for (const std::string &range : result["mem"].as<std::vector<std::string>>())
        {
            const size_t colon = range.find(':');
            const std::optional<U64> addr =
                colon == std::string::npos ? std::nullopt : parse_number(range.substr(0, colon));
            const std::optional<U64> len =
                colon == std::string::npos ? std::nullopt : parse_number(range.substr(colon + 1));
            if (!addr || !len)
            {
                std::cerr << "ERROR: Invalid --mem range (expected addr:len): " << range << "\n";
                parse_error = true;
                continue;
            }
            args.mem_ranges.emplace_back(static_cast<word>(*addr), static_cast<word>(*len));
        }
    }

    if (result.count("reg"))
    {
        U64 written_registers = 0;
        for (const std::string &entry : result["reg"].as<std::vector<std::string>>())
        {
            const size_t eq = entry.find('=');
            const std::optional<U8> reg =
                eq == std::string::npos ? std::nullopt : parse_register_name(entry.substr(0, eq));
            const std::optional<U64> val =
                eq == std::string::npos ? std::nullopt : parse_number(entry.substr(eq + 1));
            if (!reg || !val)
            {
                std::cerr << "ERROR: Invalid --reg entry (expected xN=value): " << entry << "\n";
                parse_error = true;
                continue;
            }

            if (written_registers & (1ULL << *reg))
            {
                std::cerr << "ERROR: Register defined more than once: " << entry << "\n";
                parse_error = true;
                continue;
            }
            written_registers |= 1ULL << *reg;
            args.reg_values.emplace_back(*reg, static_cast<word>(*val));
        }
    }

    if (result.count("disk-file"))
    {
        if (!result.count("disk-pages") || !result.count("disk-start"))
        {
            std::cerr << "ERROR: --disk-pages and --disk-start are required with --disk-file\n";
            parse_error = true;
        }
        else
        {
            args.disk_npages = number_option("disk-pages");
            args.disk_start_page = number_option("disk-start");
        }
    }

    // Breakpoints can be symbols, which are known once the executable is read.
    if (result.count("exe") && std::filesystem::is_regular_file(result["exe"].as<std::string>()))
    {
        args.symbols = read_symbols(result["exe"].as<std::string>());
    }

    if (result.count("break"))
    {
        for (const std::string &text : result["break"].as<std::vector<std::string>>())
        {
            const std::optional<word> address = resolve_address(args.symbols, text);
            if (!address)
            {
                std::cerr << "ERROR: --break '" << text
                          << "' is not an address or a symbol of the executable\n";
                parse_error = true;
                continue;
            }
            args.breakpoints.push_back(*address);
        }
    }

    if (result.count("watch"))
    {
        for (const std::string &text : result["watch"].as<std::vector<std::string>>())
        {
            const std::optional<WatchSpec> watch = parse_watch_spec(args.symbols, text);
            if (!watch)
            {
                std::cerr << "ERROR: --watch '" << text
                          << "' is not ADDR|SYMBOL[:LENGTH][:r|w|rw][:p][:OPVALUE]\n";
                parse_error = true;
                continue;
            }
            args.watchpoints.push_back(*watch);
        }
    }

    if (result.count("watch-reg"))
    {
        for (const std::string &text : result["watch-reg"].as<std::vector<std::string>>())
        {
            const std::optional<RegisterWatchSpec> watch = parse_register_watch_spec(text);
            if (!watch)
            {
                std::cerr << "ERROR: --watch-reg '" << text << "' is not REG[=VALUE]\n";
                parse_error = true;
                continue;
            }
            args.register_watches.push_back(*watch);
        }
    }

    if (result.count("trace"))
    {
        const std::string path = result["trace"].as<std::string>();
        if (path == "-")
        {
            args.trace = &std::cout;
        }
        else
        {
            args.trace_file.open(path);
            if (!args.trace_file)
            {
                std::cerr << "ERROR: Cannot open trace file: " << path << "\n";
                parse_error = true;
            }
            args.trace = &args.trace_file;
        }
    }

    if (result.count("block-sectors"))
    {
        const std::optional<U64> number = parse_number(result["block-sectors"].as<std::string>());
        if (!number)
        {
            std::cerr << "ERROR: Invalid number for --block-sectors\n";
            parse_error = true;
        }
        else
        {
            args.block_sectors = static_cast<word>(*number);
        }
    }

    return !parse_error;
}

/// Makes the machine, loads the program and sets the state and the devices up as asked.
///
/// @return the machine, or null (after saying why) if a file could not be read
static std::unique_ptr<Emulator32bit> make_machine(const cxxopts::ParseResult &result,
                                                   CliArgs &args)
{
    auto ram = std::make_unique<RAM>(args.ram_npages, args.ram_start_page);
    auto rom = result.count("rom-file")
                   ? std::make_unique<ROM>(File(result["rom-file"].as<std::string>()),
                                           args.rom_npages, args.rom_start_page)
                   : std::make_unique<ROM>(args.rom_npages, args.rom_start_page);

    std::unique_ptr<Disk> disk;
    if (result.count("disk-file"))
    {
        disk = std::make_unique<Disk>(File(result["disk-file"].as<std::string>()),
                                      args.disk_npages, args.disk_start_page);
    }
    else
    {
        // A default constructed Disk has no backing file and errors when it saves.
        disk = std::make_unique<MockDisk>();
    }

    auto emu = std::make_unique<Emulator32bit>(std::move(ram), std::move(rom), std::move(disk));

    if (result.count("exe"))
    {
        const std::string exe_path = result["exe"].as<std::string>();
        if (!std::filesystem::is_regular_file(exe_path))
        {
            std::cerr << "ERROR: Executable does not exist: " << exe_path << "\n";
            return nullptr;
        }

        emu->mmu->begin_process();
        LoadExecutable(*emu, File(exe_path)).load();
    }
    else
    {
        emu->set_pc(args.pc);
    }

    emu->set_NZCV(args.flags & 0b1000, args.flags & 0b0100, args.flags & 0b0010,
                  args.flags & 0b0001);
    for (const auto &[reg, val] : args.reg_values)
    {
        emu->write_reg(reg, val);
    }

    emu->set_semihosting(result.count("no-semihosting") == 0);

    if (result.count("block-file"))
    {
        const std::string block_path = result["block-file"].as<std::string>();
        if (!emu->system_bus->block.open_file(block_path, args.block_sectors))
        {
            std::cerr << "ERROR: Cannot read the block device file: " << block_path << "\n";
            return nullptr;
        }
    }
    else if (args.block_sectors != 0)
    {
        emu->system_bus->block.set_capacity(args.block_sectors);
    }

    if (result.count("console-input"))
    {
        const std::string input_path = result["console-input"].as<std::string>();
        std::ifstream input(input_path, std::ios::binary);
        if (!input)
        {
            std::cerr << "ERROR: Cannot read the console input: " << input_path << "\n";
            return nullptr;
        }
        std::stringstream bytes;
        bytes << input.rdbuf();
        emu->system_bus->console.push_input(bytes.str());
    }

    if (result.count("console-stdin"))
    {
        // What is typed goes to the console while the machine runs. The thread reads a character
        // at a time and is left blocked on the terminal when the machine ends.
        const auto input = std::make_shared<HostInput>();
        emu->system_bus->console.attach_host_input(input);
        std::thread(
            [input]
            {
                char c;
                while (std::cin.get(c))
                {
                    input->push(std::string(1, c));
                }
                input->close();
            })
            .detach();
    }

    emu->set_symbols(&args.symbols);
    emu->set_trace(args.trace);
    emu->set_history_size(args.history_size);
    for (const word breakpoint : args.breakpoints)
    {
        emu->add_breakpoint(breakpoint);
    }
    for (const WatchSpec &watch : args.watchpoints)
    {
        emu->add_watchpoint(watch.address, watch.length, watch.kind, watch.physical, watch.compare,
                            watch.compare_value);
    }
    for (const RegisterWatchSpec &watch : args.register_watches)
    {
        emu->add_register_watch(watch.reg, watch.value);
    }
    return emu;
}

/// Writes the state after a run where `--output` says, and returns the exit code of the run.
static int report(const cxxopts::ParseResult &result, const CliArgs &args, Emulator32bit &emu,
                  const Emulator32bit::RunResult &run_result)
{
    using ExitCode = Emulator32bit::EmuCLIExitCode;

    std::ofstream out_file;
    if (result.count("output"))
    {
        out_file.open(result["output"].as<std::string>());
        if (!out_file)
        {
            std::cerr << "ERROR: Cannot open output file: " << result["output"].as<std::string>()
                      << "\n";
            return S32(ExitCode::EXIT_USAGE_ERROR);
        }
    }
    std::ostream &out = out_file.is_open() ? out_file : std::cout;

    if (args.plain)
    {
        print_plain(out, emu, run_result, args.mem_ranges);
    }
    else
    {
        emu.print();
        std::printf("\n");
    }
    if (args.history_size != 0)
    {
        print_history(out, emu, args.symbols);
    }

    switch (run_result.status)
    {
    case Emulator32bit::RunResult::Status::HALTED:
        return S32(ExitCode::EXIT_HALTED);
    case Emulator32bit::RunResult::Status::LIMIT_REACHED:
        return S32(ExitCode::EXIT_LIMIT_REACHED);
    case Emulator32bit::RunResult::Status::BREAKPOINT:
        return S32(ExitCode::EXIT_BREAKPOINT);
    case Emulator32bit::RunResult::Status::FAULT:
    default:
        return S32(ExitCode::EXIT_FAULT);
    }
}

static int run_cli(int argc, char *argv[])
{
    using ExitCode = Emulator32bit::EmuCLIExitCode;

    cxxopts::Options options = make_options();
    cxxopts::ParseResult result;
    try
    {
        result = options.parse(argc, argv);
    }
    catch (const cxxopts::exceptions::exception &e)
    {
        std::cerr << "ERROR: " << e.what() << "\n";
        return S32(ExitCode::EXIT_USAGE_ERROR);
    }

    if (result.count("help"))
    {
        // clang-format off
        std::cout << options.help () << "\n";
        std::cout << "Exit codes: "
                  << S32(ExitCode::EXIT_HALTED) << " halted, "
                  << S32(ExitCode::EXIT_USAGE_ERROR) << " usage error, "
                  << S32(ExitCode::EXIT_LIMIT_REACHED) << " instruction limit reached, "
                  << S32(ExitCode::EXIT_FAULT) << " emulator fault, "
                  << S32(ExitCode::EXIT_BREAKPOINT) << " breakpoint\n";
        // clang-format on
        return S32(Emulator32bit::EmuCLIExitCode::EXIT_HALTED);
    }

    if (!result.unmatched().empty())
    {
        std::cerr << "ERROR: Unexpected argument: " << result.unmatched().front() << "\n";
        return S32(ExitCode::EXIT_USAGE_ERROR);
    }

    if (result.count("console-stdin") && result.count("debug"))
    {
        std::cerr << "ERROR: --console-stdin and --debug both read the standard input\n";
        return S32(ExitCode::EXIT_USAGE_ERROR);
    }

    CliArgs args;
    if (!parse_args(result, args))
    {
        return S32(ExitCode::EXIT_USAGE_ERROR);
    }

    std::unique_ptr<Emulator32bit> emu = make_machine(result, args);
    if (emu == nullptr)
    {
        return S32(ExitCode::EXIT_USAGE_ERROR);
    }

    if (result.count("debug"))
    {
        Debugger debugger(*emu, args.symbols, std::cin, std::cout);
        debugger.run();

        const auto &finished = debugger.finished();
        if (finished && finished->status == Emulator32bit::RunResult::Status::FAULT)
        {
            return S32(ExitCode::EXIT_FAULT);
        }
        return S32(ExitCode::EXIT_HALTED);
    }

    return report(result, args, *emu, emu->run(args.limit));
}

int main(int argc, char *argv[])
{
    // Errors in the libraries (a bad executable, ...) are thrown. They are already logged by the
    // time they get here, so all that is left to do is to fail.
    aemu::log::set_fatal_action(aemu::log::FatalAction::Throw);
    try
    {
        return run_cli(argc, argv);
    }
    catch (const aemu::log::FatalError &)
    {
        return S32(Emulator32bit::EmuCLIExitCode::EXIT_USAGE_ERROR);
    }
    catch (const std::exception &error)
    {
        std::cerr << "ERROR: " << error.what() << "\n";
        return S32(Emulator32bit::EmuCLIExitCode::EXIT_USAGE_ERROR);
    }
}
