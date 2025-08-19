#include <emulator32bit_test/emulator32bit_test.h>

#include <iostream>

static const byte data[kPageSize] = {0x07, 0x16, 0x25, 0x34};

TEST_F (EmulatorFixture, swp_basic)
{
    cpu = new (cpu) Emulator32bit (1, 0, data, 1, 1);
    // swp x0, x1, [x2]
    cpu->system_bus->write_word (0, Emulator32bit::asm_atomic (0, 1, 2,
                                                               Emulator32bit::kAtomicWidth_word,
                                                               Emulator32bit::kAtomicId_swp));
    cpu->set_pc (0);
    cpu->write_reg (1, 0x76543210);
    cpu->write_reg (2, kPageSize);
    cpu->set_NZCV (0, 0, 0, 0);

    cpu->run (1);

    EXPECT_EQ (cpu->system_bus->read_word (kPageSize), 0x76543210)
        << "\'swp x0, x1, [x2]\' : should result in [x2] stored in x0 and x1 stored in [x2].";
    EXPECT_EQ (cpu->read_reg (0), 0x34251607)
        << "\'x0\' should contain the 4 byte value at address x2 in memory.";
    EXPECT_EQ (cpu->read_reg (1), 0x76543210) << "operation should not change operand \'x1\'";
    EXPECT_EQ (cpu->read_reg (2), kPageSize) << "operation should not change operand \'x2\'";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kNFlagBit), 0)
        << "operation should not cause N flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kZFlagBit), 0)
        << "operation should not cause Z flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kCFlagBit), 0)
        << "operation should not cause C flag to be set";
    EXPECT_EQ (cpu->get_flag (Emulator32bit::kVFlagBit), 0)
        << "operation should not cause V flag to be set";
}