// Tests for the three-operand data-processing instructions:
//   add sub rsb adc sbc rsc mul and orr eor bic   (xd = f(xn, op2), flags only if S is set)
//   cmp cmn tst teq                               (flags only, always)
//
// The arithmetic itself is covered in alu_tests. These tests check the wiring: operand decoding
// (imm14 / shifted register), the S bit, register aliasing, xzr, and that nothing else is written.
// Each op is described once in `kOps`, and the tests loop over that table.

#include <emulator32bit_test/emulator32bit_test.h>

#include <cstring>
#include <stdexcept>
#include <utility>

namespace
{
struct DpOp
{
    const char *name;
    U8 opcode;
    bool writes_dest;
    /// Expected result for the given operands and starting flags.
    AluResult (*model)(word rn, word op2, NZCVFlags in);
};

const DpOp kOps[] = {
    {"add", Emulator32bit::_op_add, true,
     [](word a, word b, NZCVFlags) { return alu_add(a, b, false); }},
    {"sub", Emulator32bit::_op_sub, true,
     [](word a, word b, NZCVFlags) { return alu_sub(a, b, true); }},
    {"rsb", Emulator32bit::_op_rsb, true,
     [](word a, word b, NZCVFlags) { return alu_sub(b, a, true); }},
    {"adc", Emulator32bit::_op_adc, true,
     [](word a, word b, NZCVFlags f) { return alu_add(a, b, f.c); }},
    {"sbc", Emulator32bit::_op_sbc, true,
     [](word a, word b, NZCVFlags f) { return alu_sub(a, b, f.c); }},
    {"rsc", Emulator32bit::_op_rsc, true,
     [](word a, word b, NZCVFlags f) { return alu_sub(b, a, f.c); }},
    {"mul", Emulator32bit::_op_mul, true,
     [](word a, word b, NZCVFlags f) { return alu_mul(a, b, f); }},
    {"udiv", Emulator32bit::_op_udiv, true,
     [](word a, word b, NZCVFlags f) { return alu_udiv(a, b, f); }},
    {"sdiv", Emulator32bit::_op_sdiv, true,
     [](word a, word b, NZCVFlags f) { return alu_sdiv(a, b, f); }},
    {"and", Emulator32bit::_op_and, true,
     [](word a, word b, NZCVFlags f) { return alu_and(a, b, f); }},
    {"orr", Emulator32bit::_op_orr, true,
     [](word a, word b, NZCVFlags f) { return alu_orr(a, b, f); }},
    {"eor", Emulator32bit::_op_eor, true,
     [](word a, word b, NZCVFlags f) { return alu_eor(a, b, f); }},
    {"bic", Emulator32bit::_op_bic, true,
     [](word a, word b, NZCVFlags f) { return alu_bic(a, b, f); }},
    {"cmp", Emulator32bit::_op_cmp, false,
     [](word a, word b, NZCVFlags) { return alu_sub(a, b, true); }},
    {"cmn", Emulator32bit::_op_cmn, false,
     [](word a, word b, NZCVFlags) { return alu_add(a, b, false); }},
    {"tst", Emulator32bit::_op_tst, false,
     [](word a, word b, NZCVFlags f) { return alu_and(a, b, f); }},
    {"teq", Emulator32bit::_op_teq, false,
     [](word a, word b, NZCVFlags f) { return alu_eor(a, b, f); }},
};

const DpOp &find_op(const char *name)
{
    for (const DpOp &op : kOps)
        if (std::strcmp(op.name, name) == 0) return op;
    throw std::logic_error(std::string("unknown op ") + name);
}

/// The second operand: either an unsigned imm14 or `xm, <shift> #amount`.
struct Op2
{
    bool is_imm;
    word value; // the immediate, or the value placed in xm
    ShiftType shift;
    unsigned amount;

    static Op2 imm(const word v)
    {
        return {true, v, ShiftType::SHIFT_LSL, 0};
    }

    static Op2 reg(const word v, const ShiftType t = ShiftType::SHIFT_LSL, const unsigned n = 0)
    {
        return {false, v, t, n};
    }

    word effective() const
    {
        return is_imm ? value : ref_shift(value, shift, amount);
    }

    std::string str() const
    {
        if (is_imm) return "#" + hex32(value);
        return "xm=" + hex32(value) + " " + shift_name(shift) + " #" + std::to_string(amount);
    }
};

class DataProcessingTest : public EmulatorFixture
{
  protected:
    static constexpr U8 kXd = 0;
    static constexpr U8 kXn = 1;
    static constexpr U8 kXm = 2;
    static constexpr U8 kXzr = U8(Register::XZR);

    static word encode(const DpOp &op, const bool s, const U8 xd, const U8 xn, const U8 xm,
                       const Op2 &op2)
    {
        return op2.is_imm
                   ? Emulator32bit::asm_format_o(op.opcode, s, xd, xn, op2.value)
                   : Emulator32bit::asm_format_o(op.opcode, s, xd, xn, xm, op2.shift, op2.amount);
    }

    /// Runs `op x0, x1, op2` and checks the result, the flags and that no other register changed.
    void run_and_check(const DpOp &op, const word rn, const Op2 &op2, const NZCVFlags in,
                       const bool s)
    {
        fill_registers();
        cpu.write_reg(kXn, rn);
        if (!op2.is_imm) cpu.write_reg(kXm, op2.value);
        set_flags(in);
        const auto before = snapshot_registers();

        const AluResult expected = op.model(rn, op2.effective(), in);
        const std::string ctx = std::string(op.name) + (s ? "s" : "") + " rn=" + hex32(rn) + " "
                                + op2.str() + " flags_in=" + flags_to_string(in);

        execute(encode(op, s, kXd, kXn, kXm, op2));

        if (op.writes_dest)
        {
            EXPECT_EQ(cpu.read_reg(kXd), word(expected.result)) << ctx;
            expect_registers_unchanged_except(before, {kXd}, ctx);
        }
        else
        {
            expect_registers_unchanged_except(before, {}, ctx);
        }

        // Compare/test instructions update the flags regardless of the S bit.
        const bool updates_flags = s || !op.writes_dest;
        EXPECT_EQ(flags(), updates_flags ? expected.flags : in) << ctx;
        EXPECT_EQ(cpu.get_pc(), 4u) << ctx << ": should fall through to the next instruction";
    }
};
} // namespace

TEST_F(DataProcessingTest, boundary_values_register_operand)
{
    for (const DpOp &op : kOps)
        for (const word rn : kBoundaryValues)
            for (const word rm : kBoundaryValues)
                for (unsigned f = 0; f < 16; ++f)
                    for (const bool s : {false, true})
                        run_and_check(op, rn, Op2::reg(rm), flags_from_bits(f), s);
}

TEST_F(DataProcessingTest, boundary_values_immediate_operand)
{
    for (const DpOp &op : kOps)
        for (const word rn : kBoundaryValues)
            for (const word imm : kImm14Values)
                for (unsigned f = 0; f < 16; ++f)
                    for (const bool s : {false, true})
                        run_and_check(op, rn, Op2::imm(imm), flags_from_bits(f), s);
}

TEST_F(DataProcessingTest, immediate_is_unsigned_14_bit)
{
    // The top bit of the imm14 field must not be sign extended.
    fill_registers();
    cpu.write_reg(kXn, 0);
    execute(Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, kXd, kXn, 0x3FFF));
    EXPECT_EQ(cpu.read_reg(kXd), 0x3FFFu);

    cpu.write_reg(kXn, 0);
    execute(Emulator32bit::asm_format_o(Emulator32bit::_op_sub, false, kXd, kXn, 0x3FFF));
    EXPECT_EQ(cpu.read_reg(kXd), 0xFFFFC001u);
}

TEST_F(DataProcessingTest, shifted_register_operand)
{
    constexpr word kRn[] = {0x00000001, 0x7FFFFFFF, 0x80000000};
    constexpr word kRm[] = {0x80000001, 0x0F0F0F0F, 0xFFFFFFFF};
    constexpr unsigned kAmounts[] = {0, 1, 5, 16, 31};

    for (const DpOp &op : kOps)
        for (const word rn : kRn)
            for (const word rm : kRm)
                for (const ShiftType t : kShiftTypes)
                    for (const unsigned n : kAmounts)
                        for (unsigned f = 0; f < 16; ++f)
                            for (const bool s : {false, true})
                                run_and_check(op, rn, Op2::reg(rm, t, n), flags_from_bits(f), s);
}

TEST_F(DataProcessingTest, shifter_carry_out_does_not_reach_the_flags)
{
    // 0x80000001 lsl #1 shifts a 1 out of bit 31. Only the ALU operation may decide C, so for the
    // logical ops it must stay as it was.
    for (const char *name : {"and", "orr", "eor", "bic", "tst", "teq"})
        for (const bool carry : {false, true})
        {
            const NZCVFlags in = {.n = false, .z = false, .c = carry, .v = false};
            run_and_check(find_op(name), 0xFFFFFFFF, Op2::reg(0x80000001, ShiftType::SHIFT_LSL, 1),
                          in, true);
            EXPECT_EQ(flags().c, carry) << name;
        }
}

TEST_F(DataProcessingTest, every_register_index_is_decoded)
{
    // xd, xn and xm are three distinct registers sliding across the whole register file, which
    // catches an operand field decoded from the wrong bit position.
    for (const DpOp &op : kOps)
        for (U8 xd = 0; xd < kNumReg; ++xd)
        {
            const U8 xn = (xd + 1) % kNumReg;
            const U8 xm = (xd + 2) % kNumReg;
            fill_registers();
            set_flags(flags_from_bits(0));
            const auto before = snapshot_registers();
            const word rn = cpu.read_reg(xn);
            const word rm = cpu.read_reg(xm);
            const AluResult expected = op.model(rn, rm, flags());
            const std::string ctx = std::string(op.name) + " x" + std::to_string(xd) + ", x"
                                    + std::to_string(xn) + ", x" + std::to_string(xm);

            execute(
                Emulator32bit::asm_format_o(op.opcode, true, xd, xn, xm, ShiftType::SHIFT_LSL, 0));

            if (op.writes_dest && xd != kXzr)
            {
                EXPECT_EQ(cpu.read_reg(xd), word(expected.result)) << ctx;
                expect_registers_unchanged_except(before, {xd}, ctx);
            }
            else
            {
                expect_registers_unchanged_except(before, {}, ctx);
            }
            EXPECT_EQ(flags(), expected.flags) << ctx;
        }
}

TEST_F(DataProcessingTest, register_aliasing)
{
    struct Alias
    {
        const char *desc;
        U8 xd, xn, xm;
    };

    // x4 and x5 are both written with values, so an alias means the same register is read twice.
    constexpr Alias kAliases[] = {
        {"xd == xn", 4, 4, 5},
        {"xd == xm", 5, 4, 5},
        {"xn == xm", 6, 4, 4},
        {"xd == xn == xm", 4, 4, 4},
    };
    constexpr std::pair<word, word> kValues[] = {{7, 3}, {0x80000000, 0x7FFFFFFF}, {1, 0xFFFFFFFF}};

    for (const DpOp &op : kOps)
        for (const Alias &alias : kAliases)
            for (const auto &[a, b] : kValues)
                for (const unsigned f : {0u, 2u, 15u})
                {
                    cpu.reset();
                    cpu.write_reg(4, a);
                    cpu.write_reg(5, b);
                    set_flags(flags_from_bits(f));
                    const word rn = cpu.read_reg(alias.xn);
                    const word rm = cpu.read_reg(alias.xm);
                    const AluResult expected = op.model(rn, rm, flags());
                    const std::string ctx = std::string(op.name) + " " + alias.desc
                                            + " a=" + hex32(a) + " b=" + hex32(b);

                    execute(Emulator32bit::asm_format_o(op.opcode, true, alias.xd, alias.xn,
                                                        alias.xm, ShiftType::SHIFT_LSL, 0));

                    if (op.writes_dest)
                    {
                        EXPECT_EQ(cpu.read_reg(alias.xd), word(expected.result)) << ctx;
                    }
                    EXPECT_EQ(flags(), expected.flags) << ctx;
                }
}

TEST_F(DataProcessingTest, zero_register_reads_as_zero)
{
    for (const DpOp &op : kOps)
        for (const word value : kBoundaryValues)
        {
            const std::string ctx = std::string(op.name) + " value=" + hex32(value);

            // op x0, xzr, x2
            fill_registers();
            cpu.write_reg(kXm, value);
            set_flags(flags_from_bits(0));
            AluResult expected = op.model(0, value, flags());
            execute(Emulator32bit::asm_format_o(op.opcode, true, kXd, kXzr, kXm,
                                                ShiftType::SHIFT_LSL, 0));
            if (op.writes_dest)
            {
                EXPECT_EQ(cpu.read_reg(kXd), word(expected.result)) << ctx;
            }
            EXPECT_EQ(flags(), expected.flags) << ctx << " (xzr as xn)";

            // op x0, x1, xzr
            fill_registers();
            cpu.write_reg(kXn, value);
            set_flags(flags_from_bits(0));
            expected = op.model(value, 0, flags());
            execute(Emulator32bit::asm_format_o(op.opcode, true, kXd, kXn, kXzr,
                                                ShiftType::SHIFT_LSL, 0));
            if (op.writes_dest)
            {
                EXPECT_EQ(cpu.read_reg(kXd), word(expected.result)) << ctx;
            }
            EXPECT_EQ(flags(), expected.flags) << ctx << " (xzr as xm)";
        }
}

TEST_F(DataProcessingTest, zero_register_destination_discards_the_result_but_sets_flags)
{
    // The assembler encodes cmp/cmn/tst/teq by injecting xzr as the destination, so this is the
    // same path for the ops that normally write a register.
    for (const DpOp &op : kOps)
    {
        fill_registers();
        cpu.write_reg(kXn, 0xFFFFFFFF);
        cpu.write_reg(kXm, 1);
        set_flags(flags_from_bits(0));
        const auto before = snapshot_registers();
        const AluResult expected = op.model(0xFFFFFFFF, 1, flags());

        execute(
            Emulator32bit::asm_format_o(op.opcode, true, kXzr, kXn, kXm, ShiftType::SHIFT_LSL, 0));

        EXPECT_EQ(cpu.read_reg(kXzr), 0u) << op.name;
        expect_registers_unchanged_except(before, {}, op.name);
        EXPECT_EQ(flags(), expected.flags) << op.name;
    }
}

namespace
{
struct Golden
{
    const char *op;
    word rn;
    word op2;
    unsigned flags_in; // bits: N=8 Z=4 C=2 V=1
    word result;       // ignored by cmp/cmn/tst/teq
    unsigned flags_out;
};

// Hand-computed, so that the tests above are not only checking the ALU against itself.
const Golden kGolden[] = {
    // clang-format off
    // add: signed overflow, carry out, zero
    {"add", 0x7FFFFFFF, 1,          0, 0x80000000, 0b1001},
    {"add", 0xFFFFFFFF, 1,          0, 0x00000000, 0b0110},
    {"add", 0x80000000, 0x80000000, 0, 0x00000000, 0b0111},
    {"add", 0,          0,          0, 0x00000000, 0b0100},
    // sub: C is "no borrow"
    {"sub", 5,          5,          0, 0x00000000, 0b0110},
    {"sub", 0,          1,          0, 0xFFFFFFFF, 0b1000},
    {"sub", 0x80000000, 1,          0, 0x7FFFFFFF, 0b0011},
    {"sub", 0x7FFFFFFF, 0xFFFFFFFF, 0, 0x80000000, 0b1001},
    // rsb: operands swapped
    {"rsb", 3,          10,         0, 7,          0b0010},
    {"rsb", 10,         3,          0, 0xFFFFFFF9, 0b1000},
    // adc: consumes C
    {"adc", 0xFFFFFFFF, 0,          0b0010, 0x00000000, 0b0110},
    {"adc", 1,          1,          0b0010, 3,          0b0000},
    {"adc", 1,          1,          0,      2,          0b0000},
    // sbc: a - b - !C
    {"sbc", 5,          3,          0b0010, 2,          0b0010},
    {"sbc", 5,          3,          0,      1,          0b0010},
    {"sbc", 5,          5,          0,      0xFFFFFFFF, 0b1000},
    {"sbc", 0x80000000, 0,          0,      0x7FFFFFFF, 0b0011},
    {"sbc", 0,          0x7FFFFFFF, 0,      0x80000000, 0b1000},
    // rsc: b - a - !C
    {"rsc", 3,          10,         0,      6,          0b0010},
    {"rsc", 10,         3,          0b0010, 0xFFFFFFF9, 0b1000},
    // mul: N and Z from the low 32 bits, C and V preserved
    {"mul", 0x10000,    0x10000,    0b0011, 0,          0b0111},
    {"mul", 0xFFFFFFFF, 0xFFFFFFFF, 0,      1,          0b0000},
    {"mul", 0x80000000, 1,          0,      0x80000000, 0b1000},
    {"mul", 3,          0,          0,      0,          0b0100},
    {"mul", 0xFFFFFFFF, 2,          0,      0xFFFFFFFE, 0b1000},
    // logical ops: N and Z only, C and V preserved
    {"and", 0xF0F0F0F0, 0xFF00FF00, 0b0011, 0xF000F000, 0b1011},
    {"and", 0xF0,       0x0F,       0,      0,          0b0100},
    {"orr", 0x80000000, 1,          0,      0x80000001, 0b1000},
    {"eor", 0xFFFFFFFF, 0xFFFFFFFF, 0b0010, 0,          0b0110},
    {"bic", 0xFFFFFFFF, 0xFF,       0,      0xFFFFFF00, 0b1000},
    {"bic", 0xFF,       0xFF,       0,      0,          0b0100},
    // compare/test: flags only
    {"cmp", 5,          5,          0,      0,          0b0110},
    {"cmp", 3,          5,          0,      0,          0b1000},
    {"cmp", 0x80000000, 1,          0,      0,          0b0011},
    {"cmn", 0xFFFFFFFF, 1,          0,      0,          0b0110},
    {"cmn", 1,          1,          0,      0,          0b0000},
    {"tst", 0xF0,       0x0F,       0b0010, 0,          0b0110},
    {"tst", 0x80000000, 0x80000000, 0,      0,          0b1000},
    {"teq", 5,          5,          0,      0,          0b0100},
    {"teq", 5,          4,          0,      0,          0b0000},
    // clang-format on
};
} // namespace

TEST_F(DataProcessingTest, golden_values)
{
    for (const Golden &g : kGolden)
    {
        const DpOp &op = find_op(g.op);
        const NZCVFlags in = flags_from_bits(g.flags_in);
        const NZCVFlags out = flags_from_bits(g.flags_out);

        // Register operand always, immediate operand when it fits in imm14.
        for (const bool use_imm : {false, true})
        {
            if (use_imm && g.op2 > 0x3FFF) continue;
            const Op2 op2 = use_imm ? Op2::imm(g.op2) : Op2::reg(g.op2);
            const std::string ctx = std::string(g.op) + " rn=" + hex32(g.rn) + " " + op2.str()
                                    + " flags_in=" + flags_to_string(in);

            fill_registers();
            cpu.write_reg(kXn, g.rn);
            if (!use_imm) cpu.write_reg(kXm, g.op2);
            set_flags(in);
            execute(encode(op, true, kXd, kXn, kXm, op2));

            if (op.writes_dest) EXPECT_EQ(cpu.read_reg(kXd), g.result) << ctx;
            else EXPECT_EQ(cpu.read_reg(kXd), 0xA5000000u) << ctx << ": xd must not be written";
            EXPECT_EQ(flags(), out) << ctx;
        }
    }
}

TEST_F(DataProcessingTest, flags_untouched_without_s_bit_golden)
{
    // Spot check with hand-picked values: an op that would set every flag leaves them alone.
    fill_registers();
    cpu.write_reg(kXn, 0x80000000);
    cpu.write_reg(kXm, 0x80000000);
    set_flags(flags_from_bits(0b1010));
    execute(Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, kXd, kXn, kXm,
                                        ShiftType::SHIFT_LSL, 0));
    EXPECT_EQ(cpu.read_reg(kXd), 0u);
    EXPECT_EQ(flags(), flags_from_bits(0b1010));
}
