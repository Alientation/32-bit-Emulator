#include <emulator32bit_test/emulator32bit_test.h>

#include <iostream>

TEST(str, offset) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// str x0, [x1, #3]
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_str, false, 0, 1, 3, Emulator32bit::ADDR_OFFSET));
	cpu->_pc = 0;
	cpu->_x[0] = 9;
	cpu->_x[1] = 5;
	cpu->set_NZCV(0, 0, 0, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->system_bus.read_word(8), 9) << "\'str x0, [x1, #3]\', where x0=9, x1=5 : should result in 4 byte value of 9 to be written at address 8";
	EXPECT_EQ(cpu->_x[0], 9) << "operation should not change operand \'x0\'";
	EXPECT_EQ(cpu->_x[1], 5) << "operation should not change operand \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(str, pre_indexed) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// str x0, [x1, #3]!
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_str, false, 0, 1, 3, Emulator32bit::ADDR_PRE_INC));
	cpu->_pc = 0;
	cpu->_x[0] = 9;
	cpu->_x[1] = 5;
	cpu->set_NZCV(0, 0, 0, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->system_bus.read_word(8), 9) << "\'str x0, [x1, #3]!\', where x0=9, x1=5 : should result in 4 byte value of 9 to be written at address 8";
	EXPECT_EQ(cpu->_x[0], 9) << "operation should not change operand \'x0\'";
	EXPECT_EQ(cpu->_x[1], 8) << "operation should preincrement \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(str, post_indexed) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// str x0, [x1], #3
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_str, false, 0, 1, 3, Emulator32bit::ADDR_POST_INC));
	cpu->_pc = 0;
	cpu->_x[0] = 9;
	cpu->_x[1] = 8;
	cpu->set_NZCV(0, 0, 0, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->system_bus.read_word(8), 9) << "\'str x0, [x1], #3\', where x0=9, x1=8 : should result in 4 byte value of 9 to be written at address 8";
	EXPECT_EQ(cpu->_x[0], 9) << "operation should not change operand \'x0\'";
	EXPECT_EQ(cpu->_x[1], 11) << "operation should postincrement \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}