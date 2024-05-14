#include <emulator32bit_test/Emulator32bitTest.h>

TEST(add, register_add_immediate) {
	Emulator32bit cpu = Emulator32bit();
	cpu.system_bus.writeWord(0, Emulator32bit::asm_add(false, 0, 1, 10));
	cpu._pc = 0;
	cpu._x[1] = 1;

	Emulator32bit::EmulatorException exception;
	cpu.run(1, exception);

	EXPECT_EQ(cpu._x[0], 11) << "\'add x0, x1 10\' : where x1=1, should result in x0=11";
	EXPECT_EQ(cpu._x[1], 1) << "operation should not alter operand register \'x1\'";
}

TEST(add, register_add_register) {
	
}

TEST(add, negative_flag) {

}

TEST(add, zero_flag) {

}

TEST(add, carry_flag) {

}

TEST(add, overflow_flag) {

}