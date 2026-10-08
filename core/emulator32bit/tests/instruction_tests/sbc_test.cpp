#include <emulator32bit_test/emulator32bit_test.h>

TEST_F(EmulatorFixture, sbc_register_sbc_immediate)
{
    // sbc x0, x1, #9
    // x1: 11
    // carry: 1
    cpu.system_bus->write_word(0,
                               Emulator32bit::asm_format_o(Emulator32bit::_op_sbc, false, 0, 1, 9));
    cpu.set_pc(0);
    cpu.write_reg(1, 11);
    cpu.set_NZCV(0, 0, 0, 0);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 1) << "\'sbc x0, x1 #9\' : where x1=11, c=1, should result in x0=1";
    EXPECT_EQ(cpu.read_reg(1), 11) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, sbc_register_sbc_register)
{
    // sbc x0, x1, x2
    // x1: 11
    // x2: 9
    // carry: 1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sbc, false, 0, 1,
                                                              2, ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 11);
    cpu.write_reg(2, 9);
    cpu.set_NZCV(0, 0, 0, 0);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 1)
        << "\'sbc x0, x1, x2\' : where x1=11, x2=9, c=1, should result in x0=1";
    EXPECT_EQ(cpu.read_reg(1), 11) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 9) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, sbc_negative_flag)
{
    // sbc x0, x1, x2
    // x1: 2
    // x2: 2
    // carry: 1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sbc, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 2);
    cpu.write_reg(2, 2);
    cpu.set_NZCV(0, 0, 0, 0);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), -1)
        << "\'sbc x0, x1, x2\' : where x1=2, x2=2, c=1, should result in x0=-1";
    EXPECT_EQ(cpu.read_reg(1), 2) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 2) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, sbc_zero_flag)
{
    // sbc x0, x1, x2
    // x1: 2
    // x2: 1
    // carry: 1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sbc, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 2);
    cpu.write_reg(2, 1);
    cpu.set_NZCV(0, 0, 0, 0);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 0)
        << "\'sbc x0, x1, x2\' : where x1=2, x2=1, c=1, should result in x0=0";
    EXPECT_EQ(cpu.read_reg(1), 2) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 1) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, sbc_carry_flag_1)
{
    // sbc x0, x1, x2
    // x1: -2
    // x2: -2
    // carry: 1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sbc, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, -2);
    cpu.write_reg(2, -2);
    cpu.set_NZCV(0, 0, 0, 0);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), -1)
        << "\'sbc x0, x1, x2\' : where x1=-2, x2=-2, c=1, should result in x0=-1";
    EXPECT_EQ(cpu.read_reg(1), -2) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), -2) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, sbc_carry_flag_2)
{
    // sbc x0, x1, x2
    // x1: 2
    // x2: -2
    // carry: 1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sbc, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 2);
    cpu.write_reg(2, -2);
    cpu.set_NZCV(0, 0, 0, 0);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 3)
        << "\'sbc x0, x1, x2\' : where x1=2, x2=-2, c=1, should result in x0=3";
    EXPECT_EQ(cpu.read_reg(1), 2) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), -2) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, sbc_overflow_flag__positive_to_negative)
{
    // sbc x0, x1, x2
    // x1: (1<<31)-1
    // x2: -2
    // carry: 1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sbc, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, (1U << 31) - 1);
    cpu.write_reg(2, -2);
    cpu.set_NZCV(0, 0, 0, 0);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 1U << 31)
        << "\'sbc x0, x1, x2\' : where x1=(1<<31)-1, x2=-2, c=1, should result in x0=1<<31";
    EXPECT_EQ(cpu.read_reg(1), (1U << 31) - 1)
        << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), -2) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 1);
}

TEST_F(EmulatorFixture, sbc_overflow_flag__negative_to_positive)
{
    // sbc x0, x1, x2
    // x1: 1U<<31
    // x2: 0
    // carry: 1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_sbc, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 1U << 31);
    cpu.write_reg(2, 0);
    cpu.set_NZCV(0, 0, 0, 0);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), (1U << 31) - 1)
        << "\'sbc x0, x1, x2\' : where x1=1<<31, x2=0, c=1, should result in x0=(1<<31)-1";
    EXPECT_EQ(cpu.read_reg(1), 1U << 31) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 0) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 1);
}