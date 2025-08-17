#include <emulator32bit_test/emulator32bit_test.h>

TEST_F (EmulatorFixture, rsc_register_rsc_immediate)
{
    cpu = new (cpu) Emulator32bit (1, 0, {}, 0, 1);
    // rsc x0, x1, #11
    // x1: 9
    // carry: 1
    cpu->system_bus->write_word (
        0, Emulator32bit::asm_format_o (Emulator32bit::_op_rsc, false, 0, 1, 11));
    cpu->set_pc (0);
    cpu->write_reg (1, 9);
    cpu->set_NZCV (0, 0, 1, 0);

    cpu->run (1);

    EXPECT_EQ (cpu->read_reg (0), 1)
        << "\'rsc x0, x1 #11\' : where x1=9, c=1, should result in x0=1";
    EXPECT_EQ (cpu->read_reg (1), 9) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ (cpu->get_flag (kNFlagBit), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ (cpu->get_flag (kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ (cpu->get_flag (kCFlagBit), 1) << "operation should not alter C flag";
    EXPECT_EQ (cpu->get_flag (kVFlagBit), 0) << "operation should not cause V flag to be set";
}

TEST_F (EmulatorFixture, rsc_register_rsc_register)
{
    cpu = new (cpu) Emulator32bit (1, 0, {}, 0, 1);
    // rsc x0, x1, x2
    // x1: 9
    // x2: 11
    // carry: 1
    cpu->system_bus->write_word (0,
                                 Emulator32bit::asm_format_o (Emulator32bit::_op_rsc, false, 0, 1,
                                                              2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc (0);
    cpu->write_reg (1, 9);
    cpu->write_reg (2, 11);
    cpu->set_NZCV (0, 0, 1, 0);

    cpu->run (1);

    EXPECT_EQ (cpu->read_reg (0), 1)
        << "\'rsc x0, x1, x2\' : where x1=9, x2=11, c=1, should result in x0=1";
    EXPECT_EQ (cpu->read_reg (1), 9) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ (cpu->read_reg (2), 11) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ (cpu->get_flag (kNFlagBit), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ (cpu->get_flag (kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ (cpu->get_flag (kCFlagBit), 1) << "operation should not alter C flag";
    EXPECT_EQ (cpu->get_flag (kVFlagBit), 0) << "operation should not cause V flag to be set";
}

TEST_F (EmulatorFixture, rsc_negative_flag)
{
    cpu = new (cpu) Emulator32bit (1, 0, {}, 0, 1);
    // rsc x0, x1, x2
    // x1: 2
    // x2: 2
    // carry: 1
    cpu->system_bus->write_word (0, Emulator32bit::asm_format_o (Emulator32bit::_op_rsc, true, 0, 1,
                                                                 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc (0);
    cpu->write_reg (1, 2);
    cpu->write_reg (2, 2);
    cpu->set_NZCV (0, 0, 1, 0);

    cpu->run (1);

    EXPECT_EQ (cpu->read_reg (0), -1)
        << "\'rsc x0, x1, x2\' : where x1=2, x2=2, c=1, should result in x0=-1";
    EXPECT_EQ (cpu->read_reg (1), 2) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ (cpu->read_reg (2), 2) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ (cpu->get_flag (kNFlagBit), 1) << "N flag should be set";
    EXPECT_EQ (cpu->get_flag (kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ (cpu->get_flag (kCFlagBit), 1) << "C flag should be set";
    EXPECT_EQ (cpu->get_flag (kVFlagBit), 0) << "operation should not cause V flag to be set";
}

TEST_F (EmulatorFixture, rsc_zero_flag)
{
    cpu = new (cpu) Emulator32bit (1, 0, {}, 0, 1);
    // rsc x0, x1, x2
    // x1: 1
    // x2: 2
    // carry: 1
    cpu->system_bus->write_word (0, Emulator32bit::asm_format_o (Emulator32bit::_op_rsc, true, 0, 1,
                                                                 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc (0);
    cpu->write_reg (1, 1);
    cpu->write_reg (2, 2);
    cpu->set_NZCV (0, 0, 1, 0);

    cpu->run (1);

    EXPECT_EQ (cpu->read_reg (0), 0)
        << "\'rsc x0, x1, x2\' : where x1=1, x2=2, c=1, should result in x0=0";
    EXPECT_EQ (cpu->read_reg (1), 1) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ (cpu->read_reg (2), 2) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ (cpu->get_flag (kNFlagBit), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ (cpu->get_flag (kZFlagBit), 1) << "Z flag should be set";
    EXPECT_EQ (cpu->get_flag (kCFlagBit), 0) << "C flag should be set";
    EXPECT_EQ (cpu->get_flag (kVFlagBit), 0) << "operation should not cause V flag to be set";
}

TEST_F (EmulatorFixture, rsc_carry_flag_1)
{
    cpu = new (cpu) Emulator32bit (1, 0, {}, 0, 1);
    // rsc x0, x1, x2
    // x1: -2
    // x2: -2
    // carry: 1
    cpu->system_bus->write_word (0, Emulator32bit::asm_format_o (Emulator32bit::_op_rsc, true, 0, 1,
                                                                 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc (0);
    cpu->write_reg (1, -2);
    cpu->write_reg (2, -2);
    cpu->set_NZCV (0, 0, 1, 0);

    cpu->run (1);

    EXPECT_EQ (cpu->read_reg (0), -1)
        << "\'rsc x0, x1, x2\' : where x1=-2, x2=-2, c=1, should result in x0=-1";
    EXPECT_EQ (cpu->read_reg (1), -2) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ (cpu->read_reg (2), -2) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ (cpu->get_flag (kNFlagBit), 1) << "N flag should be set";
    EXPECT_EQ (cpu->get_flag (kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ (cpu->get_flag (kCFlagBit), 1) << "C flag should be set";
    EXPECT_EQ (cpu->get_flag (kVFlagBit), 0) << "operation should not cause V flag to be set";
}

TEST_F (EmulatorFixture, rsc_carry_flag_2)
{
    cpu = new (cpu) Emulator32bit (1, 0, {}, 0, 1);
    // rsc x0, x1, x2
    // x1: -2
    // x2: 2
    // carry: 1
    cpu->system_bus->write_word (0, Emulator32bit::asm_format_o (Emulator32bit::_op_rsc, true, 0, 1,
                                                                 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc (0);
    cpu->write_reg (1, -2);
    cpu->write_reg (2, 2);
    cpu->set_NZCV (0, 0, 1, 0);

    cpu->run (1);

    EXPECT_EQ (cpu->read_reg (0), 3)
        << "\'rsc x0, x1, x2\' : where x1=-2, x2=2, c=1, should result in x0=3";
    EXPECT_EQ (cpu->read_reg (1), -2) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ (cpu->read_reg (2), 2) << "operation should not alter operand register \'x2\'";
    EXPECT_EQ (cpu->get_flag (kNFlagBit), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ (cpu->get_flag (kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ (cpu->get_flag (kCFlagBit), 1) << "C flag should be set";
    EXPECT_EQ (cpu->get_flag (kVFlagBit), 0) << "operation should not cause V flag to be set";
}

TEST_F (EmulatorFixture, rsc_overflow_flag__positive_to_negative)
{
    cpu = new (cpu) Emulator32bit (1, 0, {}, 0, 1);
    // rsc x0, x1, x2
    // x1: -2
    // x2: (1<<31)-1
    // carry: 1
    cpu->system_bus->write_word (0, Emulator32bit::asm_format_o (Emulator32bit::_op_rsc, true, 0, 1,
                                                                 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc (0);
    cpu->write_reg (1, -2);
    cpu->write_reg (2, (1U << 31) - 1);
    cpu->set_NZCV (0, 0, 1, 0);

    cpu->run (1);

    EXPECT_EQ (cpu->read_reg (0), 1U << 31)
        << "\'rsc x0, x1, x2\' : where x1=-2, x2=(1<<31)-1, c=1, should result in x0=1<<31";
    EXPECT_EQ (cpu->read_reg (1), -2) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ (cpu->read_reg (2), (1U << 31) - 1)
        << "operation should not alter operand register \'x2\'";
    EXPECT_EQ (cpu->get_flag (kNFlagBit), 1) << "N flag should be set";
    EXPECT_EQ (cpu->get_flag (kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ (cpu->get_flag (kCFlagBit), 1) << "C flag should be set";
    EXPECT_EQ (cpu->get_flag (kVFlagBit), 1) << "V flag should be set";
}

TEST_F (EmulatorFixture, rsc_overflow_flag__negative_to_positive)
{
    cpu = new (cpu) Emulator32bit (1, 0, {}, 0, 1);
    // rsc x0, x1, x2
    // x1: 0
    // x2: 1<<31
    // carry: 1
    cpu->system_bus->write_word (0, Emulator32bit::asm_format_o (Emulator32bit::_op_rsc, true, 0, 1,
                                                                 2, Emulator32bit::SHIFT_LSL, 0));
    cpu->set_pc (0);
    cpu->write_reg (1, 0);
    cpu->write_reg (2, 1U << 31);
    cpu->set_NZCV (0, 0, 1, 0);

    cpu->run (1);

    EXPECT_EQ (cpu->read_reg (0), (1U << 31) - 1)
        << "\'rsc x0, x1, x2\' : where x1=0, x2=1<<31, c=1, should result in x0=(1<<31)-1";
    EXPECT_EQ (cpu->read_reg (1), 0) << "operation should not alter operand register \'x1\'";
    EXPECT_EQ (cpu->read_reg (2), (1U << 31))
        << "operation should not alter operand register \'x2\'";
    EXPECT_EQ (cpu->get_flag (kNFlagBit), 0) << "operation should not cause N flag to be set";
    EXPECT_EQ (cpu->get_flag (kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ (cpu->get_flag (kCFlagBit), 0) << "operation should not cause C flag to be set";
    EXPECT_EQ (cpu->get_flag (kVFlagBit), 1) << "V flag should be set";
}