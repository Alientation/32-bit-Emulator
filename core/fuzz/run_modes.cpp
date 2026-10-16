// Fuzz target of the emulator's run loop: the input is the code of a program, which is run in three
// ways that have to end the same: run (N), N times run (1), and run (N) with every debugging hook on
// (resumed after each breakpoint and watch). The machine is compared after each: the result of the
// run, the registers, the system registers, the RAM and what the program printed. A difference,
// a crash, a sanitizer report, a failed assertion, an exception or a hang is a bug.

#include "emulator32bit/emulator32bit.h"
#include "util/logger.h"

#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <format>
#include <iostream>
#include <sstream>
#include <string>
#include <vector>

namespace
{

/// The instructions each run executes in all.
constexpr U64 kInstructions = 4096;

/// The most words of the input that become code.
constexpr size_t kMaxCodeWords = 1024;

/// Where the code is, and the vector table (VBAR), so that an exception runs the input as well.
constexpr word kCode = 0x8000;

/// The machine of the tests (assembler_test.h): 16 pages of RAM, then 16 of ROM.
constexpr word kRamPages = 16;
constexpr word kRomPages = 16;

/// Where the registers point: a register that is not a device's base has its own address in RAM.
constexpr word kRegisterArea = 0x2000;
constexpr word kRegisterStride = 0x40;

/// The registers that point at a device, so that a program reaches them without building the
/// address.
constexpr U8 kDeviceRegisters[] = {20, 21, 22, 23, 24};
constexpr word kDeviceBases[] = {kIntcBase, kTimerBase, kConsoleBase, kSysctlBase, kRtcBase};

/// The hooks of the third run: breakpoints at these instructions of the code, a watch on the
/// area the registers point at, one on a physical page of the code, and watches on registers.
constexpr word kBreakpointWords[] = {3, 17, 64, 255};
constexpr U8 kWatchedRegister = 5;
constexpr U8 kWatchedRegisterForValue = 9;
constexpr word kWatchedRegisterValue = 0;
constexpr size_t kHistory = 64;

/// What a machine looks like after a run.
struct Outcome
{
    Emulator32bit::RunResult::Status status = Emulator32bit::RunResult::Status::LIMIT_REACHED;
    U64 instructions = 0;
    std::string message;
    word pc = 0;
    word pstate = 0;
    word registers[kNumReg] = {};
    word sysregs[encoding::kSysregCount] = {};
    U8 exception_depth = 0;
    std::vector<byte> ram;
    std::string output;
    std::string errors;
    std::string console;
};

/// A machine with the code of the input at kCode and the state that run_once of the round trip
/// test starts from: kernel mode, the vector table, the registers pointing into the RAM (or at a
/// device), the RAM not zero.
class Machine
{
  public:
    explicit Machine(const std::vector<word> &code) :
        emu(std::make_unique<RAM>(kRamPages, 0), std::make_unique<ROM>(kRomPages, kRamPages),
            std::make_unique<MockDisk>())
    {
        emu.set_output(output);
        emu.set_error_output(errors);
        emu.system_bus->console.set_output(&console);
        for (word i = 0; i < kRamPages * kPageSize / sizeof(word); i++)
        {
            emu.system_bus->write_physical_word(i * sizeof(word), i * 0x9E3779B9u);
        }
        for (U8 reg = 0; reg < register_to_U8(Register::XZR); reg++)
        {
            emu.write_reg(reg, kRegisterArea + kRegisterStride * reg);
        }
        for (size_t i = 0; i < std::size(kDeviceRegisters); i++)
        {
            emu.write_reg(kDeviceRegisters[i], kDeviceBases[i]);
        }
        emu.write_sysreg(encoding::kSysregId_vbar, kCode);
        for (size_t i = 0; i < code.size(); i++)
        {
            emu.system_bus->write_physical_word(kCode + word(i * sizeof(word)), code[i]);
        }
        emu.set_pc(kCode);
    }

    /// @param[in] last The result of the last run.
    /// @param[in] instructions The instructions of all the runs.
    /// @return The machine now.
    Outcome outcome(const Emulator32bit::RunResult &last, const U64 instructions)
    {
        Outcome out;
        out.status = last.status;
        out.instructions = instructions;
        out.message = last.message;
        out.pc = emu.get_pc();
        out.pstate = emu.get_pstate();
        for (U8 reg = 0; reg < kNumReg; reg++)
        {
            out.registers[reg] = emu.read_reg(reg);
        }
        for (U8 id = 0; id < encoding::kSysregCount; id++)
        {
            out.sysregs[id] = emu.read_sysreg(id);
        }
        out.exception_depth = emu.exception_depth();
        out.ram.resize(kRamPages * kPageSize);
        for (word page = 0; page < kRamPages; page++)
        {
            emu.system_bus->read_page(out.ram.data() + page * kPageSize, page);
        }
        out.output = output.str();
        out.errors = errors.str();
        out.console = console.str();
        return out;
    }

  private:
    // Before the machine, which writes to them.
    std::ostringstream output;
    std::ostringstream errors;
    std::ostringstream console;

  public:
    Emulator32bit emu;
};

/// The run that the others are compared with.
Outcome run_at_once(const std::vector<word> &code)
{
    Machine machine(code);
    const Emulator32bit::RunResult result = machine.emu.run(kInstructions);
    return machine.outcome(result, result.instructions_ran);
}

/// An instruction at a time, until one does not end with the limit.
Outcome run_one_at_a_time(const std::vector<word> &code)
{
    Machine machine(code);
    Emulator32bit::RunResult result = {};
    U64 ran = 0;
    do
    {
        result = machine.emu.run(1);
        ran += result.instructions_ran;
    } while (result.status == Emulator32bit::RunResult::Status::LIMIT_REACHED
             && ran < kInstructions);
    return machine.outcome(result, ran);
}

/// With breakpoints, watchpoints, register watches, the history and the trace, resumed after each
/// stop at a breakpoint or a watch, which changes nothing but where the run stops.
Outcome run_hooked(const std::vector<word> &code, std::ostream &trace)
{
    Machine machine(code);
    Emulator32bit &emu = machine.emu;
    for (const word i : kBreakpointWords)
    {
        emu.add_breakpoint(kCode + i * word(sizeof(word)));
    }
    emu.add_watchpoint(kRegisterArea, kRegisterStride * kNumReg, Emulator32bit::WatchKind::ACCESS);
    emu.add_watchpoint(kCode, kPageSize, Emulator32bit::WatchKind::WRITE, true);
    emu.add_watchpoint(0, kRegisterArea, Emulator32bit::WatchKind::READ, false,
                       Emulator32bit::WatchCompare::LT, kRegisterArea);
    emu.add_register_watch(kWatchedRegister);
    emu.add_register_watch(kWatchedRegisterForValue, kWatchedRegisterValue);
    emu.set_history_size(kHistory);
    emu.set_trace(&trace);

    Emulator32bit::RunResult result = {};
    U64 ran = 0;
    do
    {
        result = emu.run(kInstructions - ran);
        ran += result.instructions_ran;
    } while (result.status == Emulator32bit::RunResult::Status::BREAKPOINT
             && result.instructions_ran != 0 && ran < kInstructions);

    // A stop at the last instruction is where the limit would have stopped.
    if (result.status == Emulator32bit::RunResult::Status::BREAKPOINT && ran == kInstructions)
    {
        result.status = Emulator32bit::RunResult::Status::LIMIT_REACHED;
        result.message.clear();
    }
    return machine.outcome(result, ran);
}

/// Stops the fuzzer, with the field that differs, if the outcomes are not the same.
void require_same(const Outcome &expected, const Outcome &actual, const char *how)
{
    const char *field = nullptr;
    if (expected.status != actual.status) field = "status";
    else if (expected.instructions != actual.instructions) field = "instructions";
    else if (expected.message != actual.message) field = "message";
    else if (expected.pc != actual.pc) field = "pc";
    else if (expected.pstate != actual.pstate) field = "pstate";
    else if (std::memcmp(expected.registers, actual.registers, sizeof(expected.registers)) != 0)
        field = "registers";
    else if (std::memcmp(expected.sysregs, actual.sysregs, sizeof(expected.sysregs)) != 0)
        field = "system registers";
    else if (expected.exception_depth != actual.exception_depth) field = "exception depth";
    else if (expected.ram != actual.ram) field = "ram";
    else if (expected.output != actual.output) field = "output";
    else if (expected.errors != actual.errors) field = "error output";
    else if (expected.console != actual.console) field = "console";
    if (field == nullptr) return;

    std::cerr << std::format("{} differs from run ({}) in the {}\n", how, kInstructions, field);
    for (const Outcome *outcome : {&expected, &actual})
    {
        std::cerr << std::format("  {}: status {}, {} instructions, pc {:#x}, '{}'\n",
                                 outcome == &expected ? "run (N)" : how, U8(outcome->status),
                                 outcome->instructions, outcome->pc, outcome->message);
    }
    for (U8 reg = 0; reg < kNumReg; reg++)
    {
        if (expected.registers[reg] != actual.registers[reg])
        {
            std::cerr << std::format("  {}: {:#x}, {:#x}\n", kRegisterNames[reg],
                                     expected.registers[reg], actual.registers[reg]);
        }
    }
    std::abort();
}

} // namespace

extern "C" int LLVMFuzzerTestOneInput(const uint8_t *data, size_t size)
{
    aemu::log::set_level(aemu::log::Level::Off);
    aemu::log::set_fatal_action(aemu::log::FatalAction::Throw);

    std::vector<word> code(size / sizeof(word) < kMaxCodeWords ? size / sizeof(word)
                                                               : kMaxCodeWords);
    if (!code.empty()) std::memcpy(code.data(), data, code.size() * sizeof(word));

    const Outcome at_once = run_at_once(code);
    require_same(at_once, run_one_at_a_time(code), "run (1) at a time");

    // The trace is formatted, but not kept.
    std::ostream discard(nullptr);
    require_same(at_once, run_hooked(code, discard), "run (N) with the hooks");
    return 0;
}
