#include <emulator32bit_test/Emulator32bitTest.h>

TEST(hlt, test_execution_halting) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	cpu->system_bus.write_word(0, Emulator32bit::asm_hlt());
	cpu->_pc = 0;

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(exception.type, Emulator32bit::EmulatorException::Type::HALT);
	EXPECT_EQ(cpu->_pc, 4);
	delete cpu;
}