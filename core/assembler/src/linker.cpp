#include "assembler/linker.h"
#include "assembler/relocation.h"
#include "emulator32bit/emulator32bit_util.h"
#include "util/logger.h"

#include <algorithm>
#include <span>

namespace
{

/// The layout used when no linker script is given. It is part of the program, so linking does not
/// depend on where the source tree is. .rodata starts on the first page after .text, with
/// .init_array and .fini_array following it, and .data on the first page after those, so that
/// code, read only data and data never share a page however big they are. .bss follows .data.
std::string default_linker_script(word text_size, word readonly_size)
{
    const auto next_page = [](word size) { return (size + kPageSize - 1) & ~(kPageSize - 1); };
    const word rodata_address = next_page(text_size);
    const word data_address = next_page(rodata_address + readonly_size);
    return "ENTRY(_start)\n\nSECTIONS (\n    .text = 0x0;\n    .rodata = "
           + std::to_string(rodata_address) + ";\n    .init_array;\n    .fini_array;\n    .data = "
           + std::to_string(data_address) + ";\n    .bss;\n)\n";
}

/// Size of a section of the executable made of the same section of each object file, each put at
/// a multiple of its alignment (what merge_sections does).
word merged_size(const std::vector<ObjectFile> &objects, const ObjectFile::ByteSection &section)
{
    word size = 0;
    for (const ObjectFile &obj : objects)
    {
        const word alignment =
            std::max<word>(1, obj.sections[obj.section_table.at(section.name)].alignment);
        size += (alignment - size % alignment) % alignment;
        size += word((obj.*section.bytes).size());
    }
    return size;
}

bool is_undefined(const ObjectFile::SymbolTableEntry &symbol)
{
    return symbol.section == U32(-1);
}

/// A relocation can use the symbol: it is defined, or it is weak and nothing defines it, which
/// makes it 0.
bool is_resolvable(const ObjectFile::SymbolTableEntry &symbol)
{
    return !is_undefined(symbol)
           || symbol.binding_info == ObjectFile::SymbolTableEntry::BindingInfo::WEAK_DECLARED;
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
        case basm::TokenType::ASSEMBLER_RODATA:
        case basm::TokenType::ASSEMBLER_INIT_ARRAY:
        case basm::TokenType::ASSEMBLER_FINI_ARRAY:
            m_sections.push_back(
                {.type = SectionAddress::Type::BYTES,
                 .byte_index = section.type == basm::TokenType::ASSEMBLER_DATA         ? 0u
                               : section.type == basm::TokenType::ASSEMBLER_RODATA     ? 1u
                               : section.type == basm::TokenType::ASSEMBLER_INIT_ARRAY ? 2u
                                                                                       : 3u,
                 .physical = m_physical});
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
    define_array_symbols(exe, addresses);
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

    // After the sections that every object file has in the same place, in the order that the
    // assembler adds them (the symbols keep the section index of the object file).
    for (const ObjectFile::ByteSection &section : ObjectFile::byte_sections())
    {
        if (!exe.section_table.count(section.name))
        {
            exe.add_section(section.name, section.type);
            exe.add_section(section.rel_name, section.rel_type);
        }
    }
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

    const auto &byte_sections = ObjectFile::byte_sections();
    SectionBase alignment = {.text = 4, .bytes = {}, .bss = 1};
    for (size_t k = 0; k < byte_sections.size(); k++)
    {
        alignment.bytes[k] = 1;
    }

    std::vector<SectionBase> bases;
    for (const ObjectFile &obj : m_obj_files)
    {
        SectionBase base;

        const word wanted_text = alignment_of(obj, ".text");
        exe.text_section.insert(exe.text_section.end(),
                                pad_to(word(exe.text_section.size() * 4), wanted_text) / 4, 0);
        alignment.text = std::max(alignment.text, wanted_text);
        base.text = word(exe.text_section.size() * 4);
        exe.text_section.insert(exe.text_section.end(), obj.text_section.begin(),
                                obj.text_section.end());

        for (size_t k = 0; k < byte_sections.size(); k++)
        {
            const ObjectFile::ByteSection &section = byte_sections[k];
            std::vector<byte> &merged = exe.*section.bytes;
            const word wanted = alignment_of(obj, section.name);
            merged.insert(merged.end(), pad_to(word(merged.size()), wanted), 0);
            alignment.bytes[k] = std::max(alignment.bytes[k], wanted);
            base.bytes[k] = word(merged.size());
            merged.insert(merged.end(), (obj.*section.bytes).begin(), (obj.*section.bytes).end());
        }

        const word wanted_bss = alignment_of(obj, ".bss");
        exe.bss_section += pad_to(exe.bss_section, wanted_bss);
        alignment.bss = std::max(alignment.bss, wanted_bss);
        base.bss = exe.bss_section;
        exe.bss_section += obj.bss_section;

        bases.push_back(base);
    }

    exe.sections[exe.section_table.at(".text")].alignment = alignment.text;
    for (size_t k = 0; k < byte_sections.size(); k++)
    {
        exe.sections[exe.section_table.at(byte_sections[k].name)].alignment = alignment.bytes[k];
    }
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
        bool writable;
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
        case SectionAddress::Type::BYTES:
        {
            const ObjectFile::ByteSection &bytes = ObjectFile::byte_sections()[section.byte_index];
            name = bytes.name;
            size = word((exe.*bytes.bytes).size()); // bytes, unlike .text
            section_address = &addresses.bytes[section.byte_index];
            break;
        }
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
                          section.type == SectionAddress::Type::TEXT,
                          section.type == SectionAddress::Type::BSS
                              || (section.type == SectionAddress::Type::BYTES
                                  && ObjectFile::byte_sections()[section.byte_index].writable)});
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
            if (((a.executable && b.writable) || (a.writable && b.executable))
                && (a.address >> kNumPageOffsetBits) <= ((b_end - 1) >> kNumPageOffsetBits)
                && (b.address >> kNumPageOffsetBits) <= ((a_end - 1) >> kNumPageOffsetBits))
            {
                AEMU_WARN("Linker::link() - {} and {} share a page, so it is both writable and "
                          "executable. Start the section on a page boundary.",
                          a.name, b.name);
            }
        }
    }

    // A section with contents that the script does not place would be loaded at address 0. (A
    // script without SECTIONS places nothing at all, which is not an error.)
    const auto check_placed = [&](const char *name, word size)
    {
        if (placed.empty())
        {
            return;
        }
        const auto listed = std::any_of(placed.begin(), placed.end(), [&](const Placed &p)
                                        { return std::string_view(p.name) == name; });
        AEMU_CHECK(size == 0 || listed,
                   "Linker::link() - The section {} has contents but the linker script does not "
                   "place it.",
                   name);
    };
    check_placed(".text", word(exe.text_section.size() * 4));
    for (const ObjectFile::ByteSection &section : ObjectFile::byte_sections())
    {
        check_placed(section.name, word((exe.*section.bytes).size()));
    }
    check_placed(".bss", exe.bss_section);

    return addresses;
}

/// `__init_array_start`, `__init_array_end`, `__fini_array_start` and `__fini_array_end` are the
/// bounds of the two arrays, for the startup code that calls what is in them. A symbol with that name
/// that the program defines itself is left alone, one that it only refers to gets the value.
void Linker::define_array_symbols(ObjectFile &exe, const SectionBase &addresses) const
{
    const auto define = [&](const char *name, size_t k, bool end)
    {
        const ObjectFile::ByteSection &section = ObjectFile::byte_sections()[k];
        const word value = addresses.bytes[k] + (end ? word((exe.*section.bytes).size()) : 0);
        const U32 section_index = exe.section_table.at(section.name);

        const auto found = exe.string_table.find(name);
        if (found == exe.string_table.end())
        {
            exe.add_symbol(name, value, ObjectFile::SymbolTableEntry::BindingInfo::GLOBAL,
                           section_index);
        }
        else if (is_undefined(exe.symbol_table.at(found->second)))
        {
            ObjectFile::SymbolTableEntry &entry = exe.symbol_table.at(found->second);
            entry.symbol_value = value;
            entry.section = section_index;
            entry.binding_info = ObjectFile::SymbolTableEntry::BindingInfo::GLOBAL;
        }
    };
    define("__init_array_start", 2, false);
    define("__init_array_end", 2, true);
    define("__fini_array_start", 3, false);
    define("__fini_array_end", 3, true);
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
        using Binding = ObjectFile::SymbolTableEntry::BindingInfo;

        for (const auto &[key, symbol] : obj.symbol_table)
        {
            std::string name = obj.strings[symbol.symbol_name];
            if (symbol.binding_info == Binding::LOCAL)
            {
                name += ":LOCAL:" + std::to_string(i);
            }

            word value = symbol.symbol_value;
            if (symbol.section != U32(-1))
            {
                const ObjectFile::SectionHeader::Type type = obj.sections[symbol.section].type;
                if (type == ObjectFile::SectionHeader::Type::TEXT)
                {
                    value += addresses.text + bases[i].text;
                }
                else if (type == ObjectFile::SectionHeader::Type::BSS)
                {
                    value += addresses.bss + bases[i].bss;
                }
                else if (const ObjectFile::ByteSection *bytes = ObjectFile::byte_section_of(type))
                {
                    const size_t k = bytes - ObjectFile::byte_sections().data();
                    value += addresses.bytes[k] + bases[i].bytes[k];
                }
            }

            const auto found = exe.string_table.find(name);
            if (found == exe.string_table.end())
            {
                exe.add_symbol(name, value, symbol.binding_info, symbol.section);
            }
            else
            {
                // Seen in an earlier file. A definition beats a reference, a strong definition
                // beats a weak one (`.weak`, `.comm`), and of two weak ones the first stays. A
                // reference that is not weak makes the symbol one that has to be defined.
                ObjectFile::SymbolTableEntry &entry = exe.symbol_table.at(found->second);
                const bool defined = symbol.section != U32(-1);
                const bool weak = symbol.binding_info == Binding::WEAK_DECLARED;
                const bool entry_defined = entry.section != U32(-1);
                const bool entry_weak = entry.binding_info == Binding::WEAK_DECLARED;

                if (!defined)
                {
                    if (!weak && !entry_defined)
                    {
                        entry.binding_info = symbol.binding_info;
                    }
                }
                else if (!entry_defined || (entry_weak && !weak))
                {
                    entry.symbol_value = value;
                    entry.section = symbol.section;
                    entry.binding_info = symbol.binding_info;
                }
                else if (!weak && !entry_weak)
                {
                    AEMU_FATAL("Linker::link() - Multiple definition of symbol '{}'.", name);
                }
            }
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

            AEMU_CHECK(is_resolvable(symbol),
                       "Linker::link() - Error, undefined reference to '{}'.",
                       exe.strings.at(symbol.symbol_name));

            const word instr_i = (bases[i].text + rel.offset) / 4;
            const word instr_address = addresses.text + bases[i].text + rel.offset;
            exe.text_section[instr_i] =
                apply_relocation(rel.type, obj.text_section[rel.offset / 4], instr_address,
                                 symbol.symbol_value + word(rel.addend));
        }

        // The words of .data, .rodata and the arrays that hold the address of a symbol.
        for (size_t k = 0; k < ObjectFile::byte_sections().size(); k++)
        {
            const ObjectFile::ByteSection &section = ObjectFile::byte_sections()[k];
            for (const ObjectFile::RelocationEntry &rel : obj.*section.relocations)
            {
                const ObjectFile::SymbolTableEntry &symbol =
                    exe.symbol_table.at(symbols[i].at(rel.symbol));

                AEMU_CHECK(is_resolvable(symbol),
                           "Linker::link() - Error, undefined reference to '{}'.",
                           exe.strings.at(symbol.symbol_name));
                AEMU_CHECK(rel.type == ObjectFile::RelocationEntry::Type::R_EMU32_ABS32,
                           "Linker::link() - A relocation in {} of type {} is not supported.",
                           section.name, U32(rel.type));

                word current = 0;
                for (size_t b = 0; b < sizeof(word); b++)
                {
                    current |= word((obj.*section.bytes)[rel.offset + b]) << (8 * b);
                }

                const word index = bases[i].bytes[k] + rel.offset;
                const word patched = apply_relocation(rel.type, current, addresses.bytes[k] + index,
                                                      symbol.symbol_value + word(rel.addend));
                for (size_t b = 0; b < sizeof(word); b++)
                {
                    (exe.*section.bytes)[index + b] = byte(patched >> (8 * b));
                }
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

    // The sections that follow .text on the pages of their own: rodata and the two arrays.
    word readonly_size = merged_size(m_obj_files, ObjectFile::byte_sections()[1]);
    for (const size_t k : {2, 3})
    {
        const word alignment = ObjectFile::byte_sections()[k].alignment;
        readonly_size += (alignment - readonly_size % alignment) % alignment;
        readonly_size += merged_size(m_obj_files, ObjectFile::byte_sections()[k]);
    }

    basm::LexOptions options;
    options.mode = basm::LexMode::LINKER_SCRIPT;
    options.keep_newlines = false;

    const basm::SourceId source =
        m_use_default_script ? m_sources.add("<default linker script>",
                                             default_linker_script(text_size, readonly_size))
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
