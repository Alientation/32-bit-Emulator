#include <emulator32bit_test/emulator32bit_test.h>

TEST(hlt, test_execution_halting) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	cpu->system_bus.write_word(0, Emulator32bit::asm_hlt());
	cpu->set_pc(0);

	cpu->run(1);

	EXPECT_EQ(cpu->get_pc(), 0);
	delete cpu;
}