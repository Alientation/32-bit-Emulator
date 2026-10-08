// The page tables in memory (docs/mmu.md): translation by a two level table that the program owns,
// the permission bits, accessed and dirty, user and kernel pages, TLBI, and the aborts they raise.

#include "emulator32bit_test/emulator32bit_test.h"

namespace
{

using Class = Emulator32bit::ExceptionClass;
using Status = Emulator32bit::RunResult::Status;
using VM = VirtualMemory;

constexpr word kL1 = 0x1000; // physical address of the first level table
constexpr word kV = VM::kPteValid;
constexpr word kW = VM::kPteWrite;
constexpr word kX = VM::kPteExecute;
constexpr word kU = VM::kPteUser;
constexpr word kA = VM::kPteAccessed;
constexpr word kD = VM::kPteDirty;

// Virtual layout of the tests.
constexpr word kCodeV = 0x00000;     // kernel code, ppage 8. The vectors are in it at 0x800.
constexpr word kVectors = 0x800;
constexpr word kDataV = 0x10000;     // kernel data, ppage 20
constexpr word kReadOnlyV = 0x11000; // kernel, read only, ppage 21
constexpr word kUserCodeV = 0x20000; // user code, ppage 24
constexpr word kUserDataV = 0x21000; // user data, ppage 25
constexpr word kFarV = 0x00800000;   // a second second level table
constexpr word kUnmappedV = 0x30000;

constexpr word kCodeP = 8, kDataP = 20, kReadOnlyP = 21, kUserCodeP = 24, kUserDataP = 25;

word mov(const U8 xd, const word value)
{
    return Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, false, xd, value);
}

word load(const U8 xt, const U8 xn)
{
    return Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, xt, xn, 0,
                                       Emulator32bit::AddrType::ADDR_OFFSET);
}

word store(const U8 xt, const U8 xn)
{
    return Emulator32bit::asm_format_m(Emulator32bit::_op_str, false, xt, xn, 0,
                                       Emulator32bit::AddrType::ADDR_OFFSET);
}

word swi()
{
    return Emulator32bit::asm_format_b1(Emulator32bit::_op_swi, ConditionCode::AL, 0);
}

class Mmu : public ::testing::Test
{
  protected:
    Emulator32bit cpu{new RAM(64, 0), new ROM(16, 64), new MockDisk()};
    word m_next_table = 2; // the physical page of the next second level table

    word read_physical(const word address)
    {
        return cpu.system_bus->ram->read_word(address);
    }

    void write_physical(const word address, const word value)
    {
        cpu.system_bus->ram->write_word(address, value);
    }

    /// Where the second level entry of a virtual address is (it makes the table if needed).
    word entry_address(const word vaddr)
    {
        const word first = kL1 + ((vaddr >> 22) << 2);
        if (!(read_physical(first) & kV))
        {
            write_physical(first, (m_next_table << kNumPageOffsetBits) | kV);
            m_next_table++;
        }
        return (read_physical(first) & VM::kPteFrameMask) + (((vaddr >> 12) & 0x3FF) << 2);
    }

    void map(const word vaddr, const word ppage, const word flags)
    {
        write_physical(entry_address(vaddr), (ppage << kNumPageOffsetBits) | flags | kV);
    }

    word entry(const word vaddr)
    {
        return read_physical(entry_address(vaddr));
    }

    void write_code(const word ppage, const word offset, const std::vector<word> &code)
    {
        for (size_t i = 0; i < code.size(); i++)
        {
            write_physical((ppage << kNumPageOffsetBits) + offset + word(i) * 4, code[i]);
        }
    }

    /// The usual layout, with a vector table whose handlers all halt.
    void SetUp() override
    {
        map(kCodeV, kCodeP, kX);
        map(kDataV, kDataP, kW);
        map(kReadOnlyV, kReadOnlyP, 0);
        map(kUserCodeV, kUserCodeP, kX | kU);
        map(kUserDataV, kUserDataP, kW | kU);
        for (word c = 0; c < 8; c++)
        {
            write_code(kCodeP, kVectors + 16 * c, {Emulator32bit::asm_hlt()});
        }
    }

    void turn_on()
    {
        cpu.write_sysreg(Emulator32bit::kSysregId_ptbr, kL1);
        cpu.write_sysreg(Emulator32bit::kSysregId_sctlr, 1);
        cpu.write_sysreg(Emulator32bit::kSysregId_vbar, kVectors);
    }

    Emulator32bit::RunResult run_at(const word pc, const U64 limit = 100)
    {
        cpu.set_pc(pc);
        return cpu.run(limit);
    }

    word esr_class() const
    {
        return cpu.read_sysreg(Emulator32bit::kSysregId_esr) >> 26;
    }

    word esr_iss() const
    {
        return cpu.read_sysreg(Emulator32bit::kSysregId_esr) & 0x3FFFFFF;
    }

    word far() const
    {
        return cpu.read_sysreg(Emulator32bit::kSysregId_far);
    }

    void expect_abort(const Class cls, const word iss, const word address)
    {
        EXPECT_EQ(esr_class(), word(cls));
        EXPECT_EQ(esr_iss(), iss);
        EXPECT_EQ(far(), address);
        EXPECT_EQ(cpu.get_pc(), kVectors + 16 * word(cls)) << "in the handler";
    }
};

} // namespace

TEST_F(Mmu, addresses_are_translated_by_the_tables)
{
    write_code(kCodeP, 0x100,
               {mov(1, kDataV), mov(2, 77), store(2, 1), load(3, 1), Emulator32bit::asm_hlt()});
    turn_on();

    const auto result = run_at(0x100);
    ASSERT_EQ(result.status, Status::HALTED) << result.message;
    EXPECT_EQ(cpu.read_reg(3), 77u);
    EXPECT_EQ(read_physical(kDataP << kNumPageOffsetBits), 77u) << "it is in the physical page";
}

TEST_F(Mmu, the_pages_can_be_anywhere_and_a_second_table_serves_other_regions)
{
    map(kFarV, 40, kW);
    // 0x00800000 does not fit mov's 19 bits: build it.
    write_code(kCodeP, 0x100,
               {mov(1, kFarV >> 14),
                Emulator32bit::asm_format_o1(Emulator32bit::_op_lsl, 1, 1, true, 0, 14), mov(2, 5),
                store(2, 1), Emulator32bit::asm_hlt()});
    turn_on();

    ASSERT_EQ(run_at(0x100).status, Status::HALTED);
    EXPECT_EQ(read_physical(40 << kNumPageOffsetBits), 5u);
    EXPECT_NE(read_physical(kL1 + ((kFarV >> 22) << 2)) & kV, 0u);
}

TEST_F(Mmu, the_hardware_marks_pages_accessed_and_dirty)
{
    write_code(kCodeP, 0x100, {mov(1, kDataV), load(2, 1), Emulator32bit::asm_hlt()});
    write_code(kCodeP, 0x200, {mov(1, kDataV), store(2, 1), Emulator32bit::asm_hlt()});
    turn_on();

    EXPECT_EQ(entry(kDataV) & (kA | kD), 0u);
    ASSERT_EQ(run_at(0x100).status, Status::HALTED);
    EXPECT_EQ(entry(kDataV) & (kA | kD), kA) << "a read marks it accessed";

    ASSERT_EQ(run_at(0x200).status, Status::HALTED);
    EXPECT_EQ(entry(kDataV) & (kA | kD), kA | kD) << "a write marks it dirty";
    EXPECT_NE(entry(kCodeV) & kA, 0u) << "so does the fetch of code";
}

TEST_F(Mmu, a_write_to_a_clean_page_after_a_read_still_marks_it_dirty)
{
    // The read cached the translation, the write then takes the slow path to set the bit.
    write_code(kCodeP, 0x100, {mov(1, kDataV), load(2, 1), store(2, 1), Emulator32bit::asm_hlt()});
    turn_on();
    ASSERT_EQ(run_at(0x100).status, Status::HALTED);
    EXPECT_EQ(entry(kDataV) & (kA | kD), kA | kD);
}

TEST_F(Mmu, an_unmapped_page_is_a_translation_fault)
{
    write_code(kCodeP, 0x100, {mov(1, kUnmappedV), load(2, 1), Emulator32bit::asm_hlt()});
    turn_on();

    ASSERT_EQ(run_at(0x100).status, Status::HALTED);
    expect_abort(Class::DATA_ABORT, 1, kUnmappedV);
}

TEST_F(Mmu, a_missing_second_level_table_is_a_translation_fault)
{
    write_code(kCodeP, 0x100,
               {mov(1, 0x7FFFF),
                Emulator32bit::asm_format_o1(Emulator32bit::_op_lsl, 1, 1, true, 0, 12), load(2, 1),
                Emulator32bit::asm_hlt()});
    turn_on();
    ASSERT_EQ(run_at(0x100).status, Status::HALTED);
    expect_abort(Class::DATA_ABORT, 1, 0x7FFFF000);
}

TEST_F(Mmu, a_write_to_a_read_only_page_is_a_permission_fault)
{
    write_code(kCodeP, 0x100, {mov(1, kReadOnlyV), store(2, 1), Emulator32bit::asm_hlt()});
    turn_on();

    ASSERT_EQ(run_at(0x100).status, Status::HALTED);
    expect_abort(Class::DATA_ABORT, 2 | 8, kReadOnlyV); // bit 3: it was a write
}

TEST_F(Mmu, a_fetch_from_a_page_that_is_not_executable_is_an_instruction_abort)
{
    write_code(kDataP, 0, {Emulator32bit::asm_hlt()});
    turn_on();

    ASSERT_EQ(run_at(kDataV).status, Status::HALTED);
    expect_abort(Class::INSTRUCTION_ABORT, 2, kDataV);
}

TEST_F(Mmu, a_fetch_from_an_unmapped_page_is_an_instruction_abort)
{
    turn_on();
    ASSERT_EQ(run_at(kUnmappedV).status, Status::HALTED);
    expect_abort(Class::INSTRUCTION_ABORT, 1, kUnmappedV);
}

TEST_F(Mmu, a_faulting_instruction_can_be_retried_after_the_page_is_fixed)
{
    write_code(kCodeP, 0x100,
               {mov(1, kReadOnlyV), mov(2, 9), store(2, 1), Emulator32bit::asm_hlt()});
    turn_on();
    ASSERT_EQ(run_at(0x100).status, Status::HALTED);
    expect_abort(Class::DATA_ABORT, 2 | 8, kReadOnlyV);

    // What a kernel does: change the entry, forget the old translation, resume at ELR.
    write_physical(entry_address(kReadOnlyV), entry(kReadOnlyV) | kW);
    cpu.set_pc(cpu.read_sysreg(Emulator32bit::kSysregId_elr));
    ASSERT_EQ(cpu.run(10).status, Status::HALTED);
    EXPECT_EQ(read_physical(kReadOnlyP << kNumPageOffsetBits), 9u);
}

TEST_F(Mmu, a_cached_translation_stays_until_tlbi)
{
    write_code(kCodeP, 0x100, {mov(1, kDataV), mov(2, 1), store(2, 1), Emulator32bit::asm_hlt()});
    // Writes again, after the entry was made read only by the test.
    write_code(kCodeP, 0x200, {mov(1, kDataV), mov(2, 2), store(2, 1), Emulator32bit::asm_hlt()});
    write_code(kCodeP, 0x300,
               {Emulator32bit::asm_tlbi(0, false, 0), mov(1, kDataV), mov(2, 3), store(2, 1),
                Emulator32bit::asm_hlt()});
    turn_on();
    ASSERT_EQ(run_at(0x100).status, Status::HALTED);

    write_physical(entry_address(kDataV), entry(kDataV) & ~kW); // revoke the write
    ASSERT_EQ(run_at(0x200).status, Status::HALTED);
    EXPECT_EQ(read_physical(kDataP << kNumPageOffsetBits), 2u)
        << "the old translation still worked";

    ASSERT_EQ(run_at(0x300).status, Status::HALTED);
    expect_abort(Class::DATA_ABORT, 2 | 8, kDataV);
    EXPECT_EQ(read_physical(kDataP << kNumPageOffsetBits), 2u) << "and now it is forgotten";
}

TEST_F(Mmu, tlbi_with_a_register_forgets_only_that_page)
{
    map(kDataV + 0x2000, 30, kW);
    // Cache both, then revoke both, and forget one.
    write_code(kCodeP, 0x100,
               {mov(1, kDataV), mov(3, kDataV + 0x2000 - kDataV),
                Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, 3, 1, 3,
                                            ShiftType::SHIFT_LSL, 0),
                mov(2, 1), store(2, 1), store(2, 3), Emulator32bit::asm_hlt()});
    write_code(
        kCodeP, 0x200,
        {Emulator32bit::asm_tlbi(1, true, 0), store(2, 3), store(2, 1), Emulator32bit::asm_hlt()});
    turn_on();
    ASSERT_EQ(run_at(0x100).status, Status::HALTED);

    write_physical(entry_address(kDataV), entry(kDataV) & ~kW);
    write_physical(entry_address(kDataV + 0x2000), entry(kDataV + 0x2000) & ~kW);
    ASSERT_EQ(run_at(0x200).status, Status::HALTED);
    // The store to the page that was not forgotten worked, the one to the forgotten page faulted.
    expect_abort(Class::DATA_ABORT, 2 | 8, kDataV);
}

TEST_F(Mmu, user_mode_may_only_use_user_pages)
{
    write_code(kUserCodeP, 0, {mov(1, kUserDataV), mov(2, 6), store(2, 1), load(3, 1), swi()});
    write_code(kUserCodeP, 0x100, {mov(1, kDataV), load(2, 1), swi()});
    turn_on();

    cpu.write_sysreg(Emulator32bit::kSysregId_pstate, 1 << kUserModeBit);
    ASSERT_EQ(run_at(kUserCodeV).status, Status::HALTED);
    EXPECT_EQ(esr_class(), word(Class::SUPERVISOR_CALL)) << "ran to the system call";
    EXPECT_EQ(cpu.read_reg(3), 6u);
    EXPECT_FALSE(cpu.user_mode());

    cpu.write_sysreg(Emulator32bit::kSysregId_pstate, 1 << kUserModeBit);
    ASSERT_EQ(run_at(kUserCodeV + 0x100).status, Status::HALTED);
    expect_abort(Class::DATA_ABORT, 2, kDataV); // a kernel page: a permission fault
}

TEST_F(Mmu, user_mode_cannot_fetch_from_a_kernel_page)
{
    turn_on();
    cpu.write_sysreg(Emulator32bit::kSysregId_pstate, 1 << kUserModeBit);
    ASSERT_EQ(run_at(kCodeV + 0x100).status, Status::HALTED);
    expect_abort(Class::INSTRUCTION_ABORT, 2, kCodeV + 0x100);
}

TEST_F(Mmu, the_kernel_does_not_execute_user_pages)
{
    turn_on();
    ASSERT_EQ(run_at(kUserCodeV).status, Status::HALTED);
    expect_abort(Class::INSTRUCTION_ABORT, 2, kUserCodeV);
}

TEST_F(Mmu, the_kernel_may_read_and_write_user_data)
{
    write_code(kCodeP, 0x100,
               {mov(1, kUserDataV), mov(2, 8), store(2, 1), load(3, 1), Emulator32bit::asm_hlt()});
    turn_on();
    ASSERT_EQ(run_at(0x100).status, Status::HALTED);
    EXPECT_EQ(cpu.read_reg(3), 8u);
}

TEST_F(Mmu, a_page_table_outside_of_memory_is_a_translation_fault)
{
    turn_on();
    cpu.write_sysreg(Emulator32bit::kSysregId_ptbr, 0xF0000000);
    // The vector page cannot be fetched either, so the fault is raised for the first fetch.
    const auto result = run_at(0x100);
    EXPECT_NE(result.status, Status::HALTED);
}

TEST_F(Mmu, an_access_across_a_page_boundary_translates_both_pages)
{
    map(kDataV + 0x1000, 31, kW); // the page after kDataV, in another frame
    write_code(kCodeP, 0x100,
               {mov(1, kDataV + 0xFFE), mov(2, 0x7FFFF), store(2, 1), load(3, 1),
                Emulator32bit::asm_hlt()});
    // kReadOnlyV is where kDataV + 0x1000 would be: map the pages next to each other.
    map(kReadOnlyV, 31, kW);
    turn_on();

    ASSERT_EQ(run_at(0x100).status, Status::HALTED);
    EXPECT_EQ(cpu.read_reg(3), 0x7FFFFu);
    EXPECT_EQ(cpu.system_bus->ram->read_hword((kDataP << kNumPageOffsetBits) + 0xFFE), 0xFFFFu);
    EXPECT_EQ(cpu.system_bus->ram->read_hword(31 << kNumPageOffsetBits), 0x0007u);
}

TEST_F(Mmu, sctlr_and_ptbr_keep_the_bits_that_exist)
{
    cpu.write_sysreg(Emulator32bit::kSysregId_ptbr, 0x12345678);
    EXPECT_EQ(cpu.read_sysreg(Emulator32bit::kSysregId_ptbr), 0x12345000u);
    cpu.write_sysreg(Emulator32bit::kSysregId_sctlr, 0xFFFFFFFE);
    EXPECT_EQ(cpu.read_sysreg(Emulator32bit::kSysregId_sctlr), 0u);
    EXPECT_FALSE(cpu.system_bus->mmu->walk_enabled());
    cpu.write_sysreg(Emulator32bit::kSysregId_sctlr, 0xFFFFFFFF);
    EXPECT_EQ(cpu.read_sysreg(Emulator32bit::kSysregId_sctlr), 1u);
    EXPECT_TRUE(cpu.system_bus->mmu->walk_enabled());
}

TEST_F(Mmu, tlbi_is_privileged)
{
    write_code(kUserCodeP, 0, {Emulator32bit::asm_tlbi(0, false, 0)});
    turn_on();
    cpu.write_sysreg(Emulator32bit::kSysregId_pstate, 1 << kUserModeBit);
    ASSERT_EQ(run_at(kUserCodeV).status, Status::HALTED);
    EXPECT_EQ(esr_class(), word(Class::UNDEFINED_INSTRUCTION));
    EXPECT_EQ(esr_iss(), Emulator32bit::kUndefinedIss_privileged);
}

TEST_F(Mmu, a_reset_turns_translation_off)
{
    turn_on();
    cpu.reset();
    EXPECT_FALSE(cpu.system_bus->mmu->walk_enabled());
    EXPECT_EQ(cpu.read_sysreg(Emulator32bit::kSysregId_sctlr), 0u);
}

TEST_F(Mmu, tlbi_disassembles_and_assembles_back)
{
    EXPECT_EQ(Emulator32bit::disassemble_instr(Emulator32bit::asm_tlbi(0, false, 0)), "tlbi");
    EXPECT_EQ(Emulator32bit::disassemble_instr(Emulator32bit::asm_tlbi(5, true, 0)), "tlbi x5");
}
