#include <emulator32bit_test/Emulator32bitTest.h>

TEST(mul, register_mul_immediate) {
	Emulator32bit *cpu = new Emulator32bit(4, 0, {}, 0, 4);
	// mul x0, x1, #9
	// x1: 2
	cpu->system_bus.writeWord(0, Emulator32bit::asm_format_o(Emulator32bit::_op_mul, false, 0, 1, 9));
	cpu->_pc = 0;
	cpu->_x[1] = 2;

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 18) << "\'mul x0, x1 #9\' : where x1=2, should result in x0=18";
	EXPECT_EQ(cpu->_x[1], 2) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(mul, register_mul_register) {
	Emulator32bit *cpu = new Emulator32bit(4, 0, {}, 0, 4);
	// mul x0, x1, x2
	// x1: 2
	// x2: 4
	cpu->system_bus.writeWord(0, Emulator32bit::asm_format_o(Emulator32bit::_op_mul, false, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 2;
	cpu->_x[2] = 4;

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 8) << "\'mul x0, x1, x2\' : where x1=2, x2=4, should result in x0=8";
	EXPECT_EQ(cpu->_x[1], 2) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 4) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(mul, register_mul_register_shift) {
	Emulator32bit *cpu = new Emulator32bit(4, 0, {}, 0, 4);
	// mul x0, x1, x2, lsr #1
	// x1: 2
	// x2: 4
	cpu->system_bus.writeWord(0, Emulator32bit::asm_format_o(Emulator32bit::_op_mul, false, 0, 1, 2, LSR, 1));
	cpu->_pc = 0;
	cpu->_x[1] = 2;
	cpu->_x[2] = 4;

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 4) << "\'mul x0, x1, x2, lsr #1\' : where x1=2, x2=4, should result in x0=4";
	EXPECT_EQ(cpu->_x[1], 2) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 4) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(mul, negative_flag) {
	Emulator32bit *cpu = new Emulator32bit(4, 0, {}, 0, 4);
	// mul x0, x1, x2
	// x1: -2
	// x2: 4
	cpu->system_bus.writeWord(0, Emulator32bit::asm_format_o(Emulator32bit::_op_mul, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = -2;
	cpu->_x[2] = 4;

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], -8) << "\'mul x0, x1, x2\' : where x1=-2, x2=4, should result in x0=-8";
	EXPECT_EQ(cpu->_x[1], -2) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 4) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}
 
TEST(mul, zero_flag) {
	Emulator32bit *cpu = new Emulator32bit(4, 0, {}, 0, 4);
	// mul x0, x1, x2
	// x1: 0
	// x2: 4
	cpu->system_bus.writeWord(0, Emulator32bit::asm_format_o(Emulator32bit::_op_mul, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 0;
	cpu->_x[2] = 4;

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 0) << "\'mul x0, x1, x2\' : where x1=0, x2=4, should result in x0=0";
	EXPECT_EQ(cpu->_x[1], 0) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 4) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 1) << "Z flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}