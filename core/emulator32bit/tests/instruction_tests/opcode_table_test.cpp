#include "emulator32bit_test/emulator32bit_test.h"

using Status = Emulator32bit::RunResult::Status;

// Every row of AEMU_OPCODES is wired to a handler, so none of them is treated as an unused opcode.
TEST_F(EmulatorFixture, every_listed_opcode_has_a_handler){
#define AEMU_CHECK_HANDLER(name, opcode)                                                           \
    {                                                                                              \
        const auto result = step(0, word(opcode) << 26);                                           \
        EXPECT_EQ(result.message.find("Bad opcode"), std::string::npos)                            \
            << #name << " (" << opcode << "): " << result.message;                                 \
    }
    AEMU_OPCODES(AEMU_CHECK_HANDLER)
#undef AEMU_CHECK_HANDLER
}

// The opcode constants come from the list, so they are what the rows say.
TEST_F(EmulatorFixture, opcode_constants_come_from_the_list)
{
    EXPECT_EQ(Emulator32bit::_op_special_instructions, 0b000000u);
    EXPECT_EQ(Emulator32bit::_op_add, 0b000001u);
    EXPECT_EQ(Emulator32bit::_op_ldr, 0b100100u);
    EXPECT_EQ(Emulator32bit::_op_adrp, 0b110010u);
    EXPECT_EQ(Emulator32bit::_op_adr, 0b110011u);
}

// Opcodes that are not in the list fault, whatever the rest of the instruction is.
TEST_F(EmulatorFixture, every_opcode_that_is_not_listed_faults)
{
    bool listed[kMaxInstructions] = {};
#define AEMU_MARK_LISTED(name, opcode) listed[opcode] = true;
    AEMU_OPCODES(AEMU_MARK_LISTED)
#undef AEMU_MARK_LISTED

    int unlisted = 0;
    for (word opcode = 0; opcode < kMaxInstructions; opcode++)
    {
        if (listed[opcode])
        {
            continue;
        }
        unlisted++;
        const auto result = step(0, (opcode << 26) | 0x123456);
        EXPECT_EQ(result.status, Status::FAULT) << "opcode " << opcode;
        EXPECT_NE(result.message.find("Bad opcode"), std::string::npos)
            << "opcode " << opcode << ": " << result.message;
    }
    EXPECT_GT(unlisted, 0);
}
