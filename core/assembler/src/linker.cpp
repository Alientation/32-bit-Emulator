#include "assembler/linker.h"
#include "assembler/relocation.h"
#include "emulator32bit/emulator32bit_util.h"
#include "util/logger.h"

#include <algorithm>
#include <span>

namespace
{

/// The layout used when no linker script is given. It is part of the program, so linking does not
/// depend on where the source tree is. .data starts on the first page after .text, so that code
/// and data never share a page however big the code is.
std::string default_linker_script(word text_size)
{
    const word data_address = (text_size + kPageSize - 1) & ~(kPageSize - 1);
    return "ENTRY(_start)\n\nSECTIONS (\n    .text = 0x0;\n    .data = "
           + std::to_string(data_address) + ";\n    .bss;\n)\n";
}

bool is_undefined(const ObjectFile::SymbolTableEntry &symbol)
{
    return symbol.section == U32(-1);
}

} // namespace

Linker::Linker(std::vector<ObjectFile> obj_files, File exe_file) :
    m_obj_files(std::move(obj_files)),
    m_exe_file(exe_file),
    m_use_default_script(true)
{
}

Linker::Linker(std::vector<ObjectFile> obj_files, File exe_file, File ld_file) :
    m_obj_files(std::move(obj_files)),
    m_exe_file(exe_file),
    m_ld_file(ld_file)
{
}

void Linker::fail(const basm::Token &at, const std::string &message)
{
    basm::fatal_at(m_sources, at.loc, message);
}

const basm::Token &Linker::expect(basm::TokenType type, const std::string &expected)
{
    if (!m_cursor.check(type))
    {
        fail(m_cursor.peek(), "expected " + expected + ", got " + basm::describe(m_cursor.peek()));
    }
    return m_cursor.next();
}

void Linker::_entry()
{
    m_cursor.next(); // ENTRY
    expect(basm::TokenType::OPEN_PARENTHESIS, "'(' after ENTRY");
    m_entry_symbol = expect(basm::TokenType::SYMBOL, "a symbol to follow ENTRY(").str();
    expect(basm::TokenType::CLOSE_PARENTHESIS, "')' after the ENTRY symbol");
}

void Linker::_sections()
{
    m_cursor.next(); // SECTIONS
    expect(basm::TokenType::OPEN_PARENTHESIS, "'(' after SECTIONS");

    while (!m_cursor.check(basm::TokenType::CLOSE_PARENTHESIS))
    {
        // `@P;` and `@V;` set whether the sections that follow are placed at physical or at
        // virtual addresses.
        if (m_cursor.accept(basm::TokenType::AT))
        {
            const basm::Token &tag = expect(basm::TokenType::SYMBOL, "a tag (P or V) after '@'");
            if (tag.text == "P")
            {
                m_physical = true;
            }
            else if (tag.text == "V")
            {
                m_physical = false;
            }
            else
            {
                fail(tag, "unknown tag '" + tag.str() + "', expected P or V");
            }

            expect(basm::TokenType::SEMICOLON, "';' to end the statement");
            continue;
        }

        const basm::Token &section = m_cursor.peek();
        switch (section.type)
        {
        case basm::TokenType::ASSEMBLER_TEXT:
            m_sections.push_back({.type = SectionAddress::Type::TEXT, .physical = m_physical});
            break;
        case basm::TokenType::ASSEMBLER_DATA:
            m_sections.push_back({.type = SectionAddress::Type::DATA, .physical = m_physical});
            break;
        case basm::TokenType::ASSEMBLER_BSS:
            m_sections.push_back({.type = SectionAddress::Type::BSS, .physical = m_physical});
            break;
        default:
            fail(section, "unexpected " + basm::describe(section) + " in SECTIONS");
        }
        m_cursor.next();

        if (m_cursor.accept(basm::TokenType::EQUAL))
        {
            m_sections.back().set_address = true;
            m_sections.back().address = parse_value();
        }

        expect(basm::TokenType::SEMICOLON, "';' to end the section definition");
    }
    m_cursor.next(); // ')'
}

void Linker::parse_ld()
{
    while (!m_cursor.at_end())
    {
        const basm::Token &command = m_cursor.peek();
        switch (command.type)
        {
        case basm::TokenType::KEYWORD_ENTRY:
            _entry();
            break;
        case basm::TokenType::KEYWORD_SECTIONS:
            _sections();
            break;
        default:
            fail(command, "unexpected " + basm::describe(command) + ", expected ENTRY or SECTIONS");
        }
    }
}

void Linker::link()
{
    tokenize_ld();
    parse_ld();

    ObjectFile exe = new_executable();
    const std::vector<SectionBase> bases = merge_sections(exe);
    const SectionBase addresses = place_sections(exe);
    const std::vector<SymbolMap> symbols = merge_symbols(exe, bases, addresses);
    define_entry(exe);
    relocate(exe, bases, addresses, symbols);

    /* There are no relocations in .data and .bss, and none are left in the executable. */
    exe.write_object_file(m_exe_file);
}

ObjectFile Linker::new_executable()
{
    ObjectFile exe;

    exe.file_type = ObjectFile::kExecutableFileType;
    exe.target_machine = ObjectFile::kEMU32MachineId;
    exe.flags = 0;

    exe.add_section(".text", ObjectFile::SectionHeader::Type::TEXT);
    exe.add_section(".data", ObjectFile::SectionHeader::Type::DATA);
    exe.add_section(".bss", ObjectFile::SectionHeader::Type::BSS);
    exe.add_section(".symtab", ObjectFile::SectionHeader::Type::SYMTAB);
    exe.add_section(".rel.text", ObjectFile::SectionHeader::Type::REL_TEXT);
    exe.add_section(".rel.data", ObjectFile::SectionHeader::Type::REL_DATA);
    exe.add_section(".rel.bss", ObjectFile::SectionHeader::Type::REL_BSS);
    exe.add_section(".strtab", ObjectFile::SectionHeader::Type::STRTAB);
    return exe;
}

/// The sections of the object files are put one after another, in the order of the object files.
/// A section of an object file starts at a multiple of its alignment (what `.align` asked for), so
/// that the offsets that it was assembled with stay aligned. The merged section has the largest
/// alignment.
/// Returns where each object file's sections start within the merged ones.
std::vector<Linker::SectionBase> Linker::merge_sections(ObjectFile &exe) const
{
    const auto alignment_of = [](const ObjectFile &obj, const char *section)
    { return std::max<word>(1, obj.sections[obj.section_table.at(section)].alignment); };
    const auto pad_to = [](word size, word alignment)
    { return (alignment - size % alignment) % alignment; };

    SectionBase alignment = {.text = 4, .data = 1, .bss = 1};
    std::vector<SectionBase> bases;
    for (const ObjectFile &obj : m_obj_files)
    {
        const SectionBase wanted = {.text = alignment_of(obj, ".text"),
                                    .data = alignment_of(obj, ".data"),
                                    .bss = alignment_of(obj, ".bss")};
        exe.text_section.insert(exe.text_section.end(),
                                pad_to(word(exe.text_section.size() * 4), wanted.text) / 4, 0);
        exe.data_section.insert(exe.data_section.end(),
                                pad_to(word(exe.data_section.size()), wanted.data), 0);
        exe.bss_section += pad_to(exe.bss_section, wanted.bss);
        alignment = {.text = std::max(alignment.text, wanted.text),
                     .data = std::max(alignment.data, wanted.data),
                     .bss = std::max(alignment.bss, wanted.bss)};

        bases.push_back({.text = word(exe.text_section.size() * 4),
                         .data = word(exe.data_section.size()),
                         .bss = exe.bss_section});

        exe.text_section.insert(exe.text_section.end(), obj.text_section.begin(),
                                obj.text_section.end());
        exe.data_section.insert(exe.data_section.end(), obj.data_section.begin(),
                                obj.data_section.end());
        exe.bss_section += obj.bss_section;
    }

    exe.sections[exe.section_table.at(".text")].alignment = alignment.text;
    exe.sections[exe.section_table.at(".data")].alignment = alignment.data;
    exe.sections[exe.section_table.at(".bss")].alignment = alignment.bss;
    return bases;
}

/// Gives the sections their addresses according to the linker script. A section that the script
/// does not give an address follows the previous one. Returns the address of each section.
Linker::SectionBase Linker::place_sections(ObjectFile &exe) const
{
    struct Placed
    {
        const char *name;
        word address;
        word size;
        bool physical;
        bool executable;
    };

    std::vector<Placed> placed;

    SectionBase addresses;
    word address = 0;
    for (const SectionAddress &section : m_sections)
    {
        const char *name;
        word size;
        word *section_address;
        switch (section.type)
        {
        case SectionAddress::Type::TEXT:
            name = ".text";
            size = word(exe.text_section.size() * 4);
            section_address = &addresses.text;
            break;
        case SectionAddress::Type::DATA:
            name = ".data";
            size = word(exe.data_section.size()); // bytes, unlike .text
            section_address = &addresses.data;
            break;
        case SectionAddress::Type::BSS:
            name = ".bss";
            size = exe.bss_section;
            section_address = &addresses.bss;
            break;
        }

        ObjectFile::SectionHeader &header = exe.sections[exe.section_table.at(name)];
        const word alignment = std::max<word>(1, header.alignment);

        // A section the script gives no address starts after the previous one, at an address that
        // keeps what was aligned in the object files aligned. An address from the script has to.
        if (section.set_address)
        {
            AEMU_CHECK(size == 0 || section.address % alignment == 0,
                       "Linker::link() - The section {} is at {:#x} but is aligned to {} bytes.",
                       name, section.address, alignment);
            *section_address = section.address;
        }
        else
        {
            *section_address = address + (alignment - address % alignment) % alignment;
        }

        header.load_at_physical_address = section.physical;
        header.address = *section_address;
        address = header.address + size;

        placed.push_back({name, header.address, size, section.physical,
                          section.type == SectionAddress::Type::TEXT});
    }

    for (size_t i = 0; i < placed.size(); i++)
    {
        if (placed[i].size == 0)
        {
            continue;
        }
        for (size_t j = 0; j < i; j++)
        {
            const Placed &a = placed[j];
            const Placed &b = placed[i];
            if (a.size == 0 || a.physical != b.physical)
            {
                continue;
            }

            // Sizes are added in 64 bits, a section can end at the top of the address space.
            const U64 a_end = U64(a.address) + a.size;
            const U64 b_end = U64(b.address) + b.size;
            AEMU_CHECK(a.address >= b_end || b.address >= a_end,
                       "Linker::link() - The sections {} [{:#x}, {:#x}) and {} [{:#x}, {:#x}) "
                       "overlap.",
                       a.name, a.address, a_end, b.name, b.address, b_end);

            // Code and data do not overlap but share a page. The loader gives such a page both
            // permissions, which is probably not what the layout means.
            if (a.executable != b.executable
                && (a.address >> kNumPageOffsetBits) <= ((b_end - 1) >> kNumPageOffsetBits)
                && (b.address >> kNumPageOffsetBits) <= ((a_end - 1) >> kNumPageOffsetBits))
            {
                AEMU_WARN("Linker::link() - {} and {} share a page, so it is both writable and "
                          "executable. Start the section on a page boundary.",
                          a.name, b.name);
            }
        }
    }

    return addresses;
}

/// Puts the symbols of all object files into the symbol table of the executable, with the final
/// addresses. A symbol of one object file is the same symbol of another by name, unless it is local.
/// Returns, for each object file, the executable's symbol for each of its symbols.
std::vector<Linker::SymbolMap> Linker::merge_symbols(ObjectFile &exe,
                                                     const std::vector<SectionBase> &bases,
                                                     const SectionBase &addresses) const
{
    std::vector<SymbolMap> maps(m_obj_files.size());
    for (size_t i = 0; i < m_obj_files.size(); i++)
    {
        const ObjectFile &obj = m_obj_files[i];
        const U32 text = obj.section_table.at(".text");
        const U32 data = obj.section_table.at(".data");
        const U32 bss = obj.section_table.at(".bss");

        for (const auto &[key, symbol] : obj.symbol_table)
        {
            std::string name = obj.strings[symbol.symbol_name];
            if (symbol.binding_info == ObjectFile::SymbolTableEntry::BindingInfo::LOCAL)
            {
                name += ":LOCAL:" + std::to_string(i);
            }

            word value = symbol.symbol_value;
            if (symbol.section == text)
            {
                value += addresses.text + bases[i].text;
            }
            else if (symbol.section == data)
            {
                value += addresses.data + bases[i].data;
            }
            else if (symbol.section == bss)
            {
                value += addresses.bss + bases[i].bss;
            }

            exe.add_symbol(name, value, symbol.binding_info, symbol.section);
            maps[i][key] = exe.string_table.at(name);
        }
    }
    return maps;
}

/// The loader starts the program at _start. ENTRY(symbol) makes `symbol` the entry point by
/// aliasing _start to it.
void Linker::define_entry(ObjectFile &exe) const
{
    const auto defined = [&](const std::string &name)
    {
        const auto string = exe.string_table.find(name);
        return string != exe.string_table.end()
               && !is_undefined(exe.symbol_table.at(string->second));
    };

    if (m_entry_symbol != "_start")
    {
        AEMU_CHECK(defined(m_entry_symbol),
                   "Linker::link() - The entry point '{}' set by ENTRY is not defined.",
                   m_entry_symbol);
        AEMU_CHECK(!defined("_start"),
                   "Linker::link() - ENTRY({}) conflicts with the symbol _start that is also "
                   "defined.",
                   m_entry_symbol);

        const ObjectFile::SymbolTableEntry entry =
            exe.symbol_table.at(exe.string_table.at(m_entry_symbol));
        exe.add_symbol("_start", entry.symbol_value,
                       ObjectFile::SymbolTableEntry::BindingInfo::GLOBAL, entry.section);
    }

    AEMU_CHECK(defined("_start"), "Linker::link() - The entry point '{}' is not defined.",
               m_entry_symbol);
}

/// Every section has its final address by now, so each relocation is resolved here and the
/// executable needs none at load time.
void Linker::relocate(ObjectFile &exe, const std::vector<SectionBase> &bases,
                      const SectionBase &addresses, const std::vector<SymbolMap> &symbols) const
{
    for (size_t i = 0; i < m_obj_files.size(); i++)
    {
        const ObjectFile &obj = m_obj_files[i];
        for (const ObjectFile::RelocationEntry &rel : obj.rel_text)
        {
            const ObjectFile::SymbolTableEntry &symbol =
                exe.symbol_table.at(symbols[i].at(rel.symbol));

            AEMU_CHECK(!is_undefined(symbol),
                       "Linker::link() - Error, undefined reference to '{}'.",
                       exe.strings.at(symbol.symbol_name));

            const word instr_i = (bases[i].text + rel.offset) / 4;
            const word instr_address = addresses.text + bases[i].text + rel.offset;
            exe.text_section[instr_i] = apply_relocation(rel.type, obj.text_section[rel.offset / 4],
                                                         instr_address, symbol.symbol_value);
        }

        // The words of .data that hold the address of a symbol.
        for (const ObjectFile::RelocationEntry &rel : obj.rel_data)
        {
            const ObjectFile::SymbolTableEntry &symbol =
                exe.symbol_table.at(symbols[i].at(rel.symbol));

            AEMU_CHECK(!is_undefined(symbol),
                       "Linker::link() - Error, undefined reference to '{}'.",
                       exe.strings.at(symbol.symbol_name));
            AEMU_CHECK(rel.type == ObjectFile::RelocationEntry::Type::R_EMU32_ABS32,
                       "Linker::link() - A relocation in .data of type {} is not supported.",
                       U32(rel.type));

            word current = 0;
            for (size_t b = 0; b < sizeof(word); b++)
            {
                current |= word(obj.data_section[rel.offset + b]) << (8 * b);
            }

            const word index = bases[i].data + rel.offset;
            const word patched =
                apply_relocation(rel.type, current, addresses.data + index, symbol.symbol_value);
            for (size_t b = 0; b < sizeof(word); b++)
            {
                exe.data_section[index + b] = byte(patched >> (8 * b));
            }
        }
    }
}

void Linker::tokenize_ld()
{
    word text_size = 0;
    for (const ObjectFile &obj_file : m_obj_files)
    {
        text_size += obj_file.text_section.size() * 4;
    }

    basm::LexOptions options;
    options.mode = basm::LexMode::LINKER_SCRIPT;
    options.keep_newlines = false;

    const basm::SourceId source =
        m_use_default_script
            ? m_sources.add("<default linker script>", default_linker_script(text_size))
            : m_sources.add_file(m_ld_file.get_path());
    AEMU_CHECK(source != basm::kInvalidSource,
               "Linker::tokenize_ld() - Cannot read the linker script '{}'.", m_ld_file.get_path());

    m_lexed = basm::lex(m_sources, source, options);
    basm::fatal_if_errors(m_sources, m_lexed);
    m_cursor = basm::TokenCursor(std::span<const basm::Token>(m_lexed.tokens));
}

word Linker::parse_value()
{
    const basm::Token &token = m_cursor.peek();
    if (!basm::is_integer_literal(token.type))
    {
        fail(token, "expected a number, got " + basm::describe(token));
    }
    m_cursor.next();

    if (token.int_value > UINT32_MAX)
    {
        fail(token, "address " + token.str() + " does not fit in 32 bits");
    }
    return static_cast<word>(token.int_value);
}
