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
    case InstructionFormat::ATOMIC:
        return "x1, x2, [x3]";
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
            || format == InstructionFormat::MSR || format == InstructionFormat::MRS
            || format == InstructionFormat::RET || format == InstructionFormat::UNIMPLEMENTED
            || format == InstructionFormat::ATOMIC)
        {
            continue;
        }

        const std::string line = std::string(spec->text) + " " + operands_of(format);
        const std::vector<word> words = assemble(line);
        ASSERT_EQ(words.size(), 1u) << line;
        EXPECT_EQ(bitfield_unsigned(words[0], 26, 6), spec->a) << line;
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
