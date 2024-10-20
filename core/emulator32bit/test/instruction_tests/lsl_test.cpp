#include <emulator32bit_test/emulator32bit_test.h>

TEST(lsl, imm5_shift) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o1(Emulator32bit::_op_lsl, 0, 1, true, 2, 5));
	cpu->_pc = 0;
	cpu->_x[1] = 1;
	cpu->set_NZCV(1, 0, 0, 1);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 32) << "\'lsl x0, x1, 5\' : where x1=1, should result in x0=32";
	EXPECT_EQ(cpu->_x[1], 1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 1) << "operation should not alter N flag";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not alter Z flag";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not alter C flag";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 1) << "operation should not alter V flag";
	delete cpu;
}

TEST(lsl, reg_shift) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o1(Emulator32bit::_op_lsl, 0, 1, false, 2, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 1;
	cpu->_x[2] = 5;
	cpu->set_NZCV(0, 0, 1, 1);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 32) << "\'lsl x0, x1, x2\' : where x1=1, x2=5, should result in x0=32";
	EXPECT_EQ(cpu->_x[1], 1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 5) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not alter N flag";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not alter Z flag";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "operation should not alter C flag";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 1) << "operation should not alter V flag";
	delete cpu;
}

TEST(lsl, reg_shift_low_byte) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o1(Emulator32bit::_op_lsl, 0, 1, false, 2, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 1;
	cpu->_x[2] = 5 + ~(0xFF);
	cpu->set_NZCV(0, 1, 0, 1);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 32) << "\'lsl x0, x1, x2\' : where x1=1, x2=5 (lower 8 bits), should result in x0=32";
	EXPECT_EQ(cpu->_x[1], 1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 5 + ~(0xFF)) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not alter N flag";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 1) << "operation should not alter Z flag";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not alter C flag";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 1) << "operation should not alter V flag";
	delete cpu;
}
