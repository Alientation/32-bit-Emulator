#include <emulator32bit_test/emulator32bit_test.h>

TEST_F(EmulatorFixture, mvn_register_mvn_immediate) {
    cpu = new(cpu) Emulator32bit(1, 0, {}, 0, 1);
    // mvn x0, #0
    cpu->system_bus.write_word(0, Emulator32bit::asm_format_o3(Emulator32bit::_op_mvn, true, 0, 0));
    cpu->set_pc(0);
    cpu->set_NZCV(0, 0, 1, 0);

    cpu->run(1);

    EXPECT_EQ(cpu->read_reg(0), ~0) << "\'mvn x0, #0\' : should result in x0=~0";
    EXPECT_EQ(cpu->get_flag(kNFlagBit), 1) << "operation should cause N flag to be set";
    EXPECT_EQ(cpu->get_flag(kZFlagBit), 0) << "operation should not cause Z flag to be set";
    EXPECT_EQ(cpu->get_flag(kCFlagBit), 1) << "operation should not alter C flag";
    EXPECT_EQ(cpu->get_flag(kVFlagBit), 0) << "operation should not cause V flag to be set";
}