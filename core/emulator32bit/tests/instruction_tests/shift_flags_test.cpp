#include <emulator32bit_test/emulator32bit_test.h>

namespace
{
// Runs `op xd=x0, x1, #amt` with the S bit set and the given starting flags.
void run_shift(Emulator32bit &cpu, const word opcode, const word x1, const int amt, const bool n,
               const bool z, const bool c, const bool v)
{
    cpu.system_bus->write_word(0, Emulator32bit::asm_format_o1(opcode, 0, 1, true, 0, amt, true));
    cpu.set_pc(0);
    cpu.write_reg(1, x1);
    cpu.set_NZCV(n, z, c, v);
    cpu.run(1);
}
} // namespace

TEST_F(EmulatorFixture, lsls_sets_flags)
{
    run_shift(cpu, Emulator32bit::_op_lsl, 0xC0000001, 1, 0, 1, 0, 1);

    EXPECT_EQ(cpu.read_reg(0), 0x80000002u);
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kNFlagBit), 1) << "N from result";
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kZFlagBit), 0) << "Z from result";
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kCFlagBit), 1) << "C is the last bit shifted out";
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kVFlagBit), 1) << "V unchanged";
}

TEST_F(EmulatorFixture, lsls_zero_result)
{
    run_shift(cpu, Emulator32bit::_op_lsl, 0x80000000, 1, 1, 0, 0, 0);

    EXPECT_EQ(cpu.read_reg(0), 0u);
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kZFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kCFlagBit), 1);
}

TEST_F(EmulatorFixture, shifts_by_zero_keep_carry)
{
    run_shift(cpu, Emulator32bit::_op_lsr, 0x80000000, 0, 0, 0, 1, 0);

    EXPECT_EQ(cpu.read_reg(0), 0x80000000u);
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kCFlagBit), 1) << "shift by 0 leaves C unchanged";
}

TEST_F(EmulatorFixture, lsrs_sets_carry_from_low_bit)
{
    run_shift(cpu, Emulator32bit::_op_lsr, 5, 1, 1, 0, 0, 0);

    EXPECT_EQ(cpu.read_reg(0), 2u);
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kZFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kCFlagBit), 1);
}

TEST_F(EmulatorFixture, asrs_sign_extends_and_sets_carry)
{
    run_shift(cpu, Emulator32bit::_op_asr, 0x80000003, 2, 0, 0, 0, 0);

    EXPECT_EQ(cpu.read_reg(0), 0xE0000000u);
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kCFlagBit), 1) << "bit 1 of the operand";
}

TEST_F(EmulatorFixture, rors_carry_is_bit_31_of_result)
{
    run_shift(cpu, Emulator32bit::_op_ror, 1, 1, 0, 0, 0, 0);

    EXPECT_EQ(cpu.read_reg(0), 0x80000000u);
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kNFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kCFlagBit), 1);
}

TEST_F(EmulatorFixture, reg_shift_of_32_or_more)
{
    cpu.system_bus->write_word(
        0, Emulator32bit::asm_format_o1(Emulator32bit::_op_lsl, 0, 1, false, 2, 0, true));
    cpu.set_pc(0);
    cpu.write_reg(1, 3);
    cpu.write_reg(2, 33);
    cpu.set_NZCV(1, 0, 1, 0);
    cpu.run(1);

    EXPECT_EQ(cpu.read_reg(0), 0u);
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kNFlagBit), 0);
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kZFlagBit), 1);
    EXPECT_EQ(cpu.get_flag(Emulator32bit::kCFlagBit), 0) << "lsl by more than 32 clears C";
}

TEST_F(EmulatorFixture, shift_disassembles_s_suffix)
{
    EXPECT_EQ(Emulator32bit::disassemble_instr(
                  Emulator32bit::asm_format_o1(Emulator32bit::_op_lsl, 0, 1, true, 0, 5, true)),
              "lsls x0, x1, 5");
}
