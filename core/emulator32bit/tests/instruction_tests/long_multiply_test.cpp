// Tests for umull and smull: {xhi:xlo} = xn * xm as a 64-bit product.
//
// With the S bit set N is bit 63 of the product, Z means the whole 64-bit product is zero (not just
// one half), and C and V are preserved.

#include <emulator32bit_test/emulator32bit_test.h>

namespace
{
struct LongMulOp
{
    const char *name;
    U8 opcode;
    dword (*product)(word a, word b);
};

constexpr LongMulOp kLongMulOps[] = {
    {"umull", Emulator32bit::_op_umull, [](word a, word b) { return dword(a) * dword(b); }},
    {"smull", Emulator32bit::_op_smull,
     [](word a, word b) { return dword(S64(S32(a)) * S64(S32(b))); }},
};

class LongMultiplyTest : public EmulatorFixture
{
  protected:
    static constexpr U8 kXlo = 0;
    static constexpr U8 kXhi = 1;
    static constexpr U8 kXn = 2;
    static constexpr U8 kXm = 3;
    static constexpr U8 kXzr = U8(Register::XZR);

    static NZCVFlags expected_flags(const dword product, const NZCVFlags in)
    {
        return {.n = bool(product >> 63), .z = product == 0, .c = in.c, .v = in.v};
    }

    void check(const LongMulOp &op, const word a, const word b, const NZCVFlags in, const bool s)
    {
        fill_registers();
        cpu.write_reg(kXn, a);
        cpu.write_reg(kXm, b);
        set_flags(in);
        const auto before = snapshot_registers();
        const dword product = op.product(a, b);
        const std::string ctx = std::string(op.name) + (s ? "s" : "") + " a=" + hex32(a)
                                + " b=" + hex32(b) + " flags_in=" + flags_to_string(in);

        execute(Emulator32bit::asm_format_o2(op.opcode, s, kXlo, kXhi, kXn, kXm));

        EXPECT_EQ(cpu.read_reg(kXlo), word(product)) << ctx << ": low word";
        EXPECT_EQ(cpu.read_reg(kXhi), word(product >> 32)) << ctx << ": high word";
        expect_registers_unchanged_except(before, {kXlo, kXhi}, ctx);
        EXPECT_EQ(flags(), s ? expected_flags(product, in) : in) << ctx;
        EXPECT_EQ(cpu.get_pc(), 4u) << ctx;
    }
};
} // namespace

TEST_F(LongMultiplyTest, boundary_values)
{
    for (const LongMulOp &op : kLongMulOps)
        for (const word a : kBoundaryValues)
            for (const word b : kBoundaryValues)
                for (unsigned f = 0; f < 16; ++f)
                    for (const bool s : {false, true}) check(op, a, b, flags_from_bits(f), s);
}

TEST_F(LongMultiplyTest, register_aliasing)
{
    struct Alias
    {
        const char *desc;
        U8 xlo, xhi, xn, xm;
    };

    // x4 and x5 hold the operands; every alias reads the operands before any result is written.
    constexpr Alias kAliases[] = {
        {"xlo == xn", 4, 6, 4, 5},
        {"xlo == xm", 5, 6, 4, 5},
        {"xhi == xn", 6, 4, 4, 5},
        {"xhi == xm", 6, 5, 4, 5},
        {"xlo == xn, xhi == xm", 4, 5, 4, 5},
        {"xlo == xm, xhi == xn", 5, 4, 4, 5},
        {"xn == xm", 6, 7, 4, 4},
        {"xlo == xn == xm", 4, 7, 4, 4},
    };

    for (const LongMulOp &op : kLongMulOps)
        for (const Alias &alias : kAliases)
            for (const auto &[a, b] :
                 {std::pair<word, word>{0xFFFFFFFF, 0xFFFFFFFF},
                  std::pair<word, word>{0x80000000, 3}, std::pair<word, word>{12345, 67890}})
            {
                cpu.reset();
                cpu.write_reg(4, a);
                cpu.write_reg(5, b);
                const dword product = op.product(cpu.read_reg(alias.xn), cpu.read_reg(alias.xm));
                const std::string ctx =
                    std::string(op.name) + " " + alias.desc + " a=" + hex32(a) + " b=" + hex32(b);

                execute(Emulator32bit::asm_format_o2(op.opcode, true, alias.xlo, alias.xhi,
                                                     alias.xn, alias.xm));

                EXPECT_EQ(cpu.read_reg(alias.xlo), word(product)) << ctx << ": low word";
                EXPECT_EQ(cpu.read_reg(alias.xhi), word(product >> 32)) << ctx << ": high word";
                EXPECT_EQ(flags(), expected_flags(product, {})) << ctx;
            }
}

TEST_F(LongMultiplyTest, every_register_index_is_decoded)
{
    for (const LongMulOp &op : kLongMulOps)
        for (U8 xlo = 0; xlo < kNumReg; ++xlo)
        {
            const U8 xhi = (xlo + 1) % kNumReg;
            const U8 xn = (xlo + 2) % kNumReg;
            const U8 xm = (xlo + 3) % kNumReg;
            fill_registers();
            set_flags(flags_from_bits(0));
            const auto before = snapshot_registers();
            const dword product = op.product(cpu.read_reg(xn), cpu.read_reg(xm));
            const std::string ctx = std::string(op.name) + " xlo=x" + std::to_string(xlo) + " xhi=x"
                                    + std::to_string(xhi) + " xn=x" + std::to_string(xn) + " xm=x"
                                    + std::to_string(xm);

            execute(Emulator32bit::asm_format_o2(op.opcode, true, xlo, xhi, xn, xm));

            if (xlo != kXzr)
            {
                EXPECT_EQ(cpu.read_reg(xlo), word(product)) << ctx;
            }
            if (xhi != kXzr)
            {
                EXPECT_EQ(cpu.read_reg(xhi), word(product >> 32)) << ctx;
            }
            expect_registers_unchanged_except(before, {xlo, xhi}, ctx);
            EXPECT_EQ(flags(), expected_flags(product, {})) << ctx;
        }
}

TEST_F(LongMultiplyTest, zero_register)
{
    for (const LongMulOp &op : kLongMulOps)
    {
        // xzr as an operand multiplies by zero.
        fill_registers();
        cpu.write_reg(kXn, 0x12345678);
        set_flags(flags_from_bits(0b0011));
        execute(Emulator32bit::asm_format_o2(op.opcode, true, kXlo, kXhi, kXn, kXzr));
        EXPECT_EQ(cpu.read_reg(kXlo), 0u) << op.name;
        EXPECT_EQ(cpu.read_reg(kXhi), 0u) << op.name;
        EXPECT_EQ(flags(), flags_from_bits(0b0111)) << op.name;

        // xzr as the high destination discards the high word, but N and Z still use all 64 bits.
        fill_registers();
        cpu.write_reg(kXn, 0xFFFFFFFF);
        cpu.write_reg(kXm, 0xFFFFFFFF);
        set_flags(flags_from_bits(0));
        auto before = snapshot_registers();
        const dword product = op.product(0xFFFFFFFF, 0xFFFFFFFF);
        execute(Emulator32bit::asm_format_o2(op.opcode, true, kXlo, kXzr, kXn, kXm));
        EXPECT_EQ(cpu.read_reg(kXlo), word(product)) << op.name;
        EXPECT_EQ(cpu.read_reg(kXzr), 0u) << op.name;
        expect_registers_unchanged_except(before, {kXlo}, op.name);
        EXPECT_EQ(flags(), expected_flags(product, {})) << op.name;

        // xzr as the low destination.
        fill_registers();
        cpu.write_reg(kXn, 0x10000);
        cpu.write_reg(kXm, 0x10000);
        before = snapshot_registers();
        execute(Emulator32bit::asm_format_o2(op.opcode, true, kXzr, kXhi, kXn, kXm));
        EXPECT_EQ(cpu.read_reg(kXhi), 1u) << op.name;
        expect_registers_unchanged_except(before, {kXhi}, op.name);
    }
}

namespace
{
struct Golden
{
    const char *op;
    word a, b;
    unsigned flags_in; // bits: N=8 Z=4 C=2 V=1
    word lo, hi;
    unsigned flags_out;
};

// Hand-computed expectations.
const Golden kGolden[] = {
    // clang-format off
    {"umull", 2,          4,          0b0000, 8,          0,          0b0000},
    {"umull", 0xFFFFFFFF, 0xFFFFFFFF, 0b0000, 1,          0xFFFFFFFE, 0b1000}, // (2^32-1)^2
    {"umull", 0xFFFFFFFF, 1,          0b0000, 0xFFFFFFFF, 0,          0b0000}, // low word has bit 31 set, N does not
    {"umull", 0x10000,    0x10000,    0b0000, 0,          1,          0b0000}, // low word is zero, Z must not be set
    {"umull", 0,          4,          0b0011, 0,          0,          0b0111}, // C and V preserved
    {"umull", 0x80000000, 2,          0b0000, 0,          1,          0b0000},
    {"smull", 2,          4,          0b0000, 8,          0,          0b0000},
    {"smull", 0xFFFFFFFF, 0xFFFFFFFF, 0b1000, 1,          0,          0b0000}, // -1 * -1
    {"smull", 0xFFFFFFFF, 1,          0b0000, 0xFFFFFFFF, 0xFFFFFFFF, 0b1000}, // -1 * 1
    {"smull", 0x80000000, 0x80000000, 0b0000, 0,          0x40000000, 0b0000}, // 2^31 * 2^31 = 2^62
    {"smull", 0x80000000, 2,          0b0000, 0,          0xFFFFFFFF, 0b1000}, // -2^32
    {"smull", 0x7FFFFFFF, 0x7FFFFFFF, 0b0000, 1,          0x3FFFFFFF, 0b0000},
    {"smull", 0xFFFFFFFE, 3,          0b0010, 0xFFFFFFFA, 0xFFFFFFFF, 0b1010}, // -2 * 3 = -6, C kept
    // clang-format on
};
} // namespace

TEST_F(LongMultiplyTest, golden_values)
{
    for (const Golden &g : kGolden)
    {
        const LongMulOp *op = nullptr;
        for (const LongMulOp &candidate : kLongMulOps)
            if (std::string(candidate.name) == g.op) op = &candidate;
        ASSERT_NE(op, nullptr) << g.op;

        const std::string ctx = std::string(g.op) + " a=" + hex32(g.a) + " b=" + hex32(g.b)
                                + " flags_in=" + flags_to_string(flags_from_bits(g.flags_in));
        fill_registers();
        cpu.write_reg(kXn, g.a);
        cpu.write_reg(kXm, g.b);
        set_flags(flags_from_bits(g.flags_in));
        execute(Emulator32bit::asm_format_o2(op->opcode, true, kXlo, kXhi, kXn, kXm));

        EXPECT_EQ(cpu.read_reg(kXlo), g.lo) << ctx;
        EXPECT_EQ(cpu.read_reg(kXhi), g.hi) << ctx;
        EXPECT_EQ(flags(), flags_from_bits(g.flags_out)) << ctx;
    }
}
