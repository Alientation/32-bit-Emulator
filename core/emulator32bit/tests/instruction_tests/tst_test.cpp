#include <emulator32bit_test/emulator32bit_test.h>

TEST(tst, register_tst_register) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    // tst x0, x1, x2
    // x1: 0b0011
    // x2: 0b1010
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_tst, false, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, 0b0011);
    cpu->write_reg(2, 0b1010);
    cpu->set_NZCV(1, 1, 1, 1);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(1), 0b0011) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 0b1010) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 1) << "operation should not alter C flag";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 1) << "operation should not alter V flag";
    delete cpu;
}

TEST(tst, negative_flag) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    // tst x0, x1, x2
    // x1: ~0
    // x2: 1<<31
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_tst, true, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, ~0);
    cpu->write_reg(2, 1U << 31);
    cpu->set_NZCV(0, 1, 1, 1);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(1), ~0) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 1U<<31) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 1) << "N flag should be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 1) << "operation should not alter C flag";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 1) << "operation should not alter V flag";
    delete cpu;
}

TEST(tst, zero_flag) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    // tst x0, x1, x2
    // x1: ~0
    // x2: 0
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_tst, true, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, ~0);
    cpu->write_reg(2, 0);
    cpu->write_reg(0, -1);
    cpu->set_NZCV(0, 0, 1, 1);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), -1) << "operation should not alter destination register \'x0\'";
    EXPECT_EQ(cpu->read_reg(1), ~0) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 0) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 1) << "Z flag should be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 1) << "operation should not alter C flag";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 1) << "operation should not alter V flag";
    delete cpu;
}