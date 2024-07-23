#include <emulator32bit_test/Emulator32bitTest.h>

TEST(bic, register_and_register) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// bic x0, x1, x2
	// x1: 0b0011
	// x2: ~0b1010
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_bic, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 0b0011;
	cpu->_x[2] = ~0b1010;
	cpu->set_NZCV(1, 1, 1, 1);

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 0b0010) << "\'bic x0, x1, 2\' : where x1=0b0011, x2=~0b1010, should result in x0=0b0010";
	EXPECT_EQ(cpu->_x[1], 0b0011) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], ~0b1010) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "operation should not alter C flag";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 1) << "operation should not alter V flag";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(bic, negative_flag) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// bic x0, x1, x2
	// x1: ~0
	// x2: ~(1<<31)
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_bic, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = ~0;
	cpu->_x[2] = ~(1U<<31);
	cpu->set_NZCV(0, 1, 1, 1);

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 1U<<31) << "\'bic x0, x1, 2\' : where x1=~0, x2=~(1<<31), should result in x0=1<<31";
	EXPECT_EQ(cpu->_x[1], ~0) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], ~(1U<<31)) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "operation should not alter C flag";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 1) << "operation should not alter V flag";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(bic, zero_flag) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// bic x0, x1, x2
	// x1: ~0
	// x2: ~0
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_bic, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = ~0;
	cpu->_x[2] = ~0;
	cpu->set_NZCV(0, 0, 1, 1);

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 0) << "\'bic x0, x1, 2\' : where x1=~0, x2=~0, should result in x0=0";
	EXPECT_EQ(cpu->_x[1], ~0) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], ~0) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 1) << "Z flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "operation should not alter C flag";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 1) << "operation should not alter V flag";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}