// Fuzz target of the debugger of `emu32 --debug`: the input is command lines, which
// Debugger::execute runs on a machine with a small program at 0 and a few symbols. A crash, a
// sanitizer report, a failed assertion, an exception or a hang is a bug.
//
// `step` and `continue` run at most kMaxRun instructions each (the target lowers a larger count, and
// gives `continue` without one that count), and none after kBudget in all, so that a program that
// loops ends. fuzz/debugger.dict has the commands.

#include "emulator32bit/debugger.h"
#include "emulator32bit/emulator32bit.h"
#include "emulator32bit/symbols.h"
#include "util/logger.h"

#include <cstdint>
#include <optional>
#include <sstream>
#include <string>
#include <vector>

namespace
{

/// The machine of the tests (assembler_test.h): 16 pages of RAM, then 16 of ROM.
constexpr word kRamPages = 16;
constexpr word kRomPages = 16;

/// The most instructions of one command, and of all of them.
constexpr U64 kMaxRun = 20000;
constexpr U64 kBudget = 200000;

/// Where x1 points: the program stores to it and loads from it.
constexpr word kData = 0x100;

/// x0 += 1, a store of x0 and a load of it at kData, a brk, then back to the start; a hlt after
/// the loop.
void load_program(Emulator32bit &emu, SymbolMap &symbols)
{
    const word program[] = {
        Emulator32bit::asm_format_o(encoding::_op_add, false, 0, 0, 1),
        Emulator32bit::asm_format_m(encoding::_op_str, false, 0, 1, 0,
                                    encoding::AddrType::ADDR_OFFSET),
        Emulator32bit::asm_format_m(encoding::_op_ldr, false, 2, 1, 0,
                                    encoding::AddrType::ADDR_OFFSET),
        Emulator32bit::asm_brk(0),
        Emulator32bit::asm_format_b1(encoding::_op_b, ConditionCode::AL, -4),
        Emulator32bit::asm_hlt(),
    };
    for (size_t i = 0; i < std::size(program); i++)
    {
        emu.memory.write_word(word(i * sizeof(word)), program[i]);
    }
    emu.write_reg(1, kData);
    emu.set_pc(0);

    symbols.add("start", 0);
    symbols.add("store", 4);
    symbols.add("end", 20);
    symbols.add("data", kData);
}

/// @param[in] line A command line.
/// @param[in] ran The instructions that the commands ran so far.
/// @return The line to execute: a `step` or `continue` with its count lowered to kMaxRun, nothing
///     for one once the budget is spent, the line itself for the other commands.
std::string limited(const std::string &line, const U64 ran)
{
    std::istringstream words(line);
    std::vector<std::string> args;
    for (std::string arg; words >> arg;)
    {
        args.push_back(arg);
    }
    if (args.empty()) return line;

    const bool is_step = args[0] == "step" || args[0] == "s";
    const bool is_continue = args[0] == "continue" || args[0] == "c";
    if (!is_step && !is_continue) return line;
    if (ran >= kBudget) return "";

    if (args.size() == 1) return is_continue ? args[0] + " " + std::to_string(kMaxRun) : line;
    // A count that is not a number stays, for the error.
    const std::optional<U64> count = parse_number(args[1]);
    if (count && (*count > kMaxRun || (is_continue && *count == 0)))
    {
        args[1] = std::to_string(kMaxRun);
    }
    std::string out;
    for (const std::string &arg : args)
    {
        out += (out.empty() ? "" : " ") + arg;
    }
    return out;
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    aemu::log::set_level(aemu::log::Level::Off);
    aemu::log::set_fatal_action(aemu::log::FatalAction::Throw);

    Emulator32bit emu(std::make_unique<RAM>(kRamPages, 0),
                      std::make_unique<ROM>(kRomPages, kRamPages), std::make_unique<MockDisk>());
    std::ostream discard(nullptr);
    emu.set_output(discard);
    emu.set_error_output(discard);
    emu.system_bus->console.set_output(nullptr);
    SymbolMap symbols;
    load_program(emu, symbols);

    std::istringstream in;
    std::ostringstream out;
    Debugger debugger(emu, symbols, in, out);

    std::istringstream lines(std::string(reinterpret_cast<const char *>(data), size));
    for (std::string line; std::getline(lines, line);)
    {
        if (!debugger.execute(limited(line, debugger.instructions()))) break;
        out.str("");
    }
    return 0;
}
