#include <emulator32bit_test/Emulator32bitTest.h>

TEST(mov, register_mov_immediate) {
	Emulator32bit *cpu = new Emulator32bit(4, 0, {}, 0, 4);
	// mov x0, x1, #9
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_mov, false, 0, 1, 9));
	cpu->_pc = 0;
	cpu->set_NZCV(0, 0, 1, 0);

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 9) << "\'mov x0, x1 #9\' : where x1=1, should result in x0=9";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 0) << "operation should not cause Z flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "operation should not alter C flag";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}

TEST(mov, zero_flag) {
	Emulator32bit *cpu = new Emulator32bit(4, 0, {}, 0, 4);
	// mov x0, x1, #0
	cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_mov, true, 0, 1, 0));
	cpu->_pc = 0;
	cpu->set_NZCV(0, 0, 1, 0);

	Emulator32bit::EmulatorException exception;
	cpu->run(1, exception);

	EXPECT_EQ(cpu->_x[0], 0) << "\'movs x0, #0\' : should result in x0=0";
	EXPECT_EQ(test_bit(cpu->_pstate, N_FLAG), 0) << "operation should not cause N flag to be set";
	EXPECT_EQ(test_bit(cpu->_pstate, Z_FLAG), 1) << "operation should set Z flag";
	EXPECT_EQ(test_bit(cpu->_pstate, C_FLAG), 1) << "operation should not alter C flag";
	EXPECT_EQ(test_bit(cpu->_pstate, V_FLAG), 0) << "operation should not cause V flag to be set";
	EXPECT_EQ(exception.isOK(), true) << "cpu should be OK";
	delete cpu;
}