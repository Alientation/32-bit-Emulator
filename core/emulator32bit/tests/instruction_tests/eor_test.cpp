#include <emulator32bit_test/emulator32bit_test.h>

TEST_F(EmulatorFixture, eor_register_and_register)
{
    // eor x0, x1, x2
    // x1: 0b0011
    // x2: 0b1010
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_eor, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 0b0011);
    cpu.write_reg(2, 0b1010);
    cpu.set_NZCV(1, 1, 1, 1);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 0b1001)
        << "\'eor x0, x1, 2\' : where x1=0b0011, x2=0b1010, should result in x0=0b1001";
    EXPECT_EQ(cpu.read_reg(1), 0b0011) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 0b1010) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 1);
}

TEST_F(EmulatorFixture, eor_negative_flag)
{
    // eor x0, x1, x2
    // x1: (1<<31) - 1
    // x2: ~0
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_eor, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, (1U << 31) - 1);
    cpu.write_reg(2, ~0);
    cpu.set_NZCV(0, 1, 1, 1);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 1U << 31)
        << "\'eor x0, x1, 2\' : where x1=(1<<31)-1, x2=~0, should result in x0=1<<31";
    EXPECT_EQ(cpu.read_reg(1), (1U << 31) - 1)
        << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), ~0) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 1);
}

TEST_F(EmulatorFixture, eor_zero_flag)
{
    // eor x0, x1, x2
    // x1: ~0
    // x2: ~0
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_eor, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, ~0);
    cpu.write_reg(2, ~0);
    cpu.set_NZCV(0, 0, 1, 1);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 0)
        << "\'eor x0, x1, 2\' : where x1=~0, x2=~0, should result in x0=0";
    EXPECT_EQ(cpu.read_reg(1), ~0) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), ~0) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 1);
}