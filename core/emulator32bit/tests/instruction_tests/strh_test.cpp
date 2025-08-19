#include <emulator32bit_test/emulator32bit_test.h>

#include <iostream>

TEST_F (EmulatorFixture, strh_offset)
{
    cpu = new (cpu) Emulator32bit (1, 0, {}, 0, 1);
    // strh x0, [x1, #3]
    cpu->system_bus->write_word (
        0, Emulator32bit::asm_format_m (Emulator32bit::_op_strh, false, 0, 1, 3,
                                        Emulator32bit::AddrType::ADDR_OFFSET));
    cpu->set_pc (0);
    cpu->write_reg (0, 9);
    cpu->write_reg (1, 5);
    cpu->set_NZCV (0, 0, 0, 0);

    cpu->run (1);

    EXPECT_EQ (cpu->system_bus->read_hword (8), 9)
        << "\'strh x0, [x1, #3]\', where x0=9, x1=5 : should result in 2 byte value of 9 to be "
           "written at address 8";
    EXPECT_EQ (cpu->read_reg (0), 9) << "operation should not change operand \'x0\'";
    EXPECT_EQ (cpu->read_reg (1), 5) << "operation should not change operand \'x1\'";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kNFlagBit), 0)
        << "operation should not cause N flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kZFlagBit), 0)
        << "operation should not cause Z flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kCFlagBit), 0)
        << "operation should not cause C flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kVFlagBit), 0)
        << "operation should not cause V flag to be set";
}

TEST_F (EmulatorFixture, strh_pre_indexed)
{
    cpu = new (cpu) Emulator32bit (1, 0, {}, 0, 1);
    // strh x0, [x1, #3]!
    cpu->system_bus->write_word (
        0, Emulator32bit::asm_format_m (Emulator32bit::_op_strh, false, 0, 1, 3,
                                        Emulator32bit::AddrType::ADDR_PRE_INC));
    cpu->set_pc (0);
    cpu->write_reg (0, 9);
    cpu->write_reg (1, 5);
    cpu->set_NZCV (0, 0, 0, 0);

    cpu->run (1);

    EXPECT_EQ (cpu->system_bus->read_hword (8), 9)
        << "\'strh x0, [x1, #3]!\', where x0=9, x1=5 : should result in 2 byte value of 9 to be "
           "written at address 8";
    EXPECT_EQ (cpu->read_reg (0), 9) << "operation should not change operand \'x0\'";
    EXPECT_EQ (cpu->read_reg (1), 8) << "operation should preincrement \'x1\'";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kNFlagBit), 0)
        << "operation should not cause N flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kZFlagBit), 0)
        << "operation should not cause Z flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kCFlagBit), 0)
        << "operation should not cause C flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kVFlagBit), 0)
        << "operation should not cause V flag to be set";
}

TEST_F (EmulatorFixture, strh_post_indexed)
{
    cpu = new (cpu) Emulator32bit (1, 0, {}, 0, 1);
    // strh x0, [x1], #3
    cpu->system_bus->write_word (
        0, Emulator32bit::asm_format_m (Emulator32bit::_op_strh, false, 0, 1, 3,
                                        Emulator32bit::AddrType::ADDR_POST_INC));
    cpu->set_pc (0);
    cpu->write_reg (0, 9);
    cpu->write_reg (1, 8);
    cpu->set_NZCV (0, 0, 0, 0);

    cpu->run (1);

    EXPECT_EQ (cpu->system_bus->read_hword (8), 9)
        << "\'strh x0, [x1], #3\', where x0=9, x1=8 : should result in 2 byte value of 9 to be "
           "written at address 8";
    EXPECT_EQ (cpu->read_reg (0), 9) << "operation should not change operand \'x0\'";
    EXPECT_EQ (cpu->read_reg (1), 11) << "operation should postincrement \'x1\'";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kNFlagBit), 0)
        << "operation should not cause N flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kZFlagBit), 0)
        << "operation should not cause Z flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kCFlagBit), 0)
        << "operation should not cause C flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kVFlagBit), 0)
        << "operation should not cause V flag to be set";
}