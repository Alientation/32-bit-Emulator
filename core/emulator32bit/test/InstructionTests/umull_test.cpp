#include <emulator32bit_test/Emulator32bitTest.h>

TEST(umull, register_umull_register) {
	Emulator32bit *cpu = new Emulator32bit(4, 0, {}, 0, 4);
	// umull x0, x1, x2, x3
	// x2: 2
	// x3: 4
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o2(Emulator32bit::_op_umull, false, 0, 1, 2, 3));
	cpu->_pc = 0;
	cpu->_x[2] = 2;
	cpu->_x[3] = 4;

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 8) << "\'umull x0, x1, x2, x3\' : where x2=2, x3=4, should result in x0=8, x1=0";
	EXPECT_EQ(cpu->_x[1], 0) << "\'umull x0, x1, x2, x3\' : where x2=2, x3=4, should result in x0=8, x1=0";
	EXPECT_EQ(cpu->_x[2], 2) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(cpu->_x[3], 4) << "operation should not alter operand register \'x3\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(umull, negative_flag) {
	Emulator32bit *cpu = new Emulator32bit(4, 0, {}, 0, 4);
	// umull x0, x1, x2, x3
	// x2: 1<<31
	// x3: 1<<31
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o2(Emulator32bit::_op_umull, true, 0, 1, 2, 3));
	cpu->_pc = 0;
	cpu->_x[2] = ~0;
	cpu->_x[3] = ~0;

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 1) << "\'umull x0, x1, x2, x3\' : where x2=~0, x3=~0, should result in x0=1, x1=-2";
	EXPECT_EQ(cpu->_x[1], -2) << "\'umull x0, x1, x2, x3\' : where x2=~0, x3=~0, should result in x0=1, x1=-2";
	EXPECT_EQ(cpu->_x[2], ~0) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(cpu->_x[3], ~0) << "operation should not alter operand register \'x3\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 1) << "N flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 0) << "operation should not cause C flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(umull, zero_flag) {
	Emulator32bit *cpu = new Emulator32bit(4, 0, {}, 0, 4);
	// umull x0, x1, x2, x3
	// x2: 0
	// x3: 4
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o2(Emulator32bit::_op_umull, true, 0, 1, 2, 3));
	cpu->_pc = 0;
	cpu->_x[2] = 0;
	cpu->_x[3] = 4;
	cpu->set_NZCV(0, 0, 1, 1);

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 0) << "\'umull x0, x1, x2, x3\' : where x2=0, x3=4, should result in x0=0, x1=0";
	EXPECT_EQ(cpu->_x[1], 0) << "\'umull x0, x1, x2, x3\' : where x2=0, x3=4, should result in x0=0, x1=0";
	EXPECT_EQ(cpu->_x[2], 0) << "operation should not alter operand register \'x2\'";
	EXPECT_EQ(cpu->_x[3], 4) << "operation should not alter operand register \'x3\'";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 1) << "Z flag should be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "operation should not alter C flag";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 1) << "operation should not alter V flag";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}