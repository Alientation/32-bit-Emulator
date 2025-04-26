#include <emulator32bit_test/emulator32bit_test.h>

TEST(ror, imm5_shift) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o1(Emulator32bit::_op_ror, 0, 1, true, 2, 16));
    cpu->set_pc(0);
    cpu->write_reg(1, 1<<15);
    cpu->set_NZCV(1, 0, 0, 1);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 1<<31) << "\'ror x0, x1, 5\' : where x1=1<<15, should result in x0=1<<31";
    EXPECT_EQ(cpu->read_reg(1), 1<<15) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 1) << "operation should not alter N flag";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not alter Z flag";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 0) << "operation should not alter C flag";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 1) << "operation should not alter V flag";
    delete cpu;
}

TEST(ror, reg_shift) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o1(Emulator32bit::_op_ror, 0, 1, false, 2, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, 1<<15);
    cpu->write_reg(2, 16);
    cpu->set_NZCV(0, 0, 1, 1);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 1<<31) << "\'ror x0, x1, x2\' : where x1=1<<15, x2=16, should result in x0=1<<31";
    EXPECT_EQ(cpu->read_reg(1), 1<<15) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 16) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not alter N flag";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not alter Z flag";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 1) << "operation should not alter C flag";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 1) << "operation should not alter V flag";
    delete cpu;
}
