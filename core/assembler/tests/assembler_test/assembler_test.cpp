// Unit tests for the assembler. Each test assembles a small preprocessed source (.bi) and checks
// the resulting object file: the encoded instructions against the emulator's own encoders, the
// data section bytes, symbols and relocations. Errors are checked through their message.

#include "assembler_test/toolchain_fixture.h"

#include <vector>

using Bytes = std::vector<byte>;
using Words = std::vector<word>;

namespace
{

constexpr int kXzr = 31;
constexpr int kSp = 30;
constexpr int kLinkRegister = 29;

} // namespace

class AssemblerUnit : public ToolchainFixture
{
  protected:
    std::string m_last_input;

    /// Assembles `source` as `<name>.bi` and reads the object file back.
    ObjectFile assemble(const std::string &source, const std::string &name = "main")
    {
        m_last_input = write(name + ".bi", source);
        Assembler assembler(m_process.get(), File(m_last_input),
                            (m_dir / "out" / (name + ".bo")).string());
        assembler.assemble();
        EXPECT_NE(assembler.get_state(), Assembler::ASSEMBLER_ERROR);
        return ObjectFile(assembler.get_output_file());
    }

    std::string error(const std::string &source)
    {
        return error_of([&] { assemble(source); });
    }

    /// The instructions of the `.text` section of `source`.
    Words text(const std::string &source)
    {
        return assemble(".text\n" + source).text_section;
    }

    /// The bytes of the `.data` section of `source`.
    Bytes data(const std::string &source)
    {
        return assemble(".data\n" + source).data_section;
    }

    static const ObjectFile::SymbolTableEntry &symbol(const ObjectFile &object,
                                                      const std::string &name)
    {
        return object.symbol_table.at(object.string_table.at(name));
    }

    static bool has_symbol(const ObjectFile &object, const std::string &name)
    {
        return object.string_table.count(name) != 0
               && object.symbol_table.count(object.string_table.at(name)) != 0;
    }

    static std::string name_of(const ObjectFile &object, const ObjectFile::RelocationEntry &rel)
    {
        return object.strings.at(object.symbol_table.at(rel.symbol).symbol_name);
    }
};

// ---------------------------------------------------------------------------------------------
// Instructions
// ---------------------------------------------------------------------------------------------

TEST_F(AssemblerUnit, alu_with_registers_and_immediates)
{
    EXPECT_EQ(text("add x1, x2, x3\nadd x1, x2, 40\nsub x1, x2, x3, lsl 4\n"),
              (Words{Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, 1, 2, 3,
                                                 ShiftType::SHIFT_LSL, 0),
                     Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, 1, 2, 40),
                     Emulator32bit::asm_format_o(Emulator32bit::_op_sub, false, 1, 2, 3,
                                                 ShiftType::SHIFT_LSL, 4)}));
}

TEST_F(AssemblerUnit, number_literals_in_every_base)
{
    EXPECT_EQ(text("add x0, x0, 10\nadd x0, x0, $A\nadd x0, x0, %1010\nadd x0, x0, @12\n"),
              Words(4, Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, 0, 0, 10)));
}

TEST_F(AssemblerUnit, flag_setting_suffix)
{
    const Words words = text("adds x1, x2, 3\nadd x1, x2, 3\nmovs x0, 5\nlsls x1, x2, 3\n");
    EXPECT_EQ(words[0], Emulator32bit::asm_format_o(Emulator32bit::_op_add, true, 1, 2, 3));
    EXPECT_NE(words[0], words[1]);
    EXPECT_EQ(words[2], Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, true, 0, 5));
    EXPECT_EQ(words[3],
              Emulator32bit::asm_format_o1(Emulator32bit::_op_lsl, 1, 2, true, 0, 3, true));
}

TEST_F(AssemblerUnit, shifts)
{
    EXPECT_EQ(
        text("lsl x1, x2, 3\nlsr x1, x2, x3\nasr x1, x2, 31\nror x1, x2, 1\n"),
        (Words{Emulator32bit::asm_format_o1(Emulator32bit::_op_lsl, 1, 2, true, 0, 3, false),
               Emulator32bit::asm_format_o1(Emulator32bit::_op_lsr, 1, 2, false, 3, 0, false),
               Emulator32bit::asm_format_o1(Emulator32bit::_op_asr, 1, 2, true, 0, 31, false),
               Emulator32bit::asm_format_o1(Emulator32bit::_op_ror, 1, 2, true, 0, 1, false)}));
}

TEST_F(AssemblerUnit, multiply)
{
    EXPECT_EQ(text("mul x1, x2, x3\numull x1, x2, x3, x4\n"),
              (Words{Emulator32bit::asm_format_o(Emulator32bit::_op_mul, false, 1, 2, 3,
                                                 ShiftType::SHIFT_LSL, 0),
                     Emulator32bit::asm_format_o2(Emulator32bit::_op_umull, false, 1, 2, 3, 4)}));
}

TEST_F(AssemblerUnit, compare_instructions_have_xzr_as_destination)
{
    EXPECT_EQ(text("cmp x1, 5\ncmn x1, x2\ntst x3, 7\nteq x3, x4\n"),
              (Words{Emulator32bit::asm_format_o(Emulator32bit::_op_cmp, false, kXzr, 1, 5),
                     Emulator32bit::asm_format_o(Emulator32bit::_op_cmn, false, kXzr, 1, 2,
                                                 ShiftType::SHIFT_LSL, 0),
                     Emulator32bit::asm_format_o(Emulator32bit::_op_tst, false, kXzr, 3, 7),
                     Emulator32bit::asm_format_o(Emulator32bit::_op_teq, false, kXzr, 3, 4,
                                                 ShiftType::SHIFT_LSL, 0)}));
}

TEST_F(AssemblerUnit, mov_and_mvn)
{
    EXPECT_EQ(text("mov x0, 5\nmov x1, x2\nmvn x3, 9\n"),
              (Words{Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, false, 0, 5),
                     Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, false, 1, 2, 0),
                     Emulator32bit::asm_format_o3(Emulator32bit::_op_mvn, false, 3, 9)}));
}

TEST_F(AssemblerUnit, ret_is_bx_with_the_link_register)
{
    const Words words = text("ret\nbx x29\n");
    EXPECT_EQ(words[0], Emulator32bit::asm_format_b2(Emulator32bit::_op_bx, ConditionCode::AL,
                                                     kLinkRegister));
    EXPECT_EQ(words[0], words[1]);
}

TEST_F(AssemblerUnit, hlt_and_nop)
{
    EXPECT_EQ(text("hlt\nnop\n"), (Words{Emulator32bit::asm_hlt(), Emulator32bit::asm_nop()}));
}

TEST_F(AssemblerUnit, memory_addressing_modes)
{
    using Addr = Emulator32bit::AddrType;
    EXPECT_EQ(
        text("ldr x0, [x1]\n"
             "ldr x0, [x1, 8]\n"
             "ldr x0, [x1, -4]\n"
             "ldr x0, [x1, 4]!\n"
             "ldr x0, [x1], 4\n"
             "ldr x0, [x1], -4\n"
             "str x0, [sp, -4]!\n"
             "ldr x0, [x1, x2]\n"
             "ldr x0, [x1, x2, lsl 2]\n"),
        (Words{
            Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 0, Addr::ADDR_OFFSET),
            Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 8, Addr::ADDR_OFFSET),
            Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, -4, Addr::ADDR_OFFSET),
            Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 4, Addr::ADDR_PRE_INC),
            Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 4,
                                        Addr::ADDR_POST_INC),
            Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, -4,
                                        Addr::ADDR_POST_INC),
            Emulator32bit::asm_format_m(Emulator32bit::_op_str, false, 0, kSp, -4,
                                        Addr::ADDR_PRE_INC),
            Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 2,
                                        ShiftType::SHIFT_LSL, 0, Addr::ADDR_OFFSET),
            Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 2,
                                        ShiftType::SHIFT_LSL, 2, Addr::ADDR_OFFSET)}));
}

TEST_F(AssemblerUnit, sign_extending_loads_and_stores)
{
    using Addr = Emulator32bit::AddrType;
    EXPECT_EQ(
        text("ldrsb x0, [x1]\nldrb x0, [x1]\nldrsh x2, [x3]\nstrh x2, [x3]\n"),
        (Words{
            Emulator32bit::asm_format_m(Emulator32bit::_op_ldrb, true, 0, 1, 0, Addr::ADDR_OFFSET),
            Emulator32bit::asm_format_m(Emulator32bit::_op_ldrb, false, 0, 1, 0, Addr::ADDR_OFFSET),
            Emulator32bit::asm_format_m(Emulator32bit::_op_ldrh, true, 2, 3, 0, Addr::ADDR_OFFSET),
            Emulator32bit::asm_format_m(Emulator32bit::_op_strh, false, 2, 3, 0,
                                        Addr::ADDR_OFFSET)}));
}

TEST_F(AssemblerUnit, atomics)
{
    EXPECT_EQ(text("swp x1, x2, [x3]\nldaddb x1, x2, [x3]\n"),
              (Words{Emulator32bit::asm_atomic(1, 2, 3, Emulator32bit::kAtomicWidth_word,
                                               Emulator32bit::kAtomicId_swp),
                     Emulator32bit::asm_atomic(1, 2, 3, Emulator32bit::kAtomicWidth_byte,
                                               Emulator32bit::kAtomicId_ldadd)}));
}

TEST_F(AssemblerUnit, system_registers)
{
    EXPECT_EQ(text("msr PSTATE, 5\nmsr PSTATE, x1\nmrs x2, PSTATE\n"),
              (Words{Emulator32bit::asm_msr(Emulator32bit::kSysregId_pstate, true, 5),
                     Emulator32bit::asm_msr(Emulator32bit::kSysregId_pstate, false, 1),
                     Emulator32bit::asm_mrs(2, Emulator32bit::kSysregId_pstate)}));
}

TEST_F(AssemblerUnit, branches_by_register_and_with_conditions)
{
    EXPECT_EQ(text("bx x1\nbx.eq x2\nblx.ne x3\n"),
              (Words{Emulator32bit::asm_format_b2(Emulator32bit::_op_bx, ConditionCode::AL, 1),
                     Emulator32bit::asm_format_b2(Emulator32bit::_op_bx, ConditionCode::EQ, 2),
                     Emulator32bit::asm_format_b2(Emulator32bit::_op_blx, ConditionCode::NE, 3)}));
}

TEST_F(AssemblerUnit, branch_with_an_immediate_offset)
{
    EXPECT_EQ(text("b 8\nb.lt 4\n"),
              (Words{Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, 2),
                     Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::LT, 1)}));
}

TEST_F(AssemblerUnit, condition_names_are_symbols_outside_of_a_branch)
{
    // `eq` only means equal right after `b.`
    const ObjectFile object = assemble(".text\nb.eq eq\neq:\nnop\n");
    EXPECT_TRUE(has_symbol(object, "eq"));
}

// ---------------------------------------------------------------------------------------------
// Labels, symbols and relocations
// ---------------------------------------------------------------------------------------------

TEST_F(AssemblerUnit, branch_to_a_label_in_the_file_is_resolved)
{
    const ObjectFile object = assemble(".text\nnop\nloop: nop\nb.ne loop\n");
    EXPECT_TRUE(object.rel_text.empty());
    EXPECT_EQ(object.text_section[2],
              Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::NE, -1));
    EXPECT_EQ(symbol(object, "loop").symbol_value, 4u);
}

TEST_F(AssemblerUnit, branch_to_a_label_defined_later_is_resolved)
{
    const ObjectFile object = assemble(".text\nbl func\nnop\nfunc: ret\n");
    EXPECT_TRUE(object.rel_text.empty());
    EXPECT_EQ(object.text_section[0],
              Emulator32bit::asm_format_b1(Emulator32bit::_op_bl, ConditionCode::AL, 2));
}

TEST_F(AssemblerUnit, branch_to_an_unknown_symbol_leaves_a_relocation)
{
    const ObjectFile object = assemble(".text\nnop\nbl printf\n");
    ASSERT_EQ(object.rel_text.size(), 1u);
    EXPECT_EQ(object.rel_text[0].type, ObjectFile::RelocationEntry::Type::R_EMU32_B_OFFSET22);
    EXPECT_EQ(object.rel_text[0].offset, 4u);
    EXPECT_EQ(name_of(object, object.rel_text[0]), "printf");
    EXPECT_EQ(symbol(object, "printf").binding_info,
              ObjectFile::SymbolTableEntry::BindingInfo::WEAK);
}

TEST_F(AssemblerUnit, absolute_address_relocations)
{
    const ObjectFile object = assemble(".text\n"
                                       "adrp x0, buf\n"
                                       "add x0, x0, :lo12:buf\n"
                                       "mov x1, :hi13:buf\n"
                                       "mov x1, :lo19:buf\n"
                                       ".data\n"
                                       "buf: .word 1\n");
    using Type = ObjectFile::RelocationEntry::Type;
    ASSERT_EQ(object.rel_text.size(), 4u);
    EXPECT_EQ(object.rel_text[0].type, Type::R_EMU32_ADRP_HI20);
    EXPECT_EQ(object.rel_text[1].type, Type::R_EMU32_O_LO12);
    EXPECT_EQ(object.rel_text[2].type, Type::R_EMU32_MOV_HI13);
    EXPECT_EQ(object.rel_text[3].type, Type::R_EMU32_MOV_LO19);
    for (size_t i = 0; i < object.rel_text.size(); i++)
    {
        EXPECT_EQ(object.rel_text[i].offset, i * 4);
        EXPECT_EQ(name_of(object, object.rel_text[i]), "buf");
    }
}

TEST_F(AssemblerUnit, adrp_accepts_an_explicit_relocation)
{
    EXPECT_EQ(text("adrp x0, :hi20:buf\n"), text("adrp x0, buf\n"));
}

TEST_F(AssemblerUnit, labels_in_a_scope_are_renamed_per_scope)
{
    const ObjectFile object = assemble(".text\n"
                                       ".scope\nloop: nop\n.scend\n"
                                       ".scope\nloop: nop\n.scend\n"
                                       "loop: nop\n");
    EXPECT_EQ(symbol(object, "loop::SCOPE:0").symbol_value, 0u);
    EXPECT_EQ(symbol(object, "loop::SCOPE:1").symbol_value, 4u);
    EXPECT_EQ(symbol(object, "loop").symbol_value, 8u);
}

TEST_F(AssemblerUnit, branch_inside_a_scope_is_relocated_to_the_label_of_that_scope)
{
    // Both scopes have a `loop`. A branch refers to the one in its own scope. The linker fills in
    // the offset, the assembler only points the relocation at the right symbol.
    const ObjectFile object = assemble(".text\n"
                                       ".scope\nloop: nop\nb loop\n.scend\n"
                                       ".scope\nloop: nop\nb loop\n.scend\n");
    ASSERT_EQ(object.rel_text.size(), 2u);
    EXPECT_EQ(name_of(object, object.rel_text[0]), "loop::SCOPE:0");
    EXPECT_EQ(name_of(object, object.rel_text[1]), "loop::SCOPE:1");
}

TEST_F(AssemblerUnit, branch_in_a_nested_scope_finds_the_label_of_the_enclosing_scope)
{
    const ObjectFile object = assemble(".text\n"
                                       ".scope\nouter: nop\n"
                                       ".scope\nb outer\n.scend\n"
                                       ".scend\n");
    ASSERT_EQ(object.rel_text.size(), 1u);
    EXPECT_EQ(name_of(object, object.rel_text[0]), "outer::SCOPE:0");
}

TEST_F(AssemblerUnit, label_and_instruction_on_one_line)
{
    const ObjectFile object = assemble(".text\nstart: nop\nnop\n");
    EXPECT_EQ(object.text_section.size(), 2u);
    EXPECT_EQ(symbol(object, "start").symbol_value, 0u);
}

TEST_F(AssemblerUnit, global_and_extern_symbols)
{
    const ObjectFile object =
        assemble(".global exported\n.extern imported\n.text\nexported: nop\n");
    EXPECT_EQ(symbol(object, "imported").binding_info,
              ObjectFile::SymbolTableEntry::BindingInfo::WEAK);
    EXPECT_EQ(symbol(object, "exported").binding_info,
              ObjectFile::SymbolTableEntry::BindingInfo::GLOBAL);
}

TEST_F(AssemblerUnit, stop_ends_the_assembly)
{
    EXPECT_EQ(text("nop\n.stop\nthis is not valid\n").size(), 1u);
}

// ---------------------------------------------------------------------------------------------
// Data and layout directives
// ---------------------------------------------------------------------------------------------

TEST_F(AssemblerUnit, data_directives_are_little_endian)
{
    EXPECT_EQ(data(".byte 1, $FF, %101, @17\n.dbyte $1234\n.word $11223344\n.dword 1\n"),
              (Bytes{1, 0xFF, 5, 15, 0x34, 0x12, 0x44, 0x33, 0x22, 0x11, 1, 0, 0, 0, 0, 0, 0, 0}));
}

TEST_F(AssemblerUnit, signed_data_directives_have_the_same_widths)
{
    EXPECT_EQ(data(".sbyte 1\n.sdbyte 2\n.sword 3\n.sdword 4\n"),
              (Bytes{1, 2, 0, 3, 0, 0, 0, 4, 0, 0, 0, 0, 0, 0, 0}));
}

TEST_F(AssemblerUnit, expressions_are_evaluated_left_to_right)
{
    EXPECT_EQ(data(".word 1 + 2 * 3\n.word 10 - 4 / 2\n"), (Bytes{9, 0, 0, 0, 3, 0, 0, 0}));
}

TEST_F(AssemblerUnit, characters_and_strings)
{
    EXPECT_EQ(data(R"(.char 'a', '\n'
.ascii "hi\t"
.asciz "x"
)"),
              (Bytes{'a', '\n', 'h', 'i', '\t', 'x', 0}));
}

TEST_F(AssemblerUnit, a_character_can_be_used_as_a_number)
{
    EXPECT_EQ(data(".byte 'a' + 1\n"), (Bytes{'b'}));
}

TEST_F(AssemblerUnit, data_layout_directives)
{
    // align pads to 4, advance skips 2, org pads up to 12.
    EXPECT_EQ(data(".byte 1\n.align 4\n.byte 2\n.advance 2\n.org 12\n.byte 3\n"),
              (Bytes{1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 3}));
}

TEST_F(AssemblerUnit, text_layout_directives)
{
    const Words words = text("nop\n.advance 8\n.org 16\nnop\n.align 32\n");
    ASSERT_EQ(words.size(), 8u);
    EXPECT_EQ(words[0], Emulator32bit::asm_nop());
    EXPECT_EQ(words[1], 0u);
    EXPECT_EQ(words[3], 0u);
    EXPECT_EQ(words[4], Emulator32bit::asm_nop());
}

TEST_F(AssemblerUnit, bss_only_reserves_space)
{
    const ObjectFile object = assemble(".bss\n.advance 6\nbuf:\n.align 8\nend:\n");
    EXPECT_EQ(object.bss_section, 8u);
    EXPECT_TRUE(object.data_section.empty());
    EXPECT_EQ(symbol(object, "buf").symbol_value, 6u);
    EXPECT_EQ(symbol(object, "end").symbol_value, 8u);
}

TEST_F(AssemblerUnit, data_labels_are_offsets_in_data)
{
    const ObjectFile object = assemble(".data\n.word 1\nsecond: .word 2\n");
    EXPECT_EQ(symbol(object, "second").symbol_value, 4u);
}

// ---------------------------------------------------------------------------------------------
// Errors
// ---------------------------------------------------------------------------------------------

TEST_F(AssemblerUnit, operand_errors)
{
    EXPECT_TRUE(contains(error(".text\nadd x0, x1\n"), "expected ',', got end of line"));
    EXPECT_TRUE(contains(error(".text\nadd x0 x1, 2\n"), "expected ',', got 'x1'"));
    EXPECT_TRUE(contains(error(".text\nadd 5, x1, 2\n"), "expected a register, got '5'"));
    EXPECT_TRUE(contains(error(".text\nadd x0, x1, foo\n"),
                         "expected a register, a number or a relocation, got 'foo'"));
    EXPECT_TRUE(contains(error(".text\nadd x0, x1, 99999\n"), "immediate must be a 14 bit value"));
    EXPECT_TRUE(contains(error(".text\nmov x0,\n"), "expected a number, got end of line"));
    EXPECT_TRUE(contains(error(".text\nlsl x0, x1, 40\n"), "shift amount must fit in 5 bits"));
    EXPECT_TRUE(
        contains(error(".text\nadd x0, x1, x2, lsl 40\n"), "shift amount must fit in 5 bits"));
    EXPECT_TRUE(contains(error(".text\nadd x0, x1, x2, nop 1\n"),
                         "expected lsl, lsr, asr or ror, got 'nop'"));
    EXPECT_TRUE(contains(error(".text\nmrs x0, FOO\n"), "invalid system register 'FOO'"));
    EXPECT_TRUE(contains(error(".text\nmsr PSTATE, 70000\n"), "immediate must be a 16 bit value"));
    EXPECT_TRUE(contains(error(".text\nadd x0, x1, :lo12:\n"),
                         "expected a symbol to follow the relocation"));
}

TEST_F(AssemblerUnit, memory_operand_errors)
{
    EXPECT_TRUE(
        contains(error(".text\nldr x0, [x1, 5000]\n"), "offset must be a signed 12 bit value"));
    EXPECT_TRUE(contains(error(".text\nldr x0, x1\n"),
                         "expected '[' to start the memory address, got 'x1'"));
    EXPECT_TRUE(contains(error(".text\nldr x0, [x1\n"), "invalid addressing mode"));
    EXPECT_TRUE(contains(error(".text\nldr x0, [x1, 4\n"), "expected ']' after the offset"));
    EXPECT_TRUE(contains(error(".text\nswp x1, x2, x3\n"),
                         "expected '[' to start the memory address, got 'x3'"));
}

TEST_F(AssemblerUnit, branch_errors)
{
    EXPECT_TRUE(
        contains(error(".text\nb.xx loop\n"), "expected a condition code after '.', got 'xx'"));
    EXPECT_TRUE(contains(error(".text\nb 6\n"), "branch offset must be 4 byte aligned"));
    EXPECT_TRUE(contains(error(".text\nbx 5\n"), "expected a register, got '5'"));
}

TEST_F(AssemblerUnit, statement_errors)
{
    EXPECT_TRUE(contains(error(".text\nhlt 5\n"), "unexpected '5' at the end of the statement"));
    EXPECT_TRUE(
        contains(error(".text\nnop nop\n"), "unexpected 'nop' at the end of the statement"));
    EXPECT_TRUE(
        contains(error(".data\n.byte 1 2\n"), "unexpected '2' at the end of the statement"));
    EXPECT_TRUE(contains(error(".text\nfoo x0\n"), "cannot parse 'foo'"));
    EXPECT_TRUE(contains(error(".text\n.fill 3\n"), "cannot parse '.fill'"));
    EXPECT_TRUE(contains(error(".text\nvadd.f32 x0, x1\n"), "vadd.f32 is not implemented yet"));
}

TEST_F(AssemblerUnit, section_errors)
{
    EXPECT_TRUE(contains(error("hlt\n"), "code must be located in the .text section"));
    EXPECT_TRUE(
        contains(error(".data\nadd x0, x1, 2\n"), "code must be located in the .text section"));
    EXPECT_TRUE(
        contains(error(".text\n.word 1\n"), ".word can only define data in the .data section"));
    EXPECT_TRUE(contains(error(".bss\n.asciz \"x\"\n"),
                         ".asciz can only define data in the .data section"));
    EXPECT_TRUE(contains(error("foo:\n"), "label must be located in a section"));
    EXPECT_TRUE(contains(error(".text\n.global x\n"),
                         "cannot declare a symbol as global inside a section"));
    EXPECT_TRUE(contains(error(".text\n.extern x\n"),
                         "cannot declare a symbol as extern inside a section"));
    EXPECT_TRUE(contains(error(".global\n"), "expected a symbol after .global"));
    EXPECT_TRUE(contains(error(".text\n.section \"x\"\n"), ".section is not implemented yet"));
    EXPECT_TRUE(contains(error(".text\nx: nop\nx: nop\n"), "Multiple definition of symbol x"));
    EXPECT_TRUE(contains(error(".advance 4\n"), ".advance is not inside a section"));
    EXPECT_TRUE(contains(error(".org 4\n"), ".org is not inside a section"));
}

TEST_F(AssemblerUnit, expression_errors)
{
    EXPECT_TRUE(
        contains(error(".data\n.word 1 +\n"), "expected an operand after '+', got end of line"));
    EXPECT_TRUE(contains(error(".data\n.word 4 / 0\n"), "division by zero"));
    EXPECT_TRUE(contains(error(".data\n.word foo\n"), "expected a number, got 'foo'"));
    EXPECT_TRUE(contains(error(".data\n.char 'a', 5\n"), "expected a character literal, got '5'"));
    EXPECT_TRUE(contains(error(".data\n.ascii 5\n"), "expected a string literal, got '5'"));
}

TEST_F(AssemblerUnit, layout_errors)
{
    EXPECT_TRUE(contains(error(".text\n.scend\n"), ".scend must have a matching .scope"));
    EXPECT_TRUE(contains(error(".data\n.align 0\n"), ".align expects a non-zero alignment"));
    EXPECT_TRUE(contains(error(".text\n.advance 6\n"),
                         ".advance cannot move to a byte that is not word aligned"));
    EXPECT_TRUE(contains(error(".text\n.align 6\n"), ".align in .text must be a multiple of 4"));
    EXPECT_TRUE(
        contains(error(".data\n.byte 1\n.org 0\n"), ".org cannot move the assembler backwards"));
    EXPECT_TRUE(
        contains(error(".text\nnop\n.org 2\n"), ".org cannot move the assembler backwards"));
    EXPECT_TRUE(
        contains(error(".text\n.org 6\n"), ".org cannot move to a byte that is not word aligned"));
}

TEST_F(AssemblerUnit, errors_name_the_file_line_and_column)
{
    // `5` is the 5th character of line 3.
    const std::string message = error(".text\nnop\nhlt 5\n");
    EXPECT_TRUE(
        contains(message, "main.bi:3:5: error: unexpected '5' at the end of the statement"));
    EXPECT_TRUE(contains(message, "  hlt 5\n      ^"));
}

TEST_F(AssemblerUnit, statement_wide_errors_point_at_the_start_of_the_statement)
{
    const std::string message = error(".text\nnop\n    add x0, x1, 99999\n");
    EXPECT_TRUE(contains(message, "main.bi:3:5: error: immediate must be a 14 bit value"));
}

TEST_F(AssemblerUnit, lexical_errors_end_the_assembler)
{
    EXPECT_TRUE(contains(error(".text\n.bogus\n"), "lexical error"));
    EXPECT_TRUE(contains(error(".text\nmov x0, $G\n"), "lexical error"));
}

TEST_F(AssemblerUnit, state_after_assembling)
{
    m_last_input = write("state.bi", ".text\nnop\n");
    Assembler assembler(m_process.get(), File(m_last_input), (m_dir / "out" / "state.bo").string());
    EXPECT_EQ(assembler.get_state(), Assembler::NOT_ASSEMBLED);
    assembler.assemble();
    EXPECT_EQ(assembler.get_state(), Assembler::ASSEMBLED);

    m_last_input = write("bad.bi", ".text\nhlt 1\n");
    Assembler failing(m_process.get(), File(m_last_input), (m_dir / "out" / "bad.bo").string());
    EXPECT_THROW(failing.assemble(), aemu::log::FatalError);
    EXPECT_EQ(failing.get_state(), Assembler::ASSEMBLER_ERROR);
}

TEST_F(AssemblerUnit, tokens_from_the_preprocessor_assemble_like_the_bi_file)
{
    const std::string source = "#macro bump(r)\nadd r, r, 1\n#macend\n#define N 7\n"
                               ".global _start\n.text\n_start:\n  #invoke bump(x3)\n"
                               "  mov x4, N\n  hlt\n.data\nvalue: .word N\n";
    const std::string path = write("main.basm", source);
    Preprocessor preprocessor(m_process.get(), File(path), (m_dir / "out" / "main.bi").string());
    const File bi = preprocessor.preprocess();

    Assembler from_text(m_process.get(), bi, (m_dir / "out" / "text.bo").string());
    from_text.assemble();
    Assembler from_tokens(m_process.get(), bi, preprocessor.take_result(),
                          (m_dir / "out" / "tokens.bo").string());
    from_tokens.assemble();
    EXPECT_EQ(from_tokens.get_state(), Assembler::ASSEMBLED);

    const ObjectFile a(from_text.get_output_file());
    const ObjectFile b(from_tokens.get_output_file());
    EXPECT_EQ(a.text_section, b.text_section);
    EXPECT_EQ(a.data_section, b.data_section);
    EXPECT_EQ(a.symbol_table.size(), b.symbol_table.size());
    EXPECT_EQ(a.rel_text.size(), b.rel_text.size());
}

TEST_F(AssemblerUnit, errors_from_preprocessed_tokens_point_at_the_original_source)
{
    const std::string path = write("main.basm", "#macro twice(r)\n    add r, r\n#macend\n.text\n"
                                                "  nop\n  #invoke twice(x1)\n");
    Preprocessor preprocessor(m_process.get(), File(path), (m_dir / "out" / "main.bi").string());
    const File bi = preprocessor.preprocess();
    Assembler assembler(m_process.get(), bi, preprocessor.take_result(),
                        (m_dir / "out" / "main.bo").string());

    const std::string message = error_of([&] { assembler.assemble(); });
    EXPECT_TRUE(contains(message, "main.basm:2:"));
    EXPECT_TRUE(contains(message, "    add r, r"));
    EXPECT_TRUE(contains(message, "main.basm:6:3: note: in expansion of macro 'twice'"));
    EXPECT_FALSE(contains(message, "main.bi"));
    EXPECT_EQ(assembler.get_state(), Assembler::ASSEMBLER_ERROR);
}
