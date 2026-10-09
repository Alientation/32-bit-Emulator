// Tests for adrp and adr: the address of something near the instruction. Both have the format of
// M1 (a register and a signed 21 bit number: the 20 bits of the immediate and the sign bit).
// adrp counts 4 KiB pages from the page of the instruction, adr counts bytes from the instruction.

#include "emulator32bit_test/emulator32bit_test.h"

namespace
{

constexpr U8 kXd = 5;

/// The 21 bit signed number as the instruction holds it.
word adr(const U8 xd, const int distance)
{
    word instr = Emulator32bit::asm_format_m1(Emulator32bit::_op_adr, xd,
                                              int(bitfield_unsigned(distance, 0, 20)));
    if (distance < 0) instr = set_bit(instr, kInstructionUpdateFlagBit, 1);
    return instr;
}

word adrp(const U8 xd, const int pages)
{
    word instr = Emulator32bit::asm_format_m1(Emulator32bit::_op_adrp, xd,
                                              int(bitfield_unsigned(pages, 0, 20)));
    if (pages < 0) instr = set_bit(instr, kInstructionUpdateFlagBit, 1);
    return instr;
}

} // namespace

TEST_F(EmulatorFixture, adr_is_the_address_of_the_instruction_plus_a_distance_in_bytes)
{
    reset(2, 0, nullptr, 0, 2);

    for (const int distance : {0, 1, 3, 4, 100, 4095, 4096, 0x7FFF})
    {
        execute(adr(kXd, distance));
        EXPECT_EQ(cpu.read_reg(kXd), word(distance)) << "at 0, distance " << distance;
    }

    // Not the address of the next instruction, and not the page.
    cpu.write_reg(kXd, 0);
    ASSERT_EQ(step(0x104, adr(kXd, 12)).status, Emulator32bit::RunResult::Status::LIMIT_REACHED);
    EXPECT_EQ(cpu.read_reg(kXd), 0x104u + 12);
}

TEST_F(EmulatorFixture, adr_goes_backwards_with_the_sign_bit)
{
    reset(2, 0, nullptr, 0, 2);

    ASSERT_EQ(step(0x1000, adr(kXd, -4)).status, Emulator32bit::RunResult::Status::LIMIT_REACHED);
    EXPECT_EQ(cpu.read_reg(kXd), 0xFFCu);

    ASSERT_EQ(step(0x1004, adr(kXd, -0x1004)).status,
              Emulator32bit::RunResult::Status::LIMIT_REACHED);
    EXPECT_EQ(cpu.read_reg(kXd), 0u);
}

TEST_F(EmulatorFixture, adr_reaches_a_megabyte_in_both_directions)
{
    reset(4, 0, nullptr, 0, 8);
    cpu.mmu->set_enabled(false);

    constexpr int kFar = (1 << 20) - 1;
    // The result wraps around the address space like any other address arithmetic.
    cpu.memory.write_word(0x2000, adr(kXd, kFar));
    cpu.set_pc(0x2000);
    ASSERT_EQ(cpu.run(1).status, Emulator32bit::RunResult::Status::LIMIT_REACHED);
    EXPECT_EQ(cpu.read_reg(kXd), 0x2000u + kFar);

    cpu.memory.write_word(0x2000, adr(kXd, -(1 << 20)));
    cpu.set_pc(0x2000);
    ASSERT_EQ(cpu.run(1).status, Emulator32bit::RunResult::Status::LIMIT_REACHED);
    EXPECT_EQ(cpu.read_reg(kXd), word(0x2000 - (1 << 20)));
}

TEST_F(EmulatorFixture, adr_does_not_change_the_flags_or_other_registers)
{
    set_flags({.n = true, .z = false, .c = true, .v = true});
    cpu.write_reg(U8(1), 0x1234);

    execute(adr(kXd, 40));
    EXPECT_EQ(cpu.read_reg(U8(1)), 0x1234u);
    EXPECT_TRUE(cpu.get_flag(kNFlagBit));
    EXPECT_FALSE(cpu.get_flag(kZFlagBit));
    EXPECT_TRUE(cpu.get_flag(kCFlagBit));
    EXPECT_TRUE(cpu.get_flag(kVFlagBit));
}

TEST_F(EmulatorFixture, adr_to_the_zero_register_discards_the_result)
{
    execute(adr(U8(Register::XZR), 8));
    EXPECT_EQ(cpu.read_reg(U8(Register::XZR)), 0u);
}

TEST_F(EmulatorFixture, adrp_is_the_page_of_the_instruction_plus_pages)
{
    reset(2, 0, nullptr, 0, 2);

    ASSERT_EQ(step(0x104, adrp(kXd, 1)).status, Emulator32bit::RunResult::Status::LIMIT_REACHED);
    EXPECT_EQ(cpu.read_reg(kXd), 0x1000u) << "the page of 0x104 is 0";

    ASSERT_EQ(step(0x1004, adrp(kXd, -1)).status, Emulator32bit::RunResult::Status::LIMIT_REACHED);
    EXPECT_EQ(cpu.read_reg(kXd), 0u);
}

TEST_F(EmulatorFixture, adr_and_adrp_are_disassembled)
{
    EXPECT_EQ(Emulator32bit::disassemble_instr(adr(3, 40)), "adr x3, 40");
    EXPECT_EQ(Emulator32bit::disassemble_instr(adr(3, -8)), "adr x3, -8");
    EXPECT_EQ(Emulator32bit::disassemble_instr(adrp(4, 2)), "adrp x4, 2");
}
