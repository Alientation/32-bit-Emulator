#include <emulator32bit_test/emulator32bit_test.h>

#include <iostream>

static const byte data[PAGE_SIZE] = {9U, 0U, 0U, 0U};

TEST(ldr, offset_positive_constant) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, data, 1, 1);
	// ldr x0, [x1, #3]
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 3, Emulator32bit::ADDR_OFFSET));
	cpu->_pc = 0;
	cpu->_x[1] = PAGE_SIZE - 3;
	cpu->set_NZCV(0, 0, 0, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 9) << "\'ldr x0, [x1, #3]\', where x1=PAGE_SIZE-3 : should result in x0=9";
	EXPECT_EQ(cpu->_x[1], PAGE_SIZE - 3) << "operation should not change operand \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(ldr, offset_negative_constant) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, data, 1, 1);
	// ldr x0, [x1, #-3]
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, -3, Emulator32bit::ADDR_OFFSET));
	cpu->_pc = 0;
	cpu->_x[1] = PAGE_SIZE + 3;
	cpu->set_NZCV(0, 0, 0, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 9) << "\'ldr x0, [x1, #-3]\', where x1=PAGE_SIZE+3 : should result in x0=9";
	EXPECT_EQ(cpu->_x[1], PAGE_SIZE + 3) << "operation should not change operand \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(ldr, offset_reg) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, data, 1, 1);
	// ldr x0, [x1, #3]
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 3, Emulator32bit::ADDR_OFFSET));
	cpu->_pc = 0;
	cpu->_x[1] = PAGE_SIZE - 3;
	cpu->set_NZCV(0, 0, 0, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 9) << "\'ldr x0, [x1, #3]\', where x1=PAGE_SIZE-3 : should result in x0=9";
	EXPECT_EQ(cpu->_x[1], PAGE_SIZE - 3) << "operation should not change operand \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(ldr, pre_indexed) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, data, 1, 1);
	// ldr x0, [x1, #3]!
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 3, Emulator32bit::ADDR_PRE_INC));
	cpu->_pc = 0;
	cpu->_x[1] = PAGE_SIZE - 3;
	cpu->set_NZCV(0, 0, 0, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 9) << "\'ldr x0, [x1, #3]!\', where x1=PAGESIZE - 3 : should result in x0=9";
	EXPECT_EQ(cpu->_x[1], PAGE_SIZE) << "operation should preincrement \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(ldr, post_indexed) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, data, 1, 1);
	// ldr x0, [x1, #3]!
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 3, Emulator32bit::ADDR_POST_INC));
	cpu->_pc = 0;
	cpu->_x[1] = PAGE_SIZE;
	cpu->set_NZCV(0, 0, 0, 0);

	cpu->run(1);

	EXPECT_EQ(cpu->_x[0], 9) << "\'ldr x0, [x1], #3\', where x1=PAGE_SIZE : should result in x0=9";
	EXPECT_EQ(cpu->_x[1], 3 + PAGE_SIZE) << "operation should postincrement \'x1\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}