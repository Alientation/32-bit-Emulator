// The interrupt controller, timer and console (docs/devices.md), and what the CPU does with them:
// taking an interrupt between instructions, WFI, and running code from the ROM.

#include "emulator32bit/devices.h"
#include "emulator32bit/host_display.h"
#include "emulator32bit/host_input.h"
#include "emulator32bit_test/emulator32bit_test.h"

#include <filesystem>
#include <sstream>
#include <thread>

namespace
{

using Class = Emulator32bit::ExceptionClass;
using Status = Emulator32bit::RunResult::Status;
using E = Emulator32bit;

constexpr word kProgram = 0x100;
constexpr word kVectors = 0x800;
constexpr word kIrqVector = kVectors + 16 * word(Class::IRQ);
constexpr word kIrqMask = 1 << kIrqMaskBit;

word mov(const U8 xd, const word value)
{
    return E::asm_format_o3(encoding::_op_mov, false, xd, value);
}

word store_at(const U8 xt, const U8 xn, const int offset)
{
    return E::asm_format_m(encoding::_op_str, false, xt, xn, offset,
                           encoding::AddrType::ADDR_OFFSET);
}

word load_at(const U8 xt, const U8 xn, const int offset)
{
    return E::asm_format_m(encoding::_op_ldr, false, xt, xn, offset,
                           encoding::AddrType::ADDR_OFFSET);
}

word nop()
{
    return E::asm_nop();
}

/// The instructions that put a 32 bit value in a register: mov, lsl, orr.
std::vector<word> constant(const U8 xd, const word value)
{
    return {mov(xd, value >> 14), E::asm_format_o1(ShiftType::SHIFT_LSL, xd, xd, true, 0, 14),
            E::asm_format_o(encoding::_op_orr, false, xd, xd, int(value & 0x3FFF))};
}

void append(std::vector<word> &code, const std::vector<word> &more)
{
    code.insert(code.end(), more.begin(), more.end());
}

class Devices : public ::testing::Test
{
  protected:
    Emulator32bit cpu{std::make_unique<RAM>(16, 0), std::make_unique<ROM>(16, 16),
                      std::make_unique<MockDisk>()};
    std::ostringstream m_console;

    void SetUp() override
    {
        cpu.system_bus->console.set_output(&m_console);
    }

    void write(const word address, const std::vector<word> &code)
    {
        for (size_t i = 0; i < code.size(); i++)
        {
            cpu.memory.write_word(address + word(i) * 4, code[i]);
        }
    }

    /// A vector table whose handlers halt, and the IRQ handler `irq`.
    void install_vectors(const std::vector<word> &irq)
    {
        for (word c = 0; c < 8; c++)
        {
            write(kVectors + 16 * c, {E::asm_hlt()});
        }
        write(kIrqVector, irq);
        cpu.write_sysreg(encoding::kSysregId_vbar, kVectors);
    }

    Emulator32bit::RunResult run(const std::vector<word> &program, const U64 limit = 1000)
    {
        write(kProgram, program);
        cpu.set_pc(kProgram);
        return cpu.run(limit);
    }

    word intc_read(const word offset)
    {
        return cpu.memory.read_word(kIntcBase + offset);
    }
};

} // namespace

// --- the interrupt controller -------------------------------------------------------------

TEST(InterruptControllerUnit, lines_are_served_in_the_order_they_were_raised)
{
    InterruptController intc;
    intc.write_word(kIntcBase + 0x08, 0xFFFFFFFF);
    intc.raise(5);
    intc.raise(2);
    intc.raise(9);
    EXPECT_EQ(intc.read_word(kIntcBase + 0x04), 3u);
    EXPECT_EQ(intc.read_word(kIntcBase + 0x10), (1u << 5) | (1u << 2) | (1u << 9));
    EXPECT_EQ(intc.read_word(kIntcBase), 5u);
    EXPECT_EQ(intc.read_word(kIntcBase), 2u);
    EXPECT_EQ(intc.read_word(kIntcBase), 9u);
    EXPECT_EQ(intc.read_word(kIntcBase), 0xFFFFFFFFu) << "empty";
    EXPECT_FALSE(intc.has_pending());
}

TEST(InterruptControllerUnit,
     a_line_in_the_queue_is_not_queued_twice_but_can_be_after_it_is_claimed)
{
    InterruptController intc;
    intc.write_word(kIntcBase + 0x08, 0xFF);
    intc.raise(1);
    intc.raise(2);
    intc.raise(1); // already waiting: it keeps its place
    EXPECT_EQ(intc.claim(), 1u);
    intc.raise(1); // now it waits behind line 2
    EXPECT_EQ(intc.claim(), 2u);
    EXPECT_EQ(intc.claim(), 1u);
    EXPECT_EQ(intc.claim(), 0xFFFFFFFFu);
}

TEST(InterruptControllerUnit, the_queue_holds_all_32_lines_and_keeps_its_order_when_it_wraps)
{
    InterruptController intc;
    intc.write_word(kIntcBase + 0x08, 0xFFFFFFFF);
    for (word line = 0; line < 32; ++line)
    {
        intc.raise(31 - line);
    }
    EXPECT_EQ(intc.read_word(kIntcBase + 0x04), 32u);
    for (word line = 31; line >= 12; --line)
    {
        EXPECT_EQ(intc.claim(), line);
    }
    for (word line = 31; line >= 12; --line)
    {
        intc.raise(line); // the tail passes the end of the ring; lines 11-0 are still waiting
    }
    EXPECT_EQ(intc.read_word(kIntcBase + 0x04), 32u);
    for (word line = 11; line != ~word(0); --line)
    {
        EXPECT_EQ(intc.claim(), line);
    }
    for (word line = 31; line >= 12; --line)
    {
        EXPECT_EQ(intc.claim(), line);
    }
    EXPECT_FALSE(intc.has_pending());
    EXPECT_EQ(intc.claim(), 0xFFFFFFFFu);
}

TEST(InterruptControllerUnit, a_line_that_is_not_enabled_is_lost)
{
    InterruptController intc;
    intc.raise(3);
    EXPECT_FALSE(intc.has_pending()) << "nothing is enabled after reset";
    intc.write_word(kIntcBase + 0x08, 1u << 3);
    intc.raise(3);
    intc.raise(4);
    EXPECT_EQ(intc.read_word(kIntcBase + 0x04), 1u);
    intc.write_word(kIntcBase + 0x0C, 3); // a software interrupt
    EXPECT_EQ(intc.read_word(kIntcBase + 0x04), 1u) << "already waiting";
    intc.raise(40);                       // there are 32 lines
    EXPECT_EQ(intc.read_word(kIntcBase + 0x04), 1u);
}

TEST(InterruptControllerUnit, reset_empties_the_queue_and_disables_the_lines)
{
    InterruptController intc;
    intc.write_word(kIntcBase + 0x08, 0xFF);
    intc.raise(0);
    intc.reset();
    EXPECT_FALSE(intc.has_pending());
    EXPECT_EQ(intc.read_word(kIntcBase + 0x10), 0u);
    EXPECT_EQ(intc.read_word(kIntcBase + 0x08), 0u);
}

// --- the timer -----------------------------------------------------------------------------

TEST(TimerUnit, a_one_shot_timer_raises_its_line_once)
{
    InterruptController intc;
    intc.write_word(kIntcBase + 0x08, 1);
    Timer timer(intc);
    timer.write_word(kTimerBase + 0x04, 5); // compare
    timer.write_word(kTimerBase + 0x08, Timer::kEnabled);

    for (int i = 0; i < 4; i++)
    {
        timer.tick();
    }
    EXPECT_FALSE(intc.has_pending());
    timer.tick();
    EXPECT_TRUE(intc.has_pending());
    EXPECT_EQ(timer.read_word(kTimerBase), 5u);
    EXPECT_EQ(timer.read_word(kTimerBase + 0x08), 0u) << "it disabled itself";
    intc.claim();
    for (int i = 0; i < 100; i++)
    {
        timer.tick();
    }
    EXPECT_FALSE(intc.has_pending());
}

TEST(TimerUnit, a_periodic_timer_goes_on)
{
    InterruptController intc;
    intc.write_word(kIntcBase + 0x08, 1);
    Timer timer(intc);
    timer.write_word(kTimerBase + 0x04, 10);
    timer.write_word(kTimerBase + 0x0C, 10);
    timer.write_word(kTimerBase + 0x08, Timer::kEnabled | Timer::kPeriodic);

    int interrupts = 0;
    for (int i = 0; i < 35; i++)
    {
        timer.tick();
        if (intc.has_pending())
        {
            interrupts++;
            intc.claim();
        }
    }
    EXPECT_EQ(interrupts, 3) << "at 10, 20 and 30";
    EXPECT_EQ(timer.read_word(kTimerBase + 0x04), 40u);
}

TEST(TimerUnit, the_count_can_be_written_and_runs_with_the_timer_off)
{
    InterruptController intc;
    Timer timer(intc);
    timer.write_word(kTimerBase, 100);
    timer.tick();
    timer.tick();
    EXPECT_EQ(timer.read_word(kTimerBase), 102u);
}

// --- the console ---------------------------------------------------------------------------

TEST_F(Devices, a_byte_written_to_the_console_is_sent)
{
    cpu.memory.write_byte(kConsoleBase, 'h');
    cpu.memory.write_word(kConsoleBase, 'i');
    cpu.memory.write_hword(kConsoleBase, '!');
    EXPECT_EQ(m_console.str(), "hi!");
}

TEST_F(Devices, received_bytes_are_read_one_by_one_and_the_status_says_when_there_are_some)
{
    EXPECT_EQ(cpu.memory.read_word(kConsoleBase + 4), 2u) << "ready to send, nothing received";
    cpu.system_bus->console.push_input("ab");
    EXPECT_EQ(cpu.memory.read_word(kConsoleBase + 4), 3u);
    EXPECT_EQ(cpu.memory.read_byte(kConsoleBase), 'a');
    EXPECT_EQ(cpu.memory.read_word(kConsoleBase), word('b'));
    EXPECT_EQ(cpu.memory.read_word(kConsoleBase + 4), 2u);
    EXPECT_EQ(cpu.memory.read_word(kConsoleBase), 0u) << "nothing left";
}

TEST_F(Devices, input_raises_line_1_when_the_receive_interrupt_is_enabled)
{
    cpu.memory.write_word(kIntcBase + 8, 1u << kIrqLineConsole);
    cpu.system_bus->console.push_input("x");
    EXPECT_FALSE(cpu.system_bus->intc.has_pending()) << "not enabled yet";

    cpu.memory.write_word(kConsoleBase + 8, 1); // enabling it with a byte waiting
    EXPECT_EQ(intc_read(0x10), 1u << kIrqLineConsole);
    EXPECT_EQ(intc_read(0x00), kIrqLineConsole);

    cpu.system_bus->console.push_input("y");
    EXPECT_TRUE(cpu.system_bus->intc.has_pending());
}

// --- input that arrives while the machine runs ---------------------------------------------

TEST_F(Devices, bytes_from_the_host_are_read_through_the_registers)
{
    const auto input = std::make_shared<HostInput>();
    cpu.system_bus->console.attach_host_input(input);
    EXPECT_EQ(cpu.memory.read_word(kConsoleBase + 4), 2u) << "nothing yet";

    input->push("hi");
    EXPECT_EQ(cpu.memory.read_word(kConsoleBase + 4), 3u) << "the status looks at the host";
    EXPECT_EQ(cpu.memory.read_byte(kConsoleBase), 'h');
    EXPECT_EQ(cpu.memory.read_byte(kConsoleBase), 'i');
    EXPECT_EQ(cpu.memory.read_word(kConsoleBase + 4), 2u);
}

TEST_F(Devices, input_from_the_host_interrupts_a_program_that_is_running)
{
    const auto input = std::make_shared<HostInput>();
    cpu.system_bus->console.attach_host_input(input);
    install_vectors({mov(7, 1), load_at(8, 10, 0), E::asm_eret()});

    std::vector<word> program;
    append(program, constant(10, kIntcBase));
    append(program, constant(11, kConsoleBase));
    program.insert(program.end(),
                   {mov(1, 1u << kIrqLineConsole), store_at(1, 10, 0x08), mov(1, 1),
                    store_at(1, 11, 0x08), E::asm_msr(encoding::kSysregId_pstate, true, 0)});
    program.insert(program.end(), 20, nop());
    program.push_back(E::asm_hlt());

    input->push("x");
    const auto result = run(program);
    ASSERT_EQ(result.status, Status::HALTED) << result.message;
    EXPECT_EQ(cpu.read_reg(7), 1u) << "the handler ran";
    EXPECT_EQ(cpu.read_reg(8), kIrqLineConsole);
    EXPECT_EQ(cpu.memory.read_byte(kConsoleBase), 'x');
}

// A program that never looks at the console still gets what is typed while it runs: the clock looks
// at the input at least every Console::kHostInputPollInterval instructions.
TEST_F(Devices, a_byte_typed_while_a_program_runs_in_a_loop_interrupts_it)
{
    const auto input = std::make_shared<HostInput>();
    cpu.system_bus->console.attach_host_input(input);
    install_vectors({E::asm_hlt()});

    std::vector<word> program;
    append(program, constant(10, kIntcBase));
    append(program, constant(11, kConsoleBase));
    program.insert(program.end(),
                   {mov(1, 1u << kIrqLineConsole), store_at(1, 10, 0x08), mov(1, 1),
                    store_at(1, 11, 0x08), E::asm_msr(encoding::kSysregId_pstate, true, 0),
                    E::asm_format_b1(encoding::_op_b, ConditionCode::AL, 0)}); // to itself
    std::thread typist(
        [input]
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            input->push("z");
        });
    constexpr U64 kLimit = 100'000'000;
    const auto result = run(program, kLimit);
    typist.join();

    ASSERT_EQ(result.status, Status::HALTED) << result.message;
    EXPECT_LT(result.instructions_ran, kLimit);
    EXPECT_EQ(cpu.memory.read_byte(kConsoleBase), 'z');
}

// The machine has nothing to do, so wfi waits for what is typed (in the time of the host).
TEST_F(Devices, wfi_waits_for_the_host_when_the_program_asked_to_hear_of_a_byte)
{
    const auto input = std::make_shared<HostInput>();
    cpu.system_bus->console.attach_host_input(input);
    std::thread typist(
        [input]
        {
            std::this_thread::sleep_for(std::chrono::milliseconds(20));
            input->push("k");
        });

    std::vector<word> program;
    append(program, constant(10, kIntcBase));
    append(program, constant(11, kConsoleBase));
    program.insert(program.end(), {mov(1, 1u << kIrqLineConsole), store_at(1, 10, 0x08), mov(1, 1),
                                   store_at(1, 11, 0x08), E::asm_wfi(), load_at(8, 10, 0), // claim
                                   load_at(9, 11, 0), // the byte
                                   E::asm_hlt()});
    const auto result = run(program);
    typist.join();

    ASSERT_EQ(result.status, Status::HALTED) << result.message;
    EXPECT_EQ(result.message.find("no interrupt source"), std::string::npos) << result.message;
    EXPECT_EQ(cpu.read_reg(8), kIrqLineConsole);
    EXPECT_EQ(cpu.read_reg(9), word('k'));
}

TEST_F(Devices, wfi_ends_the_run_when_the_input_from_the_host_has_ended)
{
    const auto input = std::make_shared<HostInput>();
    cpu.system_bus->console.attach_host_input(input);
    input->close();

    std::vector<word> program;
    append(program, constant(11, kConsoleBase));
    program.insert(program.end(),
                   {mov(1, 1), store_at(1, 11, 0x08), E::asm_wfi(), mov(3, 7), E::asm_hlt()});
    const auto result = run(program);
    EXPECT_EQ(result.status, Status::HALTED);
    EXPECT_NE(result.message.find("no interrupt source"), std::string::npos) << result.message;
    EXPECT_EQ(cpu.read_reg(3), 0u);
    EXPECT_TRUE(input->finished());
}

TEST(HostInputUnit, bytes_are_taken_in_order_and_wait_ends_with_them_or_with_the_end)
{
    HostInput input;
    EXPECT_FALSE(input.pending());
    input.push("ab");
    input.push("c");
    EXPECT_TRUE(input.pending());
    EXPECT_TRUE(input.wait());

    std::deque<byte> taken;
    input.take(taken);
    EXPECT_EQ(std::string(taken.begin(), taken.end()), "abc");
    EXPECT_FALSE(input.pending());
    EXPECT_FALSE(input.finished());

    input.close();
    EXPECT_TRUE(input.pending()) << "so that the end is noticed";
    EXPECT_FALSE(input.wait());
    EXPECT_TRUE(input.finished());
}

TEST_F(Devices, a_reset_clears_the_devices)
{
    cpu.memory.write_word(kIntcBase + 8, 0xFF);
    cpu.system_bus->console.push_input("x");
    cpu.reset();
    EXPECT_EQ(intc_read(0x08), 0u);
    EXPECT_FALSE(cpu.system_bus->console.input_waiting());
}

// --- the CPU takes interrupts --------------------------------------------------------------

// The timer fires while a run of nops executes, the handler claims the line and returns.
TEST_F(Devices, an_interrupt_runs_the_handler_between_two_instructions_and_eret_goes_on)
{
    install_vectors({mov(7, 1), load_at(8, 10, 0),
                     E::asm_format_o(encoding::_op_add, false, 9, 9, 1), E::asm_eret()});

    std::vector<word> program;
    append(program, constant(10, kIntcBase));
    append(program, constant(11, kTimerBase));
    program.insert(program.end(), {mov(1, 1), store_at(1, 10, 0x08),  // enable line 0
                                   mov(1, 40), store_at(1, 11, 0x04), // compare
                                   mov(1, 1), store_at(1, 11, 0x08),  // start
                                   E::asm_msr(encoding::kSysregId_pstate, true, 0)}); // unmask
    const size_t nops_at = program.size();
    program.insert(program.end(), 80, nop());
    program.push_back(E::asm_hlt());

    const auto result = run(program);
    ASSERT_EQ(result.status, Status::HALTED) << result.message;
    EXPECT_EQ(cpu.read_reg(7), 1u) << "the handler ran";
    EXPECT_EQ(cpu.read_reg(8), kIrqLineTimer) << "and claimed the timer's line";
    EXPECT_EQ(cpu.read_reg(9), 1u) << "once";
    EXPECT_FALSE(cpu.system_bus->intc.has_pending());

    // It came back into the nops: the resume address was a nop, not the handler.
    const word elr = cpu.read_sysreg(encoding::kSysregId_elr);
    EXPECT_GE(elr, kProgram + 4 * nops_at);
    EXPECT_LT(elr, kProgram + 4 * (nops_at + 80));
    EXPECT_EQ(cpu.read_sysreg(encoding::kSysregId_esr) >> 26, word(Class::IRQ));
    EXPECT_EQ(cpu.get_pc(), kProgram + 4 * (nops_at + 80)) << "the final hlt";
    EXPECT_FALSE(cpu.get_flag(kIrqMaskBit)) << "ERET restored the unmasked state";
}

TEST_F(Devices, masked_interrupts_wait)
{
    install_vectors({mov(7, 1), E::asm_hlt()});

    std::vector<word> program;
    append(program, constant(10, kIntcBase));
    program.insert(program.end(), {mov(1, 1), store_at(1, 10, 0x08), mov(2, 0),
                                   store_at(2, 10, 0x0C)}); // enable line 0, raise line 0
    program.insert(program.end(), 20, nop());
    program.push_back(E::asm_hlt());

    const auto result = run(program);
    ASSERT_EQ(result.status, Status::HALTED);
    EXPECT_EQ(cpu.read_reg(7), 0u) << "IRQs are masked after reset";
    EXPECT_TRUE(cpu.system_bus->intc.has_pending());
    EXPECT_TRUE(cpu.get_flag(kIrqMaskBit));
}

TEST_F(Devices, without_a_vector_table_nothing_is_taken)
{
    std::vector<word> program;
    append(program, constant(10, kIntcBase));
    program.insert(program.end(),
                   {mov(1, 1), store_at(1, 10, 0x08), mov(2, 0), store_at(2, 10, 0x0C),
                    E::asm_msr(encoding::kSysregId_pstate, true, 0), nop(), nop(), E::asm_hlt()});
    const auto result = run(program);
    EXPECT_EQ(result.status, Status::HALTED);
    EXPECT_TRUE(cpu.system_bus->intc.has_pending()) << "it was raised";
    EXPECT_EQ(cpu.get_pc(), kProgram + 4 * (3 + 7 + 0)) << "and the program went on to its hlt";
}

TEST_F(Devices, interrupts_are_served_first_come_first_served)
{
    // Two lines are raised, line 1 before line 0. The handler claims both.
    install_vectors({load_at(8, 10, 0), load_at(12, 10, 0), E::asm_eret()});

    std::vector<word> program;
    append(program, constant(10, kIntcBase));
    program.insert(program.end(),
                   {mov(1, 3), store_at(1, 10, 0x08), // lines 0 and 1
                    mov(1, 1), store_at(1, 10, 0x0C), // raise 1
                    mov(1, 0), store_at(1, 10, 0x0C), // raise 0
                    E::asm_msr(encoding::kSysregId_pstate, true, 0), nop(), nop(), E::asm_hlt()});
    const auto result = run(program);
    ASSERT_EQ(result.status, Status::HALTED) << result.message;
    EXPECT_EQ(cpu.read_reg(8), 1u);
    EXPECT_EQ(cpu.read_reg(12), 0u);
}

TEST_F(Devices, the_handler_runs_with_irqs_masked_in_kernel_mode_on_its_own_stack)
{
    install_vectors({E::asm_msr(encoding::kSysregId_spsr, true, 0), E::asm_hlt()});
    cpu.write_reg(U8(Register::SP), 0x1000);
    cpu.write_sysreg(encoding::kSysregId_usp, 0x2000);

    std::vector<word> program;
    append(program, constant(10, kIntcBase));
    program.insert(program.end(),
                   {mov(1, 1), store_at(1, 10, 0x08), mov(2, 0), store_at(2, 10, 0x0C),
                    E::asm_msr(encoding::kSysregId_pstate, true, 0), nop(), nop(), E::asm_hlt()});
    ASSERT_EQ(run(program).status, Status::HALTED);
    EXPECT_EQ(cpu.get_pc(), kIrqVector + 4) << "stopped in the handler";
    EXPECT_FALSE(cpu.user_mode());
    EXPECT_TRUE(cpu.get_flag(kIrqMaskBit));
}

TEST_F(Devices, an_interrupt_in_user_mode_comes_back_to_user_mode)
{
    // The handler claims the line (so it is not taken again), and runs on past its 16 bytes into the
    // unused vector after it.
    std::vector<word> handler = constant(10, kIntcBase);
    append(handler, {load_at(8, 10, 0), mov(7, 1), E::asm_eret()});
    install_vectors(handler);
    cpu.system_bus->intc.write_word(kIntcBase + 8, 1);
    cpu.system_bus->intc.raise(0);

    // The user program is a few nops and the system call that stops the run (hlt is privileged).
    write(kProgram, {nop(), nop(), E::asm_format_b1(encoding::_op_swi, ConditionCode::AL, 0)});
    write(kVectors + 16 * word(Class::SUPERVISOR_CALL), {E::asm_hlt()});
    cpu.write_sysreg(encoding::kSysregId_pstate, 1 << kUserModeBit); // user mode, IRQs unmasked
    cpu.set_pc(kProgram);
    const auto result = cpu.run(100);
    ASSERT_EQ(result.status, Status::HALTED);
    EXPECT_EQ(cpu.read_reg(7), 1u) << "the handler ran";
    EXPECT_EQ(cpu.read_sysreg(encoding::kSysregId_elr), kProgram + 4 * 2 + 4)
        << "the system call was reached after the handler returned";
}

// --- WFI -----------------------------------------------------------------------------------

TEST_F(Devices, wfi_jumps_to_the_moment_the_timer_fires)
{
    std::vector<word> program;
    append(program, constant(10, kIntcBase));
    append(program, constant(11, kTimerBase));
    program.insert(program.end(), {mov(1, 1), store_at(1, 10, 0x08), mov(1, 5000),
                                   store_at(1, 11, 0x04), mov(1, 1), store_at(1, 11, 0x08),
                                   E::asm_wfi(),      // IRQs are masked, so this just wakes up
                                   load_at(8, 10, 0), // claim: the timer's line
                                   E::asm_hlt()});
    const auto result = run(program);
    ASSERT_EQ(result.status, Status::HALTED) << result.message;
    EXPECT_EQ(cpu.read_reg(8), kIrqLineTimer);
    EXPECT_LT(result.instructions_ran, 30u) << "it did not execute 5000 instructions";
    EXPECT_GE(cpu.system_bus->timer.count(), 5000u) << "but time passed";
}

TEST_F(Devices, wfi_with_an_interrupt_already_pending_goes_on)
{
    cpu.system_bus->intc.write_word(kIntcBase + 8, 1);
    cpu.system_bus->intc.raise(0);
    const auto result = run({E::asm_wfi(), mov(3, 7), E::asm_hlt()});
    EXPECT_EQ(result.status, Status::HALTED);
    EXPECT_EQ(cpu.read_reg(3), 7u);
}

TEST_F(Devices, wfi_with_nothing_to_wait_for_ends_the_run)
{
    const auto result = run({E::asm_wfi(), mov(3, 7), E::asm_hlt()});
    EXPECT_EQ(result.status, Status::HALTED);
    EXPECT_EQ(cpu.read_reg(3), 0u);
    EXPECT_NE(result.message.find("no interrupt source"), std::string::npos) << result.message;
}

// --- the boot ROM --------------------------------------------------------------------------

TEST_F(Devices, code_can_run_from_the_rom)
{
    const word rom = 16 << kNumPageOffsetBits;
    cpu.system_bus->rom->write_word(rom, mov(3, 99));
    cpu.system_bus->rom->write_word(rom + 4, E::asm_hlt());
    cpu.set_pc(rom);
    const auto result = cpu.run(10);
    EXPECT_EQ(result.status, Status::HALTED) << result.message;
    EXPECT_EQ(cpu.read_reg(3), 99u);
}

TEST_F(Devices, the_console_works_from_a_program)
{
    std::vector<word> program;
    append(program, constant(10, kConsoleBase));
    program.insert(program.end(), {mov(1, 'O'), store_at(1, 10, 0), mov(1, 'K'), store_at(1, 10, 0),
                                   E::asm_hlt()});
    ASSERT_EQ(run(program).status, Status::HALTED);
    EXPECT_EQ(m_console.str(), "OK");
}

// --- the block device ----------------------------------------------------------------------

namespace
{

/// A block device with a disk of `sectors` sectors, and helpers to drive it like a driver.
struct Disk512
{
    InterruptController intc;
    BlockDevice block{intc};

    explicit Disk512(const word sectors)
    {
        block.set_capacity(sectors);
    }

    word reg(const word offset)
    {
        return block.read_word(kBlockBase + offset);
    }

    void set(const word offset, const word value)
    {
        block.write_word(kBlockBase + offset, value);
    }

    void wait()
    {
        for (word i = 0; i < 1000 && (reg(0x04) & BlockDevice::kBusy); i++)
        {
            block.tick();
        }
    }

    /// Fills the buffer with `base + word index` and writes it to the sector.
    void write_sector(const word sector, const word base)
    {
        set(0x10, 0);
        for (word i = 0; i < 128; i++)
        {
            set(0x0C, base + i);
        }
        set(0x08, sector);
        set(0x00, BlockDevice::kCmdWrite);
        wait();
        set(0x04, 0);
    }

    /// Reads the sector into the buffer and returns its first and last word.
    std::pair<word, word> read_sector(const word sector)
    {
        set(0x08, sector);
        set(0x00, BlockDevice::kCmdRead);
        wait();
        set(0x04, 0);
        word first = reg(0x0C), last = 0;
        for (word i = 1; i < 128; i++)
        {
            last = reg(0x0C);
        }
        return {first, last};
    }
};

} // namespace

TEST(BlockDeviceUnit, a_sector_that_is_written_can_be_read_back)
{
    Disk512 disk(4);
    EXPECT_EQ(disk.reg(0x14), 4u);
    disk.write_sector(2, 1000);
    disk.write_sector(1, 5000);

    // Scribble over the buffer, then read the sector.
    disk.set(0x10, 0);
    disk.set(0x0C, 0xDEADBEEF);
    const auto [first, last] = disk.read_sector(2);
    EXPECT_EQ(first, 1000u);
    EXPECT_EQ(last, 1127u);
    EXPECT_EQ(disk.read_sector(1).first, 5000u);
    EXPECT_EQ(disk.read_sector(0).first, 0u) << "the others are untouched";
    EXPECT_EQ(disk.block.sector(2).size(), 512u);
    EXPECT_EQ(disk.block.sector(2)[0], 1000 & 0xFF);
    EXPECT_EQ(disk.block.sector(2)[1], 1000 >> 8);
}

TEST(BlockDeviceUnit, a_command_takes_latency_instructions)
{
    Disk512 disk(1);
    disk.set(0x1C, 5);
    disk.set(0x00, BlockDevice::kCmdRead);
    EXPECT_EQ(disk.reg(0x04), BlockDevice::kBusy);
    for (int i = 0; i < 4; i++)
    {
        disk.block.tick();
    }
    EXPECT_EQ(disk.reg(0x04), BlockDevice::kBusy) << "four of five";
    disk.block.tick();
    EXPECT_EQ(disk.reg(0x04), BlockDevice::kDone);
    EXPECT_EQ(disk.reg(0x1C), 5u);

    disk.set(0x04, 0); // acknowledge
    EXPECT_EQ(disk.reg(0x04), 0u);
}

TEST(BlockDeviceUnit, a_sector_outside_of_the_disk_is_an_error_and_changes_nothing)
{
    Disk512 disk(2);
    disk.write_sector(1, 77);
    disk.set(0x10, 0);
    disk.set(0x0C, 1);
    disk.set(0x08, 2);
    disk.set(0x00, BlockDevice::kCmdWrite);
    disk.wait();
    EXPECT_EQ(disk.reg(0x04), BlockDevice::kDone | BlockDevice::kError);
    EXPECT_TRUE(disk.block.sector(2).empty());

    Disk512 none(0);
    none.set(0x00, BlockDevice::kCmdRead);
    none.wait();
    EXPECT_TRUE(none.reg(0x04) & BlockDevice::kError) << "no disk";
    EXPECT_EQ(none.reg(0x14), 0u);
}

TEST(BlockDeviceUnit, a_command_while_busy_is_refused)
{
    Disk512 disk(2);
    disk.set(0x00, BlockDevice::kCmdRead);
    disk.set(0x00, BlockDevice::kCmdWrite);
    EXPECT_TRUE(disk.reg(0x04) & BlockDevice::kError);
    disk.wait();
    EXPECT_TRUE(disk.reg(0x04) & BlockDevice::kDone);
    disk.set(0x04, 0);
    disk.set(0x00, 9); // not a command
    EXPECT_EQ(disk.reg(0x04), BlockDevice::kError);
}

TEST(BlockDeviceUnit, the_data_register_walks_through_the_buffer)
{
    Disk512 disk(1);
    disk.set(0x10, 8);
    EXPECT_EQ(disk.reg(0x10), 8u);
    disk.set(0x0C, 0x11223344);
    EXPECT_EQ(disk.reg(0x10), 12u);
    disk.set(0x10, 8);
    EXPECT_EQ(disk.reg(0x0C), 0x11223344u);

    disk.set(0x10, 512); // past the end: nothing to read or write
    EXPECT_EQ(disk.reg(0x0C), 0u);
    disk.set(0x0C, 5);
    EXPECT_EQ(disk.reg(0x10), 512u);
}

TEST(BlockDeviceUnit, completion_raises_line_2_when_enabled)
{
    Disk512 disk(1);
    disk.intc.write_word(kIntcBase + 8, 1u << kIrqLineBlock);
    disk.set(0x00, BlockDevice::kCmdRead);
    disk.wait();
    EXPECT_FALSE(disk.intc.has_pending()) << "the disk's interrupt is off";
    disk.set(0x04, 0);

    disk.set(0x18, 1);
    disk.set(0x00, BlockDevice::kCmdRead);
    disk.wait();
    EXPECT_EQ(disk.intc.claim(), kIrqLineBlock);
}

TEST(BlockDeviceUnit, the_disk_can_live_in_a_file_and_flush_writes_it)
{
    const auto path = std::filesystem::temp_directory_path() / "aemu_block_test.img";
    std::filesystem::remove(path);
    {
        Disk512 disk(0);
        ASSERT_TRUE(disk.block.open_file(path.string(), 3));
        EXPECT_EQ(disk.reg(0x14), 3u) << "the file is made with 3 sectors";
        disk.write_sector(2, 42);
        disk.set(0x00, BlockDevice::kCmdFlush);
        disk.wait();
        EXPECT_EQ(disk.reg(0x04), BlockDevice::kDone);
    }
    EXPECT_EQ(std::filesystem::file_size(path), 3u * 512);

    Disk512 again(0);
    ASSERT_TRUE(again.block.open_file(path.string()));
    EXPECT_EQ(again.reg(0x14), 3u) << "the size of the file";
    EXPECT_EQ(again.read_sector(2).first, 42u);
    std::filesystem::remove(path);
}

TEST(BlockDeviceUnit, a_reset_keeps_the_disk_but_not_the_controller)
{
    Disk512 disk(2);
    disk.write_sector(1, 9);
    disk.set(0x1C, 7);
    disk.set(0x18, 1);
    disk.block.reset();
    EXPECT_EQ(disk.reg(0x1C), 100u);
    EXPECT_EQ(disk.reg(0x18), 0u);
    EXPECT_EQ(disk.reg(0x14), 2u);
    EXPECT_EQ(disk.read_sector(1).first, 9u);
}

// WFI waits for the disk the way it does for the timer: the time jumps to its completion.
TEST_F(Devices, wfi_waits_for_the_block_device)
{
    cpu.system_bus->block.set_capacity(4);
    BlockDevice &block = cpu.system_bus->block;
    block.write_word(kBlockBase + 0x0C, 0xCAFE);
    block.write_word(kBlockBase + 0x08, 3);
    block.write_word(kBlockBase + 0x00, BlockDevice::kCmdWrite);
    block.advance(1000);
    block.write_word(kBlockBase + 0x04, 0);

    std::vector<word> program;
    append(program, constant(10, kIntcBase));
    append(program, constant(11, kBlockBase));
    program.insert(program.end(),
                   {mov(1, 1u << kIrqLineBlock), store_at(1, 10, 0x08), // enable line 2
                    mov(1, 1), store_at(1, 11, 0x18),                   // interrupt on completion
                    mov(1, 3), store_at(1, 11, 0x08),                   // sector 3
                    mov(1, 5000), store_at(1, 11, 0x1C),                // slow disk
                    mov(1, BlockDevice::kCmdRead), store_at(1, 11, 0x00), E::asm_wfi(),
                    load_at(8, 10, 0),                                  // claim: the disk's line
                    load_at(9, 11, 0x0C),  // the first word of the sector
                    load_at(12, 11, 0x04), // status
                    E::asm_hlt()});
    const auto result = run(program);
    ASSERT_EQ(result.status, Status::HALTED) << result.message;
    EXPECT_EQ(cpu.read_reg(8), kIrqLineBlock);
    EXPECT_EQ(cpu.read_reg(9), 0xCAFEu);
    EXPECT_EQ(cpu.read_reg(12), BlockDevice::kDone);
    EXPECT_LT(result.instructions_ran, 60u) << "it did not execute 5000 instructions";
}

// --- block device DMA ----------------------------------------------------------------------

namespace
{

/// A disk512 plus 2 pages of RAM at page 1 (0x1000-0x2FFF) that DMA reaches.
struct DmaDisk : Disk512
{
    RAM ram{2, 1};

    explicit DmaDisk(const word sectors) :
        Disk512(sectors)
    {
        block.set_dma_memory(&ram);
    }

    /// Runs a DMA command to completion and returns the status (acknowledged).
    word dma(const word command, const word sector, const word address, const word count)
    {
        set(0x08, sector);
        set(0x20, address);
        set(0x24, count);
        set(0x00, command);
        wait();
        const word status = reg(0x04);
        set(0x04, 0);
        return status;
    }
};

} // namespace

TEST(BlockDeviceDma, many_sectors_move_to_ram_and_back)
{
    DmaDisk disk(8);
    for (word i = 0; i < 3; i++)
    {
        disk.write_sector(2 + i, 1000 * (i + 1));
    }

    EXPECT_EQ(disk.dma(BlockDevice::kCmdDmaRead, 2, 0x1000, 3), BlockDevice::kDone);
    EXPECT_EQ(disk.ram.read_word(0x1000), 1000u);
    EXPECT_EQ(disk.ram.read_word(0x1000 + 512 + 4), 2001u);
    EXPECT_EQ(disk.ram.read_word(0x1000 + 1024 + 508), 3127u);

    // Write them to other sectors from RAM.
    EXPECT_EQ(disk.dma(BlockDevice::kCmdDmaWrite, 5, 0x1000, 3), BlockDevice::kDone);
    EXPECT_EQ(disk.block.sector(5), disk.block.sector(2));
    EXPECT_EQ(disk.block.sector(7), disk.block.sector(4));
    EXPECT_EQ(disk.reg(0x20), 0x1000u);
    EXPECT_EQ(disk.reg(0x24), 3u);
}

TEST(BlockDeviceDma, it_takes_latency_per_sector_and_leaves_the_data_buffer_alone)
{
    DmaDisk disk(4);
    disk.write_sector(0, 7); // leaves the buffer at 7...
    disk.set(0x1C, 3);
    disk.set(0x08, 0);
    disk.set(0x20, 0x1000);
    disk.set(0x24, 2);
    disk.set(0x00, BlockDevice::kCmdDmaRead);
    for (int i = 0; i < 5; i++)
    {
        disk.block.tick();
    }
    EXPECT_EQ(disk.reg(0x04), BlockDevice::kBusy) << "5 of 6";
    EXPECT_EQ(disk.ram.read_word(0x1000), 0u) << "nothing moved yet";
    disk.block.tick();
    EXPECT_EQ(disk.reg(0x04), BlockDevice::kDone);
    EXPECT_EQ(disk.ram.read_word(0x1000), 7u);
    disk.set(0x10, 0);
    EXPECT_EQ(disk.reg(0x0C), 7u) << "the buffer is as it was";
}

TEST(BlockDeviceDma, a_bad_transfer_is_an_error_and_moves_nothing)
{
    DmaDisk disk(4);
    disk.write_sector(1, 5);
    const word failed = BlockDevice::kDone | BlockDevice::kError;

    EXPECT_EQ(disk.dma(BlockDevice::kCmdDmaRead, 1, 0x1000, 0), failed) << "no sectors";
    EXPECT_EQ(disk.dma(BlockDevice::kCmdDmaRead, 1, 0x1002, 1), failed) << "not word aligned";
    EXPECT_EQ(disk.dma(BlockDevice::kCmdDmaRead, 1, 0x0000, 1), failed) << "not RAM";
    EXPECT_EQ(disk.dma(BlockDevice::kCmdDmaRead, 1, 0x2E00 + 4, 1), failed) << "runs off the RAM";
    EXPECT_EQ(disk.dma(BlockDevice::kCmdDmaRead, 1, 0xFFFFFE00, 2), failed) << "wraps around";
    EXPECT_EQ(disk.dma(BlockDevice::kCmdDmaRead, 3, 0x1000, 2), failed) << "runs off the disk";
    disk.set(0x08, 0); // a huge count takes long (capped), so skip the wait
    disk.set(0x20, 0x1000);
    disk.set(0x24, 0xFFFFFFFF);
    disk.set(0x00, BlockDevice::kCmdDmaRead);
    disk.block.advance(0xFFFFFFFF);
    EXPECT_EQ(disk.reg(0x04), failed) << "huge";
    disk.set(0x04, 0);
    EXPECT_EQ(disk.dma(BlockDevice::kCmdDmaWrite, 4, 0x1000, 1), failed) << "past the disk";
    EXPECT_EQ(disk.ram.read_word(0x1000), 0u);
    EXPECT_EQ(disk.ram.read_word(0x2E04), 0u);
    EXPECT_EQ(disk.block.sector(1)[0], 5) << "the disk is untouched";

    BlockDevice lone{disk.intc}; // no DMA memory
    lone.set_capacity(1);
    lone.write_word(kBlockBase + 0x24, 1);
    lone.write_word(kBlockBase + 0x00, BlockDevice::kCmdDmaRead);
    lone.advance(1000);
    EXPECT_EQ(lone.read_word(kBlockBase + 0x04), failed);
}

TEST(BlockDeviceDma, a_reset_clears_the_dma_registers_and_an_interrupt_is_raised)
{
    DmaDisk disk(2);
    disk.intc.write_word(kIntcBase + 8, 1u << kIrqLineBlock);
    disk.set(0x18, 1);
    disk.dma(BlockDevice::kCmdDmaRead, 0, 0x1000, 1);
    EXPECT_EQ(disk.intc.claim(), kIrqLineBlock);

    disk.block.reset();
    EXPECT_EQ(disk.reg(0x20), 0u);
    EXPECT_EQ(disk.reg(0x24), 0u);
}

// --- scatter/gather ----------------------------------------------------------------------

namespace
{

struct Descriptor
{
    word sector, count, address;
};

/// Writes the descriptors at `list` (in the RAM of the fixture) and runs a command 6 or 7 on them.
word scatter(DmaDisk &disk, const word command, const word list,
             const std::vector<Descriptor> &descriptors, const word count_register)
{
    for (size_t i = 0; i < descriptors.size(); i++)
    {
        disk.ram.write_word(list + word(i) * 12, descriptors[i].sector);
        disk.ram.write_word(list + word(i) * 12 + 4, descriptors[i].count);
        disk.ram.write_word(list + word(i) * 12 + 8, descriptors[i].address);
    }
    disk.set(0x28, list);
    disk.set(0x24, count_register);
    disk.set(0x00, command);
    disk.wait();
    const word status = disk.reg(0x04);
    disk.set(0x04, 0);
    return status;
}

} // namespace

TEST(BlockDeviceScatterGather, a_list_of_pieces_moves_between_the_disk_and_several_buffers)
{
    DmaDisk disk(8);
    for (word i = 0; i < 6; i++)
    {
        disk.write_sector(i, 100 * (i + 1));
    }

    // Sector 4 to 0x1000, then sectors 0 and 1 to 0x1400, which is not where the first ends.
    EXPECT_EQ(
        scatter(disk, BlockDevice::kCmdScatterRead, 0x2000, {{4, 1, 0x1000}, {0, 2, 0x1400}}, 2),
        BlockDevice::kDone);
    EXPECT_EQ(disk.ram.read_word(0x1000), 500u);
    EXPECT_EQ(disk.ram.read_word(0x1400), 100u);
    EXPECT_EQ(disk.ram.read_word(0x1400 + 512), 200u);
    EXPECT_EQ(disk.ram.read_word(0x1200), 0u) << "between the buffers nothing is written";

    // And back to other sectors: RAM 0x1400 (two sectors) to sector 6, RAM 0x1000 to sector 3.
    EXPECT_EQ(
        scatter(disk, BlockDevice::kCmdScatterWrite, 0x2000, {{6, 2, 0x1400}, {3, 1, 0x1000}}, 2),
        BlockDevice::kDone);
    EXPECT_EQ(disk.block.sector(6), disk.block.sector(0));
    EXPECT_EQ(disk.block.sector(7), disk.block.sector(1));
    EXPECT_EQ(disk.block.sector(3), disk.block.sector(4));
    EXPECT_EQ(disk.reg(0x28), 0x2000u);
}

TEST(BlockDeviceScatterGather, it_takes_latency_for_each_sector_of_all_the_pieces)
{
    DmaDisk disk(8);
    disk.ram.write_word(0x2000 + 0, 0);
    disk.ram.write_word(0x2000 + 4, 2);
    disk.ram.write_word(0x2000 + 8, 0x1000);
    disk.ram.write_word(0x2000 + 12, 5);
    disk.ram.write_word(0x2000 + 16, 1);
    disk.ram.write_word(0x2000 + 20, 0x1800);
    disk.set(0x1C, 2);
    disk.set(0x28, 0x2000);
    disk.set(0x24, 2);
    disk.set(0x00, BlockDevice::kCmdScatterRead);

    for (int i = 0; i < 5; i++)
    {
        disk.block.tick();
    }
    EXPECT_EQ(disk.reg(0x04), BlockDevice::kBusy) << "5 of 6 (3 sectors, 2 instructions each)";
    disk.block.tick();
    EXPECT_EQ(disk.reg(0x04), BlockDevice::kDone);
}

TEST(BlockDeviceScatterGather, one_piece_that_is_not_valid_fails_all_of_them)
{
    DmaDisk disk(4);
    disk.write_sector(1, 5);
    const word failed = BlockDevice::kDone | BlockDevice::kError;

    EXPECT_EQ(
        scatter(disk, BlockDevice::kCmdScatterRead, 0x2000, {{1, 1, 0x1000}, {3, 2, 0x1400}}, 2),
        failed)
        << "the second runs off the disk";
    EXPECT_EQ(disk.ram.read_word(0x1000), 0u) << "and the first was not done";
    EXPECT_EQ(
        scatter(disk, BlockDevice::kCmdScatterRead, 0x2000, {{1, 1, 0x1000}, {0, 1, 0x1402}}, 2),
        failed)
        << "not word aligned";
    EXPECT_EQ(scatter(disk, BlockDevice::kCmdScatterRead, 0x2000, {{1, 0, 0x1000}}, 1), failed)
        << "no sectors";
    EXPECT_EQ(scatter(disk, BlockDevice::kCmdScatterRead, 0x2000, {{1, 1, 0x0000}}, 1), failed)
        << "not RAM";
    EXPECT_EQ(scatter(disk, BlockDevice::kCmdScatterWrite, 0x2000, {{9, 1, 0x1000}}, 1), failed)
        << "past the disk";
    EXPECT_EQ(disk.block.sector(1)[0], 5) << "the disk is untouched";
}

TEST(BlockDeviceScatterGather, the_list_has_to_be_there)
{
    DmaDisk disk(4);
    const word failed = BlockDevice::kDone | BlockDevice::kError;

    EXPECT_EQ(scatter(disk, BlockDevice::kCmdScatterRead, 0x2000, {}, 0), failed) << "no pieces";
    EXPECT_EQ(
        scatter(disk, BlockDevice::kCmdScatterRead, 0x2000, {}, BlockDevice::kMaxDescriptors + 1),
        failed)
        << "too many";
    EXPECT_EQ(scatter(disk, BlockDevice::kCmdScatterRead, 0x2002, {{0, 1, 0x1000}}, 1), failed)
        << "the list is not word aligned";
    EXPECT_EQ(scatter(disk, BlockDevice::kCmdScatterRead, 0x2FF8, {}, 1), failed)
        << "the list runs off the RAM";
    EXPECT_EQ(scatter(disk, BlockDevice::kCmdScatterRead, 0x0000, {}, 1), failed)
        << "the list is not in RAM";
    EXPECT_EQ(scatter(disk, BlockDevice::kCmdScatterRead, 0xFFFFFFF8, {}, 2), failed) << "wraps";

    BlockDevice lone{disk.intc}; // no DMA memory
    lone.set_capacity(1);
    lone.write_word(kBlockBase + 0x24, 1);
    lone.write_word(kBlockBase + 0x00, BlockDevice::kCmdScatterRead);
    lone.advance(1000);
    EXPECT_EQ(lone.read_word(kBlockBase + 0x04), failed);
}

TEST(BlockDeviceScatterGather,
     where_the_pieces_overlap_the_later_one_wins_and_a_reset_clears_the_list)
{
    DmaDisk disk(4);
    disk.write_sector(0, 11);
    disk.write_sector(1, 22);

    EXPECT_EQ(
        scatter(disk, BlockDevice::kCmdScatterRead, 0x2000, {{0, 1, 0x1000}, {1, 1, 0x1000}}, 2),
        BlockDevice::kDone);
    EXPECT_EQ(disk.ram.read_word(0x1000), 22u);

    disk.block.reset();
    EXPECT_EQ(disk.reg(0x28), 0u);
}

// The machine wires the block device to its RAM: a program moves two sectors in one command.
TEST_F(Devices, a_program_reads_sectors_by_dma)
{
    BlockDevice &block = cpu.system_bus->block;
    block.set_capacity(4);
    for (word s = 0; s < 2; s++)
    {
        block.write_word(kBlockBase + 0x10, 0);
        for (word i = 0; i < 128; i++)
        {
            block.write_word(kBlockBase + 0x0C, (s + 1) * 0x100 + i);
        }
        block.write_word(kBlockBase + 0x08, s + 1);
        block.write_word(kBlockBase + 0x00, BlockDevice::kCmdWrite);
        block.advance(1000);
        block.write_word(kBlockBase + 0x04, 0);
    }

    std::vector<word> program;
    append(program, constant(10, kIntcBase));
    append(program, constant(11, kBlockBase));
    append(program, constant(12, 0x8000));
    program.insert(program.end(),
                   {mov(1, 1u << kIrqLineBlock), store_at(1, 10, 0x08), // enable line 2
                    mov(1, 1), store_at(1, 11, 0x18),                   // interrupt on completion
                    mov(1, 1), store_at(1, 11, 0x08),                   // sector 1
                    mov(1, 2), store_at(1, 11, 0x24),                   // two sectors
                    store_at(12, 11, 0x20),                             // to 0x8000
                    mov(1, BlockDevice::kCmdDmaRead), store_at(1, 11, 0x00), E::asm_wfi(),
                    load_at(3, 12, 0), load_at(4, 12, 512), E::asm_hlt()});
    const auto result = run(program);
    ASSERT_EQ(result.status, Status::HALTED) << result.message;
    EXPECT_EQ(cpu.read_reg(3), 0x100u);
    EXPECT_EQ(cpu.read_reg(4), 0x200u);
}

TEST_F(Devices, an_address_in_the_device_window_without_a_device_is_a_bus_error)
{
    EXPECT_THROW(cpu.memory.read_word(kDeviceEnd), SystemBus::BusError);
}

// --- display -------------------------------------------------------------------------------

namespace
{

/// A display with 2 pages of RAM at page 1 (0x1000-0x2FFF) for its picture, and line 3 enabled.
struct Screen
{
    InterruptController intc;
    Display display{intc};
    RAM ram{2, 1};

    Screen()
    {
        intc.write_word(kIntcBase + InterruptController::kRegEnable, 1u << kIrqLineDisplay);
        display.set_ram(&ram);
    }

    word reg(const word offset)
    {
        return display.read_word(kDisplayBase + offset);
    }

    void set(const word offset, const word value)
    {
        display.write_word(kDisplayBase + offset, value);
    }

    void ticks(const word count)
    {
        for (word i = 0; i < count; i++)
        {
            display.tick();
        }
    }

    /// A picture of `width` by `height` at 0x1000 in the format.
    void picture(const word width, const word height, const word format)
    {
        set(Display::kRegAddress, 0x1000);
        set(Display::kRegWidth, width);
        set(Display::kRegHeight, height);
        set(Display::kRegFormat, format);
    }
};

} // namespace

TEST(DisplayUnit, a_frame_ends_every_interval_instructions_while_it_is_on)
{
    Screen screen;
    screen.picture(4, 4, Display::kFormatIndexed);
    screen.set(Display::kRegInterval, 10);
    screen.ticks(50);
    EXPECT_EQ(screen.reg(Display::kRegFrames), 0u) << "it is off";

    screen.set(Display::kRegCtrl, Display::kCtrlOn | Display::kCtrlFrameInterrupt);
    screen.ticks(9);
    EXPECT_EQ(screen.reg(Display::kRegStatus), 0u);
    EXPECT_FALSE(screen.intc.has_pending());
    screen.ticks(1);
    EXPECT_EQ(screen.reg(Display::kRegFrames), 1u);
    EXPECT_EQ(screen.reg(Display::kRegStatus), Display::kStatusFrame);
    EXPECT_EQ(screen.intc.claim(), kIrqLineDisplay);

    screen.set(Display::kRegStatus, 0);
    EXPECT_EQ(screen.reg(Display::kRegStatus), 0u);
    screen.ticks(5);
    screen.set(Display::kRegInterval, 10); // a new frame starts
    screen.ticks(9);
    EXPECT_EQ(screen.reg(Display::kRegFrames), 1u);
    screen.ticks(1);
    EXPECT_EQ(screen.reg(Display::kRegFrames), 2u);
    EXPECT_EQ(screen.intc.claim(), kIrqLineDisplay);

    screen.set(Display::kRegCtrl, Display::kCtrlOn);
    screen.ticks(10);
    EXPECT_EQ(screen.reg(Display::kRegFrames), 3u);
    EXPECT_FALSE(screen.intc.has_pending()) << "the interrupt is off";

    screen.ticks(4);
    screen.set(Display::kRegCtrl, 0);
    screen.set(Display::kRegCtrl, Display::kCtrlOn); // turning it on starts a frame
    screen.ticks(9);
    EXPECT_EQ(screen.reg(Display::kRegFrames), 3u);
    screen.ticks(1);
    EXPECT_EQ(screen.reg(Display::kRegFrames), 4u);
}

TEST(DisplayUnit, the_picture_is_read_through_the_palette_or_as_words)
{
    Screen screen;
    screen.picture(3, 2, Display::kFormatIndexed);
    const byte indexes[] = {0x00, 0xFF, 0xE0, 0x1C, 0x03, 0x01};
    screen.ram.write_block(0x1000, indexes, sizeof(indexes));

    Frame frame;
    ASSERT_TRUE(screen.display.capture(frame));
    EXPECT_EQ(frame.width, 3u);
    EXPECT_EQ(frame.height, 2u);
    EXPECT_EQ(frame.pixels,
              (std::vector<word>{0x000000, 0xFFFFFF, 0xFF0000, 0x00FF00, 0x0000FF, 0x000055}))
        << "the palette of a reset is rrrgggbb";

    screen.set(Display::kRegPalette + 4 * 1, 0xFF123456);
    EXPECT_EQ(screen.reg(Display::kRegPalette + 4 * 1), 0x123456u);
    ASSERT_TRUE(screen.display.capture(frame));
    EXPECT_EQ(frame.pixels[5], 0x123456u);

    screen.picture(2, 1, Display::kFormatRgb);
    screen.ram.write_word(0x1000, 0xFFABCDEF);
    screen.ram.write_word(0x1004, 0x00010203);
    ASSERT_TRUE(screen.display.capture(frame));
    EXPECT_EQ(frame.pixels, (std::vector<word>{0xABCDEF, 0x010203}));
}

TEST(DisplayUnit, a_picture_that_is_not_all_in_ram_is_an_error)
{
    Screen screen;
    Frame frame;
    screen.picture(64, 32, Display::kFormatRgb); // 8 KiB: one byte more than the RAM has
    screen.set(Display::kRegAddress, 0x1004);
    EXPECT_FALSE(screen.display.capture(frame));

    screen.set(Display::kRegInterval, 1);
    screen.set(Display::kRegCtrl, Display::kCtrlOn);
    screen.ticks(1);
    EXPECT_EQ(screen.reg(Display::kRegStatus), Display::kStatusFrame | Display::kStatusError);

    screen.set(Display::kRegAddress, 0x1000);
    EXPECT_TRUE(screen.display.capture(frame)) << "it fits from the start of the RAM";
    screen.set(Display::kRegAddress, 0x1002);
    EXPECT_FALSE(screen.display.capture(frame)) << "not a multiple of 4";
    screen.set(Display::kRegAddress, 0x1000);
    screen.set(Display::kRegWidth, 0);
    EXPECT_FALSE(screen.display.capture(frame));
    screen.set(Display::kRegWidth, Display::kMaxSize + 1);
    screen.set(Display::kRegHeight, 1);
    EXPECT_FALSE(screen.display.capture(frame));
    screen.display.set_ram(nullptr);
    screen.set(Display::kRegWidth, 1);
    EXPECT_FALSE(screen.display.capture(frame));
}

TEST(DisplayUnit, advance_lets_many_frames_pass_and_wfi_only_waits_for_the_interrupt)
{
    Screen screen;
    screen.set(Display::kRegInterval, 10);
    screen.set(Display::kRegCtrl, Display::kCtrlOn);
    EXPECT_EQ(screen.display.cycles_until_event(), word(-1)) << "no interrupt to wait for";
    screen.set(Display::kRegCtrl, Display::kCtrlOn | Display::kCtrlFrameInterrupt);
    EXPECT_EQ(screen.display.cycles_until_event(), 10u);

    screen.display.advance(3);
    EXPECT_EQ(screen.display.cycles_until_event(), 7u);
    screen.display.advance(22);
    EXPECT_EQ(screen.reg(Display::kRegFrames), 2u);
    EXPECT_EQ(screen.display.cycles_until_event(), 5u);
    screen.ticks(4);
    EXPECT_EQ(screen.reg(Display::kRegFrames), 2u);
    screen.ticks(1);
    EXPECT_EQ(screen.reg(Display::kRegFrames), 3u);
}

TEST(DisplayUnit, a_reset_turns_it_off_and_restores_the_registers_and_the_palette)
{
    Screen screen;
    screen.picture(8, 8, Display::kFormatRgb);
    screen.set(Display::kRegInterval, 1);
    screen.set(Display::kRegCtrl, Display::kCtrlOn);
    screen.set(Display::kRegPalette, 0x123456);
    screen.ticks(3);
    screen.set(Display::kRegFormat, 3);
    EXPECT_EQ(screen.reg(Display::kRegFormat), Display::kFormatRgb) << "the bits of a format";
    screen.set(Display::kRegInterval, 0);
    EXPECT_EQ(screen.reg(Display::kRegInterval), 1u) << "at least 1";

    screen.display.reset();
    EXPECT_EQ(screen.reg(Display::kRegCtrl), 0u);
    EXPECT_EQ(screen.reg(Display::kRegStatus), 0u);
    EXPECT_EQ(screen.reg(Display::kRegAddress), 0u);
    EXPECT_EQ(screen.reg(Display::kRegWidth), Display::kDefaultWidth);
    EXPECT_EQ(screen.reg(Display::kRegHeight), Display::kDefaultHeight);
    EXPECT_EQ(screen.reg(Display::kRegFormat), Display::kFormatIndexed);
    EXPECT_EQ(screen.reg(Display::kRegInterval), Display::kDefaultInterval);
    EXPECT_EQ(screen.reg(Display::kRegFrames), 0u);
    EXPECT_EQ(screen.reg(Display::kRegPalette), 0u);
    EXPECT_EQ(screen.reg(Display::kRegPalette + 4 * 0xFF), 0xFFFFFFu);
}

TEST(DisplayUnit, each_frame_goes_to_the_window_of_the_host)
{
    Screen screen;
    const auto host = std::make_shared<HostDisplay>(0);
    screen.display.attach_host_display(host);
    screen.picture(2, 1, Display::kFormatIndexed);
    screen.ram.write_byte(0x1001, 0xE0);
    screen.set(Display::kRegInterval, 5);
    screen.set(Display::kRegCtrl, Display::kCtrlOn);

    Frame frame;
    EXPECT_FALSE(host->take(frame));
    screen.ticks(5);
    ASSERT_TRUE(host->take(frame));
    EXPECT_EQ(frame.pixels, (std::vector<word>{0x000000, 0xFF0000}));
    EXPECT_FALSE(host->take(frame)) << "it was taken";

    screen.ram.write_byte(0x1000, 0x03);
    screen.ticks(5);
    ASSERT_TRUE(host->take(frame));
    EXPECT_EQ(frame.pixels, (std::vector<word>{0x0000FF, 0xFF0000}));

    screen.set(Display::kRegWidth, 0);
    screen.ticks(5);
    EXPECT_FALSE(host->take(frame)) << "a picture that is not valid is not handed over";

    EXPECT_FALSE(host->closed());
    host->close();
    EXPECT_TRUE(host->closed());
}

// A program draws a pixel, turns the display on and waits for the end of the frame with WFI.
TEST_F(Devices, wfi_waits_for_the_end_of_a_frame)
{
    std::vector<word> program;
    append(program, constant(10, kIntcBase));
    append(program, constant(11, kDisplayBase));
    program.insert(program.end(),
                   {mov(1, 1u << kIrqLineDisplay), store_at(1, 10, 0x08),    // enable line 3
                    mov(12, 0x4000), mov(1, 0x1C), store_at(1, 12, 0),       // a green pixel
                    store_at(12, 11, 0x08),                                  // the picture
                    mov(1, 1), store_at(1, 11, 0x0C), store_at(1, 11, 0x10), // 1 by 1
                    mov(1, 5000), store_at(1, 11, 0x18),                     // a slow frame
                    mov(1, 3), store_at(1, 11, 0x00), E::asm_wfi(),          // on, interrupt
                    load_at(8, 10, 0),                                       // claim
                    load_at(9, 11, 0x1C),                                    // frames
                    E::asm_hlt()});
    const auto result = run(program);
    ASSERT_EQ(result.status, Status::HALTED) << result.message;
    EXPECT_EQ(cpu.read_reg(8), kIrqLineDisplay);
    EXPECT_EQ(cpu.read_reg(9), 1u);
    EXPECT_LT(result.instructions_ran, 40u) << "it did not execute 5000 instructions";

    Frame frame;
    ASSERT_TRUE(cpu.system_bus->display.capture(frame)) << "the machine wired the RAM";
    EXPECT_EQ(frame.pixels, std::vector<word>{0x00FF00});
}

// --- the clock of the devices --------------------------------------------------------------

// The devices are moved on only when one of them has something to do or is looked at, and still
// see every instruction: across WFI, register reads and a write of the count.
TEST_F(Devices, the_timer_counts_every_instruction_across_wfi_and_its_registers)
{
    std::vector<word> program;
    append(program, constant(10, kIntcBase));
    append(program, constant(11, kTimerBase));
    program.insert(program.end(), {mov(1, 1), store_at(1, 10, 0x08),    // enable line 0
                                   mov(1, 1000), store_at(1, 11, 0x04), // compare
                                   mov(1, 100), store_at(1, 11, 0x0C),  // interval
                                   mov(1, 3), store_at(1, 11, 0x08),    // on, periodic
                                   nop(), nop(), nop(),                 // no device access
                                   E::asm_wfi()});                      // to count 1000
    // It fires at 1100 and 1200, then the compare and the count are read and the count is set
    // to 0.
    program.insert(program.end(), 250, nop());
    program.insert(program.end(),
                   {load_at(8, 11, 0x04), load_at(9, 11, 0x00), mov(2, 0), store_at(2, 11, 0x00)});
    program.insert(program.end(), 10, nop());
    program.insert(program.end(), {load_at(12, 11, 0x00), E::asm_hlt()});

    const auto result = run(program);
    ASSERT_EQ(result.status, Status::HALTED) << result.message;
    EXPECT_EQ(cpu.read_reg(8), 1300u) << "the compare of the third period";
    EXPECT_EQ(cpu.read_reg(9), 1252u) << "1000, the wfi, 250 nops and the load of the compare";
    EXPECT_EQ(cpu.read_reg(12), 11u) << "the store and 10 nops";
}

// What the host changes directly between two runs is seen by the next run, and the devices are up
// to date when a run ends.
TEST_F(Devices, a_run_starts_and_ends_with_the_devices_up_to_date)
{
    Timer &timer = cpu.system_bus->timer;
    cpu.system_bus->intc.write_word(kIntcBase + InterruptController::kRegEnable, 1);
    timer.write_word(kTimerBase + Timer::kRegCompare, 100);
    timer.write_word(kTimerBase + Timer::kRegCtrl, Timer::kEnabled);
    timer.advance(90);

    const auto result = run(std::vector<word>(20, nop()), 15);
    EXPECT_EQ(result.instructions_ran, 15u);
    EXPECT_EQ(timer.count(), 105u);
    EXPECT_TRUE(cpu.system_bus->intc.has_pending()) << "it fired at 100, 10 instructions in";
}

// A frame ends INTERVAL instructions after the display was turned on, which a program that reads
// STATUS sees at that instruction.
TEST_F(Devices, a_program_sees_the_end_of_a_frame_at_the_instruction_it_happens)
{
    std::vector<word> program;
    append(program, constant(11, kDisplayBase));
    program.insert(program.end(), {mov(12, 0x4000), store_at(12, 11, 0x08), // the picture
                                   mov(1, 1), store_at(1, 11, 0x0C), store_at(1, 11, 0x10),
                                   mov(1, 50), store_at(1, 11, 0x18),       // interval
                                   mov(1, 1), store_at(1, 11, 0x00)});      // on
    program.insert(program.end(), 48, nop());
    program.insert(program.end(), {load_at(3, 11, 0x04), // the 49th instruction: not yet
                                   load_at(4, 11, 0x04), // after the 50th
                                   load_at(5, 11, 0x1C), E::asm_hlt()});

    const auto result = run(program);
    ASSERT_EQ(result.status, Status::HALTED) << result.message;
    EXPECT_EQ(cpu.read_reg(3), 0u);
    EXPECT_EQ(cpu.read_reg(4), Display::kStatusFrame);
    EXPECT_EQ(cpu.read_reg(5), 1u);
}

// --- the system controller -----------------------------------------------------------------

TEST(SystemControllerUnit, it_says_what_memory_there_is_and_keeps_a_request_until_a_reset)
{
    const RAM ram(16, 2);
    const ROM rom(4, 32);
    SystemController sysctl(ram, rom);
    EXPECT_EQ(sysctl.read_word(kSysctlBase + SystemController::kRegRamStart), 0x2000u);
    EXPECT_EQ(sysctl.read_word(kSysctlBase + SystemController::kRegRamSize), 16u * kPageSize);
    EXPECT_EQ(sysctl.read_word(kSysctlBase + SystemController::kRegRomStart), 0x20000u);
    EXPECT_EQ(sysctl.read_word(kSysctlBase + SystemController::kRegRomSize), 4u * kPageSize);
    EXPECT_EQ(sysctl.request(), SystemController::Request::NONE);

    sysctl.write_word(kSysctlBase + SystemController::kRegPowerOff, 300);
    EXPECT_EQ(sysctl.request(), SystemController::Request::POWER_OFF);
    EXPECT_EQ(sysctl.exit_code(), 300u);
    EXPECT_EQ(sysctl.read_word(kSysctlBase + SystemController::kRegPowerOff), 0u) << "write only";

    sysctl.reset();
    EXPECT_EQ(sysctl.request(), SystemController::Request::NONE);
    EXPECT_EQ(sysctl.exit_code(), 0u);
    sysctl.write_byte(kSysctlBase + SystemController::kRegReset, 1);
    EXPECT_EQ(sysctl.request(), SystemController::Request::RESET);

    const ROM no_rom(0, 40);
    SystemController without_rom(ram, no_rom);
    EXPECT_EQ(without_rom.read_word(kSysctlBase + SystemController::kRegRomStart), 0u);
    EXPECT_EQ(without_rom.read_word(kSysctlBase + SystemController::kRegRomSize), 0u);
}

// The store to POWEROFF completes, and the run ends before the next instruction. The machine stays
// off until it is reset.
TEST_F(Devices, a_program_turns_the_machine_off_with_an_exit_code)
{
    std::vector<word> program;
    append(program, constant(10, kSysctlBase));
    program.insert(program.end(), {mov(1, 42), store_at(1, 10, SystemController::kRegPowerOff),
                                   mov(3, 1), E::asm_hlt()});
    auto result = run(program);
    ASSERT_EQ(result.status, Status::POWER_OFF) << result.message;
    EXPECT_EQ(result.message, "Power off with exit code 42");
    EXPECT_EQ(result.instructions_ran, 5u) << "the store is the last instruction";
    EXPECT_EQ(cpu.get_pc(), kProgram + 5 * 4);
    EXPECT_EQ(cpu.read_reg(3), 0u);
    EXPECT_EQ(cpu.system_bus->sysctl.exit_code(), 42u);

    result = cpu.run(100);
    EXPECT_EQ(result.status, Status::POWER_OFF) << "it is still off";
    EXPECT_EQ(result.instructions_ran, 0u);

    cpu.reset();
    write(kProgram, {mov(3, 1), E::asm_hlt()});
    cpu.set_pc(kProgram);
    EXPECT_EQ(cpu.run(100).status, Status::HALTED) << "a reset turns it on again";
    EXPECT_EQ(cpu.read_reg(3), 1u);
}

// A reset goes on with the program in the ROM at the boot address, with the RAM, the registers
// and the devices as they are at power on, within the same run.
TEST_F(Devices, a_reset_by_the_program_starts_again_at_the_boot_address)
{
    const word boot = 16 << kNumPageOffsetBits;
    std::vector<word> program{mov(3, 5)};
    append(program, constant(10, kSysctlBase));
    program.insert(program.end(),
                   {store_at(1, 10, SystemController::kRegReset), mov(6, 9), E::asm_hlt()});
    for (size_t i = 0; i < program.size(); i++)
    {
        cpu.system_bus->rom->write_word(boot + word(i) * 4, program[i]);
    }
    cpu.set_boot_pc(boot);
    cpu.set_pc(boot);
    cpu.write_reg(20, 99);
    cpu.memory.write_word(0x2000, 0x1234);
    cpu.memory.write_word(kIntcBase + InterruptController::kRegEnable, 0xFF);

    // The reset after 5 instructions, then the mov and the first of the constant again.
    const auto result = cpu.run(7);
    ASSERT_EQ(result.status, Status::LIMIT_REACHED) << result.message;
    EXPECT_EQ(result.instructions_ran, 7u);
    EXPECT_EQ(cpu.get_pc(), boot + 2 * 4);
    EXPECT_EQ(cpu.read_reg(3), 5u);
    EXPECT_EQ(cpu.read_reg(6), 0u) << "the instruction after the store never ran";
    EXPECT_EQ(cpu.read_reg(20), 0u);
    EXPECT_EQ(cpu.memory.read_word(0x2000), 0u);
    EXPECT_EQ(intc_read(InterruptController::kRegEnable), 0u);
    EXPECT_EQ(cpu.system_bus->sysctl.request(), SystemController::Request::NONE);

    cpu.set_boot_pc(0);
    cpu.reset();
    EXPECT_EQ(cpu.get_pc(), 0u);
}
