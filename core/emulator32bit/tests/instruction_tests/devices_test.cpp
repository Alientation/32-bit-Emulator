// The interrupt controller, timer and console (docs/devices.md), and what the CPU does with them:
// taking an interrupt between instructions, WFI, and running code from the ROM.

#include "emulator32bit_test/emulator32bit_test.h"

#include <emulator32bit/devices.h>

#include <filesystem>
#include <sstream>

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
    return E::asm_format_o3(E::_op_mov, false, xd, value);
}

word store_at(const U8 xt, const U8 xn, const int offset)
{
    return E::asm_format_m(E::_op_str, false, xt, xn, offset, E::AddrType::ADDR_OFFSET);
}

word load_at(const U8 xt, const U8 xn, const int offset)
{
    return E::asm_format_m(E::_op_ldr, false, xt, xn, offset, E::AddrType::ADDR_OFFSET);
}

word nop()
{
    return E::asm_nop();
}

/// The instructions that put a 32 bit value in a register: mov, lsl, orr.
std::vector<word> constant(const U8 xd, const word value)
{
    return {mov(xd, value >> 14), E::asm_format_o1(E::_op_lsl, xd, xd, true, 0, 14),
            E::asm_format_o(E::_op_orr, false, xd, xd, int(value & 0x3FFF))};
}

void append(std::vector<word> &code, const std::vector<word> &more)
{
    code.insert(code.end(), more.begin(), more.end());
}

class Devices : public ::testing::Test
{
  protected:
    Emulator32bit cpu{new RAM(16, 0), new ROM(16, 16), new MockDisk()};
    std::ostringstream m_console;

    void SetUp() override
    {
        cpu.system_bus->console.set_output(&m_console);
    }

    void write(const word address, const std::vector<word> &code)
    {
        for (size_t i = 0; i < code.size(); i++)
        {
            cpu.system_bus->write_word(address + word(i) * 4, code[i]);
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
        cpu.write_sysreg(E::kSysregId_vbar, kVectors);
    }

    Emulator32bit::RunResult run(const std::vector<word> &program, const U64 limit = 1000)
    {
        write(kProgram, program);
        cpu.set_pc(kProgram);
        return cpu.run(limit);
    }

    word intc_read(const word offset)
    {
        return cpu.system_bus->read_word(kIntcBase + offset);
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
    cpu.system_bus->write_byte(kConsoleBase, 'h');
    cpu.system_bus->write_word(kConsoleBase, 'i');
    cpu.system_bus->write_hword(kConsoleBase, '!');
    EXPECT_EQ(m_console.str(), "hi!");
}

TEST_F(Devices, received_bytes_are_read_one_by_one_and_the_status_says_when_there_are_some)
{
    EXPECT_EQ(cpu.system_bus->read_word(kConsoleBase + 4), 2u) << "ready to send, nothing received";
    cpu.system_bus->console.push_input("ab");
    EXPECT_EQ(cpu.system_bus->read_word(kConsoleBase + 4), 3u);
    EXPECT_EQ(cpu.system_bus->read_byte(kConsoleBase), 'a');
    EXPECT_EQ(cpu.system_bus->read_word(kConsoleBase), word('b'));
    EXPECT_EQ(cpu.system_bus->read_word(kConsoleBase + 4), 2u);
    EXPECT_EQ(cpu.system_bus->read_word(kConsoleBase), 0u) << "nothing left";
}

TEST_F(Devices, input_raises_line_1_when_the_receive_interrupt_is_enabled)
{
    cpu.system_bus->write_word(kIntcBase + 8, 1u << kIrqLineConsole);
    cpu.system_bus->console.push_input("x");
    EXPECT_FALSE(cpu.system_bus->intc.has_pending()) << "not enabled yet";

    cpu.system_bus->write_word(kConsoleBase + 8, 1); // enabling it with a byte waiting
    EXPECT_EQ(intc_read(0x10), 1u << kIrqLineConsole);
    EXPECT_EQ(intc_read(0x00), kIrqLineConsole);

    cpu.system_bus->console.push_input("y");
    EXPECT_TRUE(cpu.system_bus->intc.has_pending());
}

TEST_F(Devices, a_reset_clears_the_devices)
{
    cpu.system_bus->write_word(kIntcBase + 8, 0xFF);
    cpu.system_bus->console.push_input("x");
    cpu.reset();
    EXPECT_EQ(intc_read(0x08), 0u);
    EXPECT_FALSE(cpu.system_bus->console.input_waiting());
}

// --- the CPU takes interrupts --------------------------------------------------------------

// The timer fires while a run of nops executes, the handler claims the line and returns.
TEST_F(Devices, an_interrupt_runs_the_handler_between_two_instructions_and_eret_goes_on)
{
    install_vectors(
        {mov(7, 1), load_at(8, 10, 0), E::asm_format_o(E::_op_add, false, 9, 9, 1), E::asm_eret()});

    std::vector<word> program;
    append(program, constant(10, kIntcBase));
    append(program, constant(11, kTimerBase));
    program.insert(program.end(), {mov(1, 1), store_at(1, 10, 0x08),           // enable line 0
                                   mov(1, 40), store_at(1, 11, 0x04),          // compare
                                   mov(1, 1), store_at(1, 11, 0x08),           // start
                                   E::asm_msr(E::kSysregId_pstate, true, 0)}); // unmask
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
    const word elr = cpu.read_sysreg(E::kSysregId_elr);
    EXPECT_GE(elr, kProgram + 4 * nops_at);
    EXPECT_LT(elr, kProgram + 4 * (nops_at + 80));
    EXPECT_EQ(cpu.read_sysreg(E::kSysregId_esr) >> 26, word(Class::IRQ));
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
                    E::asm_msr(E::kSysregId_pstate, true, 0), nop(), nop(), E::asm_hlt()});
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
                    E::asm_msr(E::kSysregId_pstate, true, 0), nop(), nop(), E::asm_hlt()});
    const auto result = run(program);
    ASSERT_EQ(result.status, Status::HALTED) << result.message;
    EXPECT_EQ(cpu.read_reg(8), 1u);
    EXPECT_EQ(cpu.read_reg(12), 0u);
}

TEST_F(Devices, the_handler_runs_with_irqs_masked_in_kernel_mode_on_its_own_stack)
{
    install_vectors({E::asm_msr(E::kSysregId_spsr, true, 0), E::asm_hlt()});
    cpu.write_reg(U8(Register::SP), 0x1000);
    cpu.write_sysreg(E::kSysregId_usp, 0x2000);

    std::vector<word> program;
    append(program, constant(10, kIntcBase));
    program.insert(program.end(),
                   {mov(1, 1), store_at(1, 10, 0x08), mov(2, 0), store_at(2, 10, 0x0C),
                    E::asm_msr(E::kSysregId_pstate, true, 0), nop(), nop(), E::asm_hlt()});
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
    write(kProgram, {nop(), nop(), E::asm_format_b1(E::_op_swi, ConditionCode::AL, 0)});
    write(kVectors + 16 * word(Class::SUPERVISOR_CALL), {E::asm_hlt()});
    cpu.write_sysreg(E::kSysregId_pstate, 1 << kUserModeBit); // user mode, IRQs unmasked
    cpu.set_pc(kProgram);
    const auto result = cpu.run(100);
    ASSERT_EQ(result.status, Status::HALTED);
    EXPECT_EQ(cpu.read_reg(7), 1u) << "the handler ran";
    EXPECT_EQ(cpu.read_sysreg(E::kSysregId_elr), kProgram + 4 * 2 + 4)
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
    EXPECT_THROW(cpu.system_bus->read_word(kDeviceBase + 0x5000), SystemBus::Exception);
}
