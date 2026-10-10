#include "emulator32bit_test/emulator32bit_test.h"

using Status = Emulator32bit::RunResult::Status;

TEST_F(EmulatorFixture, unused_opcode_faults_instead_of_halting)
{
    const auto result = step(0, word(0b111111) << 26);

    EXPECT_EQ(result.status, Status::FAULT);
    EXPECT_NE(result.message.find("Bad opcode"), std::string::npos) << result.message;
    EXPECT_EQ(cpu.get_pc(), 0);
}

TEST_F(EmulatorFixture, a_floating_point_function_that_is_not_assigned_faults)
{
    const auto result = step(0, Emulator32bit::asm_vop2(31, false, 0, 0, 0));

    EXPECT_EQ(result.status, Status::FAULT);
    EXPECT_NE(result.message.find("Undefined vop2 function 31"), std::string::npos)
        << result.message;
}

TEST_F(EmulatorFixture, hlt_still_halts_cleanly)
{
    const auto result = step(0, Emulator32bit::asm_hlt());

    EXPECT_EQ(result.status, Status::HALTED);
}

TEST_F(EmulatorFixture, misaligned_pc_faults)
{
    cpu.memory.write_word(0, Emulator32bit::asm_nop());
    cpu.set_pc(2);

    const auto result = cpu.run(1);

    EXPECT_EQ(result.status, Status::FAULT);
    EXPECT_NE(result.message.find("Misaligned"), std::string::npos) << result.message;
    EXPECT_EQ(result.instructions_ran, 0u);
}

TEST_F(EmulatorFixture, fetch_outside_of_ram_faults)
{
    // The fixture has one page of RAM at page 0 and no process, so addresses are physical.
    cpu.set_pc(kPageSize * 4);

    const auto result = cpu.run(1);

    EXPECT_EQ(result.status, Status::FAULT);
}

TEST_F(EmulatorFixture, atomic_with_invalid_width_faults_instead_of_exiting)
{
    // Width field 0b11 is not a valid atomic width.
    const word instr = Emulator32bit::asm_atomic(1, 2, 3, 0b11, Emulator32bit::kAtomicId_swp);
    const auto result = step(0, instr);

    EXPECT_EQ(result.status, Status::FAULT);
}
