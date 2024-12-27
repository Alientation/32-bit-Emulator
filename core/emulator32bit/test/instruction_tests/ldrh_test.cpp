#include <emulator32bit_test/emulator32bit_test.h>

#include <iostream>

static const byte data[PAGE_SIZE] = {9U, 0U, 2U, 3U};

TEST(ldrh, offset) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, data, 1, 1);
	// ldrh x0, [x1, #3]
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldrh, false, 0, 1, 3, 0));
	cpu->_pc = 0;
	cpu->_x[1] = PAGE_SIZE - 3;
	cpu->set_NZCV(0, 0, 0, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 9) << "\'ldrh x0, [x1, #3]\', where x1=PAGESIZE - 3 : should result in x0=9";
	EXPECT_EQ(cpu->_x[1], PAGE_SIZE - 3) << "operation should not change operand \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(ldrh, pre_indexed) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, data, 1, 1);
	// ldrh x0, [x1, #3]!
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldrh, false, 0, 1, 3, 1));
	cpu->_pc = 0;
	cpu->_x[1] = PAGE_SIZE - 3;
	cpu->set_NZCV(0, 0, 0, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 9) << "\'ldrh x0, [x1, #3]\', where x1=PAGESIZE - 3 : should result in x0=9";
	EXPECT_EQ(cpu->_x[1], PAGE_SIZE) << "operation should preincrement \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(ldrh, post_indexed) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, data, 1, 1);
	// ldrh x0, [x1, #3]!
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldrh, false, 0, 1, 3, 2));
	cpu->_pc = 0;
	cpu->_x[1] = PAGE_SIZE;
	cpu->set_NZCV(0, 0, 0, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 9) << "\'ldrh x0, [x1, #3]\', where x1=PAGESIZE : should result in x0=9";
	EXPECT_EQ(cpu->_x[1], 3 + PAGE_SIZE) << "operation should postincrement \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}