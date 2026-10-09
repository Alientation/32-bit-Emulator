// Tests for the conditional select family (csel, csinc, csinv, csneg and the aliases built from
// them) and the unary instructions of the special group (sxtb, sxth, uxtb, uxth, clz, rev, rev16).

#include "emulator32bit_test/emulator32bit_test.h"

namespace
{

/// The condition table of the ISA, written out independently of check_cond().
bool ref_condition(const unsigned cond, const NZCVFlags f)
{
    switch (cond)
    {
    case 0:
        return f.z;
    case 1:
        return !f.z;
    case 2:
        return f.c;
    case 3:
        return !f.c;
    case 4:
        return f.n;
    case 5:
        return !f.n;
    case 6:
        return f.v;
    case 7:
        return !f.v;
    case 8:
        return f.c && !f.z;
    case 9:
        return !f.c || f.z;
    case 10:
        return f.n == f.v;
    case 11:
        return f.n != f.v;
    case 12:
        return !f.z && f.n == f.v;
    case 13:
        return f.z || f.n != f.v;
    case 14:
        return true;
    default:
        return false;
    }
}

constexpr U8 kXd = 3;
constexpr U8 kXn = 4;
constexpr U8 kXm = 5;
constexpr U8 kXzr = U8(Register::XZR);

class SelectTest : public EmulatorFixture
{
  protected:
    /// Runs `csel`-family instruction `instr` with xn = 100 and xm = 7 under `flags`.
    word run_select(const word instr, const NZCVFlags flags)
    {
        fill_registers();
        cpu.write_reg(kXn, 100);
        cpu.write_reg(kXm, 7);
        set_flags(flags);
        execute(instr);
        EXPECT_EQ(flags_to_string(this->flags()), flags_to_string(flags)) << "no flags change";
        return cpu.read_reg(kXd);
    }
};

} // namespace

// What the condition tests is check_cond()'s business; this walks every condition against every
// flag combination and compares with the table of the ISA.
TEST_F(SelectTest, csel_picks_xn_when_the_condition_holds)
{
    for (unsigned cond = 0; cond < 16; ++cond)
    {
        for (unsigned f = 0; f < 16; ++f)
        {
            const NZCVFlags flags = flags_from_bits(f);
            const bool holds = ref_condition(cond, flags);
            const word result =
                run_select(Emulator32bit::asm_csel(Emulator32bit::kCselId_csel, ConditionCode(cond),
                                                   kXd, kXn, kXm),
                           flags);
            EXPECT_EQ(result, holds ? 100u : 7u)
                << "cond " << cond << " flags " << flags_to_string(flags);
        }
    }
}

TEST_F(SelectTest, the_variants_change_xm_when_the_condition_fails)
{
    const NZCVFlags none = flags_from_bits(0);
    // eq does not hold with Z clear, so xm is used.
    const auto select = [&](const word variant) {
        return run_select(Emulator32bit::asm_csel(variant, ConditionCode::EQ, kXd, kXn, kXm), none);
    };
    EXPECT_EQ(select(Emulator32bit::kCselId_csel), 7u);
    EXPECT_EQ(select(Emulator32bit::kCselId_csinc), 8u);
    EXPECT_EQ(select(Emulator32bit::kCselId_csinv), ~word(7));
    EXPECT_EQ(select(Emulator32bit::kCselId_csneg), word(0) - 7);

    // And xn is untouched when it holds.
    const NZCVFlags zero = {.n = false, .z = true, .c = false, .v = false};
    for (const word variant : {Emulator32bit::kCselId_csel, Emulator32bit::kCselId_csinc,
                               Emulator32bit::kCselId_csinv, Emulator32bit::kCselId_csneg})
    {
        EXPECT_EQ(
            run_select(Emulator32bit::asm_csel(variant, ConditionCode::EQ, kXd, kXn, kXm), zero),
            100u);
    }
}

TEST_F(SelectTest, csel_reads_xzr_as_zero_and_writes_to_it_nowhere)
{
    const NZCVFlags none = flags_from_bits(0);
    EXPECT_EQ(run_select(Emulator32bit::asm_csel(Emulator32bit::kCselId_csel, ConditionCode::EQ,
                                                 kXd, kXn, kXzr),
                         none),
              0u);

    const auto before = snapshot_registers();
    execute(
        Emulator32bit::asm_csel(Emulator32bit::kCselId_csel, ConditionCode::AL, kXzr, kXn, kXm));
    EXPECT_EQ(cpu.read_reg(kXzr), 0u);
    EXPECT_EQ(cpu.read_reg(kXn), before[kXn]);
}

// cset xd, lt is csinc xd, xzr, xzr, ge: 1 if lt holds, else 0.
TEST_F(SelectTest, cset_is_csinc_of_the_zero_register_with_the_opposite_condition)
{
    const NZCVFlags less = {.n = true, .z = false, .c = false, .v = false};
    const NZCVFlags not_less = flags_from_bits(0);
    const word cset_lt =
        Emulator32bit::asm_csel(Emulator32bit::kCselId_csinc, ConditionCode::GE, kXd, kXzr, kXzr);
    EXPECT_EQ(run_select(cset_lt, less), 1u);
    EXPECT_EQ(run_select(cset_lt, not_less), 0u);

    const word csetm_lt =
        Emulator32bit::asm_csel(Emulator32bit::kCselId_csinv, ConditionCode::GE, kXd, kXzr, kXzr);
    EXPECT_EQ(run_select(csetm_lt, less), 0xFFFFFFFFu);
    EXPECT_EQ(run_select(csetm_lt, not_less), 0u);

    EXPECT_EQ(Emulator32bit::disassemble_instr(cset_lt), "cset x3, lt");
    EXPECT_EQ(Emulator32bit::disassemble_instr(csetm_lt), "csetm x3, lt");
}

TEST_F(SelectTest, disassembly_names_the_aliases)
{
    const auto text = [](const word variant, const ConditionCode cond, const U8 xn, const U8 xm) {
        return Emulator32bit::disassemble_instr(
            Emulator32bit::asm_csel(variant, cond, kXd, xn, xm));
    };

    EXPECT_EQ(text(Emulator32bit::kCselId_csel, ConditionCode::EQ, kXn, kXm),
              "csel x3, x4, x5, eq");
    EXPECT_EQ(text(Emulator32bit::kCselId_csinc, ConditionCode::EQ, kXn, kXm),
              "csinc x3, x4, x5, eq");
    EXPECT_EQ(text(Emulator32bit::kCselId_csinc, ConditionCode::NE, kXn, kXn), "cinc x3, x4, eq");
    EXPECT_EQ(text(Emulator32bit::kCselId_csinv, ConditionCode::NE, kXn, kXn), "cinv x3, x4, eq");
    EXPECT_EQ(text(Emulator32bit::kCselId_csneg, ConditionCode::MI, kXn, kXn), "cneg x3, x4, pl");

    // al and nv have no opposite, so they stay as they were written.
    EXPECT_EQ(text(Emulator32bit::kCselId_csinc, ConditionCode::AL, kXzr, kXzr),
              "csinc x3, xzr, xzr, al");
}

namespace
{

class UnaryTest : public EmulatorFixture
{
  protected:
    word apply(const word op, const word value)
    {
        fill_registers();
        cpu.write_reg(kXn, value);
        set_flags(flags_from_bits(0b1010));
        execute(Emulator32bit::asm_unary(op, kXd, kXn));
        EXPECT_EQ(flags_to_string(flags()), flags_to_string(flags_from_bits(0b1010)));
        EXPECT_EQ(cpu.read_reg(kXn), value) << "the source is not changed";
        return cpu.read_reg(kXd);
    }
};

} // namespace

TEST_F(UnaryTest, sign_and_zero_extension)
{
    EXPECT_EQ(apply(Emulator32bit::kUnaryId_sxtb, 0x12345680), 0xFFFFFF80u);
    EXPECT_EQ(apply(Emulator32bit::kUnaryId_sxtb, 0x1234567F), 0x0000007Fu);
    EXPECT_EQ(apply(Emulator32bit::kUnaryId_sxth, 0x12348000), 0xFFFF8000u);
    EXPECT_EQ(apply(Emulator32bit::kUnaryId_sxth, 0x12347FFF), 0x00007FFFu);
    EXPECT_EQ(apply(Emulator32bit::kUnaryId_uxtb, 0xFFFFFF80), 0x00000080u);
    EXPECT_EQ(apply(Emulator32bit::kUnaryId_uxth, 0xFFFF8000), 0x00008000u);
}

TEST_F(UnaryTest, count_leading_zeros)
{
    EXPECT_EQ(apply(Emulator32bit::kUnaryId_clz, 0), 32u);
    EXPECT_EQ(apply(Emulator32bit::kUnaryId_clz, 1), 31u);
    EXPECT_EQ(apply(Emulator32bit::kUnaryId_clz, 0x80000000), 0u);
    EXPECT_EQ(apply(Emulator32bit::kUnaryId_clz, 0x00010000), 15u);
    EXPECT_EQ(apply(Emulator32bit::kUnaryId_clz, 0xFFFFFFFF), 0u);
}

TEST_F(UnaryTest, byte_reversal)
{
    EXPECT_EQ(apply(Emulator32bit::kUnaryId_rev, 0x11223344), 0x44332211u);
    EXPECT_EQ(apply(Emulator32bit::kUnaryId_rev, 0x000000FF), 0xFF000000u);
    EXPECT_EQ(apply(Emulator32bit::kUnaryId_rev16, 0x11223344), 0x22114433u);
    EXPECT_EQ(apply(Emulator32bit::kUnaryId_rev16, 0x0000FF00), 0x000000FFu);
}

TEST_F(UnaryTest, the_destination_can_be_the_source)
{
    fill_registers();
    cpu.write_reg(kXd, 0x00000180);
    execute(Emulator32bit::asm_unary(Emulator32bit::kUnaryId_sxtb, kXd, kXd));
    EXPECT_EQ(cpu.read_reg(kXd), 0xFFFFFF80u);
}

TEST_F(UnaryTest, an_unassigned_operation_is_undefined)
{
    const word instr = Emulator32bit::asm_unary(0b1111, kXd, kXn);
    const auto result = step(0, instr);
    EXPECT_EQ(result.status, Emulator32bit::RunResult::Status::FAULT);
    EXPECT_EQ(Emulator32bit::disassemble_instr(instr), "ERROR: INVALID UNARY OPERATION");
}

TEST_F(UnaryTest, disassembly)
{
    EXPECT_EQ(Emulator32bit::disassemble_instr(
                  Emulator32bit::asm_unary(Emulator32bit::kUnaryId_sxtb, 3, 4)),
              "sxtb x3, x4");
    EXPECT_EQ(Emulator32bit::disassemble_instr(
                  Emulator32bit::asm_unary(Emulator32bit::kUnaryId_rev16, 3, kXzr)),
              "rev16 x3, xzr");
}

// The width is in bits 4-5, the operation in bits 0-3.
TEST(AtomicDisassembly, names_the_operation_and_the_width)
{
    using E = Emulator32bit;
    const std::pair<word, const char *> ops[] = {{E::kAtomicId_swp, "swp"},
                                                 {E::kAtomicId_ldadd, "ldadd"},
                                                 {E::kAtomicId_ldclr, "ldclr"},
                                                 {E::kAtomicId_ldset, "ldset"}};
    const std::pair<word, const char *> widths[] = {
        {E::kAtomicWidth_word, ""}, {E::kAtomicWidth_byte, "b"}, {E::kAtomicWidth_hword, "h"}};
    for (const auto &[op, name] : ops)
    {
        for (const auto &[width, suffix] : widths)
        {
            EXPECT_EQ(E::disassemble_instr(E::asm_atomic(1, 2, 3, width, op)),
                      std::string(name) + suffix + " x1, x2, [x3]");
        }
    }
}
