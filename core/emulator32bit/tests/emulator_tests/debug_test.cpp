#include "emulator32bit_test/emulator32bit_test.h"
#include "emulator32bit/debugger.h"
#include "emulator32bit/symbols.h"

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
            cpu.memory.write_word(word(i * 4), program[i]);
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

    cpu.memory.write_byte(0x100, 0xDE);
    cpu.memory.write_byte(0x101, 0xAD);
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
    cpu.memory.write_word(0x200, 0x300);
    cpu.memory.write_word(0x204, 0x44);
    cpu.memory.write_word(0x300, 0);
    cpu.memory.write_word(0x304, 0x08);
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
    cpu.memory.write_word(0x200, 0);
    cpu.memory.write_word(0x204, 0x08);
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

    cpu.memory.write_word(0x200, 0x100);      // below its own record
    cpu.memory.write_word(0x204, 0x44);
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

// Watchpoints. x1 = 0x100 is the address, x2 the value.

constexpr word kWatched = 0x100;

word mem_op(const U8 opcode, const int xt, const int xn, const int offset,
            const Emulator32bit::AddrType mode = Emulator32bit::AddrType::ADDR_OFFSET)
{
    return Emulator32bit::asm_format_m(opcode, false, xt, xn, offset, mode);
}

class WatchpointTest : public DebugFixture
{
  protected:
    void SetUp() override
    {
        write_program({mem_op(Emulator32bit::_op_str, 2, 1, 0), add_imm(0, 0, 1),
                       mem_op(Emulator32bit::_op_ldr, 3, 1, 0), Emulator32bit::asm_hlt()});
        cpu.write_reg(1, kWatched);
        cpu.write_reg(2, 0xCAFE);
    }
};

TEST_F(WatchpointTest, a_write_watchpoint_stops_after_the_store)
{
    cpu.add_watchpoint(kWatched);
    const auto result = cpu.run(0);

    EXPECT_EQ(result.status, Status::BREAKPOINT);
    EXPECT_EQ(result.instructions_ran, 1u);
    EXPECT_EQ(cpu.get_pc(), 4u);
    EXPECT_EQ(cpu.memory.read_word(kWatched), 0xCAFEu);
    EXPECT_NE(result.message.find("write of 0xcafe"), std::string::npos) << result.message;
    EXPECT_NE(result.message.find("0x00000100"), std::string::npos) << result.message;
    EXPECT_NE(result.message.find("instruction at 0x00000000"), std::string::npos)
        << result.message;
}

TEST_F(WatchpointTest, the_next_run_goes_on_and_a_read_does_not_trip_a_write_watch)
{
    cpu.add_watchpoint(kWatched);
    ASSERT_EQ(cpu.run(0).status, Status::BREAKPOINT);

    const auto result = cpu.run(0);
    EXPECT_EQ(result.status, Status::HALTED); // the ldr at 8 is a read
    EXPECT_EQ(cpu.read_reg(3), 0xCAFEu);
}

TEST_F(WatchpointTest, a_read_watchpoint_ignores_the_store_and_stops_after_the_load)
{
    cpu.add_watchpoint(kWatched, 4, Emulator32bit::WatchKind::READ);
    const auto result = cpu.run(0);

    EXPECT_EQ(result.status, Status::BREAKPOINT);
    EXPECT_EQ(result.instructions_ran, 3u);
    EXPECT_EQ(cpu.get_pc(), 12u);
    EXPECT_EQ(cpu.read_reg(3), 0xCAFEu);
    EXPECT_NE(result.message.find("read of 0xcafe"), std::string::npos) << result.message;
}

TEST_F(WatchpointTest, an_access_watchpoint_reacts_to_both)
{
    cpu.add_watchpoint(kWatched, 4, Emulator32bit::WatchKind::ACCESS);
    EXPECT_EQ(cpu.run(0).instructions_ran, 1u);
    EXPECT_EQ(cpu.run(0).instructions_ran, 2u); // add, then the ldr
}

TEST_F(WatchpointTest, any_overlapping_byte_counts)
{
    write_program({mem_op(Emulator32bit::_op_strb, 2, 1, 3),
                   mem_op(Emulator32bit::_op_strb, 2, 1, 4), Emulator32bit::asm_hlt()});

    cpu.add_watchpoint(kWatched, 4); // bytes 0x100..0x103
    const auto first = cpu.run(0);
    EXPECT_EQ(first.status, Status::BREAKPOINT);
    EXPECT_NE(first.message.find("write of 0xfe (1 byte)"), std::string::npos) << first.message;
    EXPECT_EQ(cpu.run(0).status, Status::HALTED); // 0x104 is outside
}

TEST_F(WatchpointTest, an_atomic_is_a_write)
{
    write_program({Emulator32bit::asm_atomic(3, 2, 1, Emulator32bit::kAtomicWidth_word,
                                             Emulator32bit::kAtomicId_swp),
                   Emulator32bit::asm_hlt()});
    cpu.add_watchpoint(kWatched);
    const auto result = cpu.run(0);

    EXPECT_EQ(result.status, Status::BREAKPOINT);
    EXPECT_EQ(cpu.memory.read_word(kWatched), 0xCAFEu);
    EXPECT_NE(result.message.find("write"), std::string::npos) << result.message;
}

TEST_F(WatchpointTest, the_writeback_of_the_stopping_instruction_has_happened)
{
    write_program({mem_op(Emulator32bit::_op_str, 2, 1, 4, Emulator32bit::AddrType::ADDR_PRE_INC),
                   Emulator32bit::asm_hlt()});
    cpu.add_watchpoint(kWatched + 4);
    EXPECT_EQ(cpu.run(0).status, Status::BREAKPOINT);
    EXPECT_EQ(cpu.read_reg(1), kWatched + 4);
}

TEST_F(WatchpointTest, removing_and_clearing)
{
    cpu.add_watchpoint(kWatched);
    cpu.add_watchpoint(kWatched, 8); // replaces the first one
    EXPECT_EQ(cpu.watchpoints().size(), 1u);
    EXPECT_EQ(cpu.watchpoints()[0].length, 8u);

    EXPECT_FALSE(cpu.remove_watchpoint(0x200));
    EXPECT_TRUE(cpu.remove_watchpoint(kWatched));
    EXPECT_EQ(cpu.run(0).status, Status::HALTED);

    cpu.add_watchpoint(kWatched);
    cpu.clear_watchpoints();
    cpu.set_pc(0);
    EXPECT_EQ(cpu.run(0).status, Status::HALTED);
}

TEST_F(WatchpointTest, a_single_step_that_hits_reports_it)
{
    cpu.add_watchpoint(kWatched);
    const auto result = cpu.run(1);
    EXPECT_EQ(result.status, Status::BREAKPOINT);
    EXPECT_EQ(result.instructions_ran, 1u);
}

TEST_F(WatchpointTest, specs_are_parsed)
{
    SymbolMap names;
    names.add("var", 0x40);

    const auto plain = parse_watch_spec(names, "var");
    ASSERT_TRUE(plain);
    EXPECT_EQ(plain->address, 0x40u);
    EXPECT_EQ(plain->length, 1u);
    EXPECT_EQ(plain->kind, Emulator32bit::WatchKind::WRITE);

    const auto full = parse_watch_spec(names, "var+4:8:rw");
    ASSERT_TRUE(full);
    EXPECT_EQ(full->address, 0x44u);
    EXPECT_EQ(full->length, 8u);
    EXPECT_EQ(full->kind, Emulator32bit::WatchKind::ACCESS);

    EXPECT_EQ(parse_watch_spec(names, "0x10:r")->kind, Emulator32bit::WatchKind::READ);
    EXPECT_FALSE(parse_watch_spec(names, "nothing"));
    EXPECT_FALSE(parse_watch_spec(names, "var:0"));
    EXPECT_FALSE(parse_watch_spec(names, "var:x"));
    EXPECT_FALSE(parse_watch_spec(names, "var:1:r:w"));
}

// Register watches. The fixture program adds 1 to x0 three times and halts.

using RegisterWatchTest = DebugFixture;

TEST_F(RegisterWatchTest, stops_after_the_instruction_that_changed_the_register)
{
    cpu.add_register_watch(0);
    const auto result = cpu.run(0);

    EXPECT_EQ(result.status, Status::BREAKPOINT);
    EXPECT_EQ(result.instructions_ran, 1u);
    EXPECT_EQ(cpu.get_pc(), 4u);
    EXPECT_NE(result.message.find("Register watch x0: 0x0 -> 0x1"), std::string::npos)
        << result.message;
    EXPECT_NE(result.message.find("0x00000004"), std::string::npos) << result.message;

    EXPECT_EQ(cpu.run(0).instructions_ran, 1u); // and again for the next change
}

TEST_F(RegisterWatchTest, a_value_only_stops_on_a_change_to_that_value)
{
    cpu.add_register_watch(0, 3);
    const auto result = cpu.run(0);

    EXPECT_EQ(result.status, Status::BREAKPOINT);
    EXPECT_EQ(result.instructions_ran, 3u);
    EXPECT_EQ(cpu.read_reg(0), 3u);
}

TEST_F(RegisterWatchTest, a_register_that_does_not_change_never_stops)
{
    cpu.add_register_watch(1);
    EXPECT_EQ(cpu.run(0).status, Status::HALTED);
}

TEST_F(RegisterWatchTest, writing_the_value_it_already_has_is_not_a_change)
{
    write_program({add_imm(0, 0, 0), add_imm(0, 0, 0), Emulator32bit::asm_hlt()});
    cpu.add_register_watch(0);
    EXPECT_EQ(cpu.run(0).status, Status::HALTED);
}

TEST_F(RegisterWatchTest, a_single_step_that_changes_it_reports_it)
{
    cpu.add_register_watch(0);
    const auto result = cpu.run(1);
    EXPECT_EQ(result.status, Status::BREAKPOINT);
    EXPECT_EQ(result.instructions_ran, 1u);
}

TEST_F(RegisterWatchTest, a_change_made_by_the_debugger_between_runs_is_not_reported)
{
    cpu.add_register_watch(0);
    ASSERT_EQ(cpu.run(0).status, Status::BREAKPOINT);

    cpu.write_reg(0, 100);
    const auto result = cpu.run(0);
    EXPECT_EQ(result.status, Status::BREAKPOINT);
    EXPECT_NE(result.message.find("0x64 -> 0x65"), std::string::npos) << result.message;
}

TEST_F(RegisterWatchTest, the_stack_pointer_is_named_sp)
{
    write_program({add_imm(30, 30, 8), Emulator32bit::asm_hlt()});
    cpu.add_register_watch(static_cast<U8>(Register::SP));
    const auto result = cpu.run(0);
    EXPECT_NE(result.message.find("Register watch sp: 0x0 -> 0x8"), std::string::npos)
        << result.message;
}

TEST_F(RegisterWatchTest, removing_and_replacing)
{
    cpu.add_register_watch(0);
    cpu.add_register_watch(0, 3); // replaces
    EXPECT_EQ(cpu.register_watches().size(), 1u);
    EXPECT_FALSE(cpu.remove_register_watch(5));
    EXPECT_TRUE(cpu.remove_register_watch(0));
    EXPECT_EQ(cpu.run(0).status, Status::HALTED);

    cpu.add_register_watch(0);
    cpu.clear_register_watches();
    EXPECT_TRUE(cpu.register_watches().empty());
}

TEST_F(RegisterWatchTest, specs_are_parsed)
{
    const auto plain = parse_register_watch_spec("x5");
    ASSERT_TRUE(plain);
    EXPECT_EQ(plain->reg, 5);
    EXPECT_FALSE(plain->value);

    const auto with_value = parse_register_watch_spec("sp=0x100");
    ASSERT_TRUE(with_value);
    EXPECT_EQ(with_value->reg, static_cast<U8>(Register::SP));
    EXPECT_EQ(with_value->value, 0x100u);

    EXPECT_FALSE(parse_register_watch_spec("xzr"));
    EXPECT_FALSE(parse_register_watch_spec("5"));
    EXPECT_FALSE(parse_register_watch_spec("x99"));
    EXPECT_FALSE(parse_register_watch_spec("x1="));
    EXPECT_FALSE(parse_register_watch_spec("x1=zz"));
    EXPECT_FALSE(parse_register_watch_spec(""));
}

TEST_F(DebuggerTest, watchreg_commands)
{
    EXPECT_NE(command("watchreg x0 2").find("Watching x0 for a change to 0x2"), std::string::npos);
    EXPECT_NE(command("watches").find("x0, changes to 0x2"), std::string::npos);

    const std::string text = command("continue");
    EXPECT_NE(text.find("Register watch x0: 0x1 -> 0x2"), std::string::npos) << text;

    EXPECT_NE(command("unwatchreg x0").find("removed"), std::string::npos);
    EXPECT_NE(command("unwatchreg x0").find("There is no watch"), std::string::npos);
    EXPECT_NE(command("watchreg").find("Usage"), std::string::npos);
    EXPECT_NE(command("watchreg xzr").find("Expected"), std::string::npos);

    command("watchreg sp");
    EXPECT_NE(command("unwatchreg all").find("All register watches removed"), std::string::npos);
    EXPECT_TRUE(cpu.register_watches().empty());
}

class WatchpointDebuggerTest : public WatchpointTest
{
  protected:
    SymbolMap symbols;
    std::istringstream in;
    std::ostringstream out;
    std::unique_ptr<Debugger> debugger;

    void SetUp() override
    {
        WatchpointTest::SetUp();
        symbols.add("start", 0);
        symbols.add("var", kWatched);
        debugger = std::make_unique<Debugger>(cpu, symbols, in, out);
    }

    std::string command(const std::string &line)
    {
        out.str("");
        debugger->execute(line);
        return out.str();
    }
};

TEST_F(WatchpointDebuggerTest, watch_continue_unwatch)
{
    EXPECT_NE(command("watches").find("No watchpoints"), std::string::npos);
    EXPECT_NE(command("watch var 4 rw").find("Watching 4 bytes at 0x00000100 <var> for read/write"),
              std::string::npos);
    EXPECT_NE(command("watches").find("<var>, 4 bytes, read/write"), std::string::npos);

    const std::string text = command("continue");
    EXPECT_NE(text.find("Watchpoint 0x00000100: write of 0xcafe"), std::string::npos) << text;
    EXPECT_NE(text.find("=> 0x00000004"), std::string::npos) << text;

    EXPECT_NE(command("unwatch var").find("removed"), std::string::npos);
    EXPECT_NE(command("unwatch var").find("There is no watchpoint"), std::string::npos);
    EXPECT_NE(command("continue").find("halted"), std::string::npos);
}

TEST_F(WatchpointDebuggerTest, bad_input_and_unwatch_all)
{
    EXPECT_NE(command("watch").find("Usage"), std::string::npos);
    EXPECT_NE(command("watch nowhere").find("Expected"), std::string::npos);
    EXPECT_NE(command("watch var 0").find("Expected"), std::string::npos);
    EXPECT_NE(command("unwatch").find("Expected"), std::string::npos);

    command("w var");
    EXPECT_NE(command("unwatch all").find("All watchpoints removed"), std::string::npos);
    EXPECT_TRUE(cpu.watchpoints().empty());
}

} // namespace
