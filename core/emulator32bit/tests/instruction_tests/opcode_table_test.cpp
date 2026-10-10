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
    EXPECT_EQ(Emulator32bit::_op_fop1, 0b001001u);
    EXPECT_EQ(Emulator32bit::_op_fop2, 0b001010u);
    EXPECT_EQ(Emulator32bit::_op_fcmp, 0b001011u);
    EXPECT_EQ(Emulator32bit::_op_shift, 0b010000u);
    EXPECT_EQ(Emulator32bit::_op_mov, 0b010001u);
    EXPECT_EQ(Emulator32bit::_op_ldr, 0b010011u);
    EXPECT_EQ(Emulator32bit::_op_adrp, 0b100000u);
    EXPECT_EQ(Emulator32bit::_op_adr, 0b100001u);
}

// The listed opcodes are one range from 0, so that every unused opcode is in the range after it.
// The aliases (cmp...) and the variants of one opcode (lsl...) have no row of their own, so nothing
// is left in a hole.
TEST(OpcodeTable, the_unused_opcodes_are_one_range_at_the_end)
{
    bool listed[kMaxInstructions] = {};
#define AEMU_MARK_LISTED_HERE(name, opcode) listed[opcode] = true;
    AEMU_OPCODES(AEMU_MARK_LISTED_HERE)
#undef AEMU_MARK_LISTED_HERE

    word first_unused = 0;
    while (first_unused < kMaxInstructions && listed[first_unused]) first_unused++;
    EXPECT_EQ(first_unused, 0b100011u);
    for (word opcode = first_unused; opcode < kMaxInstructions; opcode++)
    {
        EXPECT_FALSE(listed[opcode]) << "opcode " << opcode;
    }
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

// lsl/lsr/asr/ror are one opcode, `shift`, with the type in bits 7-8.
TEST_F(EmulatorFixture, the_four_shifts_are_one_opcode_with_the_type_in_the_instruction)
{
    const struct
    {
        ShiftType type;
        const char *text;
    } kinds[] = {{ShiftType::SHIFT_LSL, "lsl x1, x2, 3"},
                 {ShiftType::SHIFT_LSR, "lsr x1, x2, 3"},
                 {ShiftType::SHIFT_ASR, "asr x1, x2, 3"},
                 {ShiftType::SHIFT_ROR, "ror x1, x2, 3"}};
    for (const auto &kind : kinds)
    {
        const word instr = Emulator32bit::asm_format_o1(kind.type, 1, 2, true, 0, 3);
        EXPECT_EQ(instr >> 26, Emulator32bit::_op_shift) << kind.text;
        EXPECT_EQ((instr >> 7) & 0b11, word(kind.type)) << kind.text;
        EXPECT_EQ(Emulator32bit::disassemble_instr(instr), kind.text);
    }
}

// umull/smull are one opcode, `mull`, with the signed bit in bit 0.
TEST_F(EmulatorFixture, the_long_multiplies_are_one_opcode_with_a_signed_bit)
{
    const word u = Emulator32bit::asm_format_o2(false, true, 1, 2, 3, 4);
    const word s = Emulator32bit::asm_format_o2(true, true, 1, 2, 3, 4);
    EXPECT_EQ(u >> 26, Emulator32bit::_op_mull);
    EXPECT_EQ(s >> 26, Emulator32bit::_op_mull);
    EXPECT_EQ(s ^ u, 1u << kLongMulSignedBit);
    EXPECT_EQ(Emulator32bit::disassemble_instr(u), "umulls x1, x2, x3, x4");
    EXPECT_EQ(Emulator32bit::disassemble_instr(s), "smulls x1, x2, x3, x4");
}

// bx/blx are one opcode, `bx`, with the link bit in bit 0.
TEST_F(EmulatorFixture, bx_and_blx_are_one_opcode_with_a_link_bit)
{
    const word bx = Emulator32bit::asm_format_b2(ConditionCode::AL, 5);
    const word blx = Emulator32bit::asm_format_b2(ConditionCode::AL, 5, true);
    EXPECT_EQ(bx >> 26, Emulator32bit::_op_bx);
    EXPECT_EQ(blx >> 26, Emulator32bit::_op_bx);
    EXPECT_EQ(blx ^ bx, 1u << kBranchLinkBit);
    EXPECT_EQ(Emulator32bit::disassemble_instr(bx), "bx x5");
    EXPECT_EQ(Emulator32bit::disassemble_instr(blx), "blx x5");
    EXPECT_EQ(Emulator32bit::disassemble_instr(Emulator32bit::asm_format_b2(ConditionCode::AL, 29)),
              "ret");
    EXPECT_EQ(Emulator32bit::disassemble_instr(
                  Emulator32bit::asm_format_b2(ConditionCode::AL, 29, true)),
              "blx lr")
        << "only bx x29 is a ret";
}

// A call through x29 jumps to the old x29: the target is read before the return address is written.
TEST_F(EmulatorFixture, blx_reads_its_target_before_it_writes_the_link_register)
{
    cpu.write_reg(U8(29), 0x100);
    execute(Emulator32bit::asm_format_b2(ConditionCode::AL, 29, true));
    EXPECT_EQ(cpu.get_pc(), 0x100u);
    EXPECT_EQ(cpu.read_reg(U8(29)), 4u);
}

TEST_F(EmulatorFixture, bx_does_not_touch_the_link_register)
{
    cpu.write_reg(U8(5), 0x100);
    cpu.write_reg(U8(29), 0x1234);
    execute(Emulator32bit::asm_format_b2(ConditionCode::AL, 5));
    EXPECT_EQ(cpu.get_pc(), 0x100u);
    EXPECT_EQ(cpu.read_reg(U8(29)), 0x1234u);
}

// A condition that is false does neither.
TEST_F(EmulatorFixture, a_blx_whose_condition_is_false_does_nothing)
{
    cpu.set_NZCV(false, false, false, false); // not Z
    cpu.write_reg(U8(5), 0x100);
    cpu.write_reg(U8(29), 0x1234);
    execute(Emulator32bit::asm_format_b2(ConditionCode::EQ, 5, true));
    EXPECT_EQ(cpu.get_pc(), 4u);
    EXPECT_EQ(cpu.read_reg(U8(29)), 0x1234u);
}
