#include <emulator32bit_test/Emulator32bitTest.h>

TEST(hlt, test_execution_halting) {
	Emulator32bit cpu;
	cpu.system_bus.writeWord(0, Emulator32bit::asm_hlt());
	cpu._pc = 0;

	Emulator32bit::EmulatorException exception;
	cpu.run(1, exception);

	EXPECT_EQ(exception.type, Emulator32bit::EmulatorException::Type::HALT);
	EXPECT_EQ(cpu._pc, 4);
}