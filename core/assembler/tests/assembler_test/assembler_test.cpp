// Unit tests for the assembler. Each test assembles a small preprocessed source (.bi) and checks
// the resulting object file: the encoded instructions against the emulator's own encoders, the
// data section bytes, symbols and relocations. Errors are checked through their message.

#include "assembler_test/toolchain_fixture.h"

#include <cstdint>
#include <cstring>
#include <filesystem>
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
        Assembler assembler(File(m_last_input), (m_dir / "out" / (name + ".bo")).string());
        assembler.assemble();
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

    /// The warnings the assembler logs for `source` in `.text`.
    std::vector<std::string> warnings_of(const std::string &source)
    {
        std::vector<std::string> messages;
        aemu::log::set_sink(
            [&messages](const aemu::log::Record &r)
            {
                if (r.level == aemu::log::Level::Warn) messages.emplace_back(r.message);
            });
        aemu::log::ScopedLevel level(aemu::log::Level::Warn);
        const Words words = text(source);
        aemu::log::reset_sink();
        EXPECT_FALSE(words.empty());
        return messages;
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

TEST_F(AssemblerUnit, load_into_its_own_base_register_warns_about_the_lost_writeback)
{
    for (const char *load : {"ldr", "ldrb", "ldrh", "ldrsb", "ldrsh"})
    {
        for (const char *form : {"[x1, 4]!", "[x1, x2]!", "[x1], 4", "[x1], x2"})
        {
            const auto warnings = warnings_of(std::string(load) + " x1, " + form + "\n");
            ASSERT_EQ(warnings.size(), 1u) << load << " " << form;
            EXPECT_TRUE(contains(warnings[0], "writeback is lost")) << load << " " << form;
            EXPECT_TRUE(contains(warnings[0], "x1")) << load << " " << form;
        }
    }

    // The access still assembles to the same instruction.
    EXPECT_EQ(warnings_of("ldr x1, [x1, 4]!\n").size(), 1u);
    EXPECT_EQ(text("ldr x1, [x1, 4]!\n"),
              (Words{Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 1, 1, 4,
                                                 Emulator32bit::AddrType::ADDR_PRE_INC)}));
}

TEST_F(AssemblerUnit, store_of_its_own_base_register_warns_about_the_stored_value)
{
    for (const char *store : {"str", "strb", "strh"})
    {
        for (const char *form : {"[x1, 4]!", "[x1, x2]!", "[x1], 4", "[x1], x2"})
        {
            const auto warnings = warnings_of(std::string(store) + " x1, " + form + "\n");
            ASSERT_EQ(warnings.size(), 1u) << store << " " << form;
            EXPECT_TRUE(contains(warnings[0], "before the")) << store << " " << form;
            EXPECT_TRUE(contains(warnings[0], "x1")) << store << " " << form;
        }
    }
}

TEST_F(AssemblerUnit, writeback_that_is_discarded_or_has_no_effect_warns)
{
    // Writes to xzr are discarded, so there is nothing to write back to.
    for (const char *source : {"ldr x1, [xzr, 4]!\n", "str x1, [xzr], 4\n", "ldr xzr, [xzr, 4]!\n"})
    {
        const auto warnings = warnings_of(source);
        ASSERT_EQ(warnings.size(), 1u) << source;
        EXPECT_TRUE(contains(warnings[0], "to xzr is discarded")) << source;
    }

    // A zero offset leaves the base register unchanged.
    for (const char *source : {"ldr x1, [x2, 0]!\n", "str x1, [x2], 0\n", "ldr x1, [x2, xzr]!\n"})
    {
        const auto warnings = warnings_of(source);
        ASSERT_EQ(warnings.size(), 1u) << source;
        EXPECT_TRUE(contains(warnings[0], "has no effect")) << source;
    }
}

TEST_F(AssemblerUnit, ordinary_memory_accesses_do_not_warn)
{
    const auto warnings = warnings_of("ldr x1, [x1]\n"     // no writeback
                                      "ldr x1, [x1, 4]\n"  // no writeback
                                      "str x1, [x1, 4]\n"
                                      "ldr x1, [x2, 0]\n"  // zero offset, but no writeback
                                      "ldr x1, [x2, 4]!\n" // different registers
                                      "ldr x1, [x2], 4\n"
                                      "str x1, [x2, x3]!\n"
                                      "ldr xzr, [x2, 4]!\n"
                                      "ldr x1, [sp, -4]!\n");
    EXPECT_TRUE(warnings.empty()) << warnings.size()
                                  << " unexpected warning(s), first: " << warnings[0];
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

TEST_F(AssemblerUnit, a_branch_offset_in_bytes_is_signed)
{
    using E = Emulator32bit;
    EXPECT_EQ(text("b -4\nb.ne -8\nbl 4 * 3\nb 0 - 16\n"),
              (Words{E::asm_format_b1(E::_op_b, ConditionCode::AL, -1),
                     E::asm_format_b1(E::_op_b, ConditionCode::NE, -2),
                     E::asm_format_b1(E::_op_bl, ConditionCode::AL, 3),
                     E::asm_format_b1(E::_op_b, ConditionCode::AL, -4)}));

    // The ends of the range: 22 bits of words, so -2^23 to 2^23 - 4 bytes.
    EXPECT_EQ(text("b 8388604\nb -8388608\n"),
              (Words{E::asm_format_b1(E::_op_b, ConditionCode::AL, (1 << 21) - 1),
                     E::asm_format_b1(E::_op_b, ConditionCode::AL, -(1 << 21))}));
}

TEST_F(AssemblerUnit, a_branch_offset_out_of_range_is_an_error)
{
    // Used to wrap: 0x800000 was read as a jump backwards.
    EXPECT_TRUE(contains(error(".text\nb 8388608\n"), "branch offset must be between"));
    EXPECT_TRUE(contains(error(".text\nb -8388612\n"), "branch offset must be between"));
    EXPECT_TRUE(contains(error(".text\nb $800000\n"), "branch offset must be between"));
    EXPECT_TRUE(contains(error(".text\nb -6\n"), "branch offset must be 4 byte aligned"));
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

TEST_F(AssemblerUnit, adr_is_a_pc_relative_relocation_with_an_addend)
{
    const ObjectFile object = assemble(".text\n"
                                       "adr x3, buf\n"
                                       "adr x4, buf + 8\n"
                                       "adr x5, here\n"
                                       "here: nop\n"
                                       ".data\n"
                                       "buf: .word 1\n");
    using Type = ObjectFile::RelocationEntry::Type;
    ASSERT_EQ(object.rel_text.size(), 3u);
    EXPECT_EQ(object.rel_text[0].type, Type::R_EMU32_ADR_PCREL21);
    EXPECT_EQ(object.rel_text[1].type, Type::R_EMU32_ADR_PCREL21);
    EXPECT_EQ(object.rel_text[1].addend, 8);
    EXPECT_EQ(object.rel_text[2].type, Type::R_EMU32_ADR_PCREL21)
        << "a label of the file as well, the linker places the sections";
    EXPECT_EQ(object.text_section[0], Emulator32bit::asm_format_m1(Emulator32bit::_op_adr, 3, 0));
}

TEST_F(AssemblerUnit, adr_takes_a_symbol_and_not_a_part_of_an_address)
{
    EXPECT_TRUE(contains(error(".text\nadr x0, :hi20:buf\n.data\nbuf: .word 1\n"), "symbol"));
    EXPECT_TRUE(contains(error(".text\nadr x0, 12\n"), "symbol"));
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

// The value of `expression` as a `.sdword`.
#define EXPECT_VALUE(expression, expected)                                                         \
    do                                                                                             \
    {                                                                                              \
        const Bytes bytes = data(std::string(".sdword ") + expression + "\n");                     \
        sdword value = 0;                                                                          \
        std::memcpy(&value, bytes.data(), sizeof(value));                                          \
        EXPECT_EQ(value, sdword(expected)) << expression;                                          \
    } while (0)

TEST_F(AssemblerUnit, multiplication_binds_tighter_than_addition)
{
    EXPECT_EQ(data(".word 1 + 2 * 3\n.word 10 - 4 / 2\n"), (Bytes{7, 0, 0, 0, 8, 0, 0, 0}));
    EXPECT_VALUE("2 * 3 + 4 * 5", 26);
    EXPECT_VALUE("100 - 10 - 5", 85); // left associative
    EXPECT_VALUE("100 / 10 / 5", 2);
    EXPECT_VALUE("17 % 5 * 2", 4);
}

TEST_F(AssemblerUnit, parentheses_group)
{
    EXPECT_VALUE("(1 + 2) * 3", 9);
    EXPECT_VALUE("((2))", 2);
    EXPECT_VALUE("2 * (3 + (4 - 1)) - 1", 11);
}

TEST_F(AssemblerUnit, unary_operators)
{
    EXPECT_VALUE("-5", -5);
    EXPECT_VALUE("- -5", 5);
    EXPECT_VALUE("+5", 5);
    EXPECT_VALUE("-2 + 1", -1);
    EXPECT_VALUE("-(2 + 1)", -3);
    EXPECT_VALUE("2 * -3", -6);
    EXPECT_VALUE("~0", -1);
    EXPECT_VALUE("~5 & 15", 10);
    EXPECT_VALUE("!0", 1);
    EXPECT_VALUE("!7", 0);
    EXPECT_VALUE("!0 + 1", 2);
    EXPECT_VALUE("-'a'", -97);
}

TEST_F(AssemblerUnit, bitwise_and_shift_operators_follow_the_precedence_of_c)
{
    EXPECT_VALUE("1 << 4", 16);
    EXPECT_VALUE("256 >> 4", 16);
    EXPECT_VALUE("-16 >> 2", -4);  // arithmetic shift
    EXPECT_VALUE("1 << 2 + 1", 8); // + binds tighter than <<
    EXPECT_VALUE("6 & 3", 2);
    EXPECT_VALUE("6 | 3", 7);
    EXPECT_VALUE("6 ^ 3", 5);
    EXPECT_VALUE("1 | 2 & 0", 1); // & binds tighter than |
    EXPECT_VALUE("1 | 2 ^ 3 & 1", 3);
    EXPECT_VALUE("$FF & ~$0F", 0xF0);
    EXPECT_VALUE("1 << 63", INT64_MIN);
}

TEST_F(AssemblerUnit, comparisons_and_logic_give_one_or_zero)
{
    EXPECT_VALUE("3 < 4", 1);
    EXPECT_VALUE("4 < 4", 0);
    EXPECT_VALUE("4 <= 4", 1);
    EXPECT_VALUE("5 > 4", 1);
    EXPECT_VALUE("4 >= 5", 0);
    EXPECT_VALUE("4 == 4", 1);
    EXPECT_VALUE("4 != 4", 0);
    EXPECT_VALUE("-1 < 0", 1); // signed
    EXPECT_VALUE("1 + 1 == 2 && 3 > 2", 1);
    EXPECT_VALUE("0 || 0", 0);
    EXPECT_VALUE("0 || 5", 1);
    EXPECT_VALUE("1 || 0 && 0", 1); // && binds tighter than ||
    EXPECT_VALUE("1 < 2 == 1", 1);
}

TEST_F(AssemblerUnit, division_follows_the_sign_of_c)
{
    EXPECT_VALUE("-7 / 2", -3);
    EXPECT_VALUE("-7 % 3", -1);
    EXPECT_VALUE("7 / -2", -3);
    EXPECT_VALUE("(0 - 9223372036854775807 - 1) / -1", INT64_MIN);
    EXPECT_VALUE("(0 - 9223372036854775807 - 1) % -1", 0);
}

TEST_F(AssemblerUnit, expressions_are_usable_wherever_a_number_is)
{
    EXPECT_EQ(text("mov x1, (3 + 4) * 2\nadd x1, x2, 1 << 4\n"),
              (Words{Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, false, 1, 14),
                     Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, 1, 2, 16)}));
    EXPECT_EQ(text("ldr x0, [x1, -2 * 4]\nldr x0, [x1, (1 + 1) * 4]\n"),
              (Words{Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, -8,
                                                 Emulator32bit::AddrType::ADDR_OFFSET),
                     Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, 8,
                                                 Emulator32bit::AddrType::ADDR_OFFSET)}));
    EXPECT_EQ(data(".byte -1, ~0 & 255, 3 * 3\n"), (Bytes{0xFF, 0xFF, 9}));
}

TEST_F(AssemblerUnit, a_negative_memory_offset_is_a_value_not_a_prefix)
{
    // The minus used to negate everything after it, so `-2 + 1` was -3.
    EXPECT_EQ(text("ldr x0, [x1, -4 + 2]\n"),
              (Words{Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, -2,
                                                 Emulator32bit::AddrType::ADDR_OFFSET)}));
    EXPECT_TRUE(
        contains(error(".text\nldr x0, [x1, -2049]\n"), "offset must be a signed 12 bit value"));
    EXPECT_TRUE(
        contains(error(".text\nldr x0, [x1, 2048]\n"), "offset must be a signed 12 bit value"));
}

TEST_F(AssemblerUnit, equ_names_a_number)
{
    EXPECT_EQ(data(".equ SIZE, 3 * 4\n.byte SIZE, SIZE + 1\n.equ DOUBLE, SIZE * 2\n.byte DOUBLE\n"),
              (Bytes{12, 13, 24}));

    // Wherever a number is.
    EXPECT_EQ(
        text(".equ N, 5\nmov x1, N * 2\nadd x1, x2, N\nldr x0, [x1, -N]\nb.ne N - 1 + 3 - 3 + 4\n"),
        (Words{Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, false, 1, 10),
               Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, 1, 2, 5),
               Emulator32bit::asm_format_m(Emulator32bit::_op_ldr, false, 0, 1, -5,
                                           Emulator32bit::AddrType::ADDR_OFFSET),
               Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::NE, 2)}));

    // A constant is not a symbol of the object file.
    EXPECT_EQ(assemble(".equ N, 5\n").symbol_table.size(), 0u);
}

TEST_F(AssemblerUnit, a_constant_alone_in_a_data_directive_is_its_value_not_an_address)
{
    EXPECT_EQ(data(".equ N, 7\n.word N\n"), (Bytes{7, 0, 0, 0}));
    // A label alone is still an address, for the linker.
    const ObjectFile object = assemble(".data\nfirst: .word first\n");
    EXPECT_EQ(object.rel_data.size(), 1u);
}

TEST_F(AssemblerUnit, a_constant_can_be_a_branch_distance)
{
    EXPECT_EQ(text(".equ BACK, -8\nnop\nnop\nb BACK\n").back(),
              Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, -2));
}

TEST_F(AssemblerUnit, the_distance_between_two_labels_is_a_number)
{
    EXPECT_EQ(data("start: .byte 1, 2, 3\nend:\n.byte end - start\n.word (end - start) * 2\n"),
              (Bytes{1, 2, 3, 3, 6, 0, 0, 0}));
    EXPECT_EQ(data("lo: .word 0\nhi: .word 0\n.byte hi - lo, lo - hi\n").back(), 0xFC);

    // A constant takes the distance, and the labels need not be in .data.
    EXPECT_EQ(text("first: nop\nnop\nlast:\n.equ LEN, last - first\nmov x0, LEN\n").back(),
              Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, false, 0, 8));
}

TEST_F(AssemblerUnit, constants_and_labels_follow_scopes)
{
    const Bytes bytes = data(".equ X, 1\n.scope\n.equ X, 2\n.byte X\n.scend\n.byte X\n"
                             ".scope\n.byte X\n.scend\n");
    EXPECT_EQ(bytes, (Bytes{2, 1, 1}));

    EXPECT_EQ(data("one: .byte 0\n.scope\ntwo: .byte 0\n.byte two - one\n.scend\n").back(), 1);
}

TEST_F(AssemblerUnit, errors_of_constants_and_labels_in_expressions)
{
    EXPECT_TRUE(contains(error(".data\n.byte later\n"), "cannot hold the address of a symbol"));
    EXPECT_TRUE(contains(error(".data\n.byte later - 1\n"), "cannot hold the address of a symbol"));
    EXPECT_TRUE(contains(error(".data\n.byte end - start\nstart:\nend:\n"),
                         "'end' is not defined before this point"));
    EXPECT_TRUE(contains(error(".data\nlabel: .byte 1\n.byte label + 1\n"),
                         "cannot hold the address of a symbol"));
    EXPECT_TRUE(contains(error(".data\nlabel: .byte 1\n.byte 1 + label\n"),
                         "cannot hold the address of a symbol"));
    EXPECT_TRUE(contains(error(".data\nlabel: .byte 1\n.word 1 - label\n"),
                         "the address of 'label' is not known until linking"));
    EXPECT_TRUE(contains(error(".data\nlabel: .byte 1\n.word 2 * label\n"),
                         "the address of 'label' is not known until linking"));
    EXPECT_TRUE(contains(error(".data\nlabel: .byte 1\n.word (label + 1) >> 1\n"),
                         "the address of 'label' is not known until linking"));
    EXPECT_TRUE(contains(error(".data\nfirst: .byte 1\nsecond: .byte 1\n.word first + second\n"),
                         "the address of 'first' is not known until linking"));
    EXPECT_TRUE(contains(error(".data\nlabel: .byte 1\n.word label - later\n"),
                         "'later' is not defined before this point"));
    EXPECT_TRUE(contains(error(".data\n.word later + 99999999999\n"), "does not fit in 32 bits"));
    EXPECT_TRUE(contains(error(".data\nlabel: .byte 1\n.equ L, label\n"),
                         "the address of 'label' is not known until linking"));
    EXPECT_TRUE(contains(error(".data\nlabel: .byte 1\n.byte -label\n"),
                         "the address of 'label' is not known until linking"));
    EXPECT_TRUE(contains(error(".data\ndata_label: .byte 1\n.text\ncode_label: nop\n.data\n.byte "
                               "code_label - data_label\n"),
                         "are in different sections"));
    EXPECT_TRUE(contains(error(".equ A, 1\n.equ A, 2\n"), "'A' is already a constant"));
    EXPECT_TRUE(contains(error(".data\nA: .byte 1\n.equ A, 2\n"), "'A' is already a label"));
    EXPECT_TRUE(contains(error(".equ A, 1\n.data\nA: .byte 1\n"), "'A' is already a constant"));
    EXPECT_TRUE(contains(error(".equ\n"), "expected a name after .equ"));
    EXPECT_TRUE(contains(error(".equ A 1\n"), "expected ',' and the value of the constant"));
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
    EXPECT_TRUE(
        contains(error(".text\nadd x0, x1, foo\n"), "'foo' is not defined before this point"));
    EXPECT_TRUE(contains(error(".text\nadd x0, x1, ,\n"),
                         "expected a register, a number or a relocation, got ','"));
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
    EXPECT_TRUE(contains(error(".global\n"), "expected a symbol after .global"));
    EXPECT_TRUE(contains(error(".text\nx: nop\nx: nop\n"), "Multiple definition of symbol x"));
    EXPECT_TRUE(contains(error(".advance 4\n"), ".advance is not inside a section"));
    EXPECT_TRUE(contains(error(".org 4\n"), ".org is not inside a section"));
}

TEST_F(AssemblerUnit, expression_errors)
{
    EXPECT_TRUE(
        contains(error(".data\n.word 1 +\n"), "expected an operand after '+', got end of line"));
    EXPECT_TRUE(contains(error(".data\n.word 4 / 0\n"), "division by zero"));
    EXPECT_TRUE(
        contains(error(".data\n.word 1 - foo\n"), "'foo' is not defined before this point"));
    EXPECT_TRUE(contains(error(".data\n.byte foo\n"),
                         ".byte cannot hold the address of a symbol, only .word can"));
    EXPECT_TRUE(contains(error(".data\n.word 1,\n"), "expected a number, got end of line"));
    EXPECT_TRUE(contains(error(".data\n.char 'a', 5\n"), "expected a character literal, got '5'"));
    EXPECT_TRUE(contains(error(".data\n.ascii 5\n"), "expected a string literal, got '5'"));
}

TEST_F(AssemblerUnit, errors_of_the_expression_operators)
{
    EXPECT_TRUE(
        contains(error(".data\n.word (1 + 2\n"), "expected ')' to close the '(' at column 7"));
    EXPECT_TRUE(
        contains(error(".data\n.word 1 + 2)\n"), "unexpected ')' at the end of the statement"));
    EXPECT_TRUE(contains(error(".data\n.word ()\n"), "expected a number, got ')'"));
    EXPECT_TRUE(contains(error(".data\n.word 5 % 0\n"), "remainder of a division by zero"));
    EXPECT_TRUE(
        contains(error(".data\n.word 1 << 64\n"), "the shift amount must be 0 to 63, got 64"));
    EXPECT_TRUE(
        contains(error(".data\n.word 1 >> -1\n"), "the shift amount must be 0 to 63, got -1"));
    EXPECT_TRUE(
        contains(error(".data\n.word 2 * * 3\n"), "expected an operand after '*', got '*'"));
    EXPECT_TRUE(contains(error(".data\n.word -\n"), "expected a number, got end of line"));
    EXPECT_TRUE(contains(
        error(".data\n.word " + std::string(300, '(') + "1" + std::string(300, ')') + "\n"),
        "the expression is nested too deeply"));
    EXPECT_TRUE(contains(error(".data\n.word " + std::string(300, '-') + "1\n"),
                         "the expression is nested too deeply"));
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

// A declaration does not depend on the section, so a macro can declare a symbol.
TEST_F(AssemblerUnit, global_and_extern_can_be_declared_in_a_section)
{
    const ObjectFile object = assemble(".text\n.global entry\n.extern other\nentry: nop\n");
    EXPECT_EQ(symbol(object, "entry").binding_info,
              ObjectFile::SymbolTableEntry::BindingInfo::GLOBAL);
    EXPECT_EQ(symbol(object, "other").binding_info,
              ObjectFile::SymbolTableEntry::BindingInfo::WEAK);
    EXPECT_EQ(symbol(object, "other").section, U32(-1));
}

TEST_F(AssemblerUnit, a_size_that_is_too_big_is_an_error_and_not_skipped)
{
    EXPECT_TRUE(contains(error(".bss\n.advance 16777216\nend:\n"),
                         ".advance is large and likely unintentional (16777216)"));
    EXPECT_TRUE(contains(error(".data\n.align 65536\n"),
                         ".align is large and likely unintentional (65536)"));
    EXPECT_TRUE(contains(error(".bss\n.org 16777216\n"), "new value is large"));
}

TEST_F(AssemblerUnit, data_that_does_not_fit_its_size_is_an_error)
{
    EXPECT_TRUE(contains(error(".data\n.byte 256\n"), ".byte value 256 does not fit in 1 byte(s)"));
    EXPECT_TRUE(
        contains(error(".data\n.dbyte 65536\n"), ".dbyte value 65536 does not fit in 2 byte(s)"));
    EXPECT_TRUE(contains(error(".data\n.word 4294967296\n"), ".word value"));
    EXPECT_TRUE(contains(error(".data\n.byte 1, 2, 300\n"), "does not fit"));
    EXPECT_TRUE(contains(error(".data\n.byte 0 - 129\n"), ".byte value -129 does not fit"));
}

TEST_F(AssemblerUnit, data_that_fits_its_size_is_stored)
{
    EXPECT_EQ(data(".byte 255, 0 - 1, 0 - 128\n"), (Bytes{0xFF, 0xFF, 0x80}));
    EXPECT_EQ(data(".dbyte 65535, 0 - 2\n"), (Bytes{0xFF, 0xFF, 0xFE, 0xFF}));
    EXPECT_EQ(data(".word 4294967295\n"), (Bytes{0xFF, 0xFF, 0xFF, 0xFF}));
    EXPECT_EQ(data(".dword 0 - 1\n"), Bytes(8, 0xFF));
}

// `.word label` is the address of the label, which only the linker knows.
TEST_F(AssemblerUnit, word_with_a_symbol_is_a_relocation)
{
    const ObjectFile object = assemble(".data\n.word 7, target, 9\nafter: .byte 1\n"
                                       ".text\ntarget: nop\n");

    EXPECT_EQ(object.data_section, (Bytes{7, 0, 0, 0, 0, 0, 0, 0, 9, 0, 0, 0, 1}))
        << "a place for the address";
    ASSERT_EQ(object.rel_data.size(), 1u);
    EXPECT_EQ(object.rel_data[0].offset, 4u);
    EXPECT_EQ(object.rel_data[0].type, ObjectFile::RelocationEntry::Type::R_EMU32_ABS32);
    EXPECT_EQ(object.strings.at(object.symbol_table.at(object.rel_data[0].symbol).symbol_name),
              "target");
    EXPECT_TRUE(object.rel_text.empty());
}

TEST_F(AssemblerUnit, word_with_a_symbol_of_another_file_leaves_it_undefined)
{
    const ObjectFile object = assemble(".data\npointer: .word elsewhere\n");
    EXPECT_EQ(symbol(object, "elsewhere").section, U32(-1));
    EXPECT_EQ(object.rel_data.size(), 1u);
}

// `symbol + number` is the symbol with an addend, whether the symbol is defined, comes later or is
// in another file.
TEST_F(AssemblerUnit, a_number_added_to_a_symbol_is_the_addend_of_its_relocation)
{
    const ObjectFile object =
        assemble(".equ K, 8\n"
                 ".data\nbuf: .word 1, 2, 3\n"
                 "table: .word buf, buf + 8, 4 + buf, buf - 4, buf + 2 * 3 - 6\n"
                 ".word buf + (table - buf), later + 4, elsewhere - 1, buf + K, K + buf\n"
                 "later: .word 0\n");

    const std::vector<std::pair<std::string, sword>> expected = {
        {"buf", 0},  {"buf", 8},   {"buf", 4},        {"buf", -4}, {"buf", 0},
        {"buf", 12}, {"later", 4}, {"elsewhere", -1}, {"buf", 8},  {"buf", 8}};
    ASSERT_EQ(object.rel_data.size(), expected.size());
    for (size_t i = 0; i < expected.size(); i++)
    {
        EXPECT_EQ(name_of(object, object.rel_data[i]), expected[i].first) << i;
        EXPECT_EQ(object.rel_data[i].addend, expected[i].second) << i;
        EXPECT_EQ(object.rel_data[i].type, ObjectFile::RelocationEntry::Type::R_EMU32_ABS32);
        EXPECT_EQ(object.rel_data[i].offset, 12 + i * 4);
    }

    // The words are left to the linker, the addend is not in them.
    Bytes expected_data = {1, 0, 0, 0, 2, 0, 0, 0, 3, 0, 0, 0};
    expected_data.resize(12 + 4 * expected.size() + 4, 0);
    EXPECT_EQ(object.data_section, expected_data);
}

TEST_F(AssemblerUnit, instructions_can_add_a_number_to_a_symbol)
{
    const ObjectFile object = assemble(".text\n"
                                       "adrp x0, buf + 8\n"
                                       "add x0, x0, :lo12:buf + 8\n"
                                       "mov x1, :hi13:buf - 4\n"
                                       "mov x1, :lo19:buf + $10\n"
                                       "bl printf + 8\n"
                                       ".data\nbuf: .word 1\n");
    using Type = ObjectFile::RelocationEntry::Type;
    const std::vector<std::pair<Type, sword>> expected = {{Type::R_EMU32_ADRP_HI20, 8},
                                                          {Type::R_EMU32_O_LO12, 8},
                                                          {Type::R_EMU32_MOV_HI13, -4},
                                                          {Type::R_EMU32_MOV_LO19, 16},
                                                          {Type::R_EMU32_B_OFFSET22, 8}};
    ASSERT_EQ(object.rel_text.size(), expected.size());
    for (size_t i = 0; i < expected.size(); i++)
    {
        EXPECT_EQ(object.rel_text[i].type, expected[i].first) << i;
        EXPECT_EQ(object.rel_text[i].addend, expected[i].second) << i;
        EXPECT_EQ(object.rel_text[i].offset, i * 4);
    }
    EXPECT_EQ(name_of(object, object.rel_text[4]), "printf");

    // The instruction itself does not change, only what the linker puts in it.
    EXPECT_EQ(text("adrp x0, buf + 8\n"), text("adrp x0, buf\n"));
}

TEST_F(AssemblerUnit, a_branch_to_a_label_of_the_file_with_a_number_is_resolved)
{
    ObjectFile object = assemble(".text\nstart: nop\nnop\nnop\nb start + 8\n");
    EXPECT_TRUE(object.rel_text.empty());
    EXPECT_EQ(object.text_section[3],
              Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, -1));

    object = assemble(".text\nb later + 4\nnop\nlater: nop\nnop\n");
    EXPECT_TRUE(object.rel_text.empty());
    EXPECT_EQ(object.text_section[0],
              Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, 3));

    EXPECT_TRUE(contains(error(".text\nstart: nop\nb start + 2\n"), "4 byte aligned"));
}

TEST_F(AssemblerUnit, a_symbol_with_a_number_is_looked_up_in_the_scopes)
{
    const ObjectFile object = assemble(".text\n"
                                       ".scope\nloop: nop\nb loop + 4\n.scend\n"
                                       ".scope\nloop: nop\nb loop + 4\n.scend\n");
    ASSERT_EQ(object.rel_text.size(), 2u);
    EXPECT_EQ(name_of(object, object.rel_text[0]), "loop::SCOPE:0");
    EXPECT_EQ(name_of(object, object.rel_text[1]), "loop::SCOPE:1");
    EXPECT_EQ(object.rel_text[0].addend, 4);
    EXPECT_EQ(object.rel_text[1].addend, 4);
}

TEST_F(AssemblerUnit, the_addend_is_kept_in_the_object_file)
{
    const ObjectFile object = assemble(".data\n.word target - 100, target + 2147483647\n");
    ASSERT_EQ(object.rel_data.size(), 2u);
    EXPECT_EQ(object.rel_data[0].addend, -100);
    EXPECT_EQ(object.rel_data[1].addend, 2147483647);
}

// A label of a scope is not the label of the same name in another scope (a macro that is used twice).

// A label of a scope is not the label of the same name in another scope (a macro that is used twice).
TEST_F(AssemblerUnit, a_symbol_in_data_is_looked_up_in_the_scopes)
{
    const ObjectFile object = assemble(".data\n"
                                       ".scope\nlocal: .word 5\nfirst: .word local\n.scend\n"
                                       ".scope\nlocal: .word 6\nsecond: .word local\n.scend\n");

    ASSERT_EQ(object.rel_data.size(), 2u);
    EXPECT_EQ(object.strings.at(object.symbol_table.at(object.rel_data[0].symbol).symbol_name),
              "local::SCOPE:0");
    EXPECT_EQ(object.strings.at(object.symbol_table.at(object.rel_data[1].symbol).symbol_name),
              "local::SCOPE:1");
}

TEST_F(AssemblerUnit, align_is_recorded_in_the_section)
{
    const ObjectFile object = assemble(".data\n.byte 1\n.align 8\n.align 4\n.text\nnop\n.align 16\n"
                                       ".bss\n.advance 3\n.align 32\n");
    const auto alignment = [&](const char *name)
    { return object.sections.at(object.section_table.at(name)).alignment; };

    EXPECT_EQ(alignment(".data"), 8u) << "the largest";
    EXPECT_EQ(alignment(".text"), 16u);
    EXPECT_EQ(alignment(".bss"), 32u);

    const ObjectFile plain = assemble(".data\n.byte 1\n");
    EXPECT_EQ(plain.sections.at(plain.section_table.at(".data")).alignment, 1u);
    EXPECT_EQ(plain.sections.at(plain.section_table.at(".text")).alignment, 4u);
}

TEST_F(AssemblerUnit, a_scope_that_is_never_closed_is_an_error)
{
    const std::string message = error(".text\n.scope\nnop\n");
    EXPECT_TRUE(contains(message, ".scope is never closed with .scend"));
    EXPECT_TRUE(contains(message, "main.bi:2:"));
    EXPECT_NO_THROW(assemble(".text\n.scope\nnop\n.scend\n"));
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

TEST_F(AssemblerUnit, assembling_twice_does_the_work_once_and_an_error_writes_no_file)
{
    m_last_input = write("state.bi", ".text\nnop\n");
    Assembler assembler(File(m_last_input), (m_dir / "out" / "state.bo").string());
    assembler.assemble();
    assembler.assemble();
    EXPECT_EQ(assembler.object().text_section.size(), 1u);

    m_last_input = write("bad.bi", ".text\nhlt 1\n");
    const std::string bad_path = (m_dir / "out" / "bad.bo").string();
    Assembler failing(File(m_last_input), bad_path);
    EXPECT_THROW(failing.assemble(), aemu::log::FatalError);
    EXPECT_EQ(std::filesystem::file_size(bad_path), 0u)
        << "only the empty file the constructor made";
}

TEST_F(AssemblerUnit, tokens_from_the_preprocessor_assemble_like_the_bi_file)
{
    const std::string source = "#macro bump(r)\nadd r, r, 1\n#macend\n#define N 7\n"
                               ".global _start\n.text\n_start:\n  #invoke bump(x3)\n"
                               "  mov x4, N\n  hlt\n.data\nvalue: .word N\n";
    const std::string path = write("main.basm", source);
    Preprocessor preprocessor(File(path), (m_dir / "out" / "main.bi").string(), m_options);
    const File bi = preprocessor.preprocess();

    Assembler from_text(bi, (m_dir / "out" / "text.bo").string());
    from_text.assemble();
    Assembler from_tokens(bi, preprocessor.take_result(), (m_dir / "out" / "tokens.bo").string());
    from_tokens.assemble();

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
    Preprocessor preprocessor(File(path), (m_dir / "out" / "main.bi").string(), m_options);
    const File bi = preprocessor.preprocess();
    Assembler assembler(bi, preprocessor.take_result(), (m_dir / "out" / "main.bo").string());

    const std::string message = error_of([&] { assembler.assemble(); });
    EXPECT_TRUE(contains(message, "main.basm:2:"));
    EXPECT_TRUE(contains(message, "    add r, r"));
    EXPECT_TRUE(contains(message, "main.basm:6:3: note: in expansion of macro 'twice'"));
    EXPECT_FALSE(contains(message, "main.bi"));
}

// ---------------------------------------------------------------------------------------------
// .section "name", "flags": sections of the program's own
// ---------------------------------------------------------------------------------------------

TEST_F(AssemblerUnit, a_section_of_the_programs_own_holds_the_bytes_and_labels_of_its_directives)
{
    const ObjectFile object = assemble(".section \"mine\"\n"
                                       "first: .word 1, 2\n"
                                       "second: .byte 3\n"
                                       ".text\nnop\n"
                                       ".section \"mine\"\n" // back to it
                                       ".byte 4\n"
                                       ".asciz \"hi\"\n");

    ASSERT_EQ(object.user_sections.size(), 1u);
    const ObjectFile::UserSection &mine = object.user_sections[0];
    EXPECT_EQ(mine.name, "mine");
    EXPECT_TRUE(mine.writable) << "no flags are \"rw\"";
    EXPECT_FALSE(mine.executable);
    EXPECT_EQ(mine.bytes, (Bytes{1, 0, 0, 0, 2, 0, 0, 0, 3, 4, 'h', 'i', 0}));
    EXPECT_EQ(object.sections[mine.header_index].type, ObjectFile::SectionHeader::Type::USER_RW);
    EXPECT_EQ(object.sections[mine.header_index + 1].type,
              ObjectFile::SectionHeader::Type::REL_USER);

    EXPECT_EQ(symbol(object, "first").section, mine.header_index);
    EXPECT_EQ(symbol(object, "first").symbol_value, 0u);
    EXPECT_EQ(symbol(object, "second").symbol_value, 8u);
    EXPECT_TRUE(object.data_section.empty()) << ".data is another section";
}

TEST_F(AssemblerUnit, the_flags_say_whether_the_section_is_written_or_run)
{
    const ObjectFile object = assemble(".section \"a\", \"r\"\n.byte 1\n"
                                       ".section \"b\", \"rw\"\n.byte 1\n"
                                       ".section \"c\", \"rx\"\nnop\n"
                                       ".section \"d\", \"xr\"\nnop\n"
                                       ".section \"e\", \"w\"\n.byte 1\n");
    using Type = ObjectFile::SectionHeader::Type;
    const auto type = [&](const char *name)
    { return object.sections[object.find_user_section(name)->header_index].type; };
    EXPECT_EQ(type("a"), Type::USER_R);
    EXPECT_EQ(type("b"), Type::USER_RW);
    EXPECT_EQ(type("c"), Type::USER_RX);
    EXPECT_EQ(type("d"), Type::USER_RX);
    EXPECT_EQ(type("e"), Type::USER_RW);
    EXPECT_EQ(object.sections[object.find_user_section("c")->header_index].alignment, 4u)
        << "code is made of words";
}

TEST_F(AssemblerUnit, instructions_in_an_executable_section_are_bytes_with_relocations)
{
    const ObjectFile object = assemble(".section \"code\", \"rx\"\n"
                                       "entry: nop\n"
                                       "b target\n"
                                       "adr x1, target\n"
                                       "ldr x2, =target\n"
                                       "target: hlt\n"
                                       ".text\n");
    const ObjectFile::UserSection &code = *object.find_user_section("code");
    const Words expected = {Emulator32bit::asm_nop(),
                            Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, 0),
                            Emulator32bit::asm_format_m1(Emulator32bit::_op_adr, 1, 0),
                            Emulator32bit::asm_format_m1(Emulator32bit::_op_adrp, 2, 0),
                            Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, 2, 2, 0),
                            Emulator32bit::asm_hlt()};
    ASSERT_EQ(code.bytes.size(), expected.size() * 4);
    for (size_t i = 0; i < expected.size(); i++)
    {
        word instruction = 0;
        std::memcpy(&instruction, &code.bytes[i * 4], 4);
        EXPECT_EQ(instruction, expected[i]) << "instruction " << i;
    }
    EXPECT_EQ(symbol(object, "target").symbol_value, 5 * 4u);

    using Type = ObjectFile::RelocationEntry::Type;
    ASSERT_EQ(code.relocations.size(), 4u);
    const std::vector<std::pair<Type, word>> at = {{Type::R_EMU32_B_OFFSET22, 4},
                                                  {Type::R_EMU32_ADR_PCREL21, 8},
                                                  {Type::R_EMU32_ADRP_HI20, 12},
                                                  {Type::R_EMU32_O_LO12, 16}};
    for (size_t i = 0; i < at.size(); i++)
    {
        EXPECT_EQ(code.relocations[i].type, at[i].first) << i;
        EXPECT_EQ(code.relocations[i].offset, at[i].second) << i;
        EXPECT_EQ(name_of(object, code.relocations[i]), "target") << i;
    }
    EXPECT_TRUE(object.rel_text.empty());
    EXPECT_TRUE(object.text_section.empty());
}

TEST_F(AssemblerUnit, a_word_in_a_user_section_can_be_the_address_of_a_symbol)
{
    const ObjectFile object = assemble(".section \"table\", \"r\"\n"
                                       "entries: .word handler, handler + 4\n"
                                       ".text\nhandler: nop\n");
    const ObjectFile::UserSection &table = *object.find_user_section("table");
    ASSERT_EQ(table.relocations.size(), 2u);
    EXPECT_EQ(table.relocations[0].type, ObjectFile::RelocationEntry::Type::R_EMU32_ABS32);
    EXPECT_EQ(table.relocations[0].offset, 0u);
    EXPECT_EQ(table.relocations[1].offset, 4u);
    EXPECT_EQ(table.relocations[1].addend, 4);
    EXPECT_EQ(name_of(object, table.relocations[1]), "handler");
}

TEST_F(AssemblerUnit, org_advance_and_align_work_in_a_user_section)
{
    const ObjectFile object = assemble(".section \"s\"\n.byte 1\n.align 4\n.byte 2\n.advance 3\n"
                                       ".org 12\n.byte 3\n");
    EXPECT_EQ(object.find_user_section("s")->bytes,
              (Bytes{1, 0, 0, 0, 2, 0, 0, 0, 0, 0, 0, 0, 3}));
    EXPECT_EQ(object.sections[object.find_user_section("s")->header_index].alignment, 4u);
}

TEST_F(AssemblerUnit, the_names_of_the_sections_of_the_assembler_are_those_sections)
{
    const ObjectFile object = assemble(".section \".data\"\n.byte 1\n"
                                       ".section \".rodata\", \"r\"\n.byte 2\n"
                                       ".section \".text\", \"rx\"\nnop\n"
                                       ".section \".bss\"\n.advance 8\n"
                                       ".section \".init_array\"\n.word 0\n"
                                       ".section \".fini_array\"\n.word 0\n");
    EXPECT_EQ(object.data_section, (Bytes{1}));
    EXPECT_EQ(object.rodata_section, (Bytes{2}));
    EXPECT_EQ(object.text_section.size(), 1u);
    EXPECT_EQ(object.bss_section, 8u);
    EXPECT_EQ(object.init_array_section.size(), 4u);
    EXPECT_EQ(object.fini_array_section.size(), 4u);
    EXPECT_TRUE(object.user_sections.empty());
}

TEST_F(AssemblerUnit, section_directive_errors)
{
    EXPECT_TRUE(contains(error(".section\n"), "name of the section as a string"));
    EXPECT_TRUE(contains(error(".section x\n"), "name of the section as a string"));
    EXPECT_TRUE(contains(error(".section \"x\",\n"), "flags of the section as a string"));
    EXPECT_TRUE(contains(error(".section \"x\", \"rq\"\n"), "unknown flag 'q'"));
    EXPECT_TRUE(contains(error(".section \"x\", \"\"\n"), "the flags of a section are"));
    EXPECT_TRUE(contains(error(".section \"x\", \"rwx\"\n"),
                         "cannot be both writable and executable"));
    EXPECT_TRUE(contains(error(".section \"\"\n"), "name of a section cannot be empty"));
    EXPECT_TRUE(contains(error(".section \"a b\"\n"), "cannot have spaces"));
    EXPECT_TRUE(contains(error(".section \".symtab\"\n"), "name that the object file uses"));
    EXPECT_TRUE(contains(error(".section \".rel.text\"\n"), "name that the object file uses"));
    EXPECT_TRUE(contains(error(".section \".relfoo\"\n"), "name that the object file uses"));
    EXPECT_TRUE(contains(error(".section \".data\", \"rx\"\n"),
                         "the flags of .data are \"rw\", they cannot be changed"));
    EXPECT_TRUE(contains(error(".section \".text\", \"rw\"\n"),
                         "the flags of .text are \"rx\", they cannot be changed"));
    EXPECT_TRUE(contains(error(".section \"x\", \"r\"\n.section \"x\", \"rw\"\n"),
                         "the section x was made \"r\", the flags cannot be changed"));
    EXPECT_TRUE(contains(error(".section \"x\"\nnop\n"), "code must be located in the .text"));
    EXPECT_TRUE(contains(error(".section \"x\", \"r\"\nnop\n"), "code must be located in the .text"));
    EXPECT_TRUE(contains(error(".section \"x\", \"rx\"\n.byte 1\nnop\n"),
                         "start at a multiple of 4 bytes"));
    EXPECT_TRUE(contains(error(".section \"x\", \"rx\"\nnop\n.byte 1\n"),
                         "not a whole number of instructions"));
    EXPECT_TRUE(contains(error(".section \"x\"\n.word 1\n.section \"x\"\nx: .word 2\nx: .word 3\n"),
                         "Multiple definition of symbol x"));
}
