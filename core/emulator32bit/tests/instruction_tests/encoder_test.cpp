// The functions that build instructions refuse a value that does not fit its bits, instead of
// letting it spill into the fields next to it.

#include "emulator32bit_test/emulator32bit_test.h"
#include "util/logger.h"

namespace
{

class Encoder : public ::testing::Test
{
  protected:
    aemu::log::ScopedLevel m_quiet{aemu::log::Level::Off};
    aemu::log::ScopedFatalAction m_throw{aemu::log::FatalAction::Throw};
};

using AddrType = Emulator32bit::AddrType;

} // namespace

TEST_F(Encoder, values_at_the_edge_of_their_field_fit)
{
    EXPECT_NO_THROW(Emulator32bit::asm_format_o(Emulator32bit::_op_add, true, 31, 31, 0x3FFF));
    EXPECT_NO_THROW(Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, false, 31, 0x7FFFF));
    EXPECT_NO_THROW(Emulator32bit::asm_format_m1(Emulator32bit::_op_adrp, 31, 0xFFFFF));
    EXPECT_NO_THROW(Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 1, 2, -2048,
                                                AddrType::ADDR_OFFSET));
    EXPECT_NO_THROW(Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, -1));
}

TEST_F(Encoder, an_immediate_that_is_too_big_is_an_error)
{
    EXPECT_THROW(Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, 0, 0, 0x4000),
                 aemu::log::FatalError);
    EXPECT_THROW(Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, false, 0, 0x80000),
                 aemu::log::FatalError);
    EXPECT_THROW(Emulator32bit::asm_format_m1(Emulator32bit::_op_adrp, 0, 0x100000),
                 aemu::log::FatalError);
    EXPECT_THROW(Emulator32bit::asm_msr(1, true, 0x10000), aemu::log::FatalError);
}

TEST_F(Encoder, a_negative_immediate_is_an_error_where_it_is_unsigned)
{
    EXPECT_THROW(Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, 0, 0, -1),
                 aemu::log::FatalError);
}

TEST_F(Encoder, a_register_that_does_not_exist_is_an_error)
{
    EXPECT_THROW(Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, 32, 0, 1),
                 aemu::log::FatalError);
    EXPECT_THROW(Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, 0, -1, 1),
                 aemu::log::FatalError);
    EXPECT_THROW(Emulator32bit::asm_format_b2(Emulator32bit::_op_bx, ConditionCode::AL, 32),
                 aemu::log::FatalError);
}

TEST_F(Encoder, a_shift_amount_of_32_or_more_is_an_error)
{
    EXPECT_THROW(Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, 0, 0, 1,
                                             ShiftType::SHIFT_LSL, 32),
                 aemu::log::FatalError);
}
