// Exceptions, system registers and the two privilege levels (docs/exceptions.md): what the CPU
// does with an instruction that cannot complete once a vector table is installed, how the handler
// gets back, and what user mode may not do.

#include "emulator32bit/fpu.h"
#include "emulator32bit_test/emulator32bit_test.h"
#include "emulator32bit/virtual_memory.h"

#include <sstream>

namespace
{

using Class = Emulator32bit::ExceptionClass;
using Status = Emulator32bit::RunResult::Status;

constexpr word kCode = 0x000;    // where the test programs are, up to the vectors
constexpr word kProgram = 0x100;
constexpr word kVectors = 0x800; // VBAR
constexpr word kWritable = 0x1000;
constexpr word kReadOnly = 0x2000;
constexpr word kUnmapped = 0x5000;

constexpr word kUserBit = 1 << kUserModeBit;
constexpr word kIrqMask = 1 << kIrqMaskBit;

/// The address of the handler of an exception class.
constexpr word vector(const Class cls)
{
    return kVectors + 16 * word(cls);
}

word msr(const word sysreg, const U8 xn)
{
    return Emulator32bit::asm_msr(U8(sysreg), false, xn);
}

word mrs(const U8 xd, const word sysreg)
{
    return Emulator32bit::asm_mrs(xd, U8(sysreg));
}

word mov(const U8 xd, const word value)
{
    return Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, false, xd, value);
}

word swi(const word number)
{
    return Emulator32bit::asm_format_b1(Emulator32bit::_op_swi, ConditionCode::AL, sword(number));
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

/// `stur xt, [xn]`: a word store that may be at any address.
word store_unaligned(const U8 xt, const U8 xn)
{
    return Emulator32bit::asm_format_m(Emulator32bit::_op_str, false, xt, xn, 0,
                                       Emulator32bit::AddrType::ADDR_UNALIGNED);
}

/// An opcode that is not assigned, an extended op of the special group that is not assigned, and a
/// double that starts in x29.
constexpr word kBadOpcode = 0xFC000000;
constexpr word kBadExtOp = 0b1001u << 22;
const word kBadFpPair = Emulator32bit::asm_fop2(fpu::kBinaryFn_add, true, 29, 0, 0);

class Exceptions : public ::testing::Test
{
  protected:
    Emulator32bit cpu{std::make_unique<RAM>(16, 0), std::make_unique<ROM>(16, 16),
                      std::make_unique<MockDisk>()};

    void SetUp() override
    {
        VirtualMemory &mmu = *cpu.mmu;
        const long long pid = mmu.begin_process();
        mmu.add_vpage(pid, kCode >> kNumPageOffsetBits, 1, true, true);
        mmu.add_vpage(pid, kWritable >> kNumPageOffsetBits, 1, true, false);
        mmu.add_vpage(pid, kReadOnly >> kNumPageOffsetBits, 1, false, false);
    }

    void write(const word address, const std::vector<word> &instructions)
    {
        for (size_t i = 0; i < instructions.size(); i++)
        {
            cpu.memory.write_word(address + word(i) * 4, instructions[i]);
        }
    }

    /// Installs the vector table, every handler is a `hlt` unless the test writes another one, so
    /// a run ends in the handler that was entered.
    void install_vectors()
    {
        for (word c = 0; c < 8; c++)
        {
            write(kVectors + 16 * c, {Emulator32bit::asm_hlt()});
        }
        cpu.write_sysreg(Emulator32bit::kSysregId_vbar, kVectors);
    }

    /// Runs the program at kProgram until it halts, and a limit of instructions in case it does not.
    Emulator32bit::RunResult run(const std::vector<word> &program, const U64 limit = 100)
    {
        write(kProgram, program);
        cpu.set_pc(kProgram);
        return cpu.run(limit);
    }

    word sysreg(const word id) const
    {
        return cpu.read_sysreg(U8(id));
    }

    word esr_class() const
    {
        return sysreg(Emulator32bit::kSysregId_esr) >> 26;
    }

    word esr_iss() const
    {
        return sysreg(Emulator32bit::kSysregId_esr) & 0x3FFFFFF;
    }

    /// Expects the run to have ended in the handler of `cls`, which halted.
    void expect_in_handler(const Emulator32bit::RunResult &result, const Class cls)
    {
        EXPECT_EQ(result.status, Status::HALTED) << result.message;
        EXPECT_EQ(cpu.get_pc(), vector(cls));
        EXPECT_EQ(esr_class(), word(cls));
        EXPECT_FALSE(cpu.user_mode());
        EXPECT_TRUE(cpu.get_flag(kIrqMaskBit));
    }
};

} // namespace

TEST_F(Exceptions, the_cpu_starts_in_kernel_mode_with_irqs_masked_and_no_vector_table)
{
    EXPECT_FALSE(cpu.user_mode());
    EXPECT_EQ(cpu.get_pstate(), kIrqMask);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_vbar), 0u);
}

TEST_F(Exceptions, msr_and_mrs_move_values_to_and_from_the_system_registers)
{
    const word registers[] = {Emulator32bit::kSysregId_elr, Emulator32bit::kSysregId_esr,
                              Emulator32bit::kSysregId_far, Emulator32bit::kSysregId_usp};
    // (PTBR and SCTLR keep fewer bits and have an effect, mmu_test.cpp covers them.)
    for (const word id : registers)
    {
        cpu.write_reg(U8(1), 0xCAFE0000 + id);
        const auto result = run({msr(id, 1), mrs(2, id), Emulator32bit::asm_hlt()});
        ASSERT_EQ(result.status, Status::HALTED) << "register " << id << ": " << result.message;
        EXPECT_EQ(cpu.read_reg(U8(2)), 0xCAFE0000 + id) << "register " << id;
    }

    // Only the bits that exist are kept, and a vector table is 16 byte aligned.
    cpu.write_reg(U8(1), 0xFFFFFFFF);
    run({msr(Emulator32bit::kSysregId_spsr, 1), mrs(2, Emulator32bit::kSysregId_spsr),
         msr(Emulator32bit::kSysregId_vbar, 1), mrs(3, Emulator32bit::kSysregId_vbar),
         Emulator32bit::asm_hlt()});
    EXPECT_EQ(cpu.read_reg(U8(2)), kPstateMask);
    EXPECT_EQ(cpu.read_reg(U8(3)), 0xFFFFFFF0u);
}

TEST_F(Exceptions, msr_takes_an_immediate_and_register_zero_reads_zero)
{
    run({Emulator32bit::asm_msr(Emulator32bit::kSysregId_elr, true, 0x1234),
         mrs(1, Emulator32bit::kSysregId_elr), msr(0, 1), mrs(2, 0), Emulator32bit::asm_hlt()});
    EXPECT_EQ(cpu.read_reg(U8(1)), 0x1234u);
    EXPECT_EQ(cpu.read_reg(U8(2)), 0u);
}

TEST_F(Exceptions, a_system_register_that_does_not_exist_is_a_fault_without_a_vector_table)
{
    const auto result = run({mrs(1, 20), Emulator32bit::asm_hlt()});
    EXPECT_EQ(result.status, Status::FAULT);
    EXPECT_NE(result.message.find("System register 20"), std::string::npos) << result.message;
}

TEST_F(Exceptions, without_a_vector_table_nothing_is_raised)
{
    // The same as always: the run ends with the fault, at the instruction.
    const auto result = run({kBadOpcode});
    EXPECT_EQ(result.status, Status::FAULT);
    EXPECT_EQ(cpu.get_pc(), kProgram);
    EXPECT_EQ(esr_class(), 0u);
}

TEST_F(Exceptions, an_unassigned_opcode_is_an_undefined_instruction)
{
    install_vectors();
    const auto result = run({Emulator32bit::asm_nop(), kBadOpcode});

    expect_in_handler(result, Class::UNDEFINED_INSTRUCTION);
    EXPECT_EQ(esr_iss(), Emulator32bit::kUndefinedIss_opcode);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_elr), kProgram + 4) << "the instruction itself";
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_spsr), kIrqMask) << "PSTATE before";
    EXPECT_EQ(result.instructions_ran, 1u) << "the nop, the faulting instruction did not complete";
}

TEST_F(Exceptions, the_syndrome_tells_why_an_instruction_is_undefined)
{
    install_vectors();

    expect_in_handler(run({kBadExtOp}), Class::UNDEFINED_INSTRUCTION);
    EXPECT_EQ(esr_iss(), Emulator32bit::kUndefinedIss_ext_op);

    expect_in_handler(run({kBadFpPair}), Class::UNDEFINED_INSTRUCTION);
    EXPECT_EQ(esr_iss(), Emulator32bit::kUndefinedIss_fp_operand);

    expect_in_handler(run({mrs(1, 20)}), Class::UNDEFINED_INSTRUCTION);
    EXPECT_EQ(esr_iss(), Emulator32bit::kUndefinedIss_sysreg);
}

TEST_F(Exceptions, a_handler_that_faults_again_after_running_is_not_a_double_fault)
{
    install_vectors();
    write(vector(Class::UNDEFINED_INSTRUCTION), {Emulator32bit::asm_nop(), kBadOpcode});

    // Every round runs the nop, so the loop only ends with the limit.
    const auto result = run({kBadOpcode}, 50);
    EXPECT_EQ(result.status, Status::LIMIT_REACHED) << result.message;
}

TEST_F(Exceptions, a_fault_before_any_instruction_of_the_handler_ran_is_a_double_fault)
{
    // The vector table is in a page that is not mapped.
    cpu.write_sysreg(Emulator32bit::kSysregId_vbar, kUnmapped);
    const auto result = run({kBadOpcode});

    EXPECT_EQ(result.status, Status::FAULT);
    EXPECT_NE(result.message.find("Double fault"), std::string::npos) << result.message;
}

TEST_F(Exceptions, swi_is_a_supervisor_call_with_the_number_as_syndrome)
{
    install_vectors();
    const auto result = run({Emulator32bit::asm_nop(), swi(7), Emulator32bit::asm_hlt()});

    expect_in_handler(result, Class::SUPERVISOR_CALL);
    EXPECT_EQ(esr_iss(), 7u);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_elr), kProgram + 8) << "the next instruction";
    EXPECT_EQ(result.instructions_ran, 2u) << "the swi completed";

    expect_in_handler(run({swi(0)}), Class::SUPERVISOR_CALL);
    EXPECT_EQ(esr_iss(), 0u);
}

TEST_F(Exceptions, a_conditional_swi_that_does_not_hold_does_nothing)
{
    install_vectors();
    cpu.set_NZCV(false, false, false, false);
    const auto result =
        run({Emulator32bit::asm_format_b1(Emulator32bit::_op_swi, ConditionCode::EQ, 7),
             Emulator32bit::asm_hlt()});

    EXPECT_EQ(result.status, Status::HALTED);
    EXPECT_EQ(cpu.get_pc(), kProgram + 4) << "stopped at the hlt of the program";
}

TEST_F(Exceptions, swi_1_is_still_an_emulator_call_with_a_vector_table)
{
    install_vectors();
    std::ostringstream out;
    cpu.set_output(out);
    cpu.write_reg(Register::SYSCALL, 1003); // emu_printp

    const auto result = run({swi(1), Emulator32bit::asm_hlt()});
    EXPECT_EQ(result.status, Status::HALTED);
    EXPECT_EQ(cpu.get_pc(), kProgram + 4);
    EXPECT_NE(out.str().find("PSTATE"), std::string::npos) << out.str();
}

TEST_F(Exceptions, swi_without_a_vector_table_is_an_emulator_call_and_other_numbers_fault)
{
    std::ostringstream out;
    cpu.set_output(out);
    cpu.write_reg(Register::SYSCALL, 1003);

    EXPECT_EQ(run({swi(0), Emulator32bit::asm_hlt()}).status, Status::HALTED);
    EXPECT_NE(out.str().find("PSTATE"), std::string::npos);
    EXPECT_EQ(run({swi(1), Emulator32bit::asm_hlt()}).status, Status::HALTED);
    EXPECT_EQ(run({swi(9), Emulator32bit::asm_hlt()}).status, Status::FAULT);
}

TEST_F(Exceptions, the_emulator_calls_can_be_turned_off)
{
    cpu.set_semihosting(false);
    cpu.write_reg(Register::SYSCALL, 1003);
    EXPECT_EQ(run({swi(1)}).status, Status::FAULT);
    EXPECT_EQ(run({swi(0)}).status, Status::FAULT);

    install_vectors();
    expect_in_handler(run({swi(1)}), Class::UNDEFINED_INSTRUCTION);
}

TEST_F(Exceptions, an_instruction_fetch_that_fails_is_an_instruction_abort)
{
    install_vectors();

    // Not mapped.
    write(kProgram, {});
    cpu.set_pc(kUnmapped);
    auto result = cpu.run(10);
    expect_in_handler(result, Class::INSTRUCTION_ABORT);
    EXPECT_EQ(esr_iss(), Emulator32bit::kAbortIss_translation);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_far), kUnmapped);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_elr), kUnmapped);

    // Mapped, but data.
    cpu.set_pc(kWritable);
    result = cpu.run(10);
    expect_in_handler(result, Class::INSTRUCTION_ABORT);
    EXPECT_EQ(esr_iss(), Emulator32bit::kAbortIss_permission);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_far), kWritable);

    // Not a multiple of 4.
    cpu.set_pc(kProgram + 2);
    result = cpu.run(10);
    expect_in_handler(result, Class::INSTRUCTION_ABORT);
    EXPECT_EQ(esr_iss(), Emulator32bit::kAbortIss_alignment);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_far), kProgram + 2);
}

TEST_F(Exceptions, a_load_from_memory_that_is_not_mapped_is_a_data_abort)
{
    install_vectors();
    cpu.write_reg(U8(0), kUnmapped + 8);
    cpu.write_reg(U8(1), 0x1111);

    const auto result = run({load(1, 0)});
    expect_in_handler(result, Class::DATA_ABORT);
    EXPECT_EQ(esr_iss(), Emulator32bit::kAbortIss_translation) << "a read";
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_far), kUnmapped + 8);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_elr), kProgram);
    EXPECT_EQ(cpu.read_reg(U8(1)), 0x1111u) << "the instruction that faulted did nothing";
}

TEST_F(Exceptions, a_store_to_read_only_memory_is_a_data_abort_for_a_write)
{
    install_vectors();
    cpu.write_reg(U8(0), kReadOnly + 4);

    expect_in_handler(run({store(1, 0)}), Class::DATA_ABORT);
    EXPECT_EQ(esr_iss(), Emulator32bit::kAbortIss_permission | Emulator32bit::kAbortIss_write);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_far), kReadOnly + 4);
}

TEST_F(Exceptions, an_access_that_crosses_into_a_page_that_faults_reports_the_page_boundary)
{
    install_vectors();
    cpu.write_reg(U8(0), kReadOnly - 2); // the last two bytes of the writable page and two more

    expect_in_handler(run({store_unaligned(1, 0)}), Class::DATA_ABORT);
    EXPECT_EQ(esr_iss(), Emulator32bit::kAbortIss_permission | Emulator32bit::kAbortIss_write);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_far), kReadOnly);
}

// ldr, ldrh, str and strh at an address that is not aligned to their size are a data abort of the
// alignment type, with the address in FAR. ldur... do not mind (the page crossing test above).
TEST_F(Exceptions, a_misaligned_load_or_store_is_a_data_abort_with_the_alignment_syndrome)
{
    install_vectors();
    cpu.write_reg(U8(0), kWritable + 2);
    cpu.write_reg(U8(1), 0x1111);

    expect_in_handler(run({load(1, 0)}), Class::DATA_ABORT);
    EXPECT_EQ(esr_iss(), Emulator32bit::kAbortIss_alignment) << "a read";
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_far), kWritable + 2);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_elr), kProgram);
    EXPECT_EQ(cpu.read_reg(U8(1)), 0x1111u) << "the instruction that faulted did nothing";

    cpu.write_reg(U8(0), kWritable + 6);
    expect_in_handler(run({store(1, 0)}), Class::DATA_ABORT);
    EXPECT_EQ(esr_iss(), Emulator32bit::kAbortIss_alignment | Emulator32bit::kAbortIss_write);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_far), kWritable + 6);

    // A half-word, with a pre-indexed address: FAR is the base plus the offset.
    cpu.write_reg(U8(0), kWritable);
    expect_in_handler(run({Emulator32bit::asm_format_m(Emulator32bit::_op_ldrh, false, 1, 0, 3,
                                                       Emulator32bit::AddrType::ADDR_PRE_INC)}),
                      Class::DATA_ABORT);
    EXPECT_EQ(esr_iss(), Emulator32bit::kAbortIss_alignment);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_far), kWritable + 3);
    EXPECT_EQ(cpu.read_reg(U8(0)), kWritable) << "the write back did not happen";
}

// The atomics have no unaligned form: a word or half-word one at an address that is not a multiple
// of its size is the same data abort as ldr, and it changes neither the memory nor xt.
TEST_F(Exceptions, a_misaligned_atomic_is_a_data_abort_with_the_alignment_syndrome)
{
    install_vectors();
    struct Case
    {
        word width;
        word offset;
    };
    for (const Case c : {Case{Emulator32bit::kAtomicWidth_word, 2},
                         Case{Emulator32bit::kAtomicWidth_word, 1},
                         Case{Emulator32bit::kAtomicWidth_hword, 1}})
    {
        for (const word atop : {Emulator32bit::kAtomicId_swp, Emulator32bit::kAtomicId_ldadd,
                                Emulator32bit::kAtomicId_ldclr, Emulator32bit::kAtomicId_ldset})
        {
            const std::string ctx = "width " + std::to_string(c.width) + ", atop "
                                    + std::to_string(atop) + ", offset " + std::to_string(c.offset);
            cpu.memory.write_word(kWritable, 0x11223344);
            cpu.write_reg(U8(0), 0x5555);
            cpu.write_reg(U8(1), 0x77);
            cpu.write_reg(U8(2), kWritable + c.offset);

            expect_in_handler(run({Emulator32bit::asm_atomic(0, 1, 2, U8(c.width), U8(atop))}),
                              Class::DATA_ABORT);
            EXPECT_EQ(esr_iss(),
                      Emulator32bit::kAbortIss_alignment | Emulator32bit::kAbortIss_write)
                << ctx;
            EXPECT_EQ(sysreg(Emulator32bit::kSysregId_far), kWritable + c.offset) << ctx;
            EXPECT_EQ(sysreg(Emulator32bit::kSysregId_elr), kProgram) << ctx;
            EXPECT_EQ(cpu.read_reg(U8(0)), 0x5555u) << ctx << ": xt is untouched";
            EXPECT_EQ(cpu.memory.read_word(kWritable), 0x11223344u) << ctx;
        }
    }
}

TEST_F(Exceptions, a_byte_atomic_has_no_alignment)
{
    install_vectors();
    cpu.memory.write_word(kWritable, 0x11223344);
    cpu.write_reg(U8(1), 0x55);
    cpu.write_reg(U8(2), kWritable + 1);

    const auto result = run({Emulator32bit::asm_atomic(0, 1, 2, Emulator32bit::kAtomicWidth_byte,
                                                       Emulator32bit::kAtomicId_swp),
                             Emulator32bit::asm_hlt()});
    EXPECT_EQ(result.status, Status::HALTED) << result.message;
    EXPECT_EQ(cpu.get_pc(), kProgram + 4) << "no exception was taken";
    EXPECT_EQ(cpu.read_reg(U8(0)), 0x33u);
    EXPECT_EQ(cpu.memory.read_word(kWritable), 0x11225544u);
}

// The address for FAR is worked out again from the registers when the access faults.
TEST_F(Exceptions, far_is_the_address_of_a_pre_indexed_or_post_indexed_access)
{
    install_vectors();

    // Pre-index: the base plus the offset, and the base register is left as it was.
    cpu.write_reg(U8(0), kUnmapped);
    expect_in_handler(run({Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, true, 1, 0, 12,
                                                       Emulator32bit::AddrType::ADDR_PRE_INC)}),
                      Class::DATA_ABORT);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_far), kUnmapped + 12);
    EXPECT_EQ(cpu.read_reg(U8(0)), kUnmapped) << "the write back did not happen";

    // Post-index: the base alone.
    cpu.write_reg(U8(0), kUnmapped + 4);
    expect_in_handler(run({Emulator32bit::asm_format_m(Emulator32bit::_op_str, true, 1, 0, 12,
                                                       Emulator32bit::AddrType::ADDR_POST_INC)}),
                      Class::DATA_ABORT);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_far), kUnmapped + 4);
    EXPECT_EQ(cpu.read_reg(U8(0)), kUnmapped + 4);
}

TEST_F(Exceptions, far_is_the_address_of_an_atomic)
{
    install_vectors();
    cpu.write_reg(U8(2), kUnmapped + 8);

    expect_in_handler(run({Emulator32bit::asm_atomic(1, 0, 2, Emulator32bit::kAtomicWidth_word,
                                                     Emulator32bit::kAtomicId_ldadd)}),
                      Class::DATA_ABORT);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_far), kUnmapped + 8);
}

TEST_F(Exceptions, an_access_to_a_physical_address_with_no_memory_is_a_bus_error)
{
    install_vectors();
    cpu.mmu->set_enabled(false); // addresses are physical
    cpu.write_reg(U8(0), 0x7FFF0000);

    expect_in_handler(run({load(1, 0)}), Class::DATA_ABORT);
    EXPECT_EQ(esr_iss(), Emulator32bit::kAbortIss_bus);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_far), 0x7FFF0000u);
}

TEST_F(Exceptions, a_store_to_the_rom_is_a_bus_error_and_the_rom_keeps_its_image)
{
    constexpr word kRom = 16 * kPageSize;
    install_vectors();
    cpu.mmu->set_enabled(false); // addresses are physical
    cpu.system_bus->rom->write_word(kRom, 0x600DF00D);
    cpu.write_reg(U8(0), kRom);
    cpu.write_reg(U8(1), 0xABCD1234);

    expect_in_handler(run({store(1, 0)}), Class::DATA_ABORT);
    EXPECT_EQ(esr_iss(), Emulator32bit::kAbortIss_bus);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_far), kRom);
    EXPECT_EQ(cpu.system_bus->rom->read_word(kRom), 0x600DF00Du);
}

TEST_F(Exceptions, a_handler_can_fix_the_cause_and_retry_the_instruction)
{
    install_vectors();
    // The handler points the base register at memory that can be written, and returns to the
    // store, which runs again.
    write(vector(Class::DATA_ABORT), {mov(0, kWritable), Emulator32bit::asm_eret()});
    cpu.write_reg(U8(0), kUnmapped);
    cpu.write_reg(U8(1), 0xABCD1234);

    const auto result = run({store(1, 0), Emulator32bit::asm_hlt()});
    EXPECT_EQ(result.status, Status::HALTED) << result.message;
    EXPECT_EQ(cpu.get_pc(), kProgram + 4);
    EXPECT_EQ(cpu.memory.read_word(kWritable), 0xABCD1234u);
}

TEST_F(Exceptions, brk_raises_the_breakpoint_exception)
{
    install_vectors();
    const auto result = run({Emulator32bit::asm_brk(42)});

    expect_in_handler(result, Class::BREAKPOINT);
    EXPECT_EQ(esr_iss(), 42u);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_elr), kProgram) << "the brk itself";
}

TEST_F(Exceptions, brk_stops_the_run_when_a_debugger_is_attached)
{
    install_vectors();
    cpu.set_brk_stops(true);
    const auto result = run({Emulator32bit::asm_brk(5), Emulator32bit::asm_hlt()});

    EXPECT_EQ(result.status, Status::BREAKPOINT);
    EXPECT_EQ(cpu.get_pc(), kProgram + 4) << "the run goes on with the next instruction";
    EXPECT_NE(result.message.find("brk 5"), std::string::npos) << result.message;
    EXPECT_EQ(cpu.run(0).status, Status::HALTED);
}

TEST_F(Exceptions, brk_without_a_vector_table_or_a_debugger_is_a_fault)
{
    const auto result = run({Emulator32bit::asm_brk(1)});
    EXPECT_EQ(result.status, Status::FAULT);
    EXPECT_NE(result.message.find("brk 1"), std::string::npos) << result.message;
}

TEST_F(Exceptions, wfi_ends_the_run_while_there_is_no_interrupt_source)
{
    const auto result = run({Emulator32bit::asm_wfi(), Emulator32bit::asm_nop()});
    EXPECT_EQ(result.status, Status::HALTED);
    EXPECT_EQ(cpu.get_pc(), kProgram);
}

TEST_F(Exceptions, pstate_selects_the_stack_pointer)
{
    cpu.write_reg(Register::SP, 0x900); // the kernel one
    cpu.write_sysreg(Emulator32bit::kSysregId_usp, 0x700);

    cpu.write_sysreg(Emulator32bit::kSysregId_pstate, kUserBit);
    EXPECT_TRUE(cpu.user_mode());
    EXPECT_EQ(cpu.read_reg(Register::SP), 0x700u);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_usp), 0x900u) << "now the other one";

    cpu.write_reg(Register::SP, 0x6F0);
    cpu.write_sysreg(Emulator32bit::kSysregId_pstate, 0);
    EXPECT_FALSE(cpu.user_mode());
    EXPECT_EQ(cpu.read_reg(Register::SP), 0x900u);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_usp), 0x6F0u);
}

TEST_F(Exceptions, eret_enters_user_mode_and_an_exception_comes_back_to_the_kernel)
{
    constexpr word kUserCode = 0x200;
    install_vectors();
    write(vector(Class::SUPERVISOR_CALL),
          {mrs(5, Emulator32bit::kSysregId_usp), Emulator32bit::asm_hlt()});
    write(kUserCode, {mov(4, 1), swi(3)});

    cpu.write_reg(Register::SP, 0x900); // the kernel stack
    cpu.write_reg(U8(1), 0x700);        // the user stack
    cpu.write_reg(U8(2), kUserCode);
    const auto result =
        run({msr(Emulator32bit::kSysregId_usp, 1), msr(Emulator32bit::kSysregId_elr, 2),
             Emulator32bit::asm_msr(Emulator32bit::kSysregId_spsr, true, kUserBit),
             Emulator32bit::asm_eret()});

    EXPECT_EQ(result.status, Status::HALTED) << result.message;
    EXPECT_EQ(cpu.get_pc(), vector(Class::SUPERVISOR_CALL) + 4) << "the hlt after the mrs";
    EXPECT_EQ(esr_class(), word(Class::SUPERVISOR_CALL));
    EXPECT_FALSE(cpu.user_mode());
    EXPECT_EQ(cpu.read_reg(U8(4)), 1u) << "the user code ran";
    EXPECT_EQ(cpu.read_reg(U8(5)), 0x700u) << "the user stack pointer, read by the kernel";
    EXPECT_EQ(cpu.read_reg(Register::SP), 0x900u) << "back on the kernel stack";
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_spsr), kUserBit) << "it came from user mode";
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_elr), kUserCode + 8);
}

TEST_F(Exceptions, an_exception_in_kernel_mode_keeps_the_stack_pointer)
{
    install_vectors();
    cpu.write_reg(Register::SP, 0x900);
    cpu.write_sysreg(Emulator32bit::kSysregId_usp, 0x700);

    expect_in_handler(run({swi(2)}), Class::SUPERVISOR_CALL);
    EXPECT_EQ(cpu.read_reg(Register::SP), 0x900u);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_usp), 0x700u);
}

TEST_F(Exceptions, eret_restores_the_flags_and_the_irq_mask)
{
    install_vectors();
    write(vector(Class::SUPERVISOR_CALL), {Emulator32bit::asm_eret()});
    cpu.set_NZCV(true, false, true, false);
    cpu.write_sysreg(Emulator32bit::kSysregId_pstate, 0b0101); // N and C, IRQs not masked

    const auto result = run({swi(1 + 1), Emulator32bit::asm_hlt()});
    EXPECT_EQ(result.status, Status::HALTED) << result.message;
    EXPECT_EQ(cpu.get_pstate(), 0b0101u) << "as before the swi";
    EXPECT_EQ(cpu.get_pc(), kProgram + 4);
}

TEST_F(Exceptions, taking_an_exception_masks_irqs_and_keeps_the_flags)
{
    install_vectors();
    cpu.write_sysreg(Emulator32bit::kSysregId_pstate, 0b1010); // N and C, IRQs not masked

    expect_in_handler(run({swi(2)}), Class::SUPERVISOR_CALL);
    EXPECT_EQ(sysreg(Emulator32bit::kSysregId_spsr), 0b1010u);
    EXPECT_EQ(cpu.get_pstate(), 0b1010u | kIrqMask);
}

TEST_F(Exceptions, privileged_instructions_are_undefined_in_user_mode)
{
    install_vectors();
    const word privileged[] = {Emulator32bit::asm_hlt(),
                               Emulator32bit::asm_wfi(),
                               Emulator32bit::asm_eret(),
                               Emulator32bit::asm_tlbi(0, false, 0),
                               msr(Emulator32bit::kSysregId_elr, 1),
                               mrs(1, Emulator32bit::kSysregId_elr),
                               mrs(1, Emulator32bit::kSysregId_vbar)};
    for (const word instruction : privileged)
    {
        cpu.write_sysreg(Emulator32bit::kSysregId_pstate, kUserBit);
        const auto result = run({instruction});

        expect_in_handler(result, Class::UNDEFINED_INSTRUCTION);
        EXPECT_EQ(esr_iss(), Emulator32bit::kUndefinedIss_privileged)
            << Emulator32bit::disassemble_instr(instruction);
        EXPECT_EQ(sysreg(Emulator32bit::kSysregId_spsr) & kUserBit, kUserBit);
    }
}

TEST_F(Exceptions, user_mode_can_use_the_flags_of_pstate_and_nothing_else)
{
    cpu.write_sysreg(Emulator32bit::kSysregId_pstate, kUserBit);
    cpu.write_reg(U8(1), 0xFFFFFFFF); // all bits, the mode and the mask too

    const auto result = run({msr(Emulator32bit::kSysregId_pstate, 1),
                             mrs(2, Emulator32bit::kSysregId_pstate), Emulator32bit::asm_nop()});

    // The run goes on into zeroed memory, which is a hlt, and that is not allowed in user mode
    // either: with no vector table it ends with a fault.
    EXPECT_EQ(result.status, Status::FAULT);
    EXPECT_NE(result.message.find("Privileged"), std::string::npos) << result.message;
    EXPECT_TRUE(cpu.user_mode()) << "the mode cannot be changed from user mode";
    EXPECT_EQ(cpu.read_reg(U8(2)), 0b1111u) << "the flags are all that is seen";
    EXPECT_EQ(cpu.get_pstate() & 0b1111, 0b1111u);
}

TEST_F(Exceptions, user_mode_can_use_the_floating_point_registers)
{
    cpu.write_sysreg(Emulator32bit::kSysregId_pstate, kUserBit);
    cpu.write_reg(U8(1), fpu::kRoundZero);
    cpu.write_reg(U8(3), 0x3F800000); // 1.0
    cpu.write_reg(U8(4), 0x40400000); // 3.0

    // Round toward zero, divide (inexact), read FPSR, clear it. The run ends in the hlt after
    // them, which a user program may not do; the fault is the end of the run.
    const auto result = run({msr(Emulator32bit::kSysregId_fpcr, 1),
                             Emulator32bit::asm_fop2(fpu::kBinaryFn_div, false, 5, 3, 4),
                             mrs(2, Emulator32bit::kSysregId_fpsr),
                             msr(Emulator32bit::kSysregId_fpsr, 0), mrs(6, Emulator32bit::kSysregId_fpcr),
                             mrs(7, Emulator32bit::kSysregId_fpsr)});

    EXPECT_EQ(result.status, Status::FAULT);
    EXPECT_NE(result.message.find("Privileged"), std::string::npos) << result.message;
    EXPECT_TRUE(cpu.user_mode());
    EXPECT_EQ(cpu.read_reg(U8(5)), 0x3EAAAAAAu) << "the FPCR it set rounded toward zero";
    EXPECT_EQ(cpu.read_reg(U8(2)), fpu::kInexact);
    EXPECT_EQ(cpu.read_reg(U8(6)), fpu::kRoundZero);
    EXPECT_EQ(cpu.read_reg(U8(7)), 0u) << "x0, which is 0, was written to FPSR";
}

TEST_F(Exceptions, the_trace_shows_the_exception_and_the_eret)
{
    install_vectors();
    write(vector(Class::SUPERVISOR_CALL), {Emulator32bit::asm_eret()});
    std::ostringstream trace;
    cpu.set_trace(&trace);

    run({swi(3), Emulator32bit::asm_hlt()});
    EXPECT_NE(trace.str().find("-- exception: supervisor call, ESR=0x8000003"), std::string::npos)
        << trace.str();
    EXPECT_NE(trace.str().find("eret ; pc=0x104"), std::string::npos) << trace.str();
}

TEST_F(Exceptions, the_new_instructions_are_disassembled)
{
    EXPECT_EQ(Emulator32bit::disassemble_instr(Emulator32bit::asm_eret()), "eret");
    EXPECT_EQ(Emulator32bit::disassemble_instr(Emulator32bit::asm_wfi()), "wfi");
    EXPECT_EQ(Emulator32bit::disassemble_instr(Emulator32bit::asm_brk(9)), "brk 9");
    EXPECT_EQ(Emulator32bit::disassemble_instr(swi(4)), "swi 4");
    EXPECT_EQ(Emulator32bit::disassemble_instr(msr(Emulator32bit::kSysregId_vbar, 3)),
              "msr vbar, x3");
    EXPECT_EQ(Emulator32bit::disassemble_instr(mrs(4, Emulator32bit::kSysregId_esr)),
              "mrs x4, esr");
}

TEST_F(Exceptions, system_registers_have_names)
{
    EXPECT_STREQ(Emulator32bit::sysreg_name(Emulator32bit::kSysregId_spsr), "spsr");
    EXPECT_EQ(Emulator32bit::sysreg_name(31), nullptr);
    EXPECT_EQ(Emulator32bit::sysreg_id("VBAR"), std::optional<U8>(Emulator32bit::kSysregId_vbar));
    EXPECT_EQ(Emulator32bit::sysreg_id("nothing"), std::nullopt);
}
