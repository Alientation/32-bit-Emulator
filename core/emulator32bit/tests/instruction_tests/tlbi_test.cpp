#include "emulator32bit_test/emulator32bit_test.h"

TEST_F(EmulatorFixture, tlbi_encoding)
{
    const word instr = Emulator32bit::asm_tlbi(3, true, 0x1234);

    EXPECT_EQ(bitfield_unsigned(instr, 26, 6), Emulator32bit::_op_special_instructions);
    EXPECT_EQ(bitfield_unsigned(instr, 22, 4), Emulator32bit::kSpecialOpId_tlbi);
    EXPECT_EQ(bitfield_unsigned(instr, 17, 5), 3u);
    EXPECT_EQ(bitfield_unsigned(instr, 16, 1), 1u);
    EXPECT_EQ(bitfield_unsigned(instr, 0, 16), 0x1234u);
}
