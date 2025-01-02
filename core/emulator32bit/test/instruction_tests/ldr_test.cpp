#include <emulator32bit_test/emulator32bit_test.h>

#include <iostream>

static const byte data[PAGE_SIZE] = {9U, 0U, 0U, 0U};

TEST(ldr, offset_positive_constant) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, data, 1, 1);
    // ldr x0, [x1, #3]
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 3, Emulator32bit::ADDR_OFFSET));
    cpu->set_pc(0);
    cpu->write_reg(1, PAGE_SIZE - 3);
    cpu->set_NZCV(0, 0, 0, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 9) << "\'ldr x0, [x1, #3]\', where x1=PAGE_SIZE-3 : should result in x0=9";
    EXPECT_EQ(cpu->read_reg(1), PAGE_SIZE - 3) << "operation should not change operand \'x1\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
    delete cpu;
}

TEST(ldr, offset_negative_constant) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, data, 1, 1);
    // ldr x0, [x1, #-3]
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, -3, Emulator32bit::ADDR_OFFSET));
    cpu->set_pc(0);
    cpu->write_reg(1, PAGE_SIZE + 3);
    cpu->set_NZCV(0, 0, 0, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 9) << "\'ldr x0, [x1, #-3]\', where x1=PAGE_SIZE+3 : should result in x0=9";
    EXPECT_EQ(cpu->read_reg(1), PAGE_SIZE + 3) << "operation should not change operand \'x1\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
    delete cpu;
}

TEST(ldr, offset_reg) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, data, 1, 1);
    // ldr x0, [x1, x2]
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 2,
            Emulator32bit::SHIFT_LSL, 0, Emulator32bit::ADDR_OFFSET));
    cpu->set_pc(0);
    cpu->write_reg(1, PAGE_SIZE - 3);
    cpu->write_reg(2, 3);
    cpu->set_NZCV(0, 0, 0, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 9) << "\'ldr x0, [x1, x2]\', where x1=PAGE_SIZE-3, x2=3 : should result in x0=9";
    EXPECT_EQ(cpu->read_reg(1), PAGE_SIZE - 3) << "operation should not change operand \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 3) << "operation should not change operand \'x2\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
    delete cpu;
}

TEST(ldr, offset_reg_lsl) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, data, 1, 1);
    // ldr x0, [x1, x2]
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 2,
            Emulator32bit::SHIFT_LSL, 1, Emulator32bit::ADDR_OFFSET));
    cpu->set_pc(0);
    cpu->write_reg(1, PAGE_SIZE - 4);
    cpu->write_reg(2, 2);
    cpu->set_NZCV(0, 0, 0, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 9) << "\'ldr x0, [x1, x2, LSL #1]\', where x1=PAGE_SIZE-4, x2=2 : should result in x0=9";
    EXPECT_EQ(cpu->read_reg(1), PAGE_SIZE - 4) << "operation should not change operand \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 2) << "operation should not change operand \'x2\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
    delete cpu;
}

TEST(ldr, offset_reg_lsr) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, data, 1, 1);
    // ldr x0, [x1, x2]
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 2,
            Emulator32bit::SHIFT_LSR, 1, Emulator32bit::ADDR_OFFSET));
    cpu->set_pc(0);
    cpu->write_reg(1, PAGE_SIZE - 4);
    cpu->write_reg(2, 8);
    cpu->set_NZCV(0, 0, 0, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 9) << "\'ldr x0, [x1, x2, LSR #1]\', where x1=PAGE_SIZE-4, x2=8 : should result in x0=9";
    EXPECT_EQ(cpu->read_reg(1), PAGE_SIZE - 4) << "operation should not change operand \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 8) << "operation should not change operand \'x2\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
    delete cpu;
}

TEST(ldr, pre_indexed) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, data, 1, 1);
    // ldr x0, [x1, #3]!
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 3, Emulator32bit::ADDR_PRE_INC));
    cpu->set_pc(0);
    cpu->write_reg(1, PAGE_SIZE - 3);
    cpu->set_NZCV(0, 0, 0, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 9) << "\'ldr x0, [x1, #3]!\', where x1=PAGESIZE - 3 : should result in x0=9";
    EXPECT_EQ(cpu->read_reg(1), PAGE_SIZE) << "operation should preincrement \'x1\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
    delete cpu;
}

TEST(ldr, post_indexed) {
    Emulator32bit *cpu = new Emulator32bit(1, 0, data, 1, 1);
    // ldr x0, [x1, #3]!
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 3, Emulator32bit::ADDR_POST_INC));
    cpu->set_pc(0);
    cpu->write_reg(1, PAGE_SIZE);
    cpu->set_NZCV(0, 0, 0, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 9) << "\'ldr x0, [x1], #3\', where x1=PAGE_SIZE : should result in x0=9";
    EXPECT_EQ(cpu->read_reg(1), 3 + PAGE_SIZE) << "operation should postincrement \'x1\'";
    EXPECT_EQ(cpu->get_flag(N_FLAG), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(Z_FLAG), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(C_FLAG), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(V_FLAG), 0) << "operation should not cause V flag to be set";
    delete cpu;
}