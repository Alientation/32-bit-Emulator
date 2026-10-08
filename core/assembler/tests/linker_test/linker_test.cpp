// Unit tests for reading linker scripts (.ld) in the linker. A small program is assembled, linked
// with the script under test and the section addresses of the executable are checked.

#include "assembler_test/toolchain_fixture.h"

#include <vector>

namespace
{

// 4 bytes of code, 12 bytes of data and 16 bytes of bss.
constexpr const char *kProgram = ".global _start\n"
                                 ".text\n"
                                 "_start: nop\n"
                                 ".data\n"
                                 "values: .word 1, 2, 3\n"
                                 ".bss\n"
                                 "buffer: .advance 16\n";

constexpr word kTextSize = 4;
constexpr word kDataSize = 12;

} // namespace

class LinkerScript : public ToolchainFixture
{
  protected:
    ObjectFile m_object;

    void SetUp() override
    {
        ToolchainFixture::SetUp();

        const std::string input = write("program.bi", kProgram);
        Assembler assembler(File(input), (m_dir / "out" / "program.bo").string());
        assembler.assemble();
        m_object = ObjectFile(assembler.get_output_file());
    }

    /// Links the program with the script and returns the executable.
    ObjectFile link(const std::string &script)
    {
        const std::string ld = write("script.ld", script);
        const std::string exe = (m_dir / "out" / "program.bexe").string();
        Linker linker({m_object}, File(exe, true), File(ld));
        linker.link();
        return ObjectFile(File(exe));
    }

    static const ObjectFile::SectionHeader &section(const ObjectFile &exe, const std::string &name)
    {
        return exe.sections.at(exe.section_table.at(name));
    }

    std::string error(const std::string &script)
    {
        return error_of([&] { link(script); });
    }

    /// Assembles a source into an object file.
    ObjectFile assemble(const std::string &name, const std::string &source)
    {
        const std::string input = write(name + ".bi", source);
        Assembler assembler(File(input), (m_dir / "out" / (name + ".bo")).string());
        assembler.assemble();
        return ObjectFile(assembler.get_output_file());
    }

    /// Links the objects with the default script and returns the executable.
    ObjectFile link_default(const std::vector<ObjectFile> &objects, const std::string &name)
    {
        const std::string exe = (m_dir / "out" / (name + ".bexe")).string();
        Linker linker(objects, File(exe, true));
        linker.link();
        return ObjectFile(File(exe));
    }
};

// A symbol that is declared but not defined anywhere has no address. It used to be linked at 0.
TEST_F(LinkerScript, a_global_symbol_that_is_never_defined_is_an_error)
{
    const ObjectFile object = assemble("undefined", ".global _start\n.global missing\n.text\n"
                                                    "_start: bl missing\nhlt\n");
    EXPECT_TRUE(contains(error_of([&] { link_default({object}, "undefined"); }),
                         "undefined reference to 'missing'"));
}

TEST_F(LinkerScript, an_undeclared_symbol_that_is_never_defined_is_an_error)
{
    const ObjectFile object = assemble("undeclared", ".global _start\n.text\n_start: bl nowhere\n");
    EXPECT_TRUE(contains(error_of([&] { link_default({object}, "undeclared"); }),
                         "undefined reference to 'nowhere'"));
}

TEST_F(LinkerScript, a_symbol_defined_in_another_object_is_found)
{
    const ObjectFile main =
        assemble("main", ".global _start\n.global func\n.text\n_start: bl func\n");
    const ObjectFile other = assemble("other", ".global func\n.text\nfunc: ret\n");
    const ObjectFile exe = link_default({main, other}, "found");
    EXPECT_TRUE(exe.rel_text.empty());
}

TEST_F(LinkerScript, the_entry_point_must_be_defined)
{
    const ObjectFile no_start = assemble("nostart", ".text\nfoo: nop\n");
    EXPECT_TRUE(contains(error_of([&] { link_default({no_start}, "nostart"); }),
                         "The entry point '_start' is not defined"));

    const ObjectFile declared = assemble("declared", ".global _start\n.text\nfoo: nop\n");
    EXPECT_TRUE(contains(error_of([&] { link_default({declared}, "declared"); }),
                         "The entry point '_start' is not defined"));
}

TEST_F(LinkerScript, overlapping_sections_are_an_error)
{
    EXPECT_TRUE(contains(error("SECTIONS(\n.text = 0;\n.data = 0;\n.bss = 0x10000;\n)\n"),
                         "The sections .text [0x0, 0x4) and .data [0x0, 0xc) overlap"));
    // The sections follow each other when only the first has an address, so they cannot overlap,
    // but one that starts inside another does.
    EXPECT_TRUE(contains(error("SECTIONS(\n.text = 0x100;\n.data = 0x102;\n)\n"), "overlap"));
    EXPECT_TRUE(contains(error("SECTIONS(\n.data = 0x100;\n.bss = 0x10b;\n)\n"), "overlap"));
}

TEST_F(LinkerScript, sections_that_touch_do_not_overlap)
{
    const ObjectFile exe = link("SECTIONS(\n.text = 0x100;\n.data = 0x104;\n.bss = 0x110;\n)\n");
    EXPECT_EQ(section(exe, ".data").address, 0x104u);
}

// Empty sections take no space, so they cannot overlap anything.
TEST_F(LinkerScript, an_empty_section_overlaps_nothing)
{
    const ObjectFile code_only = assemble("codeonly", ".global _start\n.text\n_start: nop\n");
    const std::string ld = write("empty.ld", "SECTIONS(\n.text = 0;\n.data = 0;\n.bss = 0;\n)\n");
    const std::string exe = (m_dir / "out" / "empty.bexe").string();
    Linker linker({code_only}, File(exe, true), File(ld));
    linker.link();
}

// 4 KiB of code used to run into the data, which started at 0x1000 whatever the size of the code.
TEST_F(LinkerScript, the_default_layout_moves_data_after_big_code)
{
    std::string source = ".global _start\n.data\nvalue: .word 7\n.text\n_start:\n";
    for (int i = 0; i < 1100; i++) source += "nop\n";
    const ObjectFile exe = link_default({assemble("big", source)}, "big");

    EXPECT_EQ(section(exe, ".text").address, 0u);
    EXPECT_EQ(section(exe, ".data").address, 0x2000u) << "1100 instructions are 4400 bytes";
    EXPECT_EQ(section(exe, ".bss").address, 0x2004u);
}

TEST_F(LinkerScript, the_default_layout_keeps_data_at_the_first_page_for_small_code)
{
    const ObjectFile exe = link_default({m_object}, "small");
    EXPECT_EQ(section(exe, ".data").address, 0x1000u);
}

// `.word label` is a word of .data that holds the address of the label.
TEST_F(LinkerScript, a_word_with_a_symbol_gets_the_address_of_the_symbol)
{
    const ObjectFile object = assemble("pointers", ".global _start\n"
                                                   ".data\n"
                                                   "pad: .word 1\n"
                                                   "table: .word func, 0, func, pad\n"
                                                   ".text\n"
                                                   "_start: nop\n"
                                                   "func: nop\n");
    const ObjectFile exe = link_default({object}, "pointers");

    EXPECT_TRUE(exe.rel_data.empty());
    // func is the second instruction, pad is the first word of .data (at 0x1000).
    EXPECT_EQ(exe.data_section, (std::vector<byte>{1, 0, 0, 0, 4, 0, 0,    0,    0, 0,
                                                   0, 0, 4, 0, 0, 0, 0x00, 0x10, 0, 0}));
}

TEST_F(LinkerScript, a_word_with_the_symbol_of_another_object)
{
    const ObjectFile main = assemble("main", ".global _start\n.global func\n"
                                             ".data\nptr: .word func\n"
                                             ".text\n_start: nop\n");
    const ObjectFile other = assemble("other", ".global func\n.text\nnop\nfunc: nop\n");
    const ObjectFile exe = link_default({main, other}, "across");

    // main has 1 instruction, and func is the second instruction of the other file.
    EXPECT_EQ(exe.data_section, (std::vector<byte>{8, 0, 0, 0}));
}

TEST_F(LinkerScript, a_word_with_a_symbol_that_is_never_defined_is_an_error)
{
    const ObjectFile object = assemble("dangling", ".global _start\n.data\nptr: .word nowhere\n"
                                                   ".text\n_start: nop\n");
    EXPECT_TRUE(contains(error_of([&] { link_default({object}, "dangling"); }),
                         "undefined reference to 'nowhere'"));
}

TEST_F(LinkerScript, a_word_with_a_symbol_goes_where_the_script_puts_the_data)
{
    const ObjectFile object = assemble("placed", ".global _start\n.data\nptr: .word _start\n"
                                                 ".text\n_start: nop\n");
    const std::string ld = write("placed.ld", "SECTIONS(\n.text = 0x400;\n.data = 0x2000;\n)\n");
    const std::string path = (m_dir / "out" / "placed.bexe").string();
    Linker linker({object}, File(path, true), File(ld));
    linker.link();
    EXPECT_EQ(ObjectFile(File(path)).data_section, (std::vector<byte>{0x00, 0x04, 0, 0}));
}

TEST_F(LinkerScript, the_data_of_an_object_file_keeps_its_alignment)
{
    const ObjectFile first = assemble("first", ".global _start\n.text\n_start: nop\n"
                                               ".data\nbytes: .byte 1, 2, 3\n");
    const ObjectFile second = assemble("second", ".global second\n.data\n.align 8\n"
                                                 "second: .word 7\n");
    const ObjectFile exe = link_default({first, second}, "aligned");

    EXPECT_EQ(exe.data_section, (std::vector<byte>{1, 2, 3, 0, 0, 0, 0, 0, 7, 0, 0, 0}));
    EXPECT_EQ(exe.symbol_table.at(exe.string_table.at("second")).symbol_value, 0x1008u);
    EXPECT_EQ(section(exe, ".data").alignment, 8u);
    EXPECT_EQ(section(exe, ".text").alignment, 4u);
}

TEST_F(LinkerScript, the_bss_of_an_object_file_keeps_its_alignment)
{
    const ObjectFile first = assemble("first", ".global _start\n.text\n_start: nop\n"
                                               ".bss\n.advance 5\n");
    const ObjectFile second = assemble("second", ".global second\n.bss\n.align 16\nsecond: "
                                                 ".advance 4\n");
    const ObjectFile exe = link_default({first, second}, "bssaligned");

    EXPECT_EQ(exe.symbol_table.at(exe.string_table.at("second")).symbol_value, 0x1000u + 16);
    EXPECT_EQ(exe.bss_section, 20u);
}

TEST_F(LinkerScript, the_text_of_an_object_file_keeps_its_alignment)
{
    const ObjectFile first = assemble("first", ".global _start\n.text\n_start: nop\n");
    const ObjectFile second = assemble("second", ".global second\n.text\n.align 16\nsecond: nop\n");
    const ObjectFile exe = link_default({first, second}, "textaligned");

    EXPECT_EQ(exe.symbol_table.at(exe.string_table.at("second")).symbol_value, 16u);
    EXPECT_EQ(exe.text_section.size(), 5u);
}

TEST_F(LinkerScript, an_address_from_the_script_has_to_keep_the_alignment)
{
    const ObjectFile aligned = assemble("wide", ".global _start\n.text\n_start: nop\n"
                                                ".data\n.align 8\nvalue: .word 1\n");
    const std::string ld = write("misaligned.ld", "SECTIONS(\n.text = 0;\n.data = 0x1004;\n)\n");
    const std::string path = (m_dir / "out" / "misaligned.bexe").string();
    EXPECT_TRUE(contains(error_of(
                             [&]
                             {
                                 Linker linker({aligned}, File(path, true), File(ld));
                                 linker.link();
                             }),
                         "The section .data is at 0x1004 but is aligned to 8 bytes"));
}

TEST_F(LinkerScript, a_section_that_follows_another_is_rounded_up_to_its_alignment)
{
    const ObjectFile aligned = assemble("wide2", ".global _start\n.text\n_start: nop\n"
                                                 ".data\n.align 8\nvalue: .word 1\n");
    const std::string ld = write("follow.ld", "SECTIONS(\n.text = 0x40;\n.data;\n)\n");
    const std::string path = (m_dir / "out" / "follow.bexe").string();
    Linker linker({aligned}, File(path, true), File(ld));
    linker.link();
    EXPECT_EQ(section(ObjectFile(File(path)), ".data").address, 0x48u) << "0x44 rounded up";
}

TEST_F(LinkerScript, code_and_data_sharing_a_page_is_allowed)
{
    // A warning at most, the layout is the user's.
    const ObjectFile exe = link("SECTIONS(\n.text = 0x40;\n.data;\n.bss;\n)\n");
    EXPECT_EQ(section(exe, ".data").address, 0x44u);
}

TEST_F(LinkerScript, the_constructor_does_not_link)
{
    const std::string exe_path = (m_dir / "out" / "lazy.bexe").string();
    Linker linker({m_object}, File(exe_path, true));
    EXPECT_EQ(fs::file_size(exe_path), 0u) << "File(path, true) creates the file, nothing is in it";

    linker.link();
    EXPECT_GT(fs::file_size(exe_path), 0u);
}

// The linker knows every final address, so it fills in all relocations itself and the executable
// has none left for the loader.
TEST_F(LinkerScript, every_relocation_is_resolved_at_link_time)
{
    const std::string input = write("reloc.bi", ".global _start\n"
                                                ".text\n"
                                                "_start:\n"
                                                "  adrp x0, buf\n"
                                                "  add x0, x0, :lo12:buf\n"
                                                "  mov x1, :hi13:buf\n"
                                                "  mov x1, :lo19:buf\n"
                                                "  bl func\n"
                                                "  hlt\n"
                                                ".data\n"
                                                "pad: .word 0\n"
                                                "buf: .word 1\n");
    Assembler assembler(File(input), (m_dir / "out" / "reloc.bo").string());
    assembler.assemble();
    const ObjectFile object(assembler.get_output_file());
    ASSERT_EQ(object.rel_text.size(), 5u) << "four absolute address uses and the call to func";

    // `func` is in another file, so only the linker can fill in the call.
    const std::string other_input = write("other.bi", ".global func\n.text\nfunc: ret\n");
    Assembler other_assembler(File(other_input), (m_dir / "out" / "other.bo").string());
    other_assembler.assemble();
    const ObjectFile other(other_assembler.get_output_file());

    const std::string ld =
        write("reloc.ld", "SECTIONS(\n.text = 0x400;\n.data = 0x2004;\n.bss;\n)\n");
    const std::string exe_path = (m_dir / "out" / "reloc.bexe").string();
    Linker linker({object, other}, File(exe_path, true), File(ld));
    linker.link();
    const ObjectFile exe{File(exe_path)};

    EXPECT_TRUE(exe.rel_text.empty());
    EXPECT_TRUE(exe.rel_data.empty());
    EXPECT_TRUE(exe.rel_bss.empty());

    constexpr word kBuf = 0x2004 + 4; // after `pad`
    constexpr word kFunc = 0x400 + 6 * 4;
    ASSERT_EQ(exe.text_section.size(), 7u);
    // adrp is relative to the page of the instruction: page 2 from page 0.
    EXPECT_EQ(exe.text_section[0], Emulator32bit::asm_format_m1(Emulator32bit::_op_adrp, 0, 2));
    EXPECT_EQ(exe.text_section[1],
              Emulator32bit::asm_format_o(Emulator32bit::_op_add, false, 0, 0, kBuf & 0xFFF));
    EXPECT_EQ(exe.text_section[2],
              Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, false, 1, kBuf >> 19));
    EXPECT_EQ(exe.text_section[3],
              Emulator32bit::asm_format_o3(Emulator32bit::_op_mov, false, 1, kBuf & 0x7FFFF));
    // The call is at 0x400 + 4 * 4 and func is two instructions later.
    EXPECT_EQ(exe.text_section[4],
              Emulator32bit::asm_format_b1(Emulator32bit::_op_bl, ConditionCode::AL,
                                           (kFunc - (0x400 + 4 * 4)) / 4));
}

TEST_F(LinkerScript, default_script)
{
    const std::string exe_path = (m_dir / "out" / "default.bexe").string();
    Linker linker({m_object}, File(exe_path, true));
    linker.link();
    const ObjectFile exe{File(exe_path)};

    EXPECT_EQ(section(exe, ".text").address, 0u);
    EXPECT_EQ(section(exe, ".data").address, 0x1000u);
    EXPECT_EQ(section(exe, ".bss").address, 0x1000u + kDataSize);
}

TEST_F(LinkerScript, sections_are_placed_at_their_addresses)
{
    const ObjectFile exe = link("ENTRY(_start)\n"
                                "SECTIONS (\n"
                                "    .text = 0x100;\n"
                                "    .data = 0x2000;\n"
                                "    .bss = 0x3000;\n"
                                ")\n");
    EXPECT_EQ(section(exe, ".text").address, 0x100u);
    EXPECT_EQ(section(exe, ".data").address, 0x2000u);
    EXPECT_EQ(section(exe, ".bss").address, 0x3000u);
}

TEST_F(LinkerScript, addresses_in_every_base)
{
    // Hexadecimal letters and multi digit decimals are easy to get wrong.
    const ObjectFile exe = link("SECTIONS(\n"
                                "    .text = 0xAbC0;\n"
                                "    .data = 8192;\n"
                                "    .bss = 0b1000000000000000;\n"
                                ")\n");
    EXPECT_EQ(section(exe, ".text").address, 0xABC0u);
    EXPECT_EQ(section(exe, ".data").address, 8192u);
    EXPECT_EQ(section(exe, ".bss").address, 0x8000u);
}

TEST_F(LinkerScript, a_section_without_an_address_follows_the_previous_one)
{
    const ObjectFile exe = link("SECTIONS(\n.text = 0x40;\n.data;\n.bss;\n)\n");
    EXPECT_EQ(section(exe, ".text").address, 0x40u);
    EXPECT_EQ(section(exe, ".data").address, 0x40u + kTextSize);
    EXPECT_EQ(section(exe, ".bss").address, 0x40u + kTextSize + kDataSize);
}

TEST_F(LinkerScript, sections_can_be_listed_in_any_order)
{
    const ObjectFile exe = link("SECTIONS(\n.data = 0x100;\n.text;\n.bss;\n)\n");
    EXPECT_EQ(section(exe, ".data").address, 0x100u);
    EXPECT_EQ(section(exe, ".text").address, 0x100u + kDataSize);
}

TEST_F(LinkerScript, comments_and_layout_do_not_matter)
{
    const ObjectFile exe = link("// the entry point\n"
                                "ENTRY(   _start   ) /* inline */\n"
                                "/* several\n"
                                "   lines */\n"
                                "SECTIONS(.text=0x10;.data=0x20;.bss=0x30;)");
    EXPECT_EQ(section(exe, ".text").address, 0x10u);
    EXPECT_EQ(section(exe, ".data").address, 0x20u);
    EXPECT_EQ(section(exe, ".bss").address, 0x30u);
}

TEST_F(LinkerScript, physical_and_virtual_tags)
{
    const ObjectFile exe = link("SECTIONS(\n@P;\n.text = 0;\n@V;\n.data = 0x1000;\n.bss;\n)\n");
    EXPECT_TRUE(section(exe, ".text").load_at_physical_address);
    EXPECT_FALSE(section(exe, ".data").load_at_physical_address);
    EXPECT_FALSE(section(exe, ".bss").load_at_physical_address);
}

TEST_F(LinkerScript, script_without_sections_places_nothing)
{
    // No SECTIONS, so no section is given an address.
    const ObjectFile exe = link("ENTRY(_start)\n");
    EXPECT_EQ(section(exe, ".text").address, 0u);
    EXPECT_EQ(section(exe, ".data").address, 0u);
}

TEST_F(LinkerScript, entry_errors)
{
    EXPECT_TRUE(contains(error("ENTRY _start\n"), "expected '(' after ENTRY, got '_start'"));
    EXPECT_TRUE(contains(error("ENTRY()\n"), "expected a symbol to follow ENTRY(, got ')'"));
    EXPECT_TRUE(
        contains(error("ENTRY(_start\n"), "expected ')' after the ENTRY symbol, got end of file"));
}

TEST_F(LinkerScript, sections_errors)
{
    EXPECT_TRUE(contains(error("SECTIONS\n"), "expected '(' after SECTIONS"));
    EXPECT_TRUE(contains(error("SECTIONS(\n.text = 0x0\n)\n"),
                         "expected ';' to end the section definition, got ')'"));
    EXPECT_TRUE(contains(error("SECTIONS(\nfoo;\n)\n"), "unexpected 'foo' in SECTIONS"));
    EXPECT_TRUE(contains(error("SECTIONS(\n@X;\n)\n"), "unknown tag 'X', expected P or V"));
    EXPECT_TRUE(contains(error("SECTIONS(\n@P\n.text;\n)\n"),
                         "expected ';' to end the statement, got '.text'"));
    EXPECT_TRUE(contains(error("SECTIONS(\n.text = foo;\n)\n"), "expected a number, got 'foo'"));
    EXPECT_TRUE(contains(error("SECTIONS(\n.text = 0x100000000;\n)\n"), "does not fit in 32 bits"));
    EXPECT_TRUE(contains(error("SECTIONS(\n.text;\n"), "unexpected end of file in SECTIONS"));
}

TEST_F(LinkerScript, unknown_commands_are_errors)
{
    EXPECT_TRUE(contains(error("FOO\n"), "unexpected 'FOO', expected ENTRY or SECTIONS"));
    EXPECT_TRUE(contains(error("SECTIONS()\n;\n"), "expected ENTRY or SECTIONS"));
}

TEST_F(LinkerScript, lexical_errors)
{
    EXPECT_TRUE(contains(error("SECTIONS(\n.text = 0xZZ;\n)\n"), "lexical error"));
    EXPECT_TRUE(contains(error("SECTIONS(\n.rodata;\n)\n"), "lexical error"));
    EXPECT_TRUE(contains(error("/* never closed\n"), "lexical error"));
}

TEST_F(LinkerScript, errors_name_the_file_line_and_column)
{
    EXPECT_TRUE(
        contains(error("SECTIONS(\n.text = foo;\n)\n"), "script.ld:2:9: error: expected a number"));
}

TEST_F(LinkerScript, missing_script_is_an_error)
{
    const std::string exe = (m_dir / "out" / "missing.bexe").string();
    EXPECT_TRUE(contains(error_of(
                             [&]
                             {
                                 Linker linker({m_object}, File(exe, true),
                                               File((m_dir / "nope.ld").string()));
                                 linker.link();
                             }),
                         "Cannot read the linker script"));
}
