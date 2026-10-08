// What a program that accesses memory it may not access sees: it stops with a fault, and the
// instruction that faulted did nothing (no register changed, nothing was stored).

#include "emulator32bit_test/emulator32bit_test.h"

#include <emulator32bit/virtual_memory.h>

namespace
{

constexpr word kCode = 0x0000;     // read, write and execute, to put the program there
constexpr word kWritable = 0x1000; // read and write
constexpr word kReadOnly = 0x2000; // read only
constexpr word kUnmapped = 0x5000;

class AccessFault : public ::testing::Test
{
  protected:
    Emulator32bit cpu{std::make_unique<RAM>(16, 0), std::make_unique<ROM>(16, 16),
                      std::make_unique<MockDisk>()};

    void SetUp() override
    {
        VirtualMemory &mmu = *cpu.system_bus->mmu;
        const long long pid = mmu.begin_process();
        mmu.add_vpage(pid, kCode >> kNumPageOffsetBits, 1, true, true);
        mmu.add_vpage(pid, kWritable >> kNumPageOffsetBits, 1, true, false);
        mmu.add_vpage(pid, kReadOnly >> kNumPageOffsetBits, 1, false, false);
    }

    /// Runs the instruction, followed by a halt.
    Emulator32bit::RunResult run(const word instruction)
    {
        cpu.system_bus->write_word(kCode, instruction);
        cpu.system_bus->write_word(kCode + 4, Emulator32bit::asm_hlt());
        cpu.set_pc(kCode);
        return cpu.run(0);
    }
};

using Status = Emulator32bit::RunResult::Status;

} // namespace

TEST_F(AccessFault, a_store_to_a_page_that_is_not_writable_is_a_fault)
{
    cpu.write_reg(U8(1), kReadOnly);
    cpu.write_reg(U8(2), 0x1234);

    const auto result = run(Emulator32bit::asm_format_m(Emulator32bit::_op_str, false, 2, 1, 0,
                                                        Emulator32bit::AddrType::ADDR_OFFSET));

    EXPECT_EQ(result.status, Status::FAULT);
    EXPECT_EQ(result.instructions_ran, 0u);
    EXPECT_NE(result.message.find("read-only"), std::string::npos) << result.message;
    EXPECT_EQ(cpu.get_pc(), kCode) << "the pc is at the instruction that faulted";
}

TEST_F(AccessFault, a_store_to_a_page_that_is_writable_works)
{
    cpu.write_reg(U8(1), kWritable);
    cpu.write_reg(U8(2), 0x1234);

    const auto result = run(Emulator32bit::asm_format_m(Emulator32bit::_op_str, false, 2, 1, 0,
                                                        Emulator32bit::AddrType::ADDR_OFFSET));

    EXPECT_EQ(result.status, Status::HALTED);
    EXPECT_EQ(cpu.system_bus->read_word(kWritable), 0x1234u);
}

TEST_F(AccessFault, a_store_that_faults_does_not_change_the_base_register)
{
    cpu.write_reg(U8(1), kReadOnly);
    cpu.write_reg(U8(2), 0x1234);

    const auto pre = run(Emulator32bit::asm_format_m(Emulator32bit::_op_str, false, 2, 1, 4,
                                                     Emulator32bit::AddrType::ADDR_PRE_INC));
    EXPECT_EQ(pre.status, Status::FAULT);
    EXPECT_EQ(cpu.read_reg(U8(1)), kReadOnly);

    const auto post = run(Emulator32bit::asm_format_m(Emulator32bit::_op_str, false, 2, 1, 4,
                                                      Emulator32bit::AddrType::ADDR_POST_INC));
    EXPECT_EQ(post.status, Status::FAULT);
    EXPECT_EQ(cpu.read_reg(U8(1)), kReadOnly);
}

TEST_F(AccessFault, a_load_that_faults_does_not_change_any_register)
{
    cpu.write_reg(U8(1), kUnmapped);
    cpu.write_reg(U8(3), 99);

    for (const auto mode :
         {Emulator32bit::AddrType::ADDR_OFFSET, Emulator32bit::AddrType::ADDR_PRE_INC,
          Emulator32bit::AddrType::ADDR_POST_INC})
    {
        const auto result =
            run(Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 3, 1, 4, mode));
        EXPECT_EQ(result.status, Status::FAULT);
        EXPECT_NE(result.message.find("unmapped"), std::string::npos) << result.message;
        EXPECT_EQ(cpu.read_reg(U8(1)), kUnmapped);
        EXPECT_EQ(cpu.read_reg(U8(3)), 99u);
    }
}

TEST_F(AccessFault, a_load_into_the_base_register_still_wins_over_the_write_back)
{
    cpu.system_bus->write_word(kWritable, 0xCAFE);
    cpu.write_reg(U8(1), kWritable);

    const auto result = run(Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 1, 1, 4,
                                                        Emulator32bit::AddrType::ADDR_POST_INC));
    EXPECT_EQ(result.status, Status::HALTED);
    EXPECT_EQ(cpu.read_reg(U8(1)), 0xCAFEu);
}

TEST_F(AccessFault, an_atomic_that_faults_does_not_change_the_result_register)
{
    cpu.write_reg(U8(1), kReadOnly); // address
    cpu.write_reg(U8(2), 77);        // result
    cpu.write_reg(U8(3), 5);         // operand

    const auto result = run(Emulator32bit::asm_atomic(2, 3, 1, Emulator32bit::kAtomicWidth_word,
                                                      Emulator32bit::kAtomicId_swp));
    EXPECT_EQ(result.status, Status::FAULT);
    EXPECT_EQ(cpu.read_reg(U8(2)), 77u);
}

TEST_F(AccessFault, a_store_across_a_page_boundary_that_faults_stores_nothing)
{
    // The first two bytes are in the page that can be written, the last two are not.
    const word address = kReadOnly - 2;
    cpu.system_bus->write_byte(address, 0x11);
    cpu.system_bus->write_byte(address + 1, 0x22);

    EXPECT_THROW(cpu.system_bus->write_word(address, 0xAABBCCDD),
                 VirtualMemory::PageFaultException);
    EXPECT_EQ(cpu.system_bus->read_byte(address), 0x11);
    EXPECT_EQ(cpu.system_bus->read_byte(address + 1), 0x22);
}

TEST_F(AccessFault, the_code_cannot_be_executed_from_a_page_that_is_not_executable)
{
    // A jump to the data.
    cpu.write_reg(U8(1), kWritable);
    const auto result =
        run(Emulator32bit::asm_format_b2(Emulator32bit::_op_bx, ConditionCode::AL, 1));
    EXPECT_EQ(result.status, Status::FAULT);
    EXPECT_NE(result.message.find("Execute permission denied"), std::string::npos)
        << result.message;
}
