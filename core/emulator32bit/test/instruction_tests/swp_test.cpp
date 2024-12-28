#include <emulator32bit_test/emulator32bit_test.h>

#include <iostream>

static const byte data[PAGE_SIZE] = {0x07, 0x16, 0x25, 0x34};

TEST(swp, basic) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, data, 1, 1);
	// swp x0, x1, [x2]
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_m1(Emulator32bit::_op_swp, 0, 1, 2));
	cpu->_pc = 0;
	cpu->_x[1] = 0x76543210;
	cpu->_x[2] = PAGE_SIZE;
	cpu->set_NZCV(0, 0, 0, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->system_bus.read_word(PAGE_SIZE), 0x76543210) << "\'swp x0, x1, [x2]\' : should result in [x2] stored in x0 and x1 stored in [x2].";
	EXPECT_EQ(cpu->_x[0], 0x34251607) << "\'x0\' should contain the 4 byte value at address x2 in memory.";
	EXPECT_EQ(cpu->_x[1], 0x76543210) << "operation should not change operand \'x1\'";
	EXPECT_EQ(cpu->_x[2], PAGE_SIZE) << "operation should not change operand \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}