// The emulator calls of `swi`: printing and asserting on registers, memory and flags.

#include "emulator32bit_test/emulator32bit_test.h"

#include <sstream>

namespace
{

constexpr word kValue = 0x100;  // holds the bytes 78 56 34 12
constexpr word kString = 0x200; // holds "hi"

class SoftwareInterrupt : public EmulatorFixture
{
  protected:
    std::ostringstream out;
    std::ostringstream err;

    // The ram of the fixture is one page.
    void SetUp() override
    {
        cpu.set_output(out);
        cpu.set_error_output(err);

        const byte value[] = {0x78, 0x56, 0x34, 0x12};
        for (word i = 0; i < sizeof(value); i++) cpu.memory.write_byte(kValue + i, value[i]);
        cpu.memory.write_byte(kString, 'h');
        cpu.memory.write_byte(kString + 1, 'i');
        cpu.memory.write_byte(kString + 2, 0);
    }

    Emulator32bit::RunResult swi(const word id, const word a0 = 0, const word a1 = 0,
                                 const word a2 = 0, const word a3 = 0, const word a4 = 0)
    {
        cpu.write_reg(Register::SYSCALL, id);
        cpu.write_reg(Register::X0, a0);
        cpu.write_reg(Register::X1, a1);
        cpu.write_reg(Register::X2, a2);
        cpu.write_reg(Register::X3, a3);
        cpu.write_reg(Register::X4, a4);
        return step(0, Emulator32bit::asm_format_b1(Emulator32bit::_op_swi, ConditionCode::AL, 0));
    }
};

using Status = Emulator32bit::RunResult::Status;

} // namespace

TEST_F(SoftwareInterrupt, printm_reads_the_flag_as_little_endian_when_it_is_set)
{
    // Little endian: the first byte in memory is the least significant one.
    EXPECT_EQ(swi(1002, kValue, 4, true).status, Status::LIMIT_REACHED);
    EXPECT_EQ(out.str(), "MEM: 100 = 12345678\n");

    out.str("");
    EXPECT_EQ(swi(1002, kValue, 4, false).status, Status::LIMIT_REACHED);
    EXPECT_EQ(out.str(), "MEM: 100 = 78563412\n");
}

TEST_F(SoftwareInterrupt, printm_prints_the_bytes_it_was_asked_for)
{
    EXPECT_EQ(swi(1002, kValue, 2, true).status, Status::LIMIT_REACHED);
    EXPECT_EQ(out.str(), "MEM: 100 = 5678\n");

    out.str("");
    EXPECT_EQ(swi(1002, kValue, 1, true).status, Status::LIMIT_REACHED);
    EXPECT_EQ(out.str(), "MEM: 100 = 78\n");
}

TEST_F(SoftwareInterrupt, a_value_is_one_to_four_bytes)
{
    EXPECT_EQ(swi(1002, kValue, 0, true).status, Status::FAULT);
    EXPECT_EQ(swi(1002, kValue, 5, true).status, Status::FAULT);
    EXPECT_EQ(out.str(), "");
}

TEST_F(SoftwareInterrupt, assertm_uses_the_same_byte_order)
{
    EXPECT_EQ(swi(1011, kValue, 4, true, 0x12345678, 0x12345678).status, Status::LIMIT_REACHED);

    const auto wrong_order = swi(1011, kValue, 4, false, 0x12345678, 0x12345678);
    EXPECT_EQ(wrong_order.status, Status::FAULT);
    EXPECT_NE(wrong_order.message.find("Got 2018915346"), std::string::npos) << wrong_order.message;

    EXPECT_EQ(swi(1011, kValue, 4, false, 0x78563412, 0x78563412).status, Status::LIMIT_REACHED);
}

TEST_F(SoftwareInterrupt, printr_prints_a_register)
{
    cpu.write_reg(U8(5), 0xAB);
    EXPECT_EQ(swi(1001, 5).status, Status::LIMIT_REACHED);
    EXPECT_EQ(out.str(), "REG: 5 = ab\n");
}

TEST_F(SoftwareInterrupt, printp_prints_the_flags)
{
    cpu.set_NZCV(true, false, true, false);
    EXPECT_EQ(swi(1003).status, Status::LIMIT_REACHED);
    EXPECT_EQ(out.str(), "PSTATE: N=1,Z=0,C=1,V=0\n");
}

TEST_F(SoftwareInterrupt, print_prints_all_registers)
{
    EXPECT_EQ(swi(1000).status, Status::LIMIT_REACHED);
    EXPECT_NE(out.str().find("x29:"), std::string::npos) << out.str();
    EXPECT_NE(out.str().find("N="), std::string::npos) << out.str();
}

TEST_F(SoftwareInterrupt, assertr_and_assertp)
{
    cpu.write_reg(U8(5), 10);
    EXPECT_EQ(swi(1010, 5, 10, 10).status, Status::LIMIT_REACHED);
    EXPECT_EQ(swi(1010, 5, 11, 20).status, Status::FAULT);

    cpu.set_NZCV(false, true, false, false);
    EXPECT_EQ(swi(1012, kZFlagBit, true).status, Status::LIMIT_REACHED);
    EXPECT_EQ(swi(1012, kZFlagBit, false).status, Status::FAULT);
}

// These two were documented and implemented, but not reachable from a program.
TEST_F(SoftwareInterrupt, log_prints_a_message)
{
    EXPECT_EQ(swi(1020, kString).status, Status::LIMIT_REACHED);
    EXPECT_EQ(out.str(), "hi\n");
    EXPECT_EQ(err.str(), "");
}

TEST_F(SoftwareInterrupt, error_prints_the_message_and_stops_the_program)
{
    const auto result = swi(1021, kString);
    EXPECT_EQ(result.status, Status::FAULT);
    EXPECT_EQ(err.str(), "hi\n");
    EXPECT_EQ(out.str(), "");
    EXPECT_NE(result.message.find("hi"), std::string::npos) << result.message;
}

TEST_F(SoftwareInterrupt, a_register_that_does_not_exist_is_a_fault)
{
    EXPECT_EQ(swi(1001, 32).status, Status::FAULT);
    EXPECT_EQ(swi(1001, 256 + 5).status, Status::FAULT) << "not register 5";
    EXPECT_EQ(swi(1010, 99, 0, 0).status, Status::FAULT);
    EXPECT_EQ(out.str(), "");
}

TEST_F(SoftwareInterrupt, an_unknown_call_is_a_fault)
{
    const auto result = swi(7);
    EXPECT_EQ(result.status, Status::FAULT);
    EXPECT_NE(result.message.find("Invalid syscall number 7"), std::string::npos);
}

// The emulator calls are not instructions of the program with a vector table: a call that is
// wrong ends the run, it is not an undefined instruction that a handler would see.
TEST_F(SoftwareInterrupt, a_wrong_call_is_a_fault_and_not_an_exception_with_a_vector_table)
{
    cpu.write_sysreg(Emulator32bit::kSysregId_vbar, 0x800);
    cpu.write_reg(Register::SYSCALL, 7);
    const auto result =
        step(0, Emulator32bit::asm_format_b1(Emulator32bit::_op_swi, ConditionCode::AL, 1));

    EXPECT_EQ(result.status, Status::FAULT);
    EXPECT_NE(result.message.find("Invalid syscall number 7"), std::string::npos);
}
