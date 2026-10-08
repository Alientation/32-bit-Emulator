#include <emulator32bit_test/emulator32bit_test.h>

TEST_F(EmulatorFixture, sub_register_sub_immediate)
{
    // sub x0, x1, #10
    // x1: 11
    cpu.system_bus->write_word(
        0, Emulator32bit::asm_format_o(Emulator32bit::_op_sub, false, 0, 1, 10));
    cpu.set_pc(0);
    cpu.write_reg(1, 11);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 1) << "\'sub x0, x1 #10\' : where x1=11, should result in x0=1";
    EXPECT_EQ(cpu.read_reg(1), 11) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, sub_register_sub_register)
{
    // sub x0, x1, x2
    // x1: 11
    // x2: 10
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sub, false, 0, 1,
                                                              2, ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 11);
    cpu.write_reg(2, 10);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 1)
        << "\'sub x0, x1, x2\' : where x1=11, x2=10, should result in x0=1";
    EXPECT_EQ(cpu.read_reg(1), 11) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 10) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, sub_negative_flag)
{
    // sub x0, x1, x2
    // x1: 1
    // x2: 2
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sub, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 1);
    cpu.write_reg(2, 2);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), -1)
        << "\'sub x0, x1, x2\' : where x1=1, x2=2, should result in x0=-1";
    EXPECT_EQ(cpu.read_reg(1), 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 2) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, sub_zero_flag)
{
    // sub x0, x1, x2
    // x1: 1
    // x2: 1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sub, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 1);
    cpu.write_reg(2, 1);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 0) << "\'sub x0, x1, x2\' : where x1=1, x2=1, should result in x0=0";
    EXPECT_EQ(cpu.read_reg(1), 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 1) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, sub_carry_flag_1)
{
    // sub x0, x1, x2
    // x1: -3
    // x2: -2
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sub, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, -3);
    cpu.write_reg(2, -2);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), -1)
        << "\'sub x0, x1, x2\' : where x1=-3, x2=-2, should result in x0=-1";
    EXPECT_EQ(cpu.read_reg(1), -3) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), -2) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, sub_carry_flag_2)
{
    // sub x0, x1, x2
    // x1: 1
    // x2: -2
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sub, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 1);
    cpu.write_reg(2, -2);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 3)
        << "\'sub x0, x1, x2\' : where x1=1, x2=-2, should result in x0=3";
    EXPECT_EQ(cpu.read_reg(1), 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), -2) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, sub_overflow_flag__positive_to_negative)
{
    // sub x0, x1, x2
    // x1: (1<<31)-1
    // x2: -1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sub, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, (1U << 31) - 1);
    cpu.write_reg(2, -1);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 1U << 31)
        << "\'sub x0, x1, x2\' : where x1=(1<<31)-1, x2=-1, should result in x0=1<<31";
    EXPECT_EQ(cpu.read_reg(1), (1U << 31) - 1)
        << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), -1) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 1);
}

TEST_F(EmulatorFixture, sub_overflow_flag__negative_to_positive)
{
    // sub x0, x1, x2
    // x1: 1U<<31
    // x2: 1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sub, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 1U << 31);
    cpu.write_reg(2, 1);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), (1U << 31) - 1)
        << "\'sub x0, x1, x2\' : where x1=1<<31, x2=1, should result in x0=(1<<31)-1";
    EXPECT_EQ(cpu.read_reg(1), 1U << 31) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 1) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 1);
}