#include <emulator32bit_test/Emulator32bitTest.h>

TEST(lsr, imm5_shift) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o1(Emulator32bit::_op_lsr, 0, 1, true, 2, 5));
	cpu->_pc = 0;
	cpu->_x[1] = 32;
	cpu->set_NZCV(1, 0, 0, 1);

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 1) << "\'lsl x0, x1, 5\' : where x1=32, should result in x0=1";
	EXPECT_EQ(cpu->_x[1], 32) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 1) << "operation should not alter N flag";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not alter Z flag";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not alter C flag";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 1) << "operation should not alter V flag";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(lsr, reg_shift) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o1(Emulator32bit::_op_lsr, 0, 1, false, 2, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 32;
	cpu->_x[2] = 5;
	cpu->set_NZCV(0, 0, 1, 1);

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 1) << "\'lsl x0, x1, x2\' : where x1=32, x2=5, should result in x0=1";
	EXPECT_EQ(cpu->_x[1], 32) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 5) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not alter N flag";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not alter Z flag";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "operation should not alter C flag";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 1) << "operation should not alter V flag";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(lsr, reg_shift_low_byte) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o1(Emulator32bit::_op_lsr, 0, 1, false, 2, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 32;
	cpu->_x[2] = 5 + ~(0xFF);
	cpu->set_NZCV(0, 1, 0, 1);

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 1) << "\'lsl x0, x1, x2\' : where x1=32, x2=5 (lower 8 bits), should result in x0=1";
	EXPECT_EQ(cpu->_x[1], 32) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 5 + ~(0xFF)) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not alter N flag";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 1) << "operation should not alter Z flag";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not alter C flag";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 1) << "operation should not alter V flag";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}
