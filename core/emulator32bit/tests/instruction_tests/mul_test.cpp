#include <emulator32bit_test/emulator32bit_test.h>

TEST_F(EmulatorFixture, mul_register_mul_immediate) {
    cpu = new(cpu) Emulator32bit(1, 0, {}, 0, 1);
    // mul x0, x1, #9
    // x1: 2
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_mul, false, 0, 1, 9));
    cpu->set_pc(0);
    cpu->write_reg(1, 2);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 18) << "\'mul x0, x1 #9\' : where x1=2, should result in x0=18";
    EXPECT_EQ(cpu->read_reg(1), 2) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->get_flag(kNFlagBit), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(kCFlagBit), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(kVFlagBit), 0) << "operation should not cause V flag to be set";
}

TEST_F(EmulatorFixture, mul_register_mul_register) {
    cpu = new(cpu) Emulator32bit(1, 0, {}, 0, 1);
    // mul x0, x1, x2
    // x1: 2
    // x2: 4
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_mul, false, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, 2);
    cpu->write_reg(2, 4);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 8) << "\'mul x0, x1, x2\' : where x1=2, x2=4, should result in x0=8";
    EXPECT_EQ(cpu->read_reg(1), 2) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 4) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(kNFlagBit), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(kCFlagBit), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(kVFlagBit), 0) << "operation should not cause V flag to be set";
}

TEST_F(EmulatorFixture, mul_register_mul_register_shift) {
    cpu = new(cpu) Emulator32bit(1, 0, {}, 0, 1);
    // mul x0, x1, x2, lsr #1
    // x1: 2
    // x2: 4
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_mul, false, 0, 1, 2, Emulator32bit::SHIFT_LSR, 1));
    cpu->set_pc(0);
    cpu->write_reg(1, 2);
    cpu->write_reg(2, 4);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 4) << "\'mul x0, x1, x2, lsr #1\' : where x1=2, x2=4, should result in x0=4";
    EXPECT_EQ(cpu->read_reg(1), 2) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 4) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(kNFlagBit), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(kCFlagBit), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(kVFlagBit), 0) << "operation should not cause V flag to be set";
}

TEST_F(EmulatorFixture, mul_negative_flag) {
    cpu = new(cpu) Emulator32bit(1, 0, {}, 0, 1);
    // mul x0, x1, x2
    // x1: -2
    // x2: 4
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_mul, true, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, -2);
    cpu->write_reg(2, 4);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), -8) << "\'mul x0, x1, x2\' : where x1=-2, x2=4, should result in x0=-8";
    EXPECT_EQ(cpu->read_reg(1), -2) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 4) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(kNFlagBit), 1) << "N flag should be set";
    EXPECT_EQ(cpu->get_flag(kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(kCFlagBit), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(kVFlagBit), 0) << "operation should not cause V flag to be set";
}

TEST_F(EmulatorFixture, mul_zero_flag) {
    cpu = new(cpu) Emulator32bit(1, 0, {}, 0, 1);
    // mul x0, x1, x2
    // x1: 0
    // x2: 4
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_mul, true, 0, 1, 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc(0);
    cpu->write_reg(1, 0);
    cpu->write_reg(2, 4);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), 0) << "\'mul x0, x1, x2\' : where x1=0, x2=4, should result in x0=0";
    EXPECT_EQ(cpu->read_reg(1), 0) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu->read_reg(2), 4) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu->get_flag(kNFlagBit), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(kZFlagBit), 1) << "Z flag should be set";
    EXPECT_EQ(cpu->get_flag(kCFlagBit), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ(cpu->get_flag(kVFlagBit), 0) << "operation should not cause V flag to be set";
}