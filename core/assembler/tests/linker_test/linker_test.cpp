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
    EXPECT_TRUE(contains(error("SECTIONS(\n.comment;\n)\n"), "lexical error"));
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

// ---------------------------------------------------------------------------------------------
// Sections of the program's own (.section "name")
// ---------------------------------------------------------------------------------------------

namespace
{

word address_of(const ObjectFile &exe, const std::string &user_section)
{
    return exe.sections.at(exe.find_user_section(user_section)->header_index).address;
}

word symbol_value(const ObjectFile &exe, const std::string &name)
{
    return exe.symbol_table.at(exe.string_table.at(name)).symbol_value;
}

/// The word at `offset` of the bytes of a section.
word word_at(const std::vector<byte> &bytes, size_t offset)
{
    return word(bytes[offset]) | (word(bytes[offset + 1]) << 8) | (word(bytes[offset + 2]) << 16)
           | (word(bytes[offset + 3]) << 24);
}

constexpr const char *kUserProgram = ".global _start\n.global vec\n.global tbl\n.global st\n"
                                     ".text\n"
                                     "_start: nop\n" // 0
                                     ".section \"vectors\", \"rx\"\n"
                                     "vec: nop\nb _start\n" // 8 bytes
                                     ".section \"table\", \"r\"\n"
                                     "tbl: .word _start, vec\n" // 8 bytes
                                     ".section \"state\"\n"
                                     "st: .word 7\n" // 4 bytes
                                     ".data\n"
                                     "values: .word 1, 2, 3\n"
                                     ".rodata\n"
                                     "text: .asciz \"hi\"\n";

} // namespace

TEST_F(LinkerScript, the_default_layout_puts_code_read_only_data_and_data_in_their_own_pages)
{
    const ObjectFile exe = link_default({assemble("user", kUserProgram)}, "user");

    EXPECT_EQ(section(exe, ".text").address, 0u);
    EXPECT_EQ(address_of(exe, "vectors"), 4u) << "after .text, still code";
    EXPECT_EQ(section(exe, ".rodata").address, 0x1000u) << "the next page";
    EXPECT_EQ(address_of(exe, "table"), 0x1004u) << "after .rodata (3 bytes) and the arrays (words)";
    EXPECT_EQ(section(exe, ".data").address, 0x2000u) << "the next page";
    EXPECT_EQ(address_of(exe, "state"), 0x2000u + 12) << "after .data";
    EXPECT_EQ(section(exe, ".bss").address, 0x2000u + 12 + 4);
}

TEST_F(LinkerScript, symbols_of_a_user_section_have_its_address_and_section)
{
    const ObjectFile exe = link_default({assemble("user", kUserProgram)}, "user");

    EXPECT_EQ(symbol_value(exe, "vec"), 4u);
    EXPECT_EQ(symbol_value(exe, "tbl"), 0x1004u);
    EXPECT_EQ(symbol_value(exe, "st"), 0x200Cu);
    EXPECT_EQ(exe.symbol_table.at(exe.string_table.at("vec")).section,
              exe.find_user_section("vectors")->header_index);
    EXPECT_EQ(exe.symbol_table.at(exe.string_table.at("st")).section,
              exe.find_user_section("state")->header_index);
}

TEST_F(LinkerScript, the_words_and_instructions_of_a_user_section_are_relocated)
{
    const ObjectFile exe = link_default({assemble("user", kUserProgram)}, "user");

    const ObjectFile::UserSection &table = *exe.find_user_section("table");
    EXPECT_EQ(word_at(table.bytes, 0), 0u) << "_start";
    EXPECT_EQ(word_at(table.bytes, 4), 4u) << "vec";

    // The branch is at 8 and goes to 0.
    const ObjectFile::UserSection &vectors = *exe.find_user_section("vectors");
    EXPECT_EQ(word_at(vectors.bytes, 4),
              Emulator32bit::asm_format_b1(Emulator32bit::_op_b, ConditionCode::AL, -2));
    EXPECT_TRUE(exe.find_user_section("vectors")->relocations.empty());
    EXPECT_TRUE(table.relocations.empty());
}

TEST_F(LinkerScript, a_user_section_of_several_files_is_joined_and_keeps_its_alignment)
{
    const ObjectFile first = assemble("first", ".global _start\n.global one\n.text\n_start: nop\n"
                                               ".section \"extra\"\n.byte 1, 2, 3\n"
                                               "one: .byte 4\n");
    const ObjectFile second = assemble("second", ".global two\n"
                                                 ".section \"extra\"\n.align 8\n.byte 5\n"
                                                 "two: .word one\n");
    const ObjectFile exe = link_default({first, second}, "joined");

    const ObjectFile::UserSection &extra = *exe.find_user_section("extra");
    // The first file's 4 bytes, then the second's, which is aligned to 8.
    ASSERT_EQ(extra.bytes.size(), 8u + 1 + 4);
    EXPECT_EQ(extra.bytes[0], 1);
    EXPECT_EQ(extra.bytes[3], 4);
    EXPECT_EQ(extra.bytes[8], 5);
    EXPECT_EQ(exe.sections[extra.header_index].alignment, 8u);
    EXPECT_EQ(address_of(exe, "extra") % 8, 0u);
    EXPECT_EQ(symbol_value(exe, "two"), address_of(exe, "extra") + 9);
    EXPECT_EQ(word_at(extra.bytes, 9), address_of(exe, "extra") + 3)
        << "the address of a label of the first file";
}

TEST_F(LinkerScript, a_symbol_keeps_its_section_when_the_files_have_different_sections)
{
    // The headers of "b" are at other places in each file.
    const ObjectFile first = assemble("first", ".global _start\n.global in_first\n.text\n_start: nop\n"
                                               ".section \"a\"\n.byte 1\n"
                                               ".section \"b\"\nin_first: .byte 2\n");
    const ObjectFile second = assemble("second", ".global in_second\n.section \"b\"\nin_second: .byte 3\n");
    ASSERT_NE(first.find_user_section("b")->header_index, second.find_user_section("b")->header_index);

    const ObjectFile exe = link_default({first, second}, "differ");
    const U32 b = exe.find_user_section("b")->header_index;
    EXPECT_EQ(exe.symbol_table.at(exe.string_table.at("in_first")).section, b);
    EXPECT_EQ(exe.symbol_table.at(exe.string_table.at("in_second")).section, b);
    EXPECT_EQ(symbol_value(exe, "in_second"), symbol_value(exe, "in_first") + 1);
}

TEST_F(LinkerScript, the_script_places_a_user_section_by_name)
{
    const std::vector<ObjectFile> objects = {assemble("user", kUserProgram)};
    const std::string ld = write("user.ld", "SECTIONS (\n"
                                            "    .text = 0x100;\n"
                                            "    \"vectors\" = 0x800;\n"
                                            "    \"table\";\n"
                                            "    \"state\" = 0x3000;\n"
                                            "    \"never_defined\" = 0x9000;\n"
                                            "    .data = 0x2000;\n"
                                            "    .rodata = 0x4000;\n"
                                            "    .bss;\n"
                                            ")\n");
    const std::string path = (m_dir / "out" / "placed.bexe").string();
    Linker linker(objects, File(path, true), File(ld));
    linker.link();
    const ObjectFile exe{File(path)};

    EXPECT_EQ(address_of(exe, "vectors"), 0x800u);
    EXPECT_EQ(address_of(exe, "table"), 0x808u) << "follows the previous section";
    EXPECT_EQ(address_of(exe, "state"), 0x3000u);
    EXPECT_EQ(symbol_value(exe, "vec"), 0x800u);
}

TEST_F(LinkerScript, a_user_section_with_contents_has_to_be_placed_by_the_script)
{
    const std::vector<ObjectFile> objects = {assemble("user", kUserProgram)};
    const std::string ld = write("partial.ld", "SECTIONS (\n.text = 0;\n\"vectors\";\n"
                                               "\"table\";\n.data = 0x2000;\n.rodata = 0x1000;\n"
                                               ".bss;\n)\n");
    const std::string path = (m_dir / "out" / "partial.bexe").string();
    EXPECT_TRUE(contains(error_of(
                             [&]
                             {
                                 Linker linker(objects, File(path, true), File(ld));
                                 linker.link();
                             }),
                         "The section state has contents but the linker script does not place it"));
}

TEST_F(LinkerScript, user_sections_that_overlap_are_an_error)
{
    const std::vector<ObjectFile> objects = {assemble("user", kUserProgram)};
    const std::string ld = write("overlap.ld", "SECTIONS (\n.text = 0;\n\"vectors\" = 0;\n"
                                               "\"table\" = 0x1000;\n\"state\" = 0x2000;\n"
                                               ".data = 0x2000;\n.rodata = 0x3000;\n.bss;\n)\n");
    const std::string path = (m_dir / "out" / "overlap.bexe").string();
    EXPECT_TRUE(contains(error_of(
                             [&]
                             {
                                 Linker linker(objects, File(path, true), File(ld));
                                 linker.link();
                             }),
                         "overlap"));
}

TEST_F(LinkerScript, the_same_section_with_other_flags_in_another_file_is_an_error)
{
    const ObjectFile first = assemble("first", ".global _start\n.text\n_start: nop\n"
                                               ".section \"x\", \"r\"\n.byte 1\n");
    const ObjectFile second = assemble("second", ".section \"x\", \"rw\"\n.byte 2\n");
    EXPECT_TRUE(contains(error_of([&] { link_default({first, second}, "flags"); }),
                         "The section x does not have the same flags in all the files"));
}

// A nobits section is joined by size, keeps its alignment, and has an address like the others.
TEST_F(LinkerScript, nobits_sections_of_several_files_are_added_up_and_aligned)
{
    const ObjectFile first = assemble("first", ".global _start\n.global one\n.text\n_start: nop\n"
                                               ".section \"heap\", \"rw\", \"nobits\"\n"
                                               ".advance 5\none: .advance 3\n");
    const ObjectFile second = assemble("second", ".global two\n"
                                                 ".section \"heap\", \"rw\", \"nobits\"\n"
                                                 ".align 16\ntwo: .advance 4\n");
    const ObjectFile exe = link_default({first, second}, "nobits");

    const ObjectFile::UserSection &heap = *exe.find_user_section("heap");
    EXPECT_TRUE(heap.nobits);
    EXPECT_TRUE(heap.bytes.empty());
    EXPECT_EQ(heap.zero_size, 16u + 4) << "8 bytes of the first, padded to 16, then 4";
    EXPECT_EQ(exe.sections[heap.header_index].alignment, 16u);
    EXPECT_EQ(address_of(exe, "heap") % 16, 0u);
    EXPECT_EQ(symbol_value(exe, "one"), address_of(exe, "heap") + 5);
    EXPECT_EQ(symbol_value(exe, "two"), address_of(exe, "heap") + 16);
    EXPECT_EQ(exe.symbol_table.at(exe.string_table.at("two")).section, heap.header_index);
}

TEST_F(LinkerScript, the_default_layout_puts_a_nobits_section_with_the_data_before_bss)
{
    const ObjectFile exe = link_default(
        {assemble("n", ".global _start\n.text\n_start: nop\n"
                       ".data\n.word 1\n"
                       ".section \"zeros\", \"rw\", \"nobits\"\n.advance 100\n"
                       ".bss\n.advance 4\n")},
        "nobitslayout");
    EXPECT_EQ(section(exe, ".data").address, 0x1000u);
    EXPECT_EQ(address_of(exe, "zeros"), 0x1004u) << "after .data";
    EXPECT_EQ(section(exe, ".bss").address, 0x1004u + 100u) << "and before .bss";
}

TEST_F(LinkerScript, the_script_places_a_nobits_section_by_name)
{
    const std::vector<ObjectFile> objects = {
        assemble("p", ".global _start\n.global top\n.text\n_start: nop\n"
                      ".section \"stack\", \"rw\", \"nobits\"\n.align 8\ntop: .advance $1000\n")};
    const std::string ld = write("stack.ld", "SECTIONS (\n.text = 0;\n\"stack\" = 0x8000;\n.data;\n.bss;\n)\n");
    const std::string path = (m_dir / "out" / "stack.bexe").string();
    Linker linker(objects, File(path, true), File(ld));
    linker.link();
    const ObjectFile exe{File(path)};

    EXPECT_EQ(address_of(exe, "stack"), 0x8000u);
    EXPECT_EQ(exe.find_user_section("stack")->zero_size, 0x1000u);
    EXPECT_EQ(symbol_value(exe, "top"), 0x8000u);

    // And one that is not in the script is the same error as for a section with bytes.
    const std::string partial = write("partial_stack.ld", "SECTIONS (\n.text = 0;\n.data;\n.bss;\n)\n");
    EXPECT_TRUE(contains(error_of(
                             [&]
                             {
                                 Linker again(objects, File(path, true), File(partial));
                                 again.link();
                             }),
                         "The section stack has contents but the linker script does not place it"));
}

TEST_F(LinkerScript, a_section_that_is_nobits_in_one_file_and_not_in_another_is_an_error)
{
    const ObjectFile first = assemble("first", ".global _start\n.text\n_start: nop\n"
                                               ".section \"x\", \"rw\", \"nobits\"\n.advance 4\n");
    const ObjectFile second = assemble("second", ".section \"x\", \"rw\"\n.byte 2\n");
    EXPECT_TRUE(contains(error_of([&] { link_default({first, second}, "kinds"); }),
                         "The section x does not have the same flags in all the files"));
}

TEST_F(LinkerScript, an_undefined_symbol_in_a_user_section_is_an_error)
{
    const ObjectFile object = assemble("undef", ".global _start\n.text\n_start: nop\n"
                                                ".section \"t\"\n.word missing\n");
    EXPECT_TRUE(contains(error_of([&] { link_default({object}, "undef"); }),
                         "undefined reference to 'missing'"));
}

TEST_F(LinkerScript, a_data_section_cannot_hold_a_relocation_that_patches_an_instruction)
{
    // The linker only patches words with an address in a section that is not executable. A
    // branch cannot get there from the assembler (it needs an instruction), so make one.
    ObjectFile object = assemble("branch", ".global _start\n.text\n_start: nop\n"
                                           ".section \"d\", \"rx\"\nb _start\n");
    ObjectFile::UserSection &d = *object.find_user_section("d");
    d.executable = false;
    d.writable = true;
    object.sections[d.header_index].type = ObjectFile::SectionHeader::Type::USER_RW;
    EXPECT_TRUE(contains(error_of([&] { link_default({object}, "bad"); }),
                         "A relocation in d of type"));
}
