#include "emulator32bit_test/emulator32bit_test.h"

TEST_F(EmulatorFixture, hlt_test_execution_halting)
{
    cpu.memory.write_word(0, Emulator32bit::asm_hlt());
    cpu.set_pc(0);

    cpu.run(1);

    EXPECT_EQ(cpu.get_pc(), 0);
}