// Tests for mov and mvn: xd = operand (mov) or ~operand (mvn).
//
// The operand is either an unsigned imm19, or `xn + imm14` (so `mov xd, xn` is the imm14 == 0 case).
// With the S bit set N and Z come from the result and C and V are preserved.

#include <emulator32bit_test/emulator32bit_test.h>

namespace
{
struct MoveOp
{
    const char *name;
    U8 opcode;
    word (*apply)(word operand);
};

constexpr MoveOp kMoveOps[] = {
    {"mov", Emulator32bit::_op_mov, [](word v) { return v; }},
    {"mvn", Emulator32bit::_op_mvn, [](word v) { return word(~v); }},
};

constexpr word kImm19Values[] = {0, 1, 0x3FFF, 0x4000, 0x40000, 0x7FFFF};

class MoveTest : public EmulatorFixture
{
  protected:
    static constexpr U8 kXd = 0;
    static constexpr U8 kXn = 1;
    static constexpr U8 kXzr = U8(Register::XZR);

    static NZCVFlags expected_flags(const word result, const NZCVFlags in)
    {
        return {.n = bool(result >> 31), .z = result == 0, .c = in.c, .v = in.v};
    }

    void check(const MoveOp &op, const word instr, const word operand, const NZCVFlags in,
               const bool s, const std::string &desc)
    {
        fill_registers();
        set_flags(in);
        const auto before = snapshot_registers();
        const word expected = op.apply(operand);
        const std::string ctx =
            std::string(op.name) + (s ? "s " : " ") + desc + " flags_in=" + flags_to_string(in);

        execute(instr);

        EXPECT_EQ(cpu.read_reg(kXd), expected) << ctx;
        expect_registers_unchanged_except(before, {kXd}, ctx);
        EXPECT_EQ(flags(), s ? expected_flags(expected, in) : in) << ctx;
        EXPECT_EQ(cpu.get_pc(), 4u) << ctx;
    }
};
} // namespace

TEST_F(MoveTest, immediate19)
{
    for (const MoveOp &op : kMoveOps)
        for (const word imm : kImm19Values)
            for (unsigned f = 0; f < 16; ++f)
                for (const bool s : {false, true})
                    check(op, Emulator32bit::asm_format_o3(op.opcode, s, kXd, imm), imm,
                          flags_from_bits(f), s, "#" + hex32(imm));
}

TEST_F(MoveTest, immediate19_is_not_sign_extended)
{
    // The top bit of the imm19 field is set here, yet the value stays positive.
    for (const MoveOp &op : kMoveOps)
    {
        fill_registers();
        execute(Emulator32bit::asm_format_o3(op.opcode, false, kXd, 0x7FFFF));
        EXPECT_EQ(cpu.read_reg(kXd), op.apply(0x7FFFF)) << op.name;
    }
}

TEST_F(MoveTest, register_plus_immediate14)
{
    for (const MoveOp &op : kMoveOps)
        for (const word rn : kBoundaryValues)
            for (const word imm : kImm14Values)
                for (unsigned f = 0; f < 16; f += 5)
                    for (const bool s : {false, true})
                    {
                        const NZCVFlags in = flags_from_bits(f);
                        fill_registers();
                        cpu.write_reg(kXn, rn);
                        set_flags(in);
                        const auto before = snapshot_registers();
                        const word expected = op.apply(rn + imm); // wraps modulo 2^32
                        const std::string ctx = std::string(op.name) + (s ? "s" : "")
                                                + " rn=" + hex32(rn) + " + " + hex32(imm)
                                                + " flags_in=" + flags_to_string(in);

                        execute(Emulator32bit::asm_format_o3(op.opcode, s, kXd, kXn, imm));

                        EXPECT_EQ(cpu.read_reg(kXd), expected) << ctx;
                        expect_registers_unchanged_except(before, {kXd}, ctx);
                        EXPECT_EQ(flags(), s ? expected_flags(expected, in) : in) << ctx;
                    }
}

TEST_F(MoveTest, register_copy_with_zero_offset)
{
    // `mov xd, xn` is encoded as xn + 0.
    fill_registers();
    cpu.write_reg(kXn, 0xDEADBEEF);
    execute(Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, false, kXd, kXn, 0));
    EXPECT_EQ(cpu.read_reg(kXd), 0xDEADBEEFu);
    EXPECT_EQ(cpu.read_reg(kXn), 0xDEADBEEFu);

    execute(Emulator32bit::asm_format_o3(Emulator32bit::_op_mvn, false, kXd, kXn, 0));
    EXPECT_EQ(cpu.read_reg(kXd), 0x21524110u);
    EXPECT_EQ(cpu.read_reg(kXn), 0xDEADBEEFu);
}

TEST_F(MoveTest, destination_may_be_the_source)
{
    // mov x1, x1 + 5
    fill_registers();
    cpu.write_reg(kXn, 100);
    execute(Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, false, kXn, kXn, 5));
    EXPECT_EQ(cpu.read_reg(kXn), 105u);

    // mvn x1, x1
    cpu.write_reg(kXn, 0x0F0F0F0F);
    execute(Emulator32bit::asm_format_o3(Emulator32bit::_op_mvn, false, kXn, kXn, 0));
    EXPECT_EQ(cpu.read_reg(kXn), 0xF0F0F0F0u);
}

TEST_F(MoveTest, every_register_index_is_decoded)
{
    for (const MoveOp &op : kMoveOps)
        for (U8 xd = 0; xd < kNumReg; ++xd)
        {
            const U8 xn = (xd + 1) % kNumReg;
            fill_registers();
            set_flags(flags_from_bits(0));
            const auto before = snapshot_registers();
            const word expected = op.apply(cpu.read_reg(xn) + 3);
            const std::string ctx = std::string(op.name) + " x" + std::to_string(xd) + ", x"
                                    + std::to_string(xn) + " + 3";

            execute(Emulator32bit::asm_format_o3(op.opcode, true, xd, xn, 3));

            if (xd != kXzr)
            {
                EXPECT_EQ(cpu.read_reg(xd), expected) << ctx;
                expect_registers_unchanged_except(before, {xd}, ctx);
            }
            else
            {
                expect_registers_unchanged_except(before, {}, ctx);
            }
            EXPECT_EQ(flags(), expected_flags(expected, {})) << ctx;
        }

    // Same sweep for the imm19 form, whose destination field is at a different offset.
    for (const MoveOp &op : kMoveOps)
        for (U8 xd = 0; xd < kNumReg - 1; ++xd)
        {
            fill_registers();
            const auto before = snapshot_registers();
            execute(Emulator32bit::asm_format_o3(op.opcode, false, xd, 0x12345));
            EXPECT_EQ(cpu.read_reg(xd), op.apply(0x12345)) << op.name << " x" << int(xd);
            expect_registers_unchanged_except(before, {xd}, op.name);
        }
}

TEST_F(MoveTest, zero_register)
{
    for (const MoveOp &op : kMoveOps)
    {
        // xzr as the source reads as 0, so the operand is just the immediate.
        fill_registers();
        execute(Emulator32bit::asm_format_o3(op.opcode, false, kXd, kXzr, 7));
        EXPECT_EQ(cpu.read_reg(kXd), op.apply(7)) << op.name;

        // xzr as the destination discards the value but the S bit still updates the flags.
        fill_registers();
        set_flags(flags_from_bits(0));
        const auto before = snapshot_registers();
        execute(Emulator32bit::asm_format_o3(op.opcode, true, kXzr, 0));
        EXPECT_EQ(cpu.read_reg(kXzr), 0u) << op.name;
        expect_registers_unchanged_except(before, {}, op.name);
        EXPECT_EQ(flags(), expected_flags(op.apply(0), {})) << op.name;
    }
}

TEST_F(MoveTest, golden_values)
{
    struct Case
    {
        word instr;
        word xn;
        unsigned flags_in; // bits: N=8 Z=4 C=2 V=1
        word result;
        unsigned flags_out;
    };

    constexpr U8 kMov = Emulator32bit::_op_mov;
    constexpr U8 kMvn = Emulator32bit::_op_mvn;
    const Case kCases[] = {
        // clang-format off
        {Emulator32bit::asm_format_o3(kMov, false, kXd, 9),            0, 0b0010, 9,          0b0010},
        {Emulator32bit::asm_format_o3(kMov, true,  kXd, 0),            0, 0b0010, 0,          0b0110}, // Z set, C kept
        {Emulator32bit::asm_format_o3(kMov, true,  kXd, 0x7FFFF),      0, 0b1101, 0x7FFFF,    0b0001}, // N and Z cleared, V kept
        {Emulator32bit::asm_format_o3(kMvn, true,  kXd, 0),            0, 0b0010, 0xFFFFFFFF, 0b1010}, // N set, C kept
        {Emulator32bit::asm_format_o3(kMvn, true,  kXd, 0x7FFFF),      0, 0b0000, 0xFFF80000, 0b1000},
        {Emulator32bit::asm_format_o3(kMvn, false, kXd, 0x7FFFF),      0, 0b0100, 0xFFF80000, 0b0100}, // S clear: flags kept
        {Emulator32bit::asm_format_o3(kMov, true,  kXd, kXn, 1),       0xFFFFFFFF, 0b0000, 0, 0b0100}, // wraps to 0
        {Emulator32bit::asm_format_o3(kMov, true,  kXd, kXn, 0),       0x80000000, 0b0001, 0x80000000, 0b1001},
        {Emulator32bit::asm_format_o3(kMvn, true,  kXd, kXn, 0x3FFF),  0xFFFFC000, 0b0000, 0, 0b0100}, // ~(0xFFFFC000 + 0x3FFF) = ~0xFFFFFFFF
        // clang-format on
    };

    for (const Case &c : kCases)
    {
        fill_registers();
        cpu.write_reg(kXn, c.xn);
        set_flags(flags_from_bits(c.flags_in));
        execute(c.instr);
        EXPECT_EQ(cpu.read_reg(kXd), c.result) << hex32(c.instr);
        EXPECT_EQ(flags(), flags_from_bits(c.flags_out)) << hex32(c.instr);
    }
}
