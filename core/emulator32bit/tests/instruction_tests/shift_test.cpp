// Tests for lsl, lsr, asr and ror: xd = xn <shift> (imm5 | xm & 0x1F).
//
// With the S bit set N and Z come from the result, C is the last bit shifted out (unchanged for a
// zero amount) and V is never touched. Expectations come from `ref_shift`/`ref_shift_carry`
// (see emulator32bit_test.h), which are independent of `alu_shift`.

#include <emulator32bit_test/emulator32bit_test.h>

namespace
{
struct ShiftOp
{
    const char *name;
    U8 opcode;
    ShiftType type;
};

constexpr ShiftOp kShiftOps[] = {
    {"lsl", Emulator32bit::_op_lsl, ShiftType::SHIFT_LSL},
    {"lsr", Emulator32bit::_op_lsr, ShiftType::SHIFT_LSR},
    {"asr", Emulator32bit::_op_asr, ShiftType::SHIFT_ASR},
    {"ror", Emulator32bit::_op_ror, ShiftType::SHIFT_ROR},
};

constexpr word kShiftedValues[] = {0x00000000, 0x00000001, 0x00000003, 0x7FFFFFFF, 0x80000000,
                                   0x80000001, 0x80000003, 0xFFFFFFFF, 0x55555555, 0xAAAAAAAA};

class ShiftTest : public EmulatorFixture
{
  protected:
    static constexpr U8 kXd = 0;
    static constexpr U8 kXn = 1;
    static constexpr U8 kXm = 2;
    static constexpr U8 kXzr = U8(Register::XZR);

    static NZCVFlags expected_flags(const ShiftOp &op, const word value, const unsigned amount,
                                    const NZCVFlags in)
    {
        const word result = ref_shift(value, op.type, amount);
        return {
            .n = bool(result >> 31),
            .z = result == 0,
            .c = ref_shift_carry(value, op.type, amount, in.c),
            .v = in.v,
        };
    }

    /// Runs `op x0, x1, #amount` (or `op x0, x1, x2` with x2 = amount_reg) and checks everything.
    void check(const ShiftOp &op, const word value, const unsigned amount, const NZCVFlags in,
               const bool s, const bool use_imm, const word amount_reg = 0)
    {
        fill_registers();
        cpu.write_reg(kXn, value);
        cpu.write_reg(kXm, amount_reg);
        set_flags(in);
        const auto before = snapshot_registers();
        const std::string ctx =
            std::string(op.name) + (s ? "s" : "") + " value=" + hex32(value)
            + (use_imm ? " #" + std::to_string(amount) : " xm=" + hex32(amount_reg))
            + " flags_in=" + flags_to_string(in);

        execute(Emulator32bit::asm_format_o1(op.opcode, kXd, kXn, use_imm, kXm,
                                             use_imm ? amount : 0, s));

        EXPECT_EQ(cpu.read_reg(kXd), ref_shift(value, op.type, amount)) << ctx;
        expect_registers_unchanged_except(before, {kXd}, ctx);
        EXPECT_EQ(flags(), s ? expected_flags(op, value, amount, in) : in) << ctx;
        EXPECT_EQ(cpu.get_pc(), 4u) << ctx;
    }
};
} // namespace

TEST_F(ShiftTest, immediate_amounts)
{
    for (const ShiftOp &op : kShiftOps)
        for (const word value : kShiftedValues)
            for (unsigned amount = 0; amount < 32; ++amount)
                for (unsigned f = 0; f < 16; ++f)
                    for (const bool s : {false, true})
                        check(op, value, amount, flags_from_bits(f), s, true);
}

TEST_F(ShiftTest, register_amounts)
{
    for (const ShiftOp &op : kShiftOps)
        for (const word value : kShiftedValues)
            for (unsigned amount = 0; amount < 32; ++amount)
                for (unsigned f = 0; f < 16; f += 3)
                    for (const bool s : {false, true})
                        check(op, value, amount, flags_from_bits(f), s, false, amount);
}

TEST_F(ShiftTest, register_amount_uses_only_the_low_five_bits)
{
    // Only bits [4:0] of xm select the amount; bit 5 and above (and a negative value) are ignored.
    // In particular a register amount of 32 shifts by 0 (it does not clear the register), and 33
    // shifts by 1.
    constexpr word kAmountRegs[] = {32, 33, 0x100 | 7, 0xFFFFFFE0, 0xFFFFFFFF, 0x80000004};
    for (const ShiftOp &op : kShiftOps)
        for (const word amount_reg : kAmountRegs)
            for (const word value : {word(0x80000001), word(3), word(0xFFFFFFFF)})
                for (const bool s : {false, true})
                    check(op, value, amount_reg & 0x1F, flags_from_bits(0b0110), s, false,
                          amount_reg);
}

TEST_F(ShiftTest, zero_amount_keeps_c_and_v)
{
    for (const ShiftOp &op : kShiftOps)
        for (const bool c : {false, true})
        {
            const NZCVFlags in = {.n = false, .z = false, .c = c, .v = true};
            check(op, 0x80000000, 0, in, true, true);
            // N and Z still reflect the (unshifted) value.
            EXPECT_TRUE(flags().n) << op.name;
            EXPECT_EQ(flags().c, c) << op.name;
            EXPECT_TRUE(flags().v) << op.name;
        }
}

TEST_F(ShiftTest, register_aliasing)
{
    struct Alias
    {
        const char *desc;
        U8 xd, xn, xm;
    };

    constexpr Alias kAliases[] = {
        {"xd == xn", 4, 4, 5},
        {"xd == xm", 5, 4, 5},
        {"xn == xm", 6, 4, 4},
        {"xd == xn == xm", 4, 4, 4},
    };

    for (const ShiftOp &op : kShiftOps)
        for (const Alias &alias : kAliases)
            for (const word a : {word(0x80000003), word(0x12345678), word(7)})
                for (const word b : {word(1), word(4), word(31), word(0x25)})
                {
                    cpu.reset();
                    cpu.write_reg(4, a);
                    cpu.write_reg(5, b);
                    const word value = cpu.read_reg(alias.xn);
                    const unsigned amount = cpu.read_reg(alias.xm) & 0x1F;
                    const std::string ctx = std::string(op.name) + " " + alias.desc
                                            + " a=" + hex32(a) + " b=" + hex32(b);

                    execute(Emulator32bit::asm_format_o1(op.opcode, alias.xd, alias.xn, false,
                                                         alias.xm, 0, true));

                    EXPECT_EQ(cpu.read_reg(alias.xd), ref_shift(value, op.type, amount)) << ctx;
                    EXPECT_EQ(flags(), expected_flags(op, value, amount, {})) << ctx;
                }
}

TEST_F(ShiftTest, every_register_index_is_decoded)
{
    for (const ShiftOp &op : kShiftOps)
        for (U8 xd = 0; xd < kNumReg; ++xd)
        {
            const U8 xn = (xd + 1) % kNumReg;
            const U8 xm = (xd + 2) % kNumReg;
            fill_registers();
            set_flags(flags_from_bits(0));
            const auto before = snapshot_registers();
            const word value = cpu.read_reg(xn);
            const unsigned amount = cpu.read_reg(xm) & 0x1F;
            const std::string ctx = std::string(op.name) + " x" + std::to_string(xd) + ", x"
                                    + std::to_string(xn) + ", x" + std::to_string(xm);

            execute(Emulator32bit::asm_format_o1(op.opcode, xd, xn, false, xm, 0, true));

            if (xd != kXzr)
            {
                EXPECT_EQ(cpu.read_reg(xd), ref_shift(value, op.type, amount)) << ctx;
                expect_registers_unchanged_except(before, {xd}, ctx);
            }
            else
            {
                expect_registers_unchanged_except(before, {}, ctx);
            }
            EXPECT_EQ(flags(), expected_flags(op, value, amount, {})) << ctx;
        }
}

TEST_F(ShiftTest, zero_register_operands)
{
    for (const ShiftOp &op : kShiftOps)
    {
        // Shifting xzr gives 0 for every shift type, and no 1 bit is shifted out.
        fill_registers();
        cpu.write_reg(kXm, 3);
        set_flags(flags_from_bits(0b0011));
        execute(Emulator32bit::asm_format_o1(op.opcode, kXd, kXzr, false, kXm, 0, true));
        EXPECT_EQ(cpu.read_reg(kXd), 0u) << op.name;
        EXPECT_EQ(flags(), flags_from_bits(0b0101)) << op.name << ": Z set, C cleared, V kept";

        // An amount register of xzr is a shift by 0: the value passes through, C is untouched.
        fill_registers();
        cpu.write_reg(kXn, 0x80000001);
        set_flags(flags_from_bits(0b0010));
        execute(Emulator32bit::asm_format_o1(op.opcode, kXd, kXn, false, kXzr, 0, true));
        EXPECT_EQ(cpu.read_reg(kXd), 0x80000001u) << op.name;
        EXPECT_EQ(flags(), flags_from_bits(0b1010)) << op.name;

        // xzr as the destination discards the result but still sets the flags.
        fill_registers();
        cpu.write_reg(kXn, 0x80000000);
        set_flags(flags_from_bits(0));
        const auto before = snapshot_registers();
        execute(Emulator32bit::asm_format_o1(op.opcode, kXzr, kXn, true, 0, 1, true));
        EXPECT_EQ(cpu.read_reg(kXzr), 0u) << op.name;
        expect_registers_unchanged_except(before, {}, op.name);
        EXPECT_EQ(flags(), expected_flags(op, 0x80000000, 1, {})) << op.name;
    }
}

namespace
{
struct Golden
{
    const char *op;
    word value;
    unsigned amount;
    unsigned flags_in; // bits: N=8 Z=4 C=2 V=1
    word result;
    unsigned flags_out;
};

// Hand-computed expectations.
const Golden kGolden[] = {
    // clang-format off
    {"lsl", 1,          5,  0b0000, 0x00000020, 0b0000},
    {"lsl", 0xC0000001, 1,  0b0101, 0x80000002, 0b1011}, // C = old bit 31, V kept, Z cleared
    {"lsl", 0x80000000, 1,  0b1000, 0,          0b0110}, // shifted out to zero: Z and C
    {"lsl", 1,          31, 0b0000, 0x80000000, 0b1000},
    {"lsl", 3,          31, 0b0000, 0x80000000, 0b1010}, // bit 1 shifts out to C
    {"lsr", 5,          1,  0b1000, 2,          0b0010}, // C = bit 0
    {"lsr", 0x80000000, 31, 0b0000, 1,          0b0000},
    {"lsr", 0x80000000, 0,  0b0010, 0x80000000, 0b1010}, // amount 0: C kept, N from value
    {"lsr", 3,          2,  0b0000, 0,          0b0110}, // bit 1 -> C
    {"asr", 0x80000003, 2,  0b0000, 0xE0000000, 0b1010}, // sign extends, C = bit 1
    {"asr", 0x80000000, 31, 0b0000, 0xFFFFFFFF, 0b1000}, // C = bit 30
    {"asr", 0x40000000, 30, 0b0000, 1,          0b0000},
    {"asr", 0x7FFFFFFF, 31, 0b0000, 0,          0b0110}, // bit 30 -> C
    {"ror", 1,          1,  0b0000, 0x80000000, 0b1010}, // C = bit 31 of the result
    {"ror", 0x80000000, 31, 0b0000, 1,          0b0000},
    {"ror", 0x12345678, 8,  0b0000, 0x78123456, 0b0000}, // bit 7 of 0x78 is 0
    {"ror", 0x12345688, 8,  0b0000, 0x88123456, 0b1010}, // bit 7 of 0x88 is 1
    // clang-format on
};
} // namespace

TEST_F(ShiftTest, golden_values)
{
    for (const Golden &g : kGolden)
    {
        const ShiftOp *op = nullptr;
        for (const ShiftOp &candidate : kShiftOps)
            if (std::string(candidate.name) == g.op) op = &candidate;
        ASSERT_NE(op, nullptr) << g.op;

        const std::string ctx = std::string(g.op) + " value=" + hex32(g.value) + " #"
                                + std::to_string(g.amount)
                                + " flags_in=" + flags_to_string(flags_from_bits(g.flags_in));
        fill_registers();
        cpu.write_reg(kXn, g.value);
        set_flags(flags_from_bits(g.flags_in));
        execute(Emulator32bit::asm_format_o1(op->opcode, kXd, kXn, true, 0, g.amount, true));

        EXPECT_EQ(cpu.read_reg(kXd), g.result) << ctx;
        EXPECT_EQ(flags(), flags_from_bits(g.flags_out)) << ctx;
    }
}

TEST_F(ShiftTest, disassembles_s_suffix)
{
    EXPECT_EQ(Emulator32bit::disassemble_instr(
                  Emulator32bit::asm_format_o1(Emulator32bit::_op_lsl, 0, 1, true, 0, 5, true)),
              "lsls x0, x1, 5");
}
