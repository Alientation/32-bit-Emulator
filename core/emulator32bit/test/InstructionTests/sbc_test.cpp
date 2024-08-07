#include <emulator32bit_test/Emulator32bitTest.h>

TEST(sbc, register_sbc_immediate) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// sbc x0, x1, #9
	// x1: 11
	// carry: 1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sbc, false, 0, 1, 9));
	cpu->_pc = 0;
	cpu->_x[1] = 11;
	cpu->set_NZCV(0, 0, 1, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 1) << "\'sbc x0, x1 #9\' : where x1=11, c=1, should result in x0=1";
	EXPECT_EQ(cpu->_x[1], 11) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "operation should not alter C flag";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(sbc, register_sbc_register) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// sbc x0, x1, x2
	// x1: 11
	// x2: 9
	// carry: 1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sbc, false, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 11;
	cpu->_x[2] = 9;
	cpu->set_NZCV(0, 0, 1, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 1) << "\'sbc x0, x1, x2\' : where x1=11, x2=9, c=1, should result in x0=1";
	EXPECT_EQ(cpu->_x[1], 11) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 9) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "operation should not alter C flag";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(sbc, negative_flag) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// sbc x0, x1, x2
	// x1: 2
	// x2: 2
	// carry: 1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sbc, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 2;
	cpu->_x[2] = 2;
	cpu->set_NZCV(0, 0, 1, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], -1) << "\'sbc x0, x1, x2\' : where x1=2, x2=2, c=1, should result in x0=-1";
	EXPECT_EQ(cpu->_x[1], 2) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 2) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(sbc, zero_flag) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// sbc x0, x1, x2
	// x1: 2
	// x2: 1
	// carry: 1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sbc, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 2;
	cpu->_x[2] = 1;
	cpu->set_NZCV(0, 0, 1, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 0) << "\'sbc x0, x1, x2\' : where x1=2, x2=1, c=1, should result in x0=0";
	EXPECT_EQ(cpu->_x[1], 2) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 1) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 1) << "Z flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "C flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(sbc, carry_flag_1) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// sbc x0, x1, x2
	// x1: -2
	// x2: -2
	// carry: 1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sbc, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = -2;
	cpu->_x[2] = -2;
	cpu->set_NZCV(0, 0, 1, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], -1) << "\'sbc x0, x1, x2\' : where x1=-2, x2=-2, c=1, should result in x0=-1";
	EXPECT_EQ(cpu->_x[1], -2) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], -2) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(sbc, carry_flag_2) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// sbc x0, x1, x2
	// x1: 2
	// x2: -2
	// carry: 1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sbc, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 2;
	cpu->_x[2] = -2;
	cpu->set_NZCV(0, 0, 1, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 3) << "\'sbc x0, x1, x2\' : where x1=2, x2=-2, c=1, should result in x0=3";
	EXPECT_EQ(cpu->_x[1], 2) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], -2) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(sbc, overflow_flag__positive_to_negative) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// sbc x0, x1, x2
	// x1: (1<<31)-1
	// x2: -2
	// carry: 1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sbc, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = (1U<<31) - 1;
	cpu->_x[2] = -2;
	cpu->set_NZCV(0, 0, 1, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 1U<<31) << "\'sbc x0, x1, x2\' : where x1=(1<<31)-1, x2=-2, c=1, should result in x0=1<<31";
	EXPECT_EQ(cpu->_x[1], (1U<<31) - 1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], -2) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 1) << "V flag should be set";
	delete cpu;
}

TEST(sbc, overflow_flag__negative_to_positive) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// sbc x0, x1, x2
	// x1: 1U<<31
	// x2: 0
	// carry: 1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sbc, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 1U<<31;
	cpu->_x[2] = 0;
	cpu->set_NZCV(0, 0, 1, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], (1U<<31) - 1) << "\'sbc x0, x1, x2\' : where x1=1<<31, x2=0, c=1, should result in x0=(1<<31)-1";
	EXPECT_EQ(cpu->_x[1], 1U<<31) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 0) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 1) << "V flag should be set";
	delete cpu;
}