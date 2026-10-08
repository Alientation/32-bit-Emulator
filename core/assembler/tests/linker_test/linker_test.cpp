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
        Assembler assembler(m_process.get(), File(input), (m_dir / "out" / "program.bo").string());
        assembler.assemble();
        m_object = ObjectFile(assembler.get_output_file());
    }

    /// Links the program with the script and returns the executable.
    ObjectFile link(const std::string &script)
    {
        const std::string ld = write("script.ld", script);
        const std::string exe = (m_dir / "out" / "program.bexe").string();
        Linker linker({m_object}, File(exe, true), File(ld));
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
};

TEST_F(LinkerScript, default_script)
{
    const std::string exe_path = (m_dir / "out" / "default.bexe").string();
    Linker linker({m_object}, File(exe_path, true));
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
                             [&] {
                                 Linker linker({m_object}, File(exe, true),
                                               File((m_dir / "nope.ld").string()));
                             }),
                         "Cannot read the linker script"));
}
