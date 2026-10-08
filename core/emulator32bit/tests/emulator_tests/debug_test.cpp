#include <emulator32bit_test/emulator32bit_test.h>

#include <emulator32bit/debugger.h>
#include <emulator32bit/symbols.h>

#include <memory>
#include <optional>
#include <sstream>
#include <vector>

namespace
{

using Status = Emulator32bit::RunResult::Status;

/// An opcode that is not assigned, running it faults with "Bad opcode".
constexpr word kBadInstruction = 0xFC000000;

word add_imm(const int xd, const int xn, const int imm, const bool s = false)
{
    return Emulator32bit::asm_format_o(Emulator32bit::_op_add, s, xd, xn, imm);
}

/// x0 += 1 three times, then hlt: the additions are at 0, 4 and 8, the hlt at 12.
class DebugFixture : public EmulatorFixture
{
  protected:
    void SetUp() override
    {
        write_program(
            {add_imm(0, 0, 1), add_imm(0, 0, 1), add_imm(0, 0, 1), Emulator32bit::asm_hlt()});
    }

    void write_program(const std::vector<word> &program)
    {
        for (size_t i = 0; i < program.size(); i++)
        {
            cpu.system_bus->write_word(word(i * 4), program[i]);
        }
        cpu.set_pc(0);
    }
};

using BreakpointTest = DebugFixture;
using TraceTest = DebugFixture;
using HistoryTest = DebugFixture;

TEST_F(BreakpointTest, stops_before_the_instruction)
{
    cpu.add_breakpoint(8);
    const auto result = cpu.run(0);

    EXPECT_EQ(result.status, Status::BREAKPOINT);
    EXPECT_EQ(result.instructions_ran, 2u);
    EXPECT_EQ(cpu.get_pc(), 8u);
    EXPECT_EQ(cpu.read_reg(0), 2u);
    EXPECT_NE(result.message.find("0x00000008"), std::string::npos) << result.message;
}

TEST_F(BreakpointTest, the_next_run_goes_on_from_the_breakpoint)
{
    cpu.add_breakpoint(8);
    ASSERT_EQ(cpu.run(0).status, Status::BREAKPOINT);

    const auto result = cpu.run(0);
    EXPECT_EQ(result.status, Status::HALTED);
    EXPECT_EQ(cpu.read_reg(0), 3u);
}

TEST_F(BreakpointTest, a_single_step_executes_the_instruction_at_a_breakpoint)
{
    cpu.add_breakpoint(0);
    const auto result = cpu.run(1);

    EXPECT_EQ(result.status, Status::LIMIT_REACHED);
    EXPECT_EQ(cpu.read_reg(0), 1u);
}

TEST_F(BreakpointTest, a_breakpoint_at_the_start_does_not_stop_the_first_instruction)
{
    cpu.add_breakpoint(0);
    EXPECT_EQ(cpu.run(0).status, Status::HALTED);
}

TEST_F(BreakpointTest, removed_and_cleared_breakpoints_do_not_stop)
{
    cpu.add_breakpoint(4);
    cpu.add_breakpoint(8);
    EXPECT_EQ(cpu.breakpoints().size(), 2u);

    EXPECT_TRUE(cpu.remove_breakpoint(4));
    EXPECT_FALSE(cpu.remove_breakpoint(4));
    EXPECT_EQ(cpu.run(0).status, Status::BREAKPOINT);
    EXPECT_EQ(cpu.get_pc(), 8u);

    cpu.clear_breakpoints();
    EXPECT_TRUE(cpu.breakpoints().empty());
    EXPECT_EQ(cpu.run(0).status, Status::HALTED);
}

TEST_F(TraceTest, writes_a_line_per_instruction_with_the_changes)
{
    std::ostringstream trace;
    cpu.set_trace(&trace);
    cpu.run(0);

    const std::string text = trace.str();
    EXPECT_NE(text.find("0x00000000: "), std::string::npos) << text;
    EXPECT_NE(text.find("0x00000008: "), std::string::npos) << text;
    EXPECT_NE(text.find("x0=0x0->0x1"), std::string::npos) << text;
    EXPECT_NE(text.find("x0=0x2->0x3"), std::string::npos) << text;
    EXPECT_NE(text.find("0x0000000c: "), std::string::npos) << text;
    EXPECT_NE(text.find("; halt"), std::string::npos) << text;
}

TEST_F(TraceTest, off_by_default_and_after_nullptr)
{
    std::ostringstream trace;
    cpu.set_trace(&trace);
    cpu.set_trace(nullptr);
    cpu.run(0);

    EXPECT_TRUE(trace.str().empty());
}

TEST_F(TraceTest, shows_flag_changes_and_taken_branches)
{
    write_program({add_imm(0, 0, 0, true),
                   Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, 2),
                   Emulator32bit::asm_hlt(), Emulator32bit::asm_hlt()});
    std::ostringstream trace;
    cpu.set_trace(&trace);
    cpu.run(2);

    const std::string text = trace.str();
    EXPECT_NE(text.find("NZCV=nzcv->nZcv"), std::string::npos) << text;
    // The branch at 4 skips one instruction and goes to 12.
    EXPECT_NE(text.find("pc=0xc"), std::string::npos) << text;
}

TEST_F(TraceTest, shows_why_an_instruction_faulted)
{
    write_program({kBadInstruction});
    std::ostringstream trace;
    cpu.set_trace(&trace);
    EXPECT_EQ(cpu.run(0).status, Status::FAULT);

    EXPECT_NE(trace.str().find("Bad opcode"), std::string::npos) << trace.str();
}

TEST_F(TraceTest, names_the_addresses_it_knows)
{
    SymbolMap symbols;
    symbols.add("start", 0);
    cpu.set_symbols(&symbols);
    std::ostringstream trace;
    cpu.set_trace(&trace);
    cpu.run(2);

    EXPECT_NE(trace.str().find("0x00000000 <start>: "), std::string::npos) << trace.str();
    EXPECT_NE(trace.str().find("0x00000004 <start+0x4>: "), std::string::npos) << trace.str();
}

TEST_F(HistoryTest, keeps_the_newest_instructions)
{
    cpu.set_history_size(2);
    cpu.run(0);

    const auto history = cpu.history();
    ASSERT_EQ(history.size(), 2u);
    EXPECT_EQ(history[0].pc, 8u);
    EXPECT_EQ(history[0].instruction, add_imm(0, 0, 1));
    EXPECT_EQ(history[1].pc, 12u);
    EXPECT_EQ(history[1].instruction, Emulator32bit::asm_hlt());
}

TEST_F(HistoryTest, ends_with_the_instruction_that_faulted)
{
    write_program({add_imm(0, 0, 1), kBadInstruction});
    cpu.set_history_size(8);
    EXPECT_EQ(cpu.run(0).status, Status::FAULT);

    const auto history = cpu.history();
    ASSERT_EQ(history.size(), 2u);
    EXPECT_EQ(history.back().pc, 4u);
    EXPECT_EQ(history.back().instruction, kBadInstruction);
}

TEST_F(HistoryTest, a_size_of_zero_turns_it_off)
{
    cpu.set_history_size(4);
    cpu.run(2);
    EXPECT_EQ(cpu.history().size(), 2u);

    cpu.set_history_size(0);
    EXPECT_TRUE(cpu.history().empty());
    cpu.run(0);
    EXPECT_TRUE(cpu.history().empty());
}

TEST(SymbolMapTest, describes_addresses_relative_to_the_closest_symbol_below)
{
    SymbolMap symbols;
    symbols.add("main", 0x100);
    symbols.add("helper", 0x140);

    EXPECT_EQ(symbols.describe(0x100), "main");
    EXPECT_EQ(symbols.describe(0x108), "main+0x8");
    EXPECT_EQ(symbols.describe(0x140), "helper");
    EXPECT_EQ(symbols.describe(0x1000), "helper+0xec0");
    EXPECT_EQ(symbols.describe(0xFC), "");
}

TEST(SymbolMapTest, finds_symbols_by_name_and_keeps_the_first_name_of_an_address)
{
    SymbolMap symbols;
    EXPECT_TRUE(symbols.empty());
    symbols.add("first", 0x10);
    symbols.add("second", 0x10);

    EXPECT_FALSE(symbols.empty());
    EXPECT_EQ(symbols.find("second"), std::optional<word>(0x10));
    EXPECT_EQ(symbols.find("missing"), std::nullopt);
    EXPECT_EQ(symbols.describe(0x10), "first");
}

TEST(ParseTest, numbers_registers_and_addresses)
{
    EXPECT_EQ(parse_number("42"), std::optional<U64>(42));
    EXPECT_EQ(parse_number("0x2A"), std::optional<U64>(42));
    EXPECT_EQ(parse_number("0b101010"), std::optional<U64>(42));
    EXPECT_EQ(parse_number("12abc"), std::nullopt);
    EXPECT_EQ(parse_number(""), std::nullopt);

    EXPECT_EQ(parse_register_name("x5"), std::optional<U8>(5));
    EXPECT_EQ(parse_register_name("sp"), std::optional<U8>(30));
    EXPECT_EQ(parse_register_name("xzr"), std::optional<U8>(31));
    EXPECT_EQ(parse_register_name("x32"), std::nullopt);
    EXPECT_EQ(parse_register_name("pc"), std::nullopt);

    SymbolMap symbols;
    symbols.add("main", 0x100);
    EXPECT_EQ(resolve_address(symbols, "0x20"), std::optional<word>(0x20));
    EXPECT_EQ(resolve_address(symbols, "main"), std::optional<word>(0x100));
    EXPECT_EQ(resolve_address(symbols, "main+8"), std::optional<word>(0x108));
    EXPECT_EQ(resolve_address(symbols, "other"), std::nullopt);
    EXPECT_EQ(resolve_address(symbols, "other+8"), std::nullopt);
}

class DebuggerTest : public DebugFixture
{
  protected:
    SymbolMap symbols;
    std::istringstream in;
    std::ostringstream out;
    std::unique_ptr<Debugger> debugger;

    void SetUp() override
    {
        DebugFixture::SetUp();
        symbols.add("start", 0);
        debugger = std::make_unique<Debugger>(cpu, symbols, in, out);
    }

    /// Executes a command and returns what it printed.
    std::string command(const std::string &line)
    {
        out.str("");
        debugger->execute(line);
        return out.str();
    }
};

TEST_F(DebuggerTest, step_executes_and_shows_the_next_instruction)
{
    const std::string text = command("step");
    EXPECT_EQ(cpu.read_reg(0), 1u);
    EXPECT_NE(text.find("=> 0x00000004 <start+0x4>: "), std::string::npos) << text;

    command("s 2");
    EXPECT_EQ(cpu.read_reg(0), 3u);
    EXPECT_EQ(debugger->instructions(), 3u);
}

TEST_F(DebuggerTest, continue_runs_to_a_breakpoint_then_to_the_halt)
{
    EXPECT_NE(command("break start+8").find("Breakpoint at 0x00000008 <start+0x8>"),
              std::string::npos);
    EXPECT_NE(command("continue").find("Breakpoint at 0x00000008"), std::string::npos);
    EXPECT_EQ(cpu.get_pc(), 8u);
    EXPECT_FALSE(debugger->finished());

    EXPECT_NE(command("c").find("halted"), std::string::npos);
    ASSERT_TRUE(debugger->finished());
    EXPECT_EQ(debugger->finished()->status, Status::HALTED);

    EXPECT_NE(command("step").find("has ended"), std::string::npos);
}

TEST_F(DebuggerTest, a_fault_is_reported_with_the_history)
{
    write_program({add_imm(0, 0, 1), kBadInstruction});
    const std::string text = command("continue");

    EXPECT_NE(text.find("Fault: "), std::string::npos) << text;
    EXPECT_NE(text.find("Bad opcode"), std::string::npos) << text;
    ASSERT_TRUE(debugger->finished());
    EXPECT_EQ(debugger->finished()->status, Status::FAULT);
}

TEST_F(DebuggerTest, set_pc_runs_the_program_again)
{
    command("continue");
    ASSERT_TRUE(debugger->finished());

    command("set pc 0");
    EXPECT_FALSE(debugger->finished());
    command("set x0 10");
    EXPECT_EQ(cpu.read_reg(0), 10u);
    command("continue");
    EXPECT_EQ(cpu.read_reg(0), 13u);
}

TEST_F(DebuggerTest, breakpoints_can_be_listed_and_deleted)
{
    EXPECT_NE(command("breaks").find("No breakpoints"), std::string::npos);
    command("break 4");
    command("break");
    EXPECT_EQ(cpu.breakpoints().size(), 2u); // 4 and the pc, 0

    EXPECT_NE(command("delete 4").find("removed"), std::string::npos);
    EXPECT_NE(command("delete 4").find("no breakpoint"), std::string::npos);
    command("delete all");
    EXPECT_TRUE(cpu.breakpoints().empty());
}

TEST_F(DebuggerTest, shows_registers_memory_and_code)
{
    command("step 2");
    EXPECT_NE(command("regs").find("x0 0x00000002"), std::string::npos);
    EXPECT_NE(command("regs").find("NZCV"), std::string::npos);

    cpu.system_bus->write_byte(0x100, 0xDE);
    cpu.system_bus->write_byte(0x101, 0xAD);
    const std::string mem = command("mem 0x100 2");
    EXPECT_NE(mem.find("0x00000100: de ad"), std::string::npos) << mem;

    const std::string code = command("disasm start 3");
    EXPECT_NE(code.find("   0x00000000 <start>: "), std::string::npos) << code;
    EXPECT_NE(code.find("=> 0x00000008 <start+0x8>: "), std::string::npos) << code;
}

TEST_F(DebuggerTest, backtrace_follows_the_frame_records)
{
    symbols.add("inner", 0x10);
    symbols.add("outer", 0x40);
    // inner was called from outer+4, outer from start+8.
    cpu.system_bus->write_word(0x200, 0x300);
    cpu.system_bus->write_word(0x204, 0x44);
    cpu.system_bus->write_word(0x300, 0);
    cpu.system_bus->write_word(0x304, 0x08);
    cpu.set_pc(0x14);
    cpu.write_reg(Register::FP, 0x200);

    const std::string text = command("bt");
    EXPECT_NE(text.find("#0  0x00000014 <inner+0x4>\n"), std::string::npos) << text;
    EXPECT_NE(text.find("#1  0x00000044 <outer+0x4>\n"), std::string::npos) << text;
    EXPECT_NE(text.find("#2  0x00000008 <start+0x8>\n"), std::string::npos) << text;
    EXPECT_EQ(text.find("#3"), std::string::npos) << text;
}

TEST_F(DebuggerTest, backtrace_at_a_function_entry_uses_the_link_register)
{
    symbols.add("inner", 0x10);
    symbols.add("outer", 0x40);
    // The record at x28 is outer's: it returns to start+8. inner was called from outer+8.
    cpu.system_bus->write_word(0x200, 0);
    cpu.system_bus->write_word(0x204, 0x08);
    cpu.set_pc(0x10);
    cpu.write_reg(Register::LR, 0x48);
    cpu.write_reg(Register::FP, 0x200);

    const std::string text = command("backtrace");
    EXPECT_NE(text.find("#0  0x00000010 <inner>\n"), std::string::npos) << text;
    EXPECT_NE(text.find("#1  0x00000048 <outer+0x8>\n"), std::string::npos) << text;
    EXPECT_NE(text.find("#2  0x00000008 <start+0x8>\n"), std::string::npos) << text;
}

TEST_F(DebuggerTest, backtrace_stops_at_a_broken_chain)
{
    cpu.set_pc(4);
    EXPECT_EQ(command("bt").find("#1"), std::string::npos); // x28 is 0: only the pc

    cpu.write_reg(Register::FP, 0x202);
    EXPECT_NE(command("bt").find("not word aligned"), std::string::npos);

    cpu.write_reg(Register::FP, 0x10000000);  // not mapped
    EXPECT_NE(command("bt").find("cannot read"), std::string::npos);

    cpu.system_bus->write_word(0x200, 0x100); // below its own record
    cpu.system_bus->write_word(0x204, 0x44);
    cpu.write_reg(Register::FP, 0x200);
    const std::string text = command("bt");
    EXPECT_NE(text.find("#1  0x00000044"), std::string::npos) << text;
    EXPECT_NE(text.find("is not above"), std::string::npos) << text;
}

TEST_F(DebuggerTest, rejects_bad_input_and_quits)
{
    EXPECT_NE(command("frobnicate").find("Unknown command"), std::string::npos);
    EXPECT_NE(command("step zero").find("Expected a number"), std::string::npos);
    EXPECT_NE(command("break nowhere").find("not an address"), std::string::npos);
    EXPECT_NE(command("set x99 1").find("not a register"), std::string::npos);
    EXPECT_NE(command("set pc 3").find("multiple of 4"), std::string::npos);
    EXPECT_EQ(command(""), "");

    EXPECT_TRUE(debugger->execute("help"));
    EXPECT_FALSE(debugger->execute("quit"));
}

TEST_F(DebuggerTest, run_reads_commands_until_quit_or_the_end_of_the_input)
{
    in.str("step\nregs\nquit\nstep\n");
    debugger->run();
    EXPECT_EQ(cpu.read_reg(0), 1u); // the step after quit did not happen

    in.clear();
    in.str("step\n");
    debugger->run(); // ends at the end of the input
    EXPECT_EQ(cpu.read_reg(0), 2u);
}

} // namespace
