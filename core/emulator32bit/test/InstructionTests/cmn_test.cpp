#include <emulator32bit_test/Emulator32bitTest.h>

// TEST(cmn, register_cmn_immediate) {
// 	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
// 	// cmn x0, x1, #10
// 	// x1: 1
// 	cpu->system_bus.writeWord(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, false, 0, 1, 10));
// 	cpu->_pc = 0;
// 	cpu->_x[1] = 1;

//
// 	cpu->run(1, exception);

// 	EXPECT_EQ(cpu->_x[1], 1) << "operation should not alter operand register \'x1\'";
// 	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
// 	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
// 	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
// 	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
//
// 	delete cpu;
// }

// TEST(cmn, register_cmn_register) {
// 	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
// 	// cmn x0, x1, x2
// 	// x1: 1
// 	// x2: 10
// 	cpu->system_bus.writeWord(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, false, 0, 1, 2, 0, 0));
// 	cpu->_pc = 0;
// 	cpu->_x[1] = 1;
// 	cpu->_x[2] = 10;

//
// 	cpu->run(1, exception);

// 	EXPECT_EQ(cpu->_x[1], 1) << "operation should not alter operand register \'x1\'";
// 	EXPECT_EQ(cpu->_x[2], 10) << "operation should not alter operand register \'x2\'";
// 	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
// 	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
// 	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
// 	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
//
// 	delete cpu;
// }

// TEST(cmn, register_cmn_register_shifted) {
// 	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
// 	// cmn x0, x1, x2, lsl #3
// 	// x1: 1
// 	// x2: 2
// 	cpu->system_bus.writeWord(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, false, 0, 1, 2, LSL, 3));
// 	cpu->_pc = 0;
// 	cpu->_x[1] = 1;
// 	cpu->_x[2] = 2;

//
// 	cpu->run(1, exception);

// 	EXPECT_EQ(cpu->_x[1], 1) << "operation should not alter operand register \'x1\'";
// 	EXPECT_EQ(cpu->_x[2], 2) << "operation should not alter operand register \'x2\'";
// 	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
// 	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
// 	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
// 	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
//
// 	delete cpu;
// }

TEST(cmn, negative_flag) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// cmn x0, x1, x2
	// x1: -2
	// x2: 1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, false, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = -2;
	cpu->_x[2] = 1;

	cpu->run(1);

	EXPECT_EQ(cpu->_x[1], -2) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 1) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";

	delete cpu;
}

TEST(cmn, zero_flag) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// cmn x0, x1, x2
	// x1: 0
	// x2: 0
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, false, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 0;
	cpu->_x[2] = 0;

	cpu->run(1);

	EXPECT_EQ(cpu->_x[1], 0) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 0) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 1) << "Z flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(cmn, carry_flag_1) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// cmn x0, x1, x2
	// x1: 0
	// x2: 0
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = -1;
	cpu->_x[2] = -1;

	cpu->run(1);

	EXPECT_EQ(cpu->_x[1], -1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], -1) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(cmn, carry_flag_2) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// cmn x0, x1, x2
	// x1: 0
	// x2: 0
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = -4;
	cpu->_x[2] = -4;

	cpu->run(1);

	EXPECT_EQ(cpu->_x[1], -4) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], -4) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(cmn, overflow_flag__neg_to_pos) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// cmn x0, x1, x2
	// x1: 1<<31
	// x2: 1<<31
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = 1U << 31;
	cpu->_x[2] = 1U << 31;

	cpu->run(1);

	EXPECT_EQ(cpu->_x[1], 1U<<31) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 1U<<31) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 1) << "Z flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 1) << "V flag should be set";
	delete cpu;
}

TEST(cmn, overflow_flag__pos_to_neg) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// cmn x0, x1, x2
	// x1: (1<<31) - 1
	// x2: 1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, true, 0, 1, 2, 0, 0));
	cpu->_pc = 0;
	cpu->_x[1] = (1U << 31) - 1;
	cpu->_x[2] = 1;

	cpu->run(1);

	EXPECT_EQ(cpu->_x[1], (1U<<31) - 1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->_x[2], 1) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 1) << "V flag should be set";
	delete cpu;
}