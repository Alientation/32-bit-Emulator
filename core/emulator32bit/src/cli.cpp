#include "emulator32bit/emulator32bit.h"

#include "assembler/load_executable.h"
#include "util/file.h"
#include "util/logger.h"

#include "cxxopts.hpp"

#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <ostream>
#include <string>
#include <vector>

namespace
{

/// Parses decimal, hex (0x), octal (leading 0) and binary (0b) numbers.
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

/// Parses a register name: x0-x29, sp, xzr, or a bare register number.
std::optional<U8> parse_register(const std::string &str)
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

const char *status_name(Emulator32bit::RunResult::Status status)
{
    switch (status)
    {
    case Emulator32bit::RunResult::Status::HALTED:
        return "halted";
    case Emulator32bit::RunResult::Status::LIMIT_REACHED:
        return "limit";
    case Emulator32bit::RunResult::Status::FAULT:
    default:
        return "fault";
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
    if (!result.message.empty())
    {
        out << "message=" << result.message << "\n";
    }

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

    // mem[<addr>]=<hex bytes, space separated>
    for (const auto &[addr, len] : mem_ranges)
    {
        out << "mem[" << hex(addr) << "]=";
        for (word i = 0; i < len; i++)
        {
            char buf[4];
            try
            {
                std::snprintf(buf, sizeof(buf), "%02x", emu.system_bus->read_byte(addr + i));
            }
            catch (const std::exception &)
            {
                std::snprintf(buf, sizeof(buf), "??");
            }
            out << (i ? " " : "") << buf;
        }
        out << "\n";
    }
}

} // namespace

static int run_cli(int argc, char *argv[])
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
    // clang-format on

    cxxopts::ParseResult result;
    try
    {
        result = options.parse(argc, argv);
    }
    catch (const cxxopts::exceptions::exception &e)
    {
        std::cerr << "ERROR: " << e.what() << "\n";
        return S32(Emulator32bit::EmuCLIExitCode::EXIT_USAGE_ERROR);
    }

    if (result.count("help"))
    {
        // clang-format off
        std::cout << options.help () << "\n";
        std::cout << "Exit codes: "
                  << S32(Emulator32bit::EmuCLIExitCode::EXIT_HALTED) << " halted, "
                  << S32(Emulator32bit::EmuCLIExitCode::EXIT_USAGE_ERROR) << " usage error, "
                  << S32(Emulator32bit::EmuCLIExitCode::EXIT_LIMIT_REACHED) << " instruction limit reached, "
                  << S32(Emulator32bit::EmuCLIExitCode::EXIT_FAULT) << " emulator fault\n";
        // clang-format on
        return S32(Emulator32bit::EmuCLIExitCode::EXIT_HALTED);
    }

    if (!result.unmatched().empty())
    {
        std::cerr << "ERROR: Unexpected argument: " << result.unmatched().front() << "\n";
        return S32(Emulator32bit::EmuCLIExitCode::EXIT_USAGE_ERROR);
    }

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

    const word ram_npages = number_option("ram-pages");
    const word ram_start_page = number_option("ram-start");
    const word rom_npages = number_option("rom-pages");
    const word rom_start_page = number_option("rom-start");
    const word pc = number_option("pc");
    const word limit = number_option("limit");
    const word flags = number_option("flags");

    const std::string format = result["format"].as<std::string>();
    if (format != "pretty" && format != "plain")
    {
        std::cerr << "ERROR: --format must be 'pretty' or 'plain'\n";
        parse_error = true;
    }

    std::vector<std::pair<word, word>> mem_ranges;
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
            mem_ranges.emplace_back(static_cast<word>(*addr), static_cast<word>(*len));
        }
    }

    std::vector<std::pair<U8, word>> reg_values;
    if (result.count("reg"))
    {
        U64 written_registers = 0;
        for (const std::string &entry : result["reg"].as<std::vector<std::string>>())
        {
            const size_t eq = entry.find('=');
            const std::optional<U8> reg =
                eq == std::string::npos ? std::nullopt : parse_register(entry.substr(0, eq));
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
            reg_values.emplace_back(*reg, static_cast<word>(*val));
        }
    }

    word disk_npages = 0;
    word disk_start_page = 0;
    if (result.count("disk-file"))
    {
        if (!result.count("disk-pages") || !result.count("disk-start"))
        {
            std::cerr << "ERROR: --disk-pages and --disk-start are required with --disk-file\n";
            parse_error = true;
        }
        else
        {
            disk_npages = number_option("disk-pages");
            disk_start_page = number_option("disk-start");
        }
    }

    if (parse_error)
    {
        return S32(Emulator32bit::EmuCLIExitCode::EXIT_USAGE_ERROR);
    }

    RAM *ram = new RAM(ram_npages, ram_start_page);
    ROM *rom = result.count("rom-file")
                   ? new ROM(File(result["rom-file"].as<std::string>()), rom_npages, rom_start_page)
                   : new ROM(rom_npages, rom_start_page);

    Disk *disk;
    if (result.count("disk-file"))
    {
        disk = new Disk(File(result["disk-file"].as<std::string>()), disk_npages, disk_start_page);
    }
    else
    {
        // A default constructed Disk has no backing file and errors when it saves.
        disk = new MockDisk();
    }

    auto emu = std::make_unique<Emulator32bit>(ram, rom, disk);

    if (result.count("exe"))
    {
        const std::string exe_path = result["exe"].as<std::string>();
        if (!std::filesystem::is_regular_file(exe_path))
        {
            std::cerr << "ERROR: Executable does not exist: " << exe_path << "\n";
            return S32(Emulator32bit::EmuCLIExitCode::EXIT_USAGE_ERROR);
        }

        emu->system_bus->mmu->begin_process();
        LoadExecutable(*emu, File(exe_path)).load();
    }
    else
    {
        emu->set_pc(pc);
    }

    emu->set_NZCV(flags & 0b1000, flags & 0b0100, flags & 0b0010, flags & 0b0001);
    for (const auto &[reg, val] : reg_values)
    {
        emu->write_reg(reg, val);
    }

    const Emulator32bit::RunResult run_result = emu->run(limit);

    std::ofstream out_file;
    if (result.count("output"))
    {
        out_file.open(result["output"].as<std::string>());
        if (!out_file)
        {
            std::cerr << "ERROR: Cannot open output file: " << result["output"].as<std::string>()
                      << "\n";
            return S32(Emulator32bit::EmuCLIExitCode::EXIT_USAGE_ERROR);
        }
    }
    std::ostream &out = out_file.is_open() ? out_file : std::cout;

    if (format == "plain")
    {
        print_plain(out, *emu, run_result, mem_ranges);
    }
    else
    {
        emu->print();
        std::printf("\n");
    }

    switch (run_result.status)
    {
    case Emulator32bit::RunResult::Status::HALTED:
        return S32(Emulator32bit::EmuCLIExitCode::EXIT_HALTED);
    case Emulator32bit::RunResult::Status::LIMIT_REACHED:
        return S32(Emulator32bit::EmuCLIExitCode::EXIT_LIMIT_REACHED);
    case Emulator32bit::RunResult::Status::FAULT:
    default:
        return S32(Emulator32bit::EmuCLIExitCode::EXIT_FAULT);
    }
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
