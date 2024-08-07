#include <emulator32bit_test/Emulator32bitTest.h>

TEST(sub, register_sub_immediate) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// sub x0, x1, #10
	// x1: 11
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sub, false, 0, 1, 10));
	cpu->_pc = 0;
	cpu->_x[1] = 11;

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 1) << "\'sub x0, x1 #10\' : where x1=11, should result in x0=1";
	EXPECT_EQ(cpu->_x[1], 11) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(sub, register_sub_register) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// sub x0, x1, x2
	// x1: 11
	// x2: 10
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sub, false, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 11;
	cpu->_x[2] = 10;

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 1) << "\'sub x0, x1, x2\' : where x1=11, x2=10, should result in x0=1";
	EXPECT_EQ(cpu->_x[1], 11) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 10) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(sub, negative_flag) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// sub x0, x1, x2
	// x1: 1
	// x2: 2
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sub, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 1;
	cpu->_x[2] = 2;

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], -1) << "\'sub x0, x1, x2\' : where x1=1, x2=2, should result in x0=-1";
	EXPECT_EQ(cpu->_x[1], 1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 2) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(sub, zero_flag) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// sub x0, x1, x2
	// x1: 1
	// x2: 1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sub, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 1;
	cpu->_x[2] = 1;

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 0) << "\'sub x0, x1, x2\' : where x1=1, x2=1, should result in x0=0";
	EXPECT_EQ(cpu->_x[1], 1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 1) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 1) << "Z flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "C flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(sub, carry_flag_1) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// sub x0, x1, x2
	// x1: -3
	// x2: -2
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sub, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = -3;
	cpu->_x[2] = -2;

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], -1) << "\'sub x0, x1, x2\' : where x1=-3, x2=-2, should result in x0=-1";
	EXPECT_EQ(cpu->_x[1], -3) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], -2) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(sub, carry_flag_2) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// sub x0, x1, x2
	// x1: 1
	// x2: -2
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sub, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 1;
	cpu->_x[2] = -2;

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 3) << "\'sub x0, x1, x2\' : where x1=1, x2=-2, should result in x0=3";
	EXPECT_EQ(cpu->_x[1], 1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], -2) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(sub, overflow_flag__positive_to_negative) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// sub x0, x1, x2
	// x1: (1<<31)-1
	// x2: -1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sub, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = (1U<<31) - 1;
	cpu->_x[2] = -1;

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 1U<<31) << "\'sub x0, x1, x2\' : where x1=(1<<31)-1, x2=-1, should result in x0=1<<31";
	EXPECT_EQ(cpu->_x[1], (1U<<31) - 1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], -1) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 1) << "V flag should be set";
	delete cpu;
}

TEST(sub, overflow_flag__negative_to_positive) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// sub x0, x1, x2
	// x1: 1U<<31
	// x2: 1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sub, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 1U<<31;
	cpu->_x[2] = 1;

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], (1U<<31) - 1) << "\'sub x0, x1, x2\' : where x1=1<<31, x2=1, should result in x0=(1<<31)-1";
	EXPECT_EQ(cpu->_x[1], 1U<<31) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 1) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 1) << "V flag should be set";
	delete cpu;
}