#include <emulator32bit_test/emulator32bit_test.h>

TEST_F(EmulatorFixture, add_register_add_immediate)
{
    constexpr word regval[] = {~word(0), ~word(0) - 1, 2, 1};
    constexpr word imm14[] = {0, 1, (1 << 14) - 2, (1 << 14) - 1};

    for (U32 testcase = 0; testcase < ARRAY_LEN(regval); ++testcase)
    {
        for (U8 xd = 0; xd < kNumReg; ++xd)
        {
            for (U8 xn = 0; xn < kNumReg; ++xn)
            {
                SCOPED_TRACE("Testcase " + std::to_string(testcase) + " | xd: " + std::to_string(xd)
                             + ", xn: " + std::to_string(xn));

                cpu.reset();
                cpu.write_reg(xd, 0xF00DBEEF);
                cpu.write_reg(xn, regval[testcase]);

                // Handle the case when xn == XZR.
                const word initial_xn = cpu.read_reg(xn);

                step(0, Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, xd, xn,
                                                    imm14[testcase]));

                // If xd == XZR, expect 0.
                const word expected_output =
                    xd == U8(Register::XZR) ? 0 : initial_xn + imm14[testcase];

                EXPECT_EQ(cpu.read_reg(xd), expected_output);

                // Nominally, xn should not change unless xd == xn.
                EXPECT_EQ(cpu.read_reg(xn), xn == xd ? expected_output : initial_xn);

                // No flags should be set by this operation.
                EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
                EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
                EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
                EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
            }
        }
    }
}

TEST_F(EmulatorFixture, add_register_add_register)
{
    // add x0, x1, x2
    // x1: 1
    // x2: 10
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, 0, 1,
                                                              2, ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 1);
    cpu.write_reg(2, 10);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 11)
        << "\'add x0, x1, x2\' : where x1=1, x2=10, should result in x0=11";
    EXPECT_EQ(cpu.read_reg(1), 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 10) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, add_register_add_register_shifted)
{
    // add x0, x1, x2, lsl #3
    // x1: 1
    // x2: 2
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, 0, 1,
                                                              2, ShiftType::SHIFT_LSL, 3));
    cpu.set_pc(0);
    cpu.write_reg(1, 1);
    cpu.write_reg(2, 2);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 17)
        << "\'add x0, x1, x2, lsl #3\' : where x1=1, x2=2, should result in x0=17";
    EXPECT_EQ(cpu.read_reg(1), 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 2) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, add_negative_flag)
{
    // add x0, x1, x2
    // x1: -2
    // x2: 1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_add, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, -2);
    cpu.write_reg(2, 1);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), -1)
        << "\'add x0, x1, x2\' : where x1=-2, x2=-1, should result in x0=-1";
    EXPECT_EQ(cpu.read_reg(1), -2) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 1) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, add_zero_flag)
{
    // add x0, x1, x2
    // x1: 0
    // x2: 0
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_add, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 0);
    cpu.write_reg(2, 0);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 0) << "\'add x0, x1, x2\' : where x1=0, x2=0, should result in x0=0";
    EXPECT_EQ(cpu.read_reg(1), 0) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 0) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, add_carry_flag_1)
{
    // add x0, x1, x2
    // x1: -1
    // x2: -1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_add, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, -1);
    cpu.write_reg(2, -1);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), -2)
        << "\'add x0, x1, x2\' : where x1=-1, x2=-1, should result in x0=-2";
    EXPECT_EQ(cpu.read_reg(1), -1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), -1) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, add_carry_flag_2)
{
    // add x0, x1, x2
    // x1: 0
    // x2: 0
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_add, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, -4);
    cpu.write_reg(2, -4);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), -8)
        << "\'add x0, x1, x2\' : where x1=-4, x2=-4, should result in x0=-8";
    EXPECT_EQ(cpu.read_reg(1), -4) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), -4) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, add_overflow_flag__neg_to_pos)
{
    // add x0, x1, x2
    // x1: 1<<31
    // x2: 1<<31
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_add, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, 1U << 31);
    cpu.write_reg(2, 1U << 31);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 0)
        << "\'add x0, x1, x2\' : where x1=1<<31, x2=1<<31, should result in x0=0";
    EXPECT_EQ(cpu.read_reg(1), 1U << 31) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 1U << 31) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 1);
}

TEST_F(EmulatorFixture, add_overflow_flag__pos_to_neg)
{
    // add x0, x1, x2
    // x1: (1<<31) - 1
    // x2: 1
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o(Emulator32bit::_op_add, true, 0, 1, 2,
                                                              ShiftType::SHIFT_LSL, 0));
    cpu.set_pc(0);
    cpu.write_reg(1, (1U << 31) - 1);
    cpu.write_reg(2, 1);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 1U << 31)
        << "\'add x0, x1, x2\' : where x1=(1<<31)-1, x2=1, should result in x0=1<<31";
    EXPECT_EQ(cpu.read_reg(1), (1U << 31) - 1)
        << "operation should not alter operand register \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 1) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 1);
}