#include <assembler/relocation.h>
#include <emulator32bit/emulator32bit.h>
#include <gtest/gtest.h>
#include <util/logger.h>

namespace
{

using Type = ObjectFile::RelocationEntry::Type;

/// What adrp computes at `pc`, the same way Emulator32bit::_adrp does.
word adrp_result(word instr, word pc)
{
    signed int simm21 = bitfield_unsigned(instr, 0, 20);
    if (test_bit(instr, kInstructionUpdateFlagBit))
    {
        simm21 -= (1 << 20);
    }
    return mask_0(pc, 0, 12) + (simm21 << 12);
}

/// Errors are thrown instead of ending the test binary, and are not logged.
struct ThrowOnFatal
{
    aemu::log::ScopedLevel quiet{aemu::log::Level::Off};
    aemu::log::ScopedFatalAction action{aemu::log::FatalAction::Throw};
};

} // namespace

TEST(Relocation, branch_offset_is_relative_to_the_branch_itself)
{
    const word b = Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::NE, 0);

    EXPECT_EQ(apply_relocation(Type::R_EMU32_B_OFFSET22, b, 8, 0x20),
              Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::NE, 6));
    EXPECT_EQ(apply_relocation(Type::R_EMU32_B_OFFSET22, b, 0x20, 8),
              Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::NE, -6));
    EXPECT_EQ(apply_relocation(Type::R_EMU32_B_OFFSET22, b, 0x400, 0x400),
              Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::NE, 0));
}

TEST(Relocation, branch_keeps_the_opcode_and_condition)
{
    const word bl = Emulator32bit::asm_format_b1(Emulator32bit::_op_bl, ConditionCode::LT, 0);
    const word patched = apply_relocation(Type::R_EMU32_B_OFFSET22, bl, 0, 0x100);
    EXPECT_EQ(bitfield_unsigned(patched, 26, 6), Emulator32bit::_op_bl);
    EXPECT_EQ(bitfield_unsigned(patched, 22, 4), word(ConditionCode::LT));
}

TEST(Relocation, branch_reaches_the_ends_of_the_22_bit_range)
{
    const word b = Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, 0);
    const word far_forward = (1u << 23) - 4; // +2^21 - 1 words
    const word far_backward = (1u << 23);    // from here, address 0 is -2^21 words away

    EXPECT_EQ(apply_relocation(Type::R_EMU32_B_OFFSET22, b, 0, far_forward),
              Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, (1 << 21) - 1));
    EXPECT_EQ(apply_relocation(Type::R_EMU32_B_OFFSET22, b, far_backward, 0),
              Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, -(1 << 21)));
}

TEST(Relocation, branch_out_of_range_is_an_error)
{
    ThrowOnFatal guard;
    const word b = Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, 0);

    EXPECT_THROW(apply_relocation(Type::R_EMU32_B_OFFSET22, b, 0, 1u << 23), aemu::log::FatalError);
    EXPECT_THROW(apply_relocation(Type::R_EMU32_B_OFFSET22, b, (1u << 23) + 4, 0),
                 aemu::log::FatalError);
}

TEST(Relocation, branch_to_a_misaligned_target_is_an_error)
{
    ThrowOnFatal guard;
    const word b = Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, 0);
    EXPECT_THROW(apply_relocation(Type::R_EMU32_B_OFFSET22, b, 0, 6), aemu::log::FatalError);
}

TEST(Relocation, lo12_fills_the_low_twelve_bits_of_the_address)
{
    const word add = Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, 1, 2, 0);
    EXPECT_EQ(apply_relocation(Type::R_EMU32_O_LO12, add, 0x40, 0x12ABC),
              Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, 1, 2, 0xABC));
}

TEST(Relocation, adrp_reaches_a_page_after_the_instruction)
{
    const word adrp = Emulator32bit::asm_format_m1(Emulator32bit::_op_adrp, 3, 0);
    const word patched = apply_relocation(Type::R_EMU32_ADRP_HI20, adrp, 0x10, 0x5000);
    EXPECT_EQ(adrp_result(patched, 0x10), 0x5000u);
    EXPECT_EQ(bitfield_unsigned(patched, 20, 5), 3u) << "the destination register is kept";
}

TEST(Relocation, adrp_reaches_a_page_before_the_instruction)
{
    const word adrp = Emulator32bit::asm_format_m1(Emulator32bit::_op_adrp, 3, 0);
    const word patched = apply_relocation(Type::R_EMU32_ADRP_HI20, adrp, 0x3004, 0x1000);
    EXPECT_EQ(adrp_result(patched, 0x3004), 0x1000u);
}

TEST(Relocation, adrp_in_the_same_page)
{
    const word adrp = Emulator32bit::asm_format_m1(Emulator32bit::_op_adrp, 3, 0);
    const word patched = apply_relocation(Type::R_EMU32_ADRP_HI20, adrp, 0x2010, 0x2FFC);
    EXPECT_EQ(adrp_result(patched, 0x2010), 0x2000u);
}

TEST(Relocation, mov_halves_split_the_address_at_bit_19)
{
    const word mov = Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, false, 1, 0);
    const word address = 0xCAFE1234;

    EXPECT_EQ(apply_relocation(Type::R_EMU32_MOV_LO19, mov, 0, address),
              Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, false, 1, address & 0x7FFFF));
    EXPECT_EQ(apply_relocation(Type::R_EMU32_MOV_HI13, mov, 0, address),
              Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, false, 1, address >> 19));
}

TEST(Relocation, an_undefined_relocation_type_is_an_error)
{
    ThrowOnFatal guard;
    EXPECT_THROW(apply_relocation(Type::UNDEFINED, 0, 0, 0), aemu::log::FatalError);
}
