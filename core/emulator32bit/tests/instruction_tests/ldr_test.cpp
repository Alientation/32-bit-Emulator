#include <emulator32bit_test/emulator32bit_test.h>

#include <iostream>

static const byte data[kPageSize] = {9U, 0U, 0U, 0U};

TEST_F(EmulatorFixture, ldr_offset_positive_constant)
{
    reset(1, 0, data, 1, 1);
    // ldr x0, [x1, #3]
    cpu.system_bus->write_word(0,
                               Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 3,
                                                           Emulator32bit::AddrType::ADDR_OFFSET));
    cpu.set_pc(0);
    cpu.write_reg(1, kPageSize - 3);
    cpu.set_NZCV(0, 0, 0, 0);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 9)
        << "\'ldr x0, [x1, #3]\', where x1=PAGE_SIZE-3 : should result in x0=9";
    EXPECT_EQ(cpu.read_reg(1), kPageSize - 3) << "operation should not change operand \'x1\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, ldr_offset_negative_constant)
{
    reset(1, 0, data, 1, 1);
    // ldr x0, [x1, #-3]
    cpu.system_bus->write_word(0,
                               Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, -3,
                                                           Emulator32bit::AddrType::ADDR_OFFSET));
    cpu.set_pc(0);
    cpu.write_reg(1, kPageSize + 3);
    cpu.set_NZCV(0, 0, 0, 0);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 9)
        << "\'ldr x0, [x1, #-3]\', where x1=PAGE_SIZE+3 : should result in x0=9";
    EXPECT_EQ(cpu.read_reg(1), kPageSize + 3) << "operation should not change operand \'x1\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, ldr_offset_reg)
{
    reset(1, 0, data, 1, 1);
    // ldr x0, [x1, x2]
    cpu.system_bus->write_word(
        0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 2, ShiftType::SHIFT_LSL,
                                       0, Emulator32bit::AddrType::ADDR_OFFSET));
    cpu.set_pc(0);
    cpu.write_reg(1, kPageSize - 3);
    cpu.write_reg(2, 3);
    cpu.set_NZCV(0, 0, 0, 0);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 9)
        << "\'ldr x0, [x1, x2]\', where x1=PAGE_SIZE-3, x2=3 : should result in x0=9";
    EXPECT_EQ(cpu.read_reg(1), kPageSize - 3) << "operation should not change operand \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 3) << "operation should not change operand \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, ldr_offset_reg_lsl)
{
    reset(1, 0, data, 1, 1);
    // ldr x0, [x1, x2]
    cpu.system_bus->write_word(
        0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 2, ShiftType::SHIFT_LSL,
                                       1, Emulator32bit::AddrType::ADDR_OFFSET));
    cpu.set_pc(0);
    cpu.write_reg(1, kPageSize - 4);
    cpu.write_reg(2, 2);
    cpu.set_NZCV(0, 0, 0, 0);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 9)
        << "\'ldr x0, [x1, x2, LSL #1]\', where x1=PAGE_SIZE-4, x2=2 : should result in x0=9";
    EXPECT_EQ(cpu.read_reg(1), kPageSize - 4) << "operation should not change operand \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 2) << "operation should not change operand \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, ldr_offset_reg_lsr)
{
    reset(1, 0, data, 1, 1);
    // ldr x0, [x1, x2]
    cpu.system_bus->write_word(
        0, Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 2, ShiftType::SHIFT_LSR,
                                       1, Emulator32bit::AddrType::ADDR_OFFSET));
    cpu.set_pc(0);
    cpu.write_reg(1, kPageSize - 4);
    cpu.write_reg(2, 8);
    cpu.set_NZCV(0, 0, 0, 0);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 9)
        << "\'ldr x0, [x1, x2, LSR #1]\', where x1=PAGE_SIZE-4, x2=8 : should result in x0=9";
    EXPECT_EQ(cpu.read_reg(1), kPageSize - 4) << "operation should not change operand \'x1\'";
    EXPECT_EQ(cpu.read_reg(2), 8) << "operation should not change operand \'x2\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, ldr_pre_indexed)
{
    reset(1, 0, data, 1, 1);
    // ldr x0, [x1, #3]!
    cpu.system_bus->write_word(0,
                               Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 3,
                                                           Emulator32bit::AddrType::ADDR_PRE_INC));
    cpu.set_pc(0);
    cpu.write_reg(1, kPageSize - 3);
    cpu.set_NZCV(0, 0, 0, 0);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 9)
        << "\'ldr x0, [x1, #3]!\', where x1=PAGESIZE - 3 : should result in x0=9";
    EXPECT_EQ(cpu.read_reg(1), kPageSize) << "operation should preincrement \'x1\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, ldr_post_indexed)
{
    reset(1, 0, data, 1, 1);
    // ldr x0, [x1, #3]!
    cpu.system_bus->write_word(0,
                               Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 3,
                                                           Emulator32bit::AddrType::ADDR_POST_INC));
    cpu.set_pc(0);
    cpu.write_reg(1, kPageSize);
    cpu.set_NZCV(0, 0, 0, 0);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 9)
        << "\'ldr x0, [x1], #3\', where x1=PAGE_SIZE : should result in x0=9";
    EXPECT_EQ(cpu.read_reg(1), 3 + kPageSize) << "operation should postincrement \'x1\'";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}