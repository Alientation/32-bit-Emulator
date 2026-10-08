#include <emulator32bit_test/emulator32bit_test.h>

TEST_F(EmulatorFixture, cmp_register_cmp_immediate)
{
    // cmp x0, x1, #10
    // x1: 11
    cpu.system_bus->write_word(
        0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmp, false, 0, 1, 10));
    cpu.set_pc(0);
    cpu.write_reg(1, 11);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(1), 11) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, cmp_register_cmp_register)
{
    // cmp x0, x1, x2
    // x1: 11
    // x2: 10
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmp, false, 0, 1,
                                                              2, ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 11);
    cpu.write_reg(2, 10);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(1), 11) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 10) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, cmp_negative_flag)
{
    // cmp x0, x1, x2
    // x1: 1
    // x2: 2
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmp, false, 0, 1,
                                                              2, ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 1);
    cpu.write_reg(2, 2);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(1), 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 2) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, cmp_zero_flag)
{
    // cmp x0, x1, x2
    // x1: 1
    // x2: 1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmp, false, 0, 1,
                                                              2, ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 1);
    cpu.write_reg(2, 1);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(1), 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 1) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, cmp_carry_flag_1)
{
    // cmp x0, x1, x2
    // x1: -3
    // x2: -2
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmp, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, -3);
    cpu.write_reg(2, -2);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(1), -3) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), -2) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, cmp_carry_flag_2)
{
    // cmp x0, x1, x2
    // x1: 1
    // x2: -2
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmp, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 1);
    cpu.write_reg(2, -2);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(1), 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), -2) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, cmp_overflow_flag__positive_to_negative)
{
    // cmp x0, x1, x2
    // x1: (1<<31)-1
    // x2: -1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmp, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, (1U << 31) - 1);
    cpu.write_reg(2, -1);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(1), (1U << 31) - 1)
        << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), -1) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 1);
}

TEST_F(EmulatorFixture, cmp_overflow_flag__negative_to_positive)
{
    // cmp x0, x1, x2
    // x1: 1U<<31
    // x2: 1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_cmp, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 1U << 31);
    cpu.write_reg(2, 1);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(1), 1U << 31) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 1) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 1);
}