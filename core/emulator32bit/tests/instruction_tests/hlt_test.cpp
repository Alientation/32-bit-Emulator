#include <emulator32bit_test/emulator32bit_test.h>

TEST_F (EmulatorFixture, hlt_test_execution_halting)
{
    cpu = new (cpu) Emulator32bit (1, 0, {}, 0, 1);
    cpu->system_bus->write_word (0, Emulator32bit::asm_hlt ());
    cpu->set_pc (0);

    cpu->run (1);

    EXPECT_EQ (cpu->get_pc (), 0);
}