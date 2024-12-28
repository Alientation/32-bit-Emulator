#include <emulator32bit_test/emulator32bit_test.h>

TEST(cmp, register_cmp_immediate) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// cmp x0, x1, #10
	// x1: 11
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmp, false, 0, 1, 10));
	cpu->set_pc(0);
	cpu->write_reg(1, 11);


	cpu->run(1);

	EXPECT_EQ(cpu->read_reg(1), 11) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(cpu->get_flag(C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";

	delete cpu;
}

TEST(cmp, register_cmp_register) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// cmp x0, x1, x2
	// x1: 11
	// x2: 10
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmp, false, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
	cpu->set_pc(0);
	cpu->write_reg(1, 11);
	cpu->write_reg(2, 10);


	cpu->run(1);

	EXPECT_EQ(cpu->read_reg(1), 11) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->read_reg(2), 10) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(cpu->get_flag(C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";

	delete cpu;
}

TEST(cmp, negative_flag) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// cmp x0, x1, x2
	// x1: 1
	// x2: 2
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmp, false, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
	cpu->set_pc(0);
	cpu->write_reg(1, 1);
	cpu->write_reg(2, 2);

	cpu->run(1);

	EXPECT_EQ(cpu->read_reg(1), 1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->read_reg(2), 2) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(cpu->get_flag(N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(cpu->get_flag(C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(cmp, zero_flag) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// cmp x0, x1, x2
	// x1: 1
	// x2: 1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmp, false, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
	cpu->set_pc(0);
	cpu->write_reg(1, 1);
	cpu->write_reg(2, 1);

	cpu->run(1);

	EXPECT_EQ(cpu->read_reg(1), 1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->read_reg(2), 1) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(cpu->get_flag(Z_FLAG), 1) << "Z flag should be set";
	EXPECT_EQ(cpu->get_flag(C_FLAG), 0) << "C flag should be set";
	EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(cmp, carry_flag_1) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// cmp x0, x1, x2
	// x1: -3
	// x2: -2
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmp, true, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
	cpu->set_pc(0);
	cpu->write_reg(1, -3);
	cpu->write_reg(2, -2);

	cpu->run(1);

	EXPECT_EQ(cpu->read_reg(1), -3) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->read_reg(2), -2) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(cpu->get_flag(N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(cpu->get_flag(C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(cmp, carry_flag_2) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// cmp x0, x1, x2
	// x1: 1
	// x2: -2
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmp, true, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
	cpu->set_pc(0);
	cpu->write_reg(1, 1);
	cpu->write_reg(2, -2);

	cpu->run(1);

	EXPECT_EQ(cpu->read_reg(1), 1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->read_reg(2), -2) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(cpu->get_flag(C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
	delete cpu;
}

TEST(cmp, overflow_flag__positive_to_negative) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// cmp x0, x1, x2
	// x1: (1<<31)-1
	// x2: -1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmp, true, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
	cpu->set_pc(0);
	cpu->write_reg(1, (1U<<31) - 1);
	cpu->write_reg(2, -1);

	cpu->run(1);

	EXPECT_EQ(cpu->read_reg(1), (1U<<31) - 1) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->read_reg(2), -1) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(cpu->get_flag(N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(cpu->get_flag(C_FLAG), 1) << "C flag should be set";
	EXPECT_EQ(cpu->get_flag(V_FLAG), 1) << "V flag should be set";
	delete cpu;
}

TEST(cmp, overflow_flag__negative_to_positive) {
	Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
	// cmp x0, x1, x2
	// x1: 1U<<31
	// x2: 1
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmp, true, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
	cpu->set_pc(0);
	cpu->write_reg(1, 1U << 31);
	cpu->write_reg(2, 1);

	cpu->run(1);

	EXPECT_EQ(cpu->read_reg(1), 1U<<31) << "operation should not alter operand register \'x1\'";
	EXPECT_EQ(cpu->read_reg(2), 1) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(cpu->get_flag(C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(cpu->get_flag(V_FLAG), 1) << "V flag should be set";
	delete cpu;
}