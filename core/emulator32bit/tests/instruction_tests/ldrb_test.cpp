#include <emulator32bit_test/emulator32bit_test.h>

#include <iostream>

static const byte data[kPageSize] = {9U, 1U, 2U, 3U};

TEST_F (EmulatorFixture, ldrb_offset)
{
    cpu = new (cpu) Emulator32bit (1, 0, data, 1, 1);
    // ldrb x0, [x1, #3]
    cpu->system_bus->write_word (
        0, Emulator32bit::asm_format_m (Emulator32bit::_op_ldrb, false, 0, 1, 3,
                                        Emulator32bit::AddrType::ADDR_OFFSET));
    cpu->set_pc (0);
    cpu->write_reg (1, kPageSize - 3);
    cpu->set_NZCV (0, 0, 0, 0);

    cpu->run (1);

    EXPECT_EQ (cpu->read_reg (0), 9)
        << "\'ldrb x0, [x1, #3]\', where x1=PAGESIZE - 3 : should result in x0=9";
    EXPECT_EQ (cpu->read_reg (1), kPageSize - 3) << "operation should not change operand \'x1\'";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kNFlagBit), 0)
        << "operation should not cause N flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kZFlagBit), 0)
        << "operation should not cause Z flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kCFlagBit), 0)
        << "operation should not cause C flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kVFlagBit), 0)
        << "operation should not cause V flag to be set";
}

TEST_F (EmulatorFixture, ldrb_pre_indexed)
{
    cpu = new (cpu) Emulator32bit (1, 0, data, 1, 1);
    // ldrb x0, [x1, #3]!
    cpu->system_bus->write_word (
        0, Emulator32bit::asm_format_m (Emulator32bit::_op_ldrb, false, 0, 1, 3,
                                        Emulator32bit::AddrType::ADDR_PRE_INC));
    cpu->set_pc (0);
    cpu->write_reg (1, kPageSize - 3);
    cpu->set_NZCV (0, 0, 0, 0);

    cpu->run (1);

    EXPECT_EQ (cpu->read_reg (0), 9)
        << "\'ldrb x0, [x1, #3]!\', where x1=PAGESIZE - 3 : should result in x0=9";
    EXPECT_EQ (cpu->read_reg (1), kPageSize) << "operation should preincrement \'x1\'";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kNFlagBit), 0)
        << "operation should not cause N flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kZFlagBit), 0)
        << "operation should not cause Z flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kCFlagBit), 0)
        << "operation should not cause C flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kVFlagBit), 0)
        << "operation should not cause V flag to be set";
}

TEST_F (EmulatorFixture, ldrb_post_indexed)
{
    cpu = new (cpu) Emulator32bit (1, 0, data, 1, 1);
    // ldrb x0, [x1, #3]!
    cpu->system_bus->write_word (
        0, Emulator32bit::asm_format_m (Emulator32bit::_op_ldrb, false, 0, 1, 3,
                                        Emulator32bit::AddrType::ADDR_POST_INC));
    cpu->set_pc (0);
    cpu->write_reg (1, kPageSize);
    cpu->set_NZCV (0, 0, 0, 0);

    cpu->run (1);

    EXPECT_EQ (cpu->read_reg (0), 9)
        << "\'ldrb x0, [x1], #3\', where x1=PAGESIZE : should result in x0=9";
    EXPECT_EQ (cpu->read_reg (1), 3 + kPageSize) << "operation should postincrement \'x1\'";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kNFlagBit), 0)
        << "operation should not cause N flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kZFlagBit), 0)
        << "operation should not cause Z flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kCFlagBit), 0)
        << "operation should not cause C flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kVFlagBit), 0)
        << "operation should not cause V flag to be set";
}