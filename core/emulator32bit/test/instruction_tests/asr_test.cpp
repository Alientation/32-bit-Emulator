#include <emulator32bit_test/emulator32bit_test.h>

TEST(asr, imm5_shift_pos) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o1(Emulator32bit::_op_asr, 0, 1, true, 2, 5));
    cpu->set_pc(0);
    cpu->write_reg(1, 32);
    cpu->set_NZCV(1, 0, 0, 1);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 1) << "\'asr x0, x1, 5\' : where x1=32, should result in x0=1";
    EXPECT_EQ(cpu->read_reg(1), 32) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 1) << "operation should not alter N flag";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not alter Z flag";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 0) << "operation should not alter C flag";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 1) << "operation should not alter V flag";
    delete cpu;
}

TEST(asr, imm5_shift_neg) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o1(Emulator32bit::_op_asr, 0, 1, true, 2, 5));
    cpu->set_pc(0);
    cpu->write_reg(1, -32);
    cpu->set_NZCV(1, 0, 0, 1);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), -1) << "\'asr x0, x1, 5\' : where x1=-32, should result in x0=-1";
    EXPECT_EQ(cpu->read_reg(1), -32) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 1) << "operation should not alter N flag";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not alter Z flag";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 0) << "operation should not alter C flag";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 1) << "operation should not alter V flag";
    delete cpu;
}

TEST(asr, reg_shift) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o1(Emulator32bit::_op_asr, 0, 1, false, 2, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, 32);
    cpu->write_reg(2, 5);
    cpu->set_NZCV(0, 0, 1, 1);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 1) << "\'asr x0, x1, x2\' : where x1=32, x2=5, should result in x0=1";
    EXPECT_EQ(cpu->read_reg(1), 32) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 5) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not alter N flag";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not alter Z flag";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 1) << "operation should not alter C flag";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 1) << "operation should not alter V flag";
    delete cpu;
}

TEST(asr, reg_shift_low_byte) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o1(Emulator32bit::_op_asr, 0, 1, false, 2, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, 32);
    cpu->write_reg(2, 5 + ~((word) 0xFF));
    cpu->set_NZCV(0, 1, 0, 1);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 1) << "\'asr x0, x1, x2\' : where x1=32, x2=5 (lower 8 bits), should result in x0=1";
    EXPECT_EQ(cpu->read_reg(1), 32) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 5 + ~((word)0xFF)) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not alter N flag";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 1) << "operation should not alter Z flag";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 0) << "operation should not alter C flag";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 1) << "operation should not alter V flag";
    delete cpu;
}
