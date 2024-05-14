#include <emulator32bit_test/Emulator32bitTest.h>

TEST(add, register_add_immediate) {
	Emulator32bit cpu = Emulator32bit();
	// add x0, x1, #10
	// x1: 1
	cpu.system_bus.writeWord(0, Emulator32bit::asm_add(false, 0, 1, 10));
	cpu._pc = 0;
	cpu._x[1] = 1;

	Emulator32bit::EmulatorException exception;
	cpu.run(1, exception);

	EXPECT_EQ(cpu._x[0], 11) << "\'add x0, x1 #10\' : where x1=1, should result in x0=11";
	EXPECT_EQ(cpu._x[1], 1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
}

TEST(add, register_add_register) {
	Emulator32bit cpu = Emulator32bit();
	// add x0, x1, x2, lsl #3
	// x1: 1
	// x2: 2
	cpu.system_bus.writeWord(0, Emulator32bit::asm_add(false, 0, 1, 2, 0, 3));
	cpu._pc = 0;
	cpu._x[1] = 1;
	cpu._x[2] = 2;

	Emulator32bit::EmulatorException exception;
	cpu.run(1, exception);

	EXPECT_EQ(cpu._x[0], 17) << "\'add x0, x1, x2, lsl #3\' : where x1=1, x2=2, should result in x0=17";
	EXPECT_EQ(cpu._x[1], 1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu._x[2], 2) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
}

TEST(add, negative_flag) {

}

TEST(add, zero_flag) {

}

TEST(add, carry_flag) {

}

TEST(add, overflow_flag) {

}