#include <emulator32bit_test/Emulator32bitTest.h>

TEST(rsb, register_rsb_immediate) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// rsb x0, x1, #11
	// x1: 10
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_rsb, false, 0, 1, 11));
	cpu->_pc = 0;
	cpu->_x[1] = 10;

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 1) << "\'rsb x0, x1 #11\' : where x1=10, should result in x0=1";
	EXPECT_EQ(cpu->_x[1], 10) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(rsb, register_rsb_register) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// rsb x0, x1, x2
	// x1: 10
	// x2: 11
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_rsb, false, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 10;
	cpu->_x[2] = 11;

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 1) << "\'rsb x0, x1, x2\' : where x1=10, x2=11, should result in x0=1";
	EXPECT_EQ(cpu->_x[1], 10) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 11) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(rsb, negative_flag) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// rsb x0, x1, x2
	// x1: 2
	// x2: 1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_rsb, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 2;
	cpu->_x[2] = 1;

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], -1) << "\'rsb x0, x1, x2\' : where x1=2, x2=1, should result in x0=-1";
	EXPECT_EQ(cpu->_x[1], 2) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 1) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(rsb, zero_flag) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// rsb x0, x1, x2
	// x1: 1
	// x2: 1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_rsb, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 1;
	cpu->_x[2] = 1;

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 0) << "\'rsb x0, x1, x2\' : where x1=1, x2=1, should result in x0=0";
	EXPECT_EQ(cpu->_x[1], 1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 1) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 1) << "Z flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "C flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(rsb, carry_flag_1) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// rsb x0, x1, x2
	// x1: -2
	// x2: -3
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_rsb, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = -2;
	cpu->_x[2] = -3;

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], -1) << "\'rsb x0, x1, x2\' : where x1=-2, x2=-3, should result in x0=-1";
	EXPECT_EQ(cpu->_x[1], -2) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], -3) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(rsb, carry_flag_2) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// rsb x0, x1, x2
	// x1: -2
	// x2: 1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_rsb, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = -2;
	cpu->_x[2] = 1;

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 3) << "\'rsb x0, x1, x2\' : where x1=-2, x2=1, should result in x0=3";
	EXPECT_EQ(cpu->_x[1], -2) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 1) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(rsb, overflow_flag__positive_to_negative) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// rsb x0, x1, x2
	// x1: -1
	// x2: (1<<31)-1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_rsb, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = -1;
	cpu->_x[2] = (1U<<31) - 1;

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 1U<<31) << "\'rsb x0, x1, x2\' : where x1=-1, x2=(1<<31)-1, should result in x0=1<<31";
	EXPECT_EQ(cpu->_x[1], -1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], (1U<<31) - 1) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 1) << "V flag should be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(rsb, overflow_flag__negative_to_positive) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// rsb x0, x1, x2
	// x1: 1
	// x2: 1<<31
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_rsb, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 1;
	cpu->_x[2] = 1U<<31;

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], (1U<<31) - 1) << "\'rsb x0, x1, x2\' : where x1=1, x2=1<<31, should result in x0=(1<<31)-1";
	EXPECT_EQ(cpu->_x[1], 1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 1U<<31) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 1) << "V flag should be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}