#include <gtest/gtest.h>

#include <emulator32bit/alu.h>

#include <array>

namespace
{
constexpr NZCVFlags kNoFlags = {false, false, false, false};
constexpr NZCVFlags kAllFlags = {true, true, true, true};

// Edge values that exercise sign, carry and overflow boundaries.
constexpr std::array<word, 12> kEdgeValues = {
    0x00000000, 0x00000001, 0x00000002, 0x7FFFFFFE, 0x7FFFFFFF, 0x80000000,
    0x80000001, 0xFFFFFFFE, 0xFFFFFFFF, 0x12345678, 0x0000FFFF, 0xFFFF0000,
};

NZCVFlags flags_from_bits(const unsigned bits)
{
    return {.n = bool(bits & 8), .z = bool(bits & 4), .c = bool(bits & 2), .v = bool(bits & 1)};
}

// Independent reference using the ARM definition of AddWithCarry on signed/unsigned 64-bit sums.
AluResult ref_add_with_carry(const word a, const word b, const bool carry)
{
    const U64 usum = U64(a) + U64(b) + U64(carry);
    const S64 ssum = S64(S32(a)) + S64(S32(b)) + S64(carry);
    const word result = word(usum);
    return {.result = result,
            .flags = {
                .n = bool(result >> 31),
                .z = result == 0,
                .c = U64(result) != usum,
                .v = S64(S32(result)) != ssum,
            }};
}

void expect_flags(const NZCVFlags actual, const NZCVFlags expected, const std::string &ctx = "")
{
    EXPECT_EQ(actual.n, expected.n) << "N " << ctx;
    EXPECT_EQ(actual.z, expected.z) << "Z " << ctx;
    EXPECT_EQ(actual.c, expected.c) << "C " << ctx;
    EXPECT_EQ(actual.v, expected.v) << "V " << ctx;
}

std::string describe(const word a, const word b, const bool carry)
{
    return "a=" + std::to_string(a) + " b=" + std::to_string(b) + " carry=" + std::to_string(carry);
}
} // namespace

// ---------------------------------------------------------------------------------------------
// Register / helpers
// ---------------------------------------------------------------------------------------------

TEST(AluRegister, aliases_and_conversion)
{
    EXPECT_EQ(kNumReg, 32);
    EXPECT_EQ(register_to_U8(Register::X0), 0);
    EXPECT_EQ(register_to_U8(Register::SYSCALL), 8);
    EXPECT_EQ(register_to_U8(Register::FP), 28);
    EXPECT_EQ(register_to_U8(Register::LR), 29);
    EXPECT_EQ(register_to_U8(Register::SP), 30);
    EXPECT_EQ(register_to_U8(Register::XZR), 31);
}

// ---------------------------------------------------------------------------------------------
// Condition codes
// ---------------------------------------------------------------------------------------------

TEST(AluCondition, all_conditions_for_all_flag_combinations)
{
    for (unsigned bits = 0; bits < 16; ++bits)
    {
        const bool N = bits & 8, Z = bits & 4, C = bits & 2, V = bits & 1;
        word pstate = 0;
        pstate = set_bit(pstate, kNFlagBit, N);
        pstate = set_bit(pstate, kZFlagBit, Z);
        pstate = set_bit(pstate, kCFlagBit, C);
        pstate = set_bit(pstate, kVFlagBit, V);

        const std::string ctx = "NZCV=" + std::to_string(bits);
        EXPECT_EQ(check_cond(pstate, U8(ConditionCode::EQ)), Z) << ctx;
        EXPECT_EQ(check_cond(pstate, U8(ConditionCode::NE)), !Z) << ctx;
        EXPECT_EQ(check_cond(pstate, U8(ConditionCode::CS)), C) << ctx;
        EXPECT_EQ(check_cond(pstate, U8(ConditionCode::HS)), C) << ctx;
        EXPECT_EQ(check_cond(pstate, U8(ConditionCode::CC)), !C) << ctx;
        EXPECT_EQ(check_cond(pstate, U8(ConditionCode::LO)), !C) << ctx;
        EXPECT_EQ(check_cond(pstate, U8(ConditionCode::MI)), N) << ctx;
        EXPECT_EQ(check_cond(pstate, U8(ConditionCode::PL)), !N) << ctx;
        EXPECT_EQ(check_cond(pstate, U8(ConditionCode::VS)), V) << ctx;
        EXPECT_EQ(check_cond(pstate, U8(ConditionCode::VC)), !V) << ctx;
        EXPECT_EQ(check_cond(pstate, U8(ConditionCode::HI)), C && !Z) << ctx;
        EXPECT_EQ(check_cond(pstate, U8(ConditionCode::LS)), !C || Z) << ctx;
        EXPECT_EQ(check_cond(pstate, U8(ConditionCode::GE)), N == V) << ctx;
        EXPECT_EQ(check_cond(pstate, U8(ConditionCode::LT)), N != V) << ctx;
        EXPECT_EQ(check_cond(pstate, U8(ConditionCode::GT)), !Z && N == V) << ctx;
        EXPECT_EQ(check_cond(pstate, U8(ConditionCode::LE)), Z || N != V) << ctx;
        EXPECT_TRUE(check_cond(pstate, U8(ConditionCode::AL))) << ctx;
        EXPECT_FALSE(check_cond(pstate, U8(ConditionCode::NV))) << ctx;
    }
}

TEST(AluCondition, signed_comparison_after_cmp)
{
    // cmp -1, 1 => -1 < 1 (signed) but 0xFFFFFFFF > 1 (unsigned)
    const AluResult r = alu_sub(0xFFFFFFFF, 1, true);
    word pstate = 0;
    pstate = set_bit(pstate, kNFlagBit, r.flags.n);
    pstate = set_bit(pstate, kZFlagBit, r.flags.z);
    pstate = set_bit(pstate, kCFlagBit, r.flags.c);
    pstate = set_bit(pstate, kVFlagBit, r.flags.v);

    EXPECT_TRUE(check_cond(pstate, U8(ConditionCode::LT)));
    EXPECT_TRUE(check_cond(pstate, U8(ConditionCode::LE)));
    EXPECT_FALSE(check_cond(pstate, U8(ConditionCode::GE)));
    EXPECT_TRUE(check_cond(pstate, U8(ConditionCode::HI)));
    EXPECT_FALSE(check_cond(pstate, U8(ConditionCode::LS)));
}

// ---------------------------------------------------------------------------------------------
// Addition
// ---------------------------------------------------------------------------------------------

TEST(AluAdd, simple)
{
    const AluResult r = alu_add(2, 3, false);
    EXPECT_EQ(r.result, 5u);
    expect_flags(r.flags, kNoFlags);
}

TEST(AluAdd, carry_in_is_added)
{
    const AluResult r = alu_add(2, 3, true);
    EXPECT_EQ(r.result, 6u);
    expect_flags(r.flags, kNoFlags);
}

TEST(AluAdd, zero_result)
{
    const AluResult r = alu_add(0, 0, false);
    EXPECT_EQ(r.result, 0u);
    expect_flags(r.flags, {false, true, false, false});
}

TEST(AluAdd, unsigned_carry_out_without_signed_overflow)
{
    const AluResult r = alu_add(0xFFFFFFFF, 1, false);
    EXPECT_EQ(r.result, 0u);
    expect_flags(r.flags, {false, true, true, false});
}

TEST(AluAdd, carry_out_only_from_carry_in)
{
    const AluResult r = alu_add(0xFFFFFFFF, 0, true);
    EXPECT_EQ(r.result, 0u);
    expect_flags(r.flags, {false, true, true, false});
}

TEST(AluAdd, signed_overflow_positive)
{
    const AluResult r = alu_add(0x7FFFFFFF, 1, false);
    EXPECT_EQ(r.result, 0x80000000u);
    expect_flags(r.flags, {true, false, false, true});
}

TEST(AluAdd, signed_overflow_negative)
{
    const AluResult r = alu_add(0x80000000, 0x80000000, false);
    EXPECT_EQ(r.result, 0u);
    expect_flags(r.flags, {false, true, true, true});
}

TEST(AluAdd, signed_overflow_caused_by_carry_in)
{
    const AluResult r = alu_add(0x7FFFFFFF, 0, true);
    EXPECT_EQ(r.result, 0x80000000u);
    expect_flags(r.flags, {true, false, false, true});
}

TEST(AluAdd, negative_result_without_overflow)
{
    const AluResult r = alu_add(0xFFFFFFFE, 0xFFFFFFFE, false); // -2 + -2
    EXPECT_EQ(r.result, 0xFFFFFFFCu);
    expect_flags(r.flags, {true, false, true, false});
}

TEST(AluAdd, matches_reference_for_edge_values)
{
    for (const word a : kEdgeValues)
        for (const word b : kEdgeValues)
            for (const bool carry : {false, true})
            {
                const AluResult got = alu_add(a, b, carry);
                const AluResult want = ref_add_with_carry(a, b, carry);
                EXPECT_EQ(got.result, want.result) << describe(a, b, carry);
                expect_flags(got.flags, want.flags, describe(a, b, carry));
            }
}

// ---------------------------------------------------------------------------------------------
// Subtraction (ARM convention: carry_in=true means "no borrow", C=1 means "no borrow")
// ---------------------------------------------------------------------------------------------

TEST(AluSub, simple)
{
    const AluResult r = alu_sub(5, 3, true);
    EXPECT_EQ(r.result, 2u);
    expect_flags(r.flags, {false, false, true, false});
}

TEST(AluSub, equal_operands_set_zero_and_no_borrow)
{
    const AluResult r = alu_sub(7, 7, true);
    EXPECT_EQ(r.result, 0u);
    expect_flags(r.flags, {false, true, true, false});
}

TEST(AluSub, borrow_clears_carry)
{
    const AluResult r = alu_sub(3, 5, true);
    EXPECT_EQ(r.result, 0xFFFFFFFEu);
    expect_flags(r.flags, {true, false, false, false});
}

TEST(AluSub, carry_in_false_subtracts_extra_one)
{
    const AluResult r = alu_sub(5, 3, false);
    EXPECT_EQ(r.result, 1u);
    expect_flags(r.flags, {false, false, true, false});
}

TEST(AluSub, carry_in_false_equal_operands_borrows)
{
    const AluResult r = alu_sub(5, 5, false);
    EXPECT_EQ(r.result, 0xFFFFFFFFu);
    expect_flags(r.flags, {true, false, false, false});
}

TEST(AluSub, signed_overflow_positive_minus_negative)
{
    const AluResult r = alu_sub(0x7FFFFFFF, 0xFFFFFFFF, true); // INT_MAX - (-1)
    EXPECT_EQ(r.result, 0x80000000u);
    expect_flags(r.flags, {true, false, false, true});
}

TEST(AluSub, signed_overflow_negative_minus_positive)
{
    const AluResult r = alu_sub(0x80000000, 1, true); // INT_MIN - 1
    EXPECT_EQ(r.result, 0x7FFFFFFFu);
    expect_flags(r.flags, {false, false, true, true});
}

TEST(AluSub, subtract_zero_with_borrow)
{
    const AluResult r = alu_sub(0, 0, false);
    EXPECT_EQ(r.result, 0xFFFFFFFFu);
    expect_flags(r.flags, {true, false, false, false});
}

TEST(AluSub, subtract_max_operand_with_borrow_keeps_value)
{
    // a - 0xFFFFFFFF - 1 == a (mod 2^32), and there is a borrow.
    const AluResult r = alu_sub(0x12345678, 0xFFFFFFFF, false);
    EXPECT_EQ(r.result, 0x12345678u);
    expect_flags(r.flags, {false, false, false, false});
}

TEST(AluSub, signed_overflow_when_borrow_pushes_operand_past_int_max)
{
    // INT_MIN - INT_MAX - 1 = -2^32, which overflows a signed 32-bit result.
    const AluResult r = alu_sub(0x80000000, 0x7FFFFFFF, false);
    EXPECT_EQ(r.result, 0u);
    expect_flags(r.flags, {false, true, true, true});
}

TEST(AluSub, matches_arm_reference_for_edge_values)
{
    // SUB / SBC are defined by ARM as AddWithCarry(a, ~b, carry_in).
    for (const word a : kEdgeValues)
        for (const word b : kEdgeValues)
            for (const bool carry : {false, true})
            {
                const AluResult got = alu_sub(a, b, carry);
                const AluResult want = ref_add_with_carry(a, ~b, carry);
                EXPECT_EQ(got.result, want.result) << describe(a, b, carry);
                expect_flags(got.flags, want.flags, describe(a, b, carry));
            }
}

TEST(AluSub, reverse_subtract_by_swapping_operands)
{
    // rsb xd, xn, op2 is op2 - xn.
    const AluResult r = alu_sub(10, 3, true);
    EXPECT_EQ(r.result, 7u);
}

// ---------------------------------------------------------------------------------------------
// Logical operations and moves
// ---------------------------------------------------------------------------------------------

TEST(AluLogic, and_result)
{
    const AluResult r = alu_and(0xF0F0F0F0, 0xFF00FF00, kNoFlags);
    EXPECT_EQ(r.result, 0xF000F000u);
    expect_flags(r.flags, {true, false, false, false});
}

TEST(AluLogic, orr_result)
{
    const AluResult r = alu_orr(0xF0F0F0F0, 0x0F000F00, kNoFlags);
    EXPECT_EQ(r.result, 0xFFF0FFF0u);
    expect_flags(r.flags, {true, false, false, false});
}

TEST(AluLogic, eor_result)
{
    const AluResult r = alu_eor(0xFFFF0000, 0xFF00FF00, kNoFlags);
    EXPECT_EQ(r.result, 0x00FFFF00u);
    expect_flags(r.flags, {false, false, false, false});
}

TEST(AluLogic, bic_clears_bits_of_second_operand)
{
    const AluResult r = alu_bic(0xFFFFFFFF, 0x0000FFFF, kNoFlags);
    EXPECT_EQ(r.result, 0xFFFF0000u);
    expect_flags(r.flags, {true, false, false, false});
}

TEST(AluLogic, zero_result_sets_z_and_clears_n)
{
    const AluResult r = alu_and(0xF0F0F0F0, 0x0F0F0F0F, {true, false, false, false});
    EXPECT_EQ(r.result, 0u);
    expect_flags(r.flags, {false, true, false, false});
}

TEST(AluLogic, eor_with_self_is_zero)
{
    const AluResult r = alu_eor(0xDEADBEEF, 0xDEADBEEF, kNoFlags);
    EXPECT_EQ(r.result, 0u);
    EXPECT_TRUE(r.flags.z);
}

TEST(AluLogic, carry_and_overflow_are_preserved)
{
    expect_flags(alu_and(1, 1, {false, false, true, true}).flags, {false, false, true, true});
    expect_flags(alu_orr(0, 0, {true, true, true, false}).flags, {false, true, true, false});
    expect_flags(alu_eor(1, 0, {false, true, false, true}).flags, {false, false, false, true});
    expect_flags(alu_bic(1, 1, kAllFlags).flags, {false, true, true, true});
}

TEST(AluLogic, mov_passes_value_through)
{
    const AluResult r = alu_mov(0x80000001, {false, true, true, false});
    EXPECT_EQ(r.result, 0x80000001u);
    expect_flags(r.flags, {true, false, true, false});
}

TEST(AluLogic, mov_zero)
{
    const AluResult r = alu_mov(0, kNoFlags);
    EXPECT_EQ(r.result, 0u);
    expect_flags(r.flags, {false, true, false, false});
}

TEST(AluLogic, mvn_inverts_value)
{
    const AluResult r = alu_mvn(0, kNoFlags);
    EXPECT_EQ(r.result, 0xFFFFFFFFu);
    expect_flags(r.flags, {true, false, false, false});

    const AluResult r2 = alu_mvn(0xFFFFFFFF, {true, false, true, true});
    EXPECT_EQ(r2.result, 0u);
    expect_flags(r2.flags, {false, true, true, true});
}

// ---------------------------------------------------------------------------------------------
// Multiplication
// ---------------------------------------------------------------------------------------------

TEST(AluMul, simple)
{
    const AluResult r = alu_mul(6, 7, kNoFlags);
    EXPECT_EQ(r.result, 42u);
    expect_flags(r.flags, kNoFlags);
}

TEST(AluMul, truncates_to_32_bits)
{
    const AluResult r = alu_mul(0x10000, 0x10000, kNoFlags);
    EXPECT_EQ(r.result, 0u);
    expect_flags(r.flags, {false, true, false, false});
}

TEST(AluMul, negative_result_sets_n)
{
    const AluResult r = alu_mul(0xFFFFFFFF, 3, kNoFlags); // -1 * 3
    EXPECT_EQ(r.result, 0xFFFFFFFDu);
    expect_flags(r.flags, {true, false, false, false});
}

TEST(AluMul, preserves_carry_and_overflow)
{
    expect_flags(alu_mul(2, 2, {true, true, true, true}).flags, {false, false, true, true});
    expect_flags(alu_mul(0, 5, {false, false, false, true}).flags, {false, true, false, true});
}

TEST(AluUmull, simple)
{
    const AluResult r = alu_umull(6, 7, kNoFlags);
    EXPECT_EQ(r.result, 42u);
    expect_flags(r.flags, kNoFlags);
}

TEST(AluUmull, max_operands_produce_full_64_bit_result)
{
    const AluResult r = alu_umull(0xFFFFFFFF, 0xFFFFFFFF, kNoFlags);
    EXPECT_EQ(r.result, 0xFFFFFFFE00000001ULL);
    expect_flags(r.flags, {true, false, false, false});
}

TEST(AluUmull, operands_are_unsigned)
{
    const AluResult r = alu_umull(0x80000000, 2, kNoFlags);
    EXPECT_EQ(r.result, 0x100000000ULL);
    expect_flags(r.flags, kNoFlags);
}

TEST(AluUmull, zero_sets_z_and_preserves_cv)
{
    const AluResult r = alu_umull(0, 0xFFFFFFFF, {true, false, true, true});
    EXPECT_EQ(r.result, 0u);
    expect_flags(r.flags, {false, true, true, true});
}

TEST(AluSmull, simple)
{
    const AluResult r = alu_smull(6, 7, kNoFlags);
    EXPECT_EQ(r.result, 42u);
    expect_flags(r.flags, kNoFlags);
}

TEST(AluSmull, operands_are_signed)
{
    const AluResult r = alu_smull(0xFFFFFFFF, 0xFFFFFFFF, kNoFlags); // -1 * -1
    EXPECT_EQ(r.result, 1u);
    expect_flags(r.flags, kNoFlags);
}

TEST(AluSmull, negative_product_sign_extends_to_64_bits)
{
    const AluResult r = alu_smull(0xFFFFFFFF, 3, kNoFlags); // -1 * 3
    EXPECT_EQ(r.result, 0xFFFFFFFFFFFFFFFDULL);
    expect_flags(r.flags, {true, false, false, false});
}

TEST(AluSmull, int_min_squared)
{
    const AluResult r = alu_smull(0x80000000, 0x80000000, kNoFlags);
    EXPECT_EQ(r.result, 0x4000000000000000ULL);
    expect_flags(r.flags, kNoFlags);
}

TEST(AluSmull, int_min_times_int_max)
{
    const AluResult r = alu_smull(0x80000000, 0x7FFFFFFF, kNoFlags);
    EXPECT_EQ(r.result, 0xC000000080000000ULL);
    expect_flags(r.flags, {true, false, false, false});
}

TEST(AluSmull, preserves_carry_and_overflow)
{
    expect_flags(alu_smull(0, 1, {true, false, true, true}).flags, {false, true, true, true});
}

// ---------------------------------------------------------------------------------------------
// Shifts
// ---------------------------------------------------------------------------------------------

TEST(AluShift, zero_amount_returns_value_and_keeps_carry_and_overflow)
{
    for (const ShiftType type :
         {ShiftType::SHIFT_LSL, ShiftType::SHIFT_LSR, ShiftType::SHIFT_ASR, ShiftType::SHIFT_ROR})
    {
        const AluResult r = alu_shift(0x80000001, type, 0, {false, true, true, true});
        EXPECT_EQ(r.result, 0x80000001u);
        expect_flags(r.flags, {true, false, true, true});
    }
}

TEST(AluShift, zero_amount_zero_value_sets_z)
{
    const AluResult r = alu_shift(0, ShiftType::SHIFT_LSL, 0, kNoFlags);
    EXPECT_EQ(r.result, 0u);
    expect_flags(r.flags, {false, true, false, false});
}

TEST(AluShift, lsl_basic)
{
    const AluResult r = alu_shift(1, ShiftType::SHIFT_LSL, 4, kNoFlags);
    EXPECT_EQ(r.result, 16u);
    expect_flags(r.flags, kNoFlags);
}

TEST(AluShift, lsl_carry_is_last_bit_shifted_out)
{
    const AluResult r = alu_shift(0xC0000001, ShiftType::SHIFT_LSL, 1, {false, false, false, true});
    EXPECT_EQ(r.result, 0x80000002u);
    expect_flags(r.flags, {true, false, true, true});
}

TEST(AluShift, lsl_carry_clear_when_last_bit_out_is_zero)
{
    const AluResult r = alu_shift(0x40000000, ShiftType::SHIFT_LSL, 1, {false, false, true, false});
    EXPECT_EQ(r.result, 0x80000000u);
    expect_flags(r.flags, {true, false, false, false});
}

TEST(AluShift, lsl_by_31)
{
    const AluResult r = alu_shift(3, ShiftType::SHIFT_LSL, 31, kNoFlags);
    EXPECT_EQ(r.result, 0x80000000u);
    expect_flags(r.flags, {true, false, true, false}); // bit 1 shifted out last
}

TEST(AluShift, lsl_all_bits_out_gives_zero)
{
    const AluResult r = alu_shift(0x80000000, ShiftType::SHIFT_LSL, 1, kNoFlags);
    EXPECT_EQ(r.result, 0u);
    expect_flags(r.flags, {false, true, true, false});
}

TEST(AluShift, lsr_basic)
{
    const AluResult r = alu_shift(0x80000000, ShiftType::SHIFT_LSR, 4, kNoFlags);
    EXPECT_EQ(r.result, 0x08000000u);
    expect_flags(r.flags, kNoFlags);
}

TEST(AluShift, lsr_carry_is_last_bit_shifted_out)
{
    const AluResult r = alu_shift(0x00000003, ShiftType::SHIFT_LSR, 1, kNoFlags);
    EXPECT_EQ(r.result, 1u);
    expect_flags(r.flags, {false, false, true, false});
}

TEST(AluShift, lsr_is_logical_and_clears_n)
{
    const AluResult r =
        alu_shift(0xFFFFFFFF, ShiftType::SHIFT_LSR, 31, {true, false, false, false});
    EXPECT_EQ(r.result, 1u);
    expect_flags(r.flags, {false, false, true, false});
}

TEST(AluShift, lsr_to_zero)
{
    const AluResult r = alu_shift(1, ShiftType::SHIFT_LSR, 1, kNoFlags);
    EXPECT_EQ(r.result, 0u);
    expect_flags(r.flags, {false, true, true, false});
}

TEST(AluShift, asr_replicates_sign_bit)
{
    const AluResult r = alu_shift(0x80000000, ShiftType::SHIFT_ASR, 4, kNoFlags);
    EXPECT_EQ(r.result, 0xF8000000u);
    expect_flags(r.flags, {true, false, false, false});
}

TEST(AluShift, asr_positive_value_shifts_in_zeros)
{
    const AluResult r = alu_shift(0x40000000, ShiftType::SHIFT_ASR, 4, kNoFlags);
    EXPECT_EQ(r.result, 0x04000000u);
    expect_flags(r.flags, kNoFlags);
}

TEST(AluShift, asr_by_31_of_negative_is_all_ones)
{
    const AluResult r = alu_shift(0x80000000, ShiftType::SHIFT_ASR, 31, kNoFlags);
    EXPECT_EQ(r.result, 0xFFFFFFFFu);
    expect_flags(r.flags, {true, false, false, false});
}

TEST(AluShift, asr_carry_is_last_bit_shifted_out)
{
    const AluResult r = alu_shift(0xFFFFFFFF, ShiftType::SHIFT_ASR, 3, kNoFlags);
    EXPECT_EQ(r.result, 0xFFFFFFFFu);
    expect_flags(r.flags, {true, false, true, false});
}

TEST(AluShift, ror_basic)
{
    const AluResult r = alu_shift(0x00000001, ShiftType::SHIFT_ROR, 1, kNoFlags);
    EXPECT_EQ(r.result, 0x80000000u);
    expect_flags(r.flags, {true, false, true, false});
}

TEST(AluShift, ror_wraps_bits)
{
    const AluResult r = alu_shift(0x12345678, ShiftType::SHIFT_ROR, 8, kNoFlags);
    EXPECT_EQ(r.result, 0x78123456u);
    expect_flags(r.flags, kNoFlags); // bit 7 of the input (0x78 -> bit 7 is 0)
}

TEST(AluShift, ror_by_16_swaps_halves)
{
    const AluResult r = alu_shift(0xAAAA5555, ShiftType::SHIFT_ROR, 16, kNoFlags);
    EXPECT_EQ(r.result, 0x5555AAAAu);
    expect_flags(r.flags, kNoFlags);
}

TEST(AluShift, ror_by_31_is_rol_by_1)
{
    const AluResult r = alu_shift(0x80000001, ShiftType::SHIFT_ROR, 31, kNoFlags);
    EXPECT_EQ(r.result, 0x00000003u);
    expect_flags(r.flags, {false, false, false, false}); // bit 30 of input is 0
}

TEST(AluShift, overflow_flag_is_never_modified)
{
    for (const ShiftType type :
         {ShiftType::SHIFT_LSL, ShiftType::SHIFT_LSR, ShiftType::SHIFT_ASR, ShiftType::SHIFT_ROR})
        for (U8 amt = 0; amt < 32; ++amt)
        {
            EXPECT_TRUE(alu_shift(0xDEADBEEF, type, amt, {false, false, false, true}).flags.v);
            EXPECT_FALSE(alu_shift(0xDEADBEEF, type, amt, {false, false, false, false}).flags.v);
        }
}

TEST(AluShift, matches_reference_for_all_amounts)
{
    for (const word value : kEdgeValues)
        for (U8 amt = 1; amt < 32; ++amt)
        {
            const std::string ctx =
                "value=" + std::to_string(value) + " amt=" + std::to_string(amt);

            const word lsl = word(U64(value) << amt);
            const word lsr = value >> amt;
            const word asr = word(S32(value) >> amt);
            const word ror = word((U64(value) >> amt) | (U64(value) << (32 - amt)));

            const AluResult rl = alu_shift(value, ShiftType::SHIFT_LSL, amt, kNoFlags);
            EXPECT_EQ(rl.result, lsl) << "lsl " << ctx;
            EXPECT_EQ(rl.flags.c, bool((U64(value) << amt) >> 32 & 1)) << "lsl carry " << ctx;

            const AluResult rr = alu_shift(value, ShiftType::SHIFT_LSR, amt, kNoFlags);
            EXPECT_EQ(rr.result, lsr) << "lsr " << ctx;
            EXPECT_EQ(rr.flags.c, bool((value >> (amt - 1)) & 1)) << "lsr carry " << ctx;

            const AluResult ra = alu_shift(value, ShiftType::SHIFT_ASR, amt, kNoFlags);
            EXPECT_EQ(ra.result, asr) << "asr " << ctx;
            EXPECT_EQ(ra.flags.c, bool((value >> (amt - 1)) & 1)) << "asr carry " << ctx;

            const AluResult ro = alu_shift(value, ShiftType::SHIFT_ROR, amt, kNoFlags);
            EXPECT_EQ(ro.result, ror) << "ror " << ctx;
            EXPECT_EQ(ro.flags.c, bool((value >> (amt - 1)) & 1)) << "ror carry " << ctx;

            for (const AluResult *r : {&rl, &rr, &ra, &ro})
            {
                EXPECT_EQ(r->flags.n, bool(word(r->result) >> 31)) << ctx;
                EXPECT_EQ(r->flags.z, word(r->result) == 0) << ctx;
            }
        }
}

TEST(AluUdiv, divides_and_rounds_down)
{
    EXPECT_EQ(alu_udiv(100, 7, kNoFlags).result, 14u);
    EXPECT_EQ(alu_udiv(6, 7, kNoFlags).result, 0u);
    EXPECT_EQ(alu_udiv(7, 7, kNoFlags).result, 1u);
    EXPECT_EQ(alu_udiv(0xFFFFFFFF, 2, kNoFlags).result, 0x7FFFFFFFu) << "the operands are unsigned";
    EXPECT_EQ(alu_udiv(0x80000000, 1, kNoFlags).result, 0x80000000u);
    EXPECT_EQ(alu_udiv(0xFFFFFFFF, 0xFFFFFFFF, kNoFlags).result, 1u);
}

TEST(AluUdiv, division_by_zero_is_zero)
{
    EXPECT_EQ(alu_udiv(1234, 0, kNoFlags).result, 0u);
    EXPECT_EQ(alu_udiv(0, 0, kNoFlags).result, 0u);
    EXPECT_EQ(alu_udiv(0xFFFFFFFF, 0, kNoFlags).result, 0u);
}

TEST(AluSdiv, rounds_toward_zero)
{
    const auto sdiv = [](const S32 a, const S32 b)
    { return S32(word(alu_sdiv(word(a), word(b), kNoFlags).result)); };

    EXPECT_EQ(sdiv(7, 2), 3);
    EXPECT_EQ(sdiv(-7, 2), -3);
    EXPECT_EQ(sdiv(7, -2), -3);
    EXPECT_EQ(sdiv(-7, -2), 3);
    EXPECT_EQ(sdiv(-1, 2), 0);
    EXPECT_EQ(sdiv(INT32_MIN, 2), INT32_MIN / 2);
    EXPECT_EQ(sdiv(INT32_MAX, -1), -INT32_MAX);
}

TEST(AluSdiv, division_by_zero_is_zero_and_int_min_by_minus_one_wraps)
{
    EXPECT_EQ(alu_sdiv(0xFFFFFFF9, 0, kNoFlags).result, 0u);
    EXPECT_EQ(alu_sdiv(0x80000000, 0xFFFFFFFF, kNoFlags).result, 0x80000000u);
}

TEST(AluDiv, flags_are_n_and_z_of_the_result_and_c_and_v_are_kept)
{
    expect_flags(alu_sdiv(0xFFFFFFF9, 2, {false, false, true, true}).flags,
                 {true, false, true, true}); // -3
    expect_flags(alu_udiv(1, 2, {true, false, true, false}).flags, {false, true, true, false}); // 0
    expect_flags(alu_udiv(8, 2, {true, true, false, true}).flags, {false, false, false, true});
}

// There is no remainder instruction, a compiler computes n - (n / d) * d.
TEST(AluDiv, the_remainder_is_n_minus_the_quotient_times_d)
{
    const auto srem = [](const S32 n, const S32 d)
    {
        const word quotient = word(alu_sdiv(word(n), word(d), kNoFlags).result);
        const word product = word(alu_mul(quotient, word(d), kNoFlags).result);
        return S32(word(n) - product);
    };
    const auto urem = [](const word n, const word d)
    {
        const word quotient = word(alu_udiv(n, d, kNoFlags).result);
        return word(n - word(alu_mul(quotient, d, kNoFlags).result));
    };

    for (const S32 n : {0, 1, 7, -7, 100, -100, INT32_MAX, INT32_MIN + 1})
        for (const S32 d : {1, 2, 3, -3, 7, -7, 1000})
            EXPECT_EQ(srem(n, d), n % d) << n << " % " << d;
    for (const word n : {0u, 1u, 7u, 100u, 0xFFFFFFFFu, 0x80000000u})
        for (const word d : {1u, 2u, 3u, 7u, 1000u, 0xFFFFFFFFu})
            EXPECT_EQ(urem(n, d), n % d) << n << " % " << d;

    EXPECT_EQ(srem(42, 0), 42) << "n % 0 is n";
    EXPECT_EQ(urem(42, 0), 42u);
}

TEST(AluFlags, struct_is_value_initialisable)
{
    const NZCVFlags f = flags_from_bits(0b1010);
    EXPECT_TRUE(f.n);
    EXPECT_FALSE(f.z);
    EXPECT_TRUE(f.c);
    EXPECT_FALSE(f.v);
}
