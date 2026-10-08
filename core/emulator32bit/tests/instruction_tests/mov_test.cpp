#include <emulator32bit_test/emulator32bit_test.h>

TEST_F(EmulatorFixture, mov_register_mov_immediate)
{
    // mov x0, #9
    cpu.system_bus->write_word(0,
                               Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, false, 0, 9));
    cpu.set_pc(0);
    cpu.set_NZCV(0, 0, 1, 0);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 9) << "\'mov x0 #9\' : should result in x0=9";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}

TEST_F(EmulatorFixture, mov_zero_flag)
{
    // mov x0, #0
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, true, 0, 0));
    cpu.set_pc(0);
    cpu.set_NZCV(0, 0, 1, 0);

    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 0) << "\'movs x0, #0\' : should result in x0=0";
    EXPECT_EQ(cpu.get_flag(kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(kZFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kCFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(kVFlagBit), 0);
}