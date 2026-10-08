#include <emulator32bit_test/emulator32bit_test.h>

TEST_F(EmulatorFixture, rsb_register_rsb_immediate)
{
    // rsb x0, x1, #11
    // x1: 10
    cpu.system_bus->write_word(
        0, Emulator32bit::asm_format_o(Emulator32bit::_op_rsb, false, 0, 1, 11));
    cpu.set_pc(0);
    cpu.write_reg(1, 10);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 1) << "\'rsb x0, x1 #11\' : where x1=10, should result in x0=1";
    EXPECT_EQ(cpu.read_reg(1), 10) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, rsb_register_rsb_register)
{
    // rsb x0, x1, x2
    // x1: 10
    // x2: 11
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_rsb, false, 0, 1,
                                                              2, ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 10);
    cpu.write_reg(2, 11);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 1)
        << "\'rsb x0, x1, x2\' : where x1=10, x2=11, should result in x0=1";
    EXPECT_EQ(cpu.read_reg(1), 10) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 11) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, rsb_negative_flag)
{
    // rsb x0, x1, x2
    // x1: 2
    // x2: 1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_rsb, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 2);
    cpu.write_reg(2, 1);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), -1)
        << "\'rsb x0, x1, x2\' : where x1=2, x2=1, should result in x0=-1";
    EXPECT_EQ(cpu.read_reg(1), 2) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 1) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, rsb_zero_flag)
{
    // rsb x0, x1, x2
    // x1: 1
    // x2: 1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_rsb, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 1);
    cpu.write_reg(2, 1);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 0) << "\'rsb x0, x1, x2\' : where x1=1, x2=1, should result in x0=0";
    EXPECT_EQ(cpu.read_reg(1), 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 1) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, rsb_carry_flag_1)
{
    // rsb x0, x1, x2
    // x1: -2
    // x2: -3
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_rsb, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, -2);
    cpu.write_reg(2, -3);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), -1)
        << "\'rsb x0, x1, x2\' : where x1=-2, x2=-3, should result in x0=-1";
    EXPECT_EQ(cpu.read_reg(1), -2) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), -3) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, rsb_carry_flag_2)
{
    // rsb x0, x1, x2
    // x1: -2
    // x2: 1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_rsb, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, -2);
    cpu.write_reg(2, 1);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 3)
        << "\'rsb x0, x1, x2\' : where x1=-2, x2=1, should result in x0=3";
    EXPECT_EQ(cpu.read_reg(1), -2) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 1) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, rsb_overflow_flag__positive_to_negative)
{
    // rsb x0, x1, x2
    // x1: -1
    // x2: (1<<31)-1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_rsb, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, -1);
    cpu.write_reg(2, (1U << 31) - 1);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 1U << 31)
        << "\'rsb x0, x1, x2\' : where x1=-1, x2=(1<<31)-1, should result in x0=1<<31";
    EXPECT_EQ(cpu.read_reg(1), -1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), (1U << 31) - 1)
        << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 1);
}

TEST_F(EmulatorFixture, rsb_overflow_flag__negative_to_positive)
{
    // rsb x0, x1, x2
    // x1: 1
    // x2: 1<<31
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_rsb, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 1);
    cpu.write_reg(2, 1U << 31);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), (1U << 31) - 1)
        << "\'rsb x0, x1, x2\' : where x1=1, x2=1<<31, should result in x0=(1<<31)-1";
    EXPECT_EQ(cpu.read_reg(1), 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 1U << 31) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 1);
}