#include <emulator32bit_test/emulator32bit_test.h>

TEST_F(EmulatorFixture, cmn_register_cmn_immediate) {
    cpu = new(cpu) Emulator32bit(1, 0, {}, 0, 1);
    // cmn x0, x1, #10
    // x1: 1
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, false, 0, 1, 10));
    cpu->set_pc(0);
    cpu->write_reg(1, 1);


    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(1), 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->get_flag(kNFlagBit), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(kCFlagBit), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(kVFlagBit), 0) << "operation should not cause V flag to be set";
}

TEST_F(EmulatorFixture, cmn_register_cmn_register) {
    cpu = new(cpu) Emulator32bit(1, 0, {}, 0, 1);
    // cmn x0, x1, x2
    // x1: 1
    // x2: 10
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, false, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, 1);
    cpu->write_reg(2, 10);


    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(1), 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 10) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(kNFlagBit), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(kCFlagBit), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(kVFlagBit), 0) << "operation should not cause V flag to be set";
}

TEST_F(EmulatorFixture, cmn_register_cmn_register_shifted) {
    cpu = new(cpu) Emulator32bit(1, 0, {}, 0, 1);
    // cmn x0, x1, x2, lsl #3
    // x1: 1
    // x2: 2
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, false, 0, 1, 2, Emulator32bit::SHIFT_LSL, 3));
    cpu->set_pc(0);
    cpu->write_reg(1, 1);
    cpu->write_reg(2, 2);


    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(1), 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 2) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(kNFlagBit), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(kCFlagBit), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(kVFlagBit), 0) << "operation should not cause V flag to be set";
}

TEST_F(EmulatorFixture, cmn_negative_flag) {
    cpu = new(cpu) Emulator32bit(1, 0, {}, 0, 1);
    // cmn x0, x1, x2
    // x1: -2
    // x2: 1
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, false, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, -2);
    cpu->write_reg(2, 1);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(1), -2) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 1) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(kNFlagBit), 1) << "N flag should be set";
    EXPECT_EQ(cpu->get_flag(kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(kCFlagBit), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(kVFlagBit), 0) << "operation should not cause V flag to be set";
}

TEST_F(EmulatorFixture, cmn_zero_flag) {
    cpu = new(cpu) Emulator32bit(1, 0, {}, 0, 1);
    // cmn x0, x1, x2
    // x1: 0
    // x2: 0
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, false, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, 0);
    cpu->write_reg(2, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(1), 0) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 0) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(kNFlagBit), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(kZFlagBit), 1) << "Z flag should be set";
    EXPECT_EQ(cpu->get_flag(kCFlagBit), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(kVFlagBit), 0) << "operation should not cause V flag to be set";
}

TEST_F(EmulatorFixture, cmn_carry_flag_1) {
    cpu = new(cpu) Emulator32bit(1, 0, {}, 0, 1);
    // cmn x0, x1, x2
    // x1: 0
    // x2: 0
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, true, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, -1);
    cpu->write_reg(2, -1);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(1), -1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), -1) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(kNFlagBit), 1) << "N flag should be set";
    EXPECT_EQ(cpu->get_flag(kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(kCFlagBit), 1) << "C flag should be set";
    EXPECT_EQ(cpu->get_flag(kVFlagBit), 0) << "operation should not cause V flag to be set";
}

TEST_F(EmulatorFixture, cmn_carry_flag_2) {
    cpu = new(cpu) Emulator32bit(1, 0, {}, 0, 1);
    // cmn x0, x1, x2
    // x1: 0
    // x2: 0
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, true, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, -4);
    cpu->write_reg(2, -4);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(1), -4) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), -4) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(kNFlagBit), 1) << "N flag should be set";
    EXPECT_EQ(cpu->get_flag(kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(kCFlagBit), 1) << "C flag should be set";
    EXPECT_EQ(cpu->get_flag(kVFlagBit), 0) << "operation should not cause V flag to be set";
}

TEST_F(EmulatorFixture, cmn_overflow_flag__neg_to_pos) {
    cpu = new(cpu) Emulator32bit(1, 0, {}, 0, 1);
    // cmn x0, x1, x2
    // x1: 1<<31
    // x2: 1<<31
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, true, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, 1U << 31);
    cpu->write_reg(2, 1U << 31);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(1), 1U<<31) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 1U<<31) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(kNFlagBit), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(kZFlagBit), 1) << "Z flag should be set";
    EXPECT_EQ(cpu->get_flag(kCFlagBit), 1) << "C flag should be set";
    EXPECT_EQ(cpu->get_flag(kVFlagBit), 1) << "V flag should be set";
}

TEST_F(EmulatorFixture, cmn_overflow_flag__pos_to_neg) {
    cpu = new(cpu) Emulator32bit(1, 0, {}, 0, 1);
    // cmn x0, x1, x2
    // x1: (1<<31) - 1
    // x2: 1
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, true, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, (1U << 31) - 1);
    cpu->write_reg(2, 1);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(1), (1U<<31) - 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 1) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(kNFlagBit), 1) << "N flag should be set";
    EXPECT_EQ(cpu->get_flag(kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(kCFlagBit), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(kVFlagBit), 1) << "V flag should be set";
}