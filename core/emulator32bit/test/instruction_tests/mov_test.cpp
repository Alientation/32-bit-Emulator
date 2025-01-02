#include <emulator32bit_test/emulator32bit_test.h>

TEST(mov, register_mov_immediate) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    // mov x0, #9
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, false, 0, 9));
    cpu->set_pc(0);
    cpu->set_NZCV(0, 0, 1, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 9) << "\'mov x0 #9\' : should result in x0=9";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 1) << "operation should not alter C flag";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
    delete cpu;
}

TEST(mov, zero_flag) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, {}, 0, 1);
    // mov x0, #0
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, true, 0, 0));
    cpu->set_pc(0);
    cpu->set_NZCV(0, 0, 1, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 0) << "\'movs x0, #0\' : should result in x0=0";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 1) << "operation should set Z flag";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 1) << "operation should not alter C flag";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
    delete cpu;
}