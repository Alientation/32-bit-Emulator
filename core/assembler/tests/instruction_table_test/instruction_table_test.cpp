// The assembler's instruction list (instruction_list.h) has to agree with the emulator's opcode
// list and its disassembler. These tests go through every row.

#include <assembler_test/toolchain_fixture.h>

#include <assembler/instruction_table.h>

namespace
{

using basm::InstructionFormat;
using basm::InstructionSpec;
using basm::TokenType;

std::vector<const InstructionSpec *> all_instructions()
{
    std::vector<const InstructionSpec *> specs;
    for (size_t type = size_t(TokenType::INSTRUCTION_HLT);
         type <= size_t(TokenType::INSTRUCTION_RET); type++)
    {
        specs.push_back(&basm::instruction_spec(TokenType(type)));
    }
    return specs;
}

/// Operands that are valid for the format, for assembling a row without caring what it does.
const char *operands_of(InstructionFormat format)
{
    switch (format)
    {
    case InstructionFormat::O:
        return "x1, x2, x3";
    case InstructionFormat::O_NO_DEST:
        return "x1, x2";
    case InstructionFormat::O1:
        return "x1, x2, 3";
    case InstructionFormat::O2:
        return "x1, x2, x3, x4";
    case InstructionFormat::O3:
        return "x1, 5";
    case InstructionFormat::M:
        return "x1, [x2]";
    case InstructionFormat::M1:
        return "x1, symbol";
    case InstructionFormat::B1:
        return "8";
    case InstructionFormat::B2:
        return "x1";
    case InstructionFormat::SWI:
        return "5";
    case InstructionFormat::ATOMIC:
        return "x1, x2, [x3]";
    case InstructionFormat::CSEL:
        return "x1, x2, x3, eq";
    case InstructionFormat::CSET:
        return "x1, eq";
    case InstructionFormat::CINC:
        return "x1, x2, eq";
    case InstructionFormat::UNARY:
        return "x1, x2";
    default:
        return "";
    }
}

/// The part of a disassembled instruction before its operands and condition: "b.ne 4" -> "b".
std::string mnemonic_of(const std::string &disassembly)
{
    return disassembly.substr(0, disassembly.find_first_of(" ."));
}

} // namespace

class InstructionTable : public ToolchainFixture
{
  protected:
    /// The words of the .text of `source`.
    std::vector<word> assemble(const std::string &source)
    {
        const std::string input = write("table.bi", ".text\n" + source + "\n");
        Assembler assembler(File(input), (m_dir / "out" / "table.bo").string());
        assembler.assemble();
        return ObjectFile(assembler.get_output_file()).text_section;
    }
};

TEST_F(InstructionTable, rows_follow_the_order_of_the_token_types)
{
    const auto specs = all_instructions();
    EXPECT_EQ(specs.front()->token, TokenType::INSTRUCTION_HLT);
    EXPECT_EQ(specs.back()->token, TokenType::INSTRUCTION_RET);
    for (size_t i = 0; i < specs.size(); i++)
    {
        EXPECT_EQ(size_t(specs[i]->token), size_t(TokenType::INSTRUCTION_HLT) + i);
        EXPECT_TRUE(basm::is_instruction(specs[i]->token));
    }
}

// Writing a row's text assembles to an instruction with the row's opcode, and the disassembler
// names that opcode with the same text.
TEST_F(InstructionTable, text_opcode_and_disassembly_agree)
{
    int checked = 0;
    for (const InstructionSpec *spec : all_instructions())
    {
        const InstructionFormat format = spec->format;
        if (format == InstructionFormat::HLT || format == InstructionFormat::NOP
            || format == InstructionFormat::ERET || format == InstructionFormat::WFI
            || format == InstructionFormat::BRK || format == InstructionFormat::MSR
            || format == InstructionFormat::MRS || format == InstructionFormat::RET
            || format == InstructionFormat::UNIMPLEMENTED || format == InstructionFormat::ATOMIC)
        {
            continue;
        }

        const std::string line = std::string(spec->text) + " " + operands_of(format);
        const std::vector<word> words = assemble(line);
        ASSERT_EQ(words.size(), 1u) << line;

        // The unary instructions are in the special group, their row has the operation instead.
        const word opcode =
            format == InstructionFormat::UNARY ? Emulator32bit::_op_special_instructions : spec->a;
        EXPECT_EQ(bitfield_unsigned(words[0], 26, 6), opcode) << line;
        EXPECT_EQ(mnemonic_of(Emulator32bit::disassemble_instr(words[0])), spec->text) << line;
        checked++;
    }
    EXPECT_GT(checked, 30);
}

TEST_F(InstructionTable, flag_setting_variants_exist_only_where_the_list_says)
{
    // `adds` is the flag setting add, `cmps` is not an instruction.
    EXPECT_EQ(assemble("adds x1, x2, x3").size(), 1u);
    EXPECT_EQ(assemble("movs x1, 5").size(), 1u);
    EXPECT_EQ(assemble("lsls x1, x2, 3").size(), 1u);
    EXPECT_TRUE(contains(error_of([&] { assemble("cmps x1, x2"); }), "cmps"));
    EXPECT_TRUE(contains(error_of([&] { assemble("ldrs x1, [x2]"); }), "ldrs"));
}

TEST_F(InstructionTable, atomics_encode_the_width_and_operation_of_their_row)
{
    int checked = 0;
    for (const InstructionSpec *spec : all_instructions())
    {
        if (spec->format != InstructionFormat::ATOMIC)
        {
            continue;
        }
        const std::vector<word> words = assemble(std::string(spec->text) + " x1, x2, [x3]");
        ASSERT_EQ(words.size(), 1u) << spec->text;
        EXPECT_EQ(words[0], Emulator32bit::asm_atomic(1, 2, 3, spec->a, spec->b)) << spec->text;
        checked++;
    }
    EXPECT_EQ(checked, 12); // swp, ldadd, ldclr, ldset in word, byte and halfword widths
}

TEST_F(InstructionTable, unimplemented_instructions_say_so)
{
    int checked = 0;
    for (const InstructionSpec *spec : all_instructions())
    {
        if (spec->format != InstructionFormat::UNIMPLEMENTED)
        {
            continue;
        }
        EXPECT_TRUE(contains(error_of([&] { assemble(std::string(spec->text) + " x1, x2"); }),
                             std::string(spec->text) + " is not implemented yet"));
        checked++;
    }
    EXPECT_GT(checked, 10);
}

TEST_F(InstructionTable, other_spellings_of_an_instruction)
{
    // The lexer has a keyword for the signed conversions and for sign extending loads on top of
    // the one of the row.
    EXPECT_TRUE(contains(error_of([&] { assemble("vcint.s32.f32 x1, x2"); }), "not implemented"));
    EXPECT_TRUE(contains(error_of([&] { assemble("vcflo.s32.f32 x1, x2"); }), "not implemented"));
    EXPECT_EQ(assemble("ldrsb x1, [x2]").size(), 1u);
    EXPECT_EQ(assemble("ldrsh x1, [x2]").size(), 1u);
}

// The instructions of the exception machinery (docs/exceptions.md).
TEST_F(InstructionTable, swi_takes_a_number_and_a_condition)
{
    using Emulator = Emulator32bit;
    const auto swi = [](const ConditionCode cond, const sword number)
    { return Emulator::asm_format_b1(Emulator::_op_swi, cond, number); };

    EXPECT_EQ(assemble("swi"), std::vector<word>{swi(ConditionCode::AL, 0)});
    EXPECT_EQ(assemble("swi 5"), std::vector<word>{swi(ConditionCode::AL, 5)});
    EXPECT_EQ(assemble("swi 1 + 1"), std::vector<word>{swi(ConditionCode::AL, 2)});
    EXPECT_EQ(assemble("swi.eq 7"), std::vector<word>{swi(ConditionCode::EQ, 7)});
    EXPECT_EQ(assemble("swi 4194303"), std::vector<word>{swi(ConditionCode::AL, 4194303)});
    EXPECT_EQ(Emulator::disassemble_instr(assemble("swi.ne 9")[0]), "swi.ne 9");
    EXPECT_TRUE(contains(error_of([&] { assemble("swi 4194304"); }), "22 bits"));
}

TEST_F(InstructionTable, eret_wfi_and_brk)
{
    EXPECT_EQ(assemble("eret"), std::vector<word>{Emulator32bit::asm_eret()});
    EXPECT_EQ(assemble("wfi"), std::vector<word>{Emulator32bit::asm_wfi()});
    EXPECT_EQ(assemble("brk"), std::vector<word>{Emulator32bit::asm_brk(0)});
    EXPECT_EQ(assemble("brk 3"), std::vector<word>{Emulator32bit::asm_brk(3)});
    EXPECT_TRUE(contains(error_of([&] { assemble("brk 4194304"); }), "22 bits"));
}

TEST_F(InstructionTable, msr_and_mrs_name_the_system_registers_in_any_case)
{
    EXPECT_EQ(assemble("msr vbar, x1"),
              std::vector<word>{Emulator32bit::asm_msr(Emulator32bit::kSysregId_vbar, false, 1)});
    EXPECT_EQ(assemble("mrs x2, ESR"),
              std::vector<word>{Emulator32bit::asm_mrs(2, Emulator32bit::kSysregId_esr)});
    EXPECT_EQ(assemble("msr SPSR, 16"),
              std::vector<word>{Emulator32bit::asm_msr(Emulator32bit::kSysregId_spsr, true, 16)});
    EXPECT_EQ(assemble("msr pstate, x0"),
              std::vector<word>{Emulator32bit::asm_msr(Emulator32bit::kSysregId_pstate, false, 0)});
    EXPECT_TRUE(
        contains(error_of([&] { assemble("msr nothing, x1"); }), "invalid system register"));
}

TEST_F(InstructionTable, conditional_selects)
{
    using E = Emulator32bit;
    constexpr word xzr = 31;
    EXPECT_EQ(assemble("csel x1, x2, x3, lt"),
              std::vector<word>{E::asm_csel(E::kCselId_csel, ConditionCode::LT, 1, 2, 3)});
    EXPECT_EQ(assemble("csinc x1, x2, x3, hs"),
              std::vector<word>{E::asm_csel(E::kCselId_csinc, ConditionCode::HS, 1, 2, 3)});
    EXPECT_EQ(assemble("csinv x1, x2, xzr, ne"),
              std::vector<word>{E::asm_csel(E::kCselId_csinv, ConditionCode::NE, 1, 2, xzr)});
    EXPECT_EQ(assemble("csneg x1, x2, x3, mi"),
              std::vector<word>{E::asm_csel(E::kCselId_csneg, ConditionCode::MI, 1, 2, 3)});

    // The aliases store the opposite condition.
    EXPECT_EQ(assemble("cset x1, lt"),
              std::vector<word>{E::asm_csel(E::kCselId_csinc, ConditionCode::GE, 1, xzr, xzr)});
    EXPECT_EQ(assemble("csetm x1, eq"),
              std::vector<word>{E::asm_csel(E::kCselId_csinv, ConditionCode::NE, 1, xzr, xzr)});
    EXPECT_EQ(assemble("cinc x1, x2, hi"),
              std::vector<word>{E::asm_csel(E::kCselId_csinc, ConditionCode::LS, 1, 2, 2)});
    EXPECT_EQ(assemble("cinv x1, x2, gt"),
              std::vector<word>{E::asm_csel(E::kCselId_csinv, ConditionCode::LE, 1, 2, 2)});
    EXPECT_EQ(assemble("cneg x1, x2, vs"),
              std::vector<word>{E::asm_csel(E::kCselId_csneg, ConditionCode::VC, 1, 2, 2)});

    // The alias disassembles back to what was written.
    EXPECT_EQ(E::disassemble_instr(assemble("cset x1, lt")[0]), "cset x1, lt");
    EXPECT_EQ(E::disassemble_instr(assemble("cneg x1, x2, vs")[0]), "cneg x1, x2, vs");

    // A label may be called like a condition: only the last operand of these is one.
    EXPECT_EQ(assemble("lt: csel x1, x2, x3, lt").size(), 1u);

    EXPECT_TRUE(contains(error_of([&] { assemble("cset x1, al"); }), "other than al and nv"));
    EXPECT_TRUE(contains(error_of([&] { assemble("csel x1, x2, x3"); }), "expected ','"));
    EXPECT_TRUE(contains(error_of([&] { assemble("csel x1, x2, x3, x4"); }), "condition code"));
}

TEST_F(InstructionTable, unary_instructions)
{
    using E = Emulator32bit;
    EXPECT_EQ(assemble("sxtb x1, x2"), std::vector<word>{E::asm_unary(E::kUnaryId_sxtb, 1, 2)});
    EXPECT_EQ(assemble("sxth x1, x2"), std::vector<word>{E::asm_unary(E::kUnaryId_sxth, 1, 2)});
    EXPECT_EQ(assemble("uxtb x1, x2"), std::vector<word>{E::asm_unary(E::kUnaryId_uxtb, 1, 2)});
    EXPECT_EQ(assemble("uxth x1, x2"), std::vector<word>{E::asm_unary(E::kUnaryId_uxth, 1, 2)});
    EXPECT_EQ(assemble("clz x1, x2"), std::vector<word>{E::asm_unary(E::kUnaryId_clz, 1, 2)});
    EXPECT_EQ(assemble("rev x1, x2"), std::vector<word>{E::asm_unary(E::kUnaryId_rev, 1, 2)});
    EXPECT_EQ(assemble("rev16 x1, sp"), std::vector<word>{E::asm_unary(E::kUnaryId_rev16, 1, 30)});
    EXPECT_TRUE(contains(error_of([&] { assemble("clz x1"); }), "expected ','"));
}

// `ldr xd, =value` builds the constant with the fewest instructions, there is no literal pool.
TEST_F(InstructionTable, load_constant_pseudo_instruction)
{
    using E = Emulator32bit;
    const auto mov = [](word v) { return E::asm_format_o3(E::_op_mov, false, 1, int(v)); };
    const auto mvn = [](word v) { return E::asm_format_o3(E::_op_mvn, false, 1, int(v)); };
    const auto lsl14 = E::asm_format_o1(E::_op_lsl, 1, 1, true, 0, 14);
    const auto orr = [](word v) { return E::asm_format_o(E::_op_orr, false, 1, 1, int(v)); };

    EXPECT_EQ(assemble("ldr x1, =0"), std::vector<word>{mov(0)});
    EXPECT_EQ(assemble("ldr x1, =524287"), std::vector<word>{mov(0x7FFFF)});
    EXPECT_EQ(assemble("ldr x1, =0 - 1"), std::vector<word>{mvn(0)});
    EXPECT_EQ(assemble("ldr x1, =0 - 524288"), std::vector<word>{mvn(0x7FFFF)});
    EXPECT_EQ(assemble("ldr x1, =$FFFFFFFF"), std::vector<word>{mvn(0)});

    // 19 bits are not enough: shift in the high part, then the low 14 bits.
    EXPECT_EQ(assemble("ldr x1, =$12345678"),
              (std::vector<word>{mov(0x12345678u >> 14), lsl14, orr(0x12345678u & 0x3FFF)}));
    // Nothing to add when those bits are zero.
    EXPECT_EQ(assemble("ldr x1, =$80000000"), (std::vector<word>{mov(0x80000000u >> 14), lsl14}));
    EXPECT_EQ(assemble("ldr x1, =524288"), (std::vector<word>{mov(524288u >> 14), lsl14}));

    // The register is the destination: the others are not touched (x16 is not used).
    EXPECT_EQ(assemble("ldr sp, =$12345678").size(), 3u);

    // An ordinary load is still an ordinary load.
    EXPECT_EQ(assemble("ldr x1, [x2]"), std::vector<word>{E::asm_format_m(
                                            E::_op_ldr, false, 1, 2, 0, E::AddrType::ADDR_OFFSET)});

    EXPECT_TRUE(
        contains(error_of([&] { assemble("ldr x1, =$100000000"); }), "does not fit in 32 bits"));
    EXPECT_TRUE(contains(error_of([&] { assemble("ldrb x1, =5"); }), "expected '['"));
}

TEST_F(InstructionTable, load_address_pseudo_instruction_is_adrp_and_add)
{
    using E = Emulator32bit;
    const std::string input =
        write("addr.bi", ".global _start\n.text\n_start:\nldr x3, =_start + 8\n");
    Assembler assembler(File(input), (m_dir / "out" / "addr.bo").string());
    assembler.assemble();
    const ObjectFile obj(assembler.get_output_file());

    ASSERT_EQ(obj.text_section.size(), 2u);
    EXPECT_EQ(obj.text_section[0], E::asm_format_m1(E::_op_adrp, 3, 0));
    EXPECT_EQ(obj.text_section[1], E::asm_format_o(E::_op_add, false, 3, 3, 0));
    ASSERT_EQ(obj.rel_text.size(), 2u);
    EXPECT_EQ(obj.rel_text[0].type, ObjectFile::RelocationEntry::Type::R_EMU32_ADRP_HI20);
    EXPECT_EQ(obj.rel_text[0].offset, 0u);
    EXPECT_EQ(obj.rel_text[0].addend, 8);
    EXPECT_EQ(obj.rel_text[1].type, ObjectFile::RelocationEntry::Type::R_EMU32_O_LO12);
    EXPECT_EQ(obj.rel_text[1].offset, 4u);
    EXPECT_EQ(obj.rel_text[1].addend, 8);
}

TEST_F(InstructionTable, mov_takes_a_19_bit_immediate)
{
    using E = Emulator32bit;
    EXPECT_EQ(assemble("mov x1, 524287"),
              std::vector<word>{E::asm_format_o3(E::_op_mov, false, 1, 0x7FFFF)});
    EXPECT_TRUE(contains(error_of([&] { assemble("mov x1, 524288"); }), "19 bit"));
}
