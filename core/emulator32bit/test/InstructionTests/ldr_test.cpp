#include <emulator32bit_test/Emulator32bitTest.h>

TEST(ldr, offset) {
	Emulator32bit *cpu = new Emulator32bit(4, 0,(const byte[]) {9U, 0U, 0U, 0U}, 4, 4);
	// ldr x0, [x1, #3]
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 3, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 1;
	cpu->set_NZCV(0, 0, 0, 0);

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 9) << "\'ldr x0, [x1, #3]\', where x1=1 : should result in x0=9";
	EXPECT_EQ(cpu->_x[1], 1) << "operation should not change operand \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(ldr, pre_indexed) {
	Emulator32bit *cpu = new Emulator32bit(4, 0,(const byte[]) {9U, 0U, 0U, 0U}, 4, 4);
	// ldr x0, [x1, #3]!
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 3, 1));
	cpu->_pc = 0;
	cpu->_x[1] = 1;
	cpu->set_NZCV(0, 0, 0, 0);

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 9) << "\'ldr x0, [x1, #3]\', where x1=1 : should result in x0=9";
	EXPECT_EQ(cpu->_x[1], 4) << "operation should preincrement \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(ldr, post_indexed) {
	Emulator32bit *cpu = new Emulator32bit(4, 0,(const byte[]) {9U, 0U, 0U, 0U}, 4, 4);
	// ldr x0, [x1, #3]!
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 3, 2));
	cpu->_pc = 0;
	cpu->_x[1] = 4;
	cpu->set_NZCV(0, 0, 0, 0);

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 9) << "\'ldr x0, [x1, #3]\', where x1=4 : should result in x0=9";
	EXPECT_EQ(cpu->_x[1], 7) << "operation should preincrement \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}