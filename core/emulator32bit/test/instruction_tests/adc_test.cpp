#include <emulator32bit_test/emulator32bit_test.h>

TEST(adc, register_adc_immediate) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    // adc x0, x1, #9
    // x1: 1
    // carry: 1
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_adc, false, 0, 1, 9));
    cpu->set_pc(0);
    cpu->write_reg(1, 1);
    cpu->set_NZCV(0, 0, 1, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 11) << "\'adc x0, x1 #9\' : where x1=1, c=1, should result in x0=11";
    EXPECT_EQ(cpu->read_reg(1), 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 1) << "operation should not alter C flag";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
    delete cpu;
}

TEST(adc, register_adc_register) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    // adc x0, x1, x2
    // x1: 1
    // x2: 9
    // carry: 1
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_adc, false, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, 1);
    cpu->write_reg(2, 9);
    cpu->set_NZCV(0, 0, 1, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 11) << "\'adc x0, x1, x2\' : where x1=1, x2=9, c=1, should result in x0=11";
    EXPECT_EQ(cpu->read_reg(1), 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 9) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 1) << "operation should not alter C flag";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
    delete cpu;
}

TEST(adc, register_adc_register_shifted) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    // adc x0, x1, x2, lsl #3
    // x1: 1
    // x2: 1
    // carry: 1
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_adc, false, 0, 1, 2, Emulator32bit::SHIFT_LSL, 3));
    cpu->set_pc(0);
    cpu->write_reg(1, 0);
    cpu->write_reg(2, 2);
    cpu->set_NZCV(0, 0, 1, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 17) << "\'adc x0, x1, x2, lsl #3\' : where x1=0, x2=2, c=1, should result in x0=17";
    EXPECT_EQ(cpu->read_reg(1), 0) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 2) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 1) << "operation should not alter C flag";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
    delete cpu;
}

TEST(adc, negative_flag) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    // adc x0, x1, x2
    // x1: -3
    // x2: 1
    // carry: 1
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_adc, true, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, -3);
    cpu->write_reg(2, 1);
    cpu->set_NZCV(0, 0, 1, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), -1) << "\'adc x0, x1, x2\' : where x1=-2, x2=-3, c=1, should result in x0=-1";
    EXPECT_EQ(cpu->read_reg(1), -3) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 1) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 1) << "N flag should be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";

    delete cpu;
}

TEST(adc, zero_flag) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    // adc x0, x1, x2
    // x1: -1
    // x2: 0
    // carry: 1
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_adc, true, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, -1);
    cpu->write_reg(2, 0);
    cpu->set_NZCV(0, 0, 1, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 0) << "\'adc x0, x1, x2\' : where x1=-1, x2=0, c=1, should result in x0=0";
    EXPECT_EQ(cpu->read_reg(1), -1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 0) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 1) << "Z flag should be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 1) << "C flag should be set";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
    delete cpu;
}

TEST(adc, carry_flag_1) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    // adc x0, x1, x2
    // x1: -1
    // x2: -2
    // carry: 1
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_adc, true, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, -1);
    cpu->write_reg(2, -2);
    cpu->set_NZCV(0, 0, 1, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), -2) << "\'adc x0, x1, x2\' : where x1=-1, x2=-2, c=1, should result in x0=-2";
    EXPECT_EQ(cpu->read_reg(1), -1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), -2) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 1) << "N flag should be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 1) << "C flag should be set";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
    delete cpu;
}

TEST(adc, carry_flag_2) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    // adc x0, x1, x2
    // x1: -4
    // x2: -5
    // carry: 1
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_adc, true, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, -4);
    cpu->write_reg(2, -5);
    cpu->set_NZCV(0, 0, 1, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), -8) << "\'adc x0, x1, x2\' : where x1=-4, x2=-5, c=1, should result in x0=-8";
    EXPECT_EQ(cpu->read_reg(1), -4) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), -5) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 1) << "N flag should be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 1) << "C flag should be set";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
    delete cpu;
}

TEST(adc, overflow_flag__neg_to_pos) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    // adc x0, x1, x2
    // x1: 1<<31
    // x2: 1<<31
    // carry: 1
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_adc, true, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, (1U << 31) - 1);
    cpu->write_reg(2, 1U << 31);
    cpu->set_NZCV(0, 0, 1, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 0) << "\'adc x0, x1, x2\' : where x1=(1<<31)-1, x2=1<<31, c=1, should result in x0=0";
    EXPECT_EQ(cpu->read_reg(1), (1U<<31) - 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 1U<<31) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 1) << "Z flag should be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 1) << "C flag should be set";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 1) << "V flag should be set";
    delete cpu;
}

TEST(adc, overflow_flag__pos_to_neg) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    // adc x0, x1, x2
    // x1: (1<<31) - 1
    // x2: 0
    // carry: 1
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_adc, true, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, (1U << 31) - 1);
    cpu->write_reg(2, 0);
    cpu->set_NZCV(0, 0, 1, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 1U<<31) << "\'adc x0, x1, x2\' : where x1=(1<<31)-1, x2=0, should result in x0=1<<31";
    EXPECT_EQ(cpu->read_reg(1), (1U<<31) - 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 0) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 1) << "N flag should be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 1) << "V flag should be set";
    delete cpu;
}