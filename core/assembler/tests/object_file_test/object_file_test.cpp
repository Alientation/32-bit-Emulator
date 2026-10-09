// Unit tests for the object file: writing and reading it back, the files that it refuses to read,
// and the listing that basm -dump prints.

#include "assembler_test/toolchain_fixture.h"

#include <functional>
#include <sstream>

namespace
{

constexpr const char *kProgram = ".global _start\n"
                                 ".global shared\n"
                                 ".data\n"
                                 "numbers: .word 1, 2\n"
                                 "         .align 8\n"
                                 "shared:  .word loop\n"
                                 ".bss\n"
                                 "buffer:  .advance 12\n"
                                 ".text\n"
                                 "_start:\n"
                                 "  adrp x0, numbers\n"
                                 "  add x0, x0, :lo12:numbers\n"
                                 "loop:\n"
                                 "  add x1, x1, 1\n"
                                 "  b.ne loop\n"
                                 "  hlt\n";

} // namespace

class ObjectFileUnit : public ToolchainFixture
{
  protected:
    /// Assembles the source as `<name>.bo` and returns the object file as it is in memory, not as
    /// it is read back.
    ObjectFile assemble(const std::string &source, const std::string &name = "main")
    {
        const std::string input = write(name + ".bi", source);
        Assembler assembler(File(input), (m_dir / "out" / (name + ".bo")).string());
        assembler.assemble();
        return ObjectFile(assembler.get_output_file());
    }

    std::string path(const std::string &name) const
    {
        return (m_dir / "out" / name).string();
    }

    /// Writes the object file after `damage` changed it, and returns the error of reading it.
    std::string error_of_damaged(const std::function<void(ObjectFile &)> &damage)
    {
        ObjectFile object = assemble(kProgram);
        damage(object);
        object.write_object_file(File(path("damaged.bo"), true));
        return error_of([&] { ObjectFile read{File(path("damaged.bo"))}; });
    }

    std::vector<byte> bytes_of(const std::string &file)
    {
        const std::string text = read(file);
        return {text.begin(), text.end()};
    }
};

TEST_F(ObjectFileUnit, what_is_written_is_read_back)
{
    const ObjectFile written = assemble(kProgram);
    const ObjectFile read{File(path("main.bo"))};

    EXPECT_EQ(read.file_type, written.file_type);
    EXPECT_EQ(read.target_machine, written.target_machine);
    EXPECT_EQ(read.text_section, written.text_section);
    EXPECT_EQ(read.data_section, written.data_section);
    EXPECT_EQ(read.bss_section, written.bss_section);
    EXPECT_EQ(read.strings, written.strings);

    ASSERT_EQ(read.symbol_table.size(), written.symbol_table.size());
    for (const auto &[key, symbol] : written.symbol_table)
    {
        const auto &other = read.symbol_table.at(key);
        EXPECT_EQ(other.symbol_name, symbol.symbol_name);
        EXPECT_EQ(other.symbol_value, symbol.symbol_value);
        EXPECT_EQ(other.binding_info, symbol.binding_info);
        EXPECT_EQ(other.section, symbol.section);
    }

    ASSERT_EQ(read.rel_text.size(), written.rel_text.size());
    ASSERT_EQ(read.rel_data.size(), 1u);
    EXPECT_EQ(read.rel_data[0].offset, written.rel_data[0].offset);
    EXPECT_EQ(read.rel_data[0].type, ObjectFile::RelocationEntry::Type::R_EMU32_ABS32);

    ASSERT_EQ(read.sections.size(), written.sections.size());
    for (size_t i = 0; i < read.sections.size(); i++)
    {
        EXPECT_EQ(read.sections[i].alignment, written.sections[i].alignment) << i;
        EXPECT_EQ(read.sections[i].type, written.sections[i].type) << i;
    }
    EXPECT_EQ(read.sections[read.section_table.at(".data")].alignment, 8u);
}

TEST_F(ObjectFileUnit, the_same_source_gives_the_same_file)
{
    assemble(kProgram, "first");
    assemble(kProgram, "second");
    EXPECT_EQ(bytes_of("out/first.bo"), bytes_of("out/second.bo"));
}

TEST_F(ObjectFileUnit, symbols_are_in_the_order_they_were_added)
{
    const ObjectFile object = assemble(kProgram);

    U32 previous = 0;
    bool first = true;
    for (const auto &[key, symbol] : object.symbol_table)
    {
        EXPECT_EQ(key, symbol.symbol_name);
        if (!first)
        {
            EXPECT_GT(key, previous);
        }
        previous = key;
        first = false;
    }
}

TEST_F(ObjectFileUnit, a_file_that_is_not_an_object_file_is_refused)
{
    write("out/text.bo", std::string(100, 'x'));
    EXPECT_TRUE(
        contains(error_of([&] { ObjectFile read{File(path("text.bo"))}; }), "bad magic number"));

    write("out/tiny.bo", "BELF");
    EXPECT_TRUE(contains(error_of([&] { ObjectFile read{File(path("tiny.bo"))}; }), "too small"));
}

TEST_F(ObjectFileUnit, a_truncated_file_is_refused)
{
    assemble(kProgram);
    const std::string whole = read(path("main.bo"));
    write("out/cut.bo", whole.substr(0, whole.size() / 2));
    EXPECT_TRUE(contains(error_of([&] { ObjectFile read{File(path("cut.bo"))}; }), "cut.bo"));
}

TEST_F(ObjectFileUnit, a_file_without_the_text_section_is_refused)
{
    assemble(kProgram);
    std::string whole = read(path("main.bo"));
    const size_t at = whole.find(std::string(".text\0", 6));
    ASSERT_NE(at, std::string::npos);
    whole.replace(at, 5, ".txet");
    write("out/notext.bo", whole);

    EXPECT_TRUE(contains(error_of([&] { ObjectFile read{File(path("notext.bo"))}; }),
                         "it has no .text section"));
}

TEST_F(ObjectFileUnit, a_section_outside_of_the_file_is_refused)
{
    assemble(kProgram);
    std::string whole = read(path("main.bo"));

    // The last 8 bytes say where the section headers are. A header is name 8, type 4, start 8.
    U64 headers = 0;
    for (int b = 0; b < 8; b++)
    {
        headers |= U64(byte(whole[whole.size() - 8 + b])) << (8 * b);
    }
    const size_t start_of_first = headers + 8 + 4;
    for (int b = 0; b < 8; b++)
    {
        whole[start_of_first + b] = char(b == 3 ? 0x7F : 0);
    }
    write("out/outside.bo", whole);

    EXPECT_TRUE(contains(error_of([&] { ObjectFile read{File(path("outside.bo"))}; }),
                         "is corrupt, section 0"));
}

TEST_F(ObjectFileUnit, a_relocation_for_a_symbol_that_does_not_exist_is_refused)
{
    EXPECT_TRUE(contains(error_of_damaged([](ObjectFile &o) { o.rel_text[0].symbol = 9999; }),
                         "for the symbol 9999, which does not exist"));
    EXPECT_TRUE(contains(error_of_damaged([](ObjectFile &o) { o.rel_data[0].symbol = 9999; }),
                         "a relocation of .data"));
}

TEST_F(ObjectFileUnit, a_relocation_outside_of_its_section_is_refused)
{
    EXPECT_TRUE(contains(error_of_damaged([](ObjectFile &o) { o.rel_text[0].offset = 4000; }),
                         "a relocation of .text is at 4000, outside of the section"));
    EXPECT_TRUE(contains(error_of_damaged([](ObjectFile &o) { o.rel_text[0].offset = 2; }),
                         "not the start of an instruction"));
    EXPECT_TRUE(contains(error_of_damaged([](ObjectFile &o) { o.rel_data[0].offset = 13; }),
                         "a relocation of .data is at 13"));
}

TEST_F(ObjectFileUnit, a_relocation_of_an_unknown_type_is_refused)
{
    EXPECT_TRUE(contains(
        error_of_damaged([](ObjectFile &o)
                         { o.rel_text[0].type = ObjectFile::RelocationEntry::Type::UNDEFINED; }),
        "invalid type 0"));
    EXPECT_TRUE(
        contains(error_of_damaged([](ObjectFile &o)
                                  { o.rel_text[0].type = ObjectFile::RelocationEntry::Type(77); }),
                 "invalid type 77"));
}

TEST_F(ObjectFileUnit, a_symbol_in_a_section_that_does_not_exist_is_refused)
{
    EXPECT_TRUE(
        contains(error_of_damaged([](ObjectFile &o)
                                  { o.symbol_table.at(o.string_table.at("loop")).section = 99; }),
                 "the symbol 'loop' is in the invalid section 99"));
}

TEST_F(ObjectFileUnit, a_symbol_with_an_unknown_binding_is_refused)
{
    EXPECT_TRUE(contains(error_of_damaged(
                             [](ObjectFile &o)
                             {
                                 o.symbol_table.at(o.string_table.at("loop")).binding_info =
                                     ObjectFile::SymbolTableEntry::BindingInfo(9);
                             }),
                         "the symbol 'loop' has the invalid binding 9"));
}

namespace
{

constexpr const char *kUserSections = ".global _start\n"
                                      ".text\n"
                                      "_start: nop\n"
                                      ".section \"table\", \"r\"\n"
                                      "entries: .word _start, 5\n"
                                      ".section \"code\", \"rx\"\n"
                                      "entry: b _start\n"
                                      "adr x1, entries\n"
                                      ".section \"state\"\n"
                                      ".byte 1, 2, 3\n";

} // namespace

TEST_F(ObjectFileUnit, user_sections_are_written_and_read_back)
{
    const ObjectFile written = assemble(kUserSections);
    const ObjectFile read{File(path("main.bo"))};

    ASSERT_EQ(read.user_sections.size(), 3u);
    for (size_t i = 0; i < written.user_sections.size(); i++)
    {
        const auto &a = written.user_sections[i];
        const auto &b = read.user_sections[i];
        EXPECT_EQ(b.name, a.name);
        EXPECT_EQ(b.writable, a.writable) << a.name;
        EXPECT_EQ(b.executable, a.executable) << a.name;
        EXPECT_EQ(b.bytes, a.bytes) << a.name;
        EXPECT_EQ(b.header_index, a.header_index) << a.name;
        ASSERT_EQ(b.relocations.size(), a.relocations.size()) << a.name;
        for (size_t r = 0; r < a.relocations.size(); r++)
        {
            EXPECT_EQ(b.relocations[r].offset, a.relocations[r].offset);
            EXPECT_EQ(b.relocations[r].type, a.relocations[r].type);
            EXPECT_EQ(b.relocations[r].symbol, a.relocations[r].symbol);
        }
    }
    EXPECT_EQ(read.find_user_section("code")->relocations.size(), 2u);
    EXPECT_EQ(read.sections[read.find_user_section("table")->header_index].type,
              ObjectFile::SectionHeader::Type::USER_R);
    EXPECT_EQ(read.sections[read.find_user_section("code")->header_index].type,
              ObjectFile::SectionHeader::Type::USER_RX);
    EXPECT_EQ(read.sections[read.find_user_section("state")->header_index].type,
              ObjectFile::SectionHeader::Type::USER_RW);
}

TEST_F(ObjectFileUnit, a_file_with_user_sections_is_the_same_bytes_every_time)
{
    assemble(kUserSections);
    const std::vector<byte> first = bytes_of("out/main.bo");
    assemble(kUserSections);
    EXPECT_EQ(bytes_of("out/main.bo"), first);
}

TEST_F(ObjectFileUnit, a_user_section_that_is_corrupt_is_refused)
{
    const auto damaged = [&](const std::function<void(ObjectFile &)> &damage)
    {
        ObjectFile object = assemble(kUserSections);
        damage(object);
        object.write_object_file(File(path("damaged.bo"), true));
        return error_of([&] { ObjectFile read{File(path("damaged.bo"))}; });
    };

    EXPECT_TRUE(contains(damaged([](ObjectFile &o) { o.find_user_section("table")->relocations[0].symbol = 999; }),
                         "a relocation of table is for the symbol 999"));
    EXPECT_TRUE(contains(damaged([](ObjectFile &o) { o.find_user_section("table")->relocations[0].offset = 6; }),
                         "a relocation of table is at 6, outside of the section"));
    EXPECT_TRUE(contains(damaged([](ObjectFile &o) { o.find_user_section("code")->relocations[0].offset = 2; }),
                         "a relocation of code is at 2, which is not the start of an instruction"));
    EXPECT_TRUE(contains(damaged([](ObjectFile &o) { o.find_user_section("code")->bytes.push_back(1); }),
                         "the code of code is not made of whole instructions"));
    EXPECT_TRUE(contains(damaged(
                             [](ObjectFile &o)
                             {
                                 // The relocations belong to the section before them, and
                                 // this one follows a section that is not of the program's.
                                 o.sections[o.find_user_section("table")->header_index].type =
                                     ObjectFile::SectionHeader::Type::REL_USER;
                             }),
                         "does not follow the section it is for"));
}

TEST_F(ObjectFileUnit, the_listing_has_the_user_sections)
{
    assemble(kUserSections);
    std::ostringstream listing;
    ObjectFile(File(path("main.bo"))).print(listing);
    const std::string text = listing.str();

    EXPECT_TRUE(contains(text, "Contents of section table:"));
    EXPECT_TRUE(contains(text, "Relocations of section table:"));
    EXPECT_TRUE(contains(text, "Contents of section code:"));
    EXPECT_TRUE(contains(text, "R_EMU32_ADR_PCREL21"));
    EXPECT_TRUE(contains(text, "Contents of section state:"));
    EXPECT_TRUE(contains(text, "010203"));
}

TEST_F(ObjectFileUnit, the_listing_has_the_symbols_the_data_and_the_code)
{
    const ObjectFile object = assemble(kProgram);
    std::ostringstream listing;
    ObjectFile(File(path("main.bo"))).print(listing);
    const std::string text = listing.str();

    EXPECT_TRUE(contains(text, "main.bo:\tfile format belf32-littleemu32"));
    EXPECT_TRUE(contains(text, "SYMBOL TABLE:"));
    EXPECT_TRUE(contains(text, "numbers"));
    EXPECT_TRUE(contains(text, "g\t .data")) << "shared is a global symbol of .data";
    EXPECT_TRUE(contains(text, "l\t .bss")) << "buffer is a local symbol of .bss";
    EXPECT_TRUE(contains(text, "Contents of section .data:"));
    EXPECT_TRUE(contains(text, "01000000 02000000")) << "the words of numbers";
    EXPECT_TRUE(contains(text, "Relocations of section .data:"));
    EXPECT_TRUE(contains(text, "R_EMU32_ABS32"));
    EXPECT_TRUE(contains(text, "Disassembly of section .text:"));
    EXPECT_TRUE(contains(text, "R_EMU32_ADRP_HI20"));
    EXPECT_TRUE(contains(text, "<_start>:"));
    EXPECT_TRUE(contains(text, "b.ne"));
    EXPECT_TRUE(contains(text, "<loop>\n")) << "the branch goes to a label";
}

TEST_F(ObjectFileUnit, the_listing_of_a_file_in_memory_is_the_same)
{
    ObjectFile written = assemble(kProgram);
    std::ostringstream from_memory;
    written.print(from_memory);

    std::ostringstream from_file;
    ObjectFile(File(path("main.bo"))).print(from_file);
    EXPECT_EQ(from_memory.str(), from_file.str());
}

TEST_F(ObjectFileUnit, the_listing_of_an_object_that_was_not_read_is_an_error_line)
{
    ObjectFile nothing;
    std::ostringstream listing;
    nothing.print(listing);
    EXPECT_EQ(listing.str(), "ERROR: Cannot print object file. It has no sections.\n");
}

// The addresses are not cut to 16 bits.
TEST_F(ObjectFileUnit, the_listing_has_addresses_of_big_programs)
{
    std::string source = ".global _start\n.data\nvalue: .word 1\n.text\n_start:\n";
    for (int i = 0; i < 17000; i++) source += "nop\n";
    source += "adrp x0, value\nhlt\n";
    assemble(source, "big");

    std::ostringstream listing;
    ObjectFile(File(path("big.bo"))).print(listing);
    // 17000 instructions are 68000 bytes: the adrp is at 0x109a0.
    EXPECT_TRUE(contains(listing.str(), "109a0")) << "the address of the adrp";
    EXPECT_TRUE(contains(listing.str(), "109a0: R_EMU32_ADRP_HI20"));
}
