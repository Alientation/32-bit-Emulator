#include "assembler/linker.h"

#include "assembler/relocation.h"
#include "emulator32bit/emulator32bit_util.h"
#include "util/logger.h"

#include <algorithm>
#include <span>

namespace
{

/// How much a section takes in the executable, and what it is aligned to.
struct Extent
{
    word size = 0;
    word alignment = 1;
};

/// Extent of a section of the executable made of the same section of each object file, each put at
/// a multiple of its alignment (what merge_sections does). `size_of` is the size of the section in
/// bytes in an object file that has it.
template <class SizeOf>
Extent merged_extent(const std::vector<ObjectFile> &objects, const std::string &name,
                     const SizeOf &size_of)
{
    Extent extent;
    for (const ObjectFile &obj : objects)
    {
        const auto header = obj.section_table.find(name);
        if (header == obj.section_table.end()) continue;

        const word alignment = std::max<word>(1, obj.sections[header->second].alignment);
        extent.alignment = std::max(extent.alignment, alignment);
        extent.size += (alignment - extent.size % alignment) % alignment;
        extent.size += size_of(obj);
    }
    return extent;
}

/// The user sections of the object files: the name, whether the program may write it, whether
/// it runs code from it and whether it is zero filled. In the order they are first seen, and the
/// same section in two files has to have the same permissions and kind.
std::vector<ObjectFile::UserSection> user_sections_of(const std::vector<ObjectFile> &objects)
{
    std::vector<ObjectFile::UserSection> found;
    for (const ObjectFile &obj : objects)
    {
        for (const ObjectFile::UserSection &section : obj.user_sections)
        {
            const auto same = std::find_if(found.begin(), found.end(),
                                           [&](const auto &s) { return s.name == section.name; });
            if (same == found.end())
            {
                ObjectFile::UserSection permissions;
                permissions.name = section.name;
                permissions.writable = section.writable;
                permissions.executable = section.executable;
                permissions.nobits = section.nobits;
                found.push_back(std::move(permissions));
            }
            else
            {
                AEMU_CHECK(same->writable == section.writable
                               && same->executable == section.executable
                               && same->nobits == section.nobits,
                           "Linker::link() - The section {} does not have the same flags in all "
                           "the files.",
                           section.name);
            }
        }
    }
    return found;
}

/// Where the user section is in the executable's list, which is the order of `found` above.
size_t user_index(const ObjectFile &exe, const std::string &name)
{
    for (size_t i = 0; i < exe.user_sections.size(); i++)
    {
        if (exe.user_sections[i].name == name) return i;
    }
    return size_t(-1);
}

std::string quoted(const std::string &name)
{
    std::string text = "\"";
    for (const char c : name)
    {
        if (c == '"' || c == '\\') text += '\\';
        text += c;
    }
    return text + "\"";
}

/// The layout used when no linker script is given. It is part of the program, so linking does not
/// depend on where the source tree is. The code is first (.text, then the executable sections of
/// the program). .rodata starts on the first page after it, with .init_array and .fini_array
/// following it, then the read only sections of the program. .data starts on the first page after
/// those, followed by the writable sections of the program, so that code, read only data and data
/// never share a page however big they are. .bss follows.
std::string default_linker_script(const std::vector<ObjectFile> &objects)
{
    const auto next_page = [](word size) { return (size + kPageSize - 1) & ~(kPageSize - 1); };
    const std::vector<ObjectFile::UserSection> user = user_sections_of(objects);

    word address = 0;
    const auto add = [&](const Extent &extent)
    {
        address += (extent.alignment - address % extent.alignment) % extent.alignment;
        address += extent.size;
    };
    const auto bytes_extent = [&](const ObjectFile::ByteSection &section)
    {
        return merged_extent(objects, section.name,
                             [&](const ObjectFile &obj) { return word((obj.*section.bytes).size()); });
    };
    const auto user_extent = [&](const std::string &name)
    {
        return merged_extent(objects, name,
                             [&](const ObjectFile &obj)
                             { return obj.find_user_section(name)->size(); });
    };

    std::string script = "ENTRY(_start)\n\nSECTIONS (\n    .text = 0x0;\n";
    add(merged_extent(objects, ".text",
                               [](const ObjectFile &obj) { return word(obj.text_section.size() * 4); }));
    for (const auto &section : user)
    {
        if (!section.executable) continue;
        add(user_extent(section.name));
        script += "    " + quoted(section.name) + ";\n";
    }

    address = next_page(address);
    script += "    .rodata = " + std::to_string(address) + ";\n";
    for (const size_t k : {1, 2, 3})
    {
        add(bytes_extent(ObjectFile::byte_sections()[k]));
        if (k != 1) script += std::string("    ") + ObjectFile::byte_sections()[k].name + ";\n";
    }
    for (const auto &section : user)
    {
        if (section.executable || section.writable) continue;
        add(user_extent(section.name));
        script += "    " + quoted(section.name) + ";\n";
    }

    address = next_page(address);
    script += "    .data = " + std::to_string(address) + ";\n";
    for (const auto &section : user)
    {
        if (!section.writable) continue;
        script += "    " + quoted(section.name) + ";\n";
    }
    script += "    .bss;\n)\n";
    return script;
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
        case basm::TokenType::LITERAL_STRING:
            // A section of the program's own, by its name.
            m_sections.push_back({.type = SectionAddress::Type::USER,
                                  .name = basm::unescape_string_literal(section),
                                  .physical = m_physical});
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

    // There are no relocations in .data and .bss, and none are left in the executable.
    exe.write_object_file(m_exe_file);
}

ObjectFile Linker::new_executable() const
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

    // And the sections of the program's own, which can be anywhere in the object files.
    for (const ObjectFile::UserSection &section : user_sections_of(m_obj_files))
    {
        exe.add_user_section(section.name, section.writable, section.executable, section.nobits);
    }
    return exe;
}

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
    alignment.users.assign(exe.user_sections.size(), 1);

    std::vector<SectionBase> bases;
    for (const ObjectFile &obj : m_obj_files)
    {
        SectionBase base;
        base.users.assign(exe.user_sections.size(), 0);

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

        for (const ObjectFile::UserSection &section : obj.user_sections)
        {
            const size_t e = user_index(exe, section.name);
            ObjectFile::UserSection &merged = exe.user_sections[e];
            const word wanted = alignment_of(obj, section.name.c_str());
            alignment.users[e] = std::max(alignment.users[e], wanted);
            if (merged.nobits)
            {
                merged.zero_size += pad_to(merged.zero_size, wanted);
                base.users[e] = merged.zero_size;
                merged.zero_size += section.zero_size;
                continue;
            }
            merged.bytes.insert(merged.bytes.end(), pad_to(word(merged.bytes.size()), wanted), 0);
            base.users[e] = word(merged.bytes.size());
            merged.bytes.insert(merged.bytes.end(), section.bytes.begin(), section.bytes.end());
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
    for (size_t e = 0; e < exe.user_sections.size(); e++)
    {
        exe.sections[exe.user_sections[e].header_index].alignment = alignment.users[e];
    }
    return bases;
}

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
    addresses.users.assign(exe.user_sections.size(), 0);
    word address = 0;
    for (const SectionAddress &section : m_sections)
    {
        const char *name;
        word size;
        word *section_address;
        bool executable = section.type == SectionAddress::Type::TEXT;
        bool writable = section.type == SectionAddress::Type::BSS
                        || (section.type == SectionAddress::Type::BYTES
                            && ObjectFile::byte_sections()[section.byte_index].writable);
        switch (section.type)
        {
        case SectionAddress::Type::USER:
        {
            // A script can list a section that none of the files has (it may be shared between
            // programs), which is empty.
            const size_t e = user_index(exe, section.name);
            if (e == size_t(-1)) continue;

            const ObjectFile::UserSection &user = exe.user_sections[e];
            name = user.name.c_str();
            size = user.size();
            section_address = &addresses.users[e];
            executable = user.executable;
            writable = user.writable;
            break;
        }
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

        placed.push_back({name, header.address, size, section.physical, executable, writable});
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
    for (const ObjectFile::UserSection &user : exe.user_sections)
    {
        check_placed(user.name.c_str(), user.size());
    }

    return addresses;
}

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

            // The symbol is in the section of the same name of the executable. The index of its
            // header is the same for the sections that every file has, but a section of the
            // program's own can be anywhere in a file.
            word value = symbol.symbol_value;
            U32 section = U32(-1);
            if (symbol.section != U32(-1))
            {
                const ObjectFile::SectionHeader::Type type = obj.sections[symbol.section].type;
                const std::string &section_name =
                    obj.strings[obj.sections[symbol.section].section_name];
                section = exe.section_table.at(section_name);

                if (type == ObjectFile::SectionHeader::Type::TEXT)
                {
                    value += addresses.text + bases[i].text;
                }
                else if (type == ObjectFile::SectionHeader::Type::BSS)
                {
                    value += addresses.bss + bases[i].bss;
                }
                else if (ObjectFile::is_user_section_type(type))
                {
                    const size_t e = user_index(exe, section_name);
                    value += addresses.users[e] + bases[i].users[e];
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
                exe.add_symbol(name, value, symbol.binding_info, section);
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
                    entry.section = section;
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

        // The sections of the program's own: a word of data like those above, or an instruction
        // in an executable one, which is patched like one of .text.
        for (const ObjectFile::UserSection &user : obj.user_sections)
        {
            const size_t e = user_index(exe, user.name);
            for (const ObjectFile::RelocationEntry &rel : user.relocations)
            {
                const ObjectFile::SymbolTableEntry &symbol =
                    exe.symbol_table.at(symbols[i].at(rel.symbol));

                AEMU_CHECK(is_resolvable(symbol),
                           "Linker::link() - Error, undefined reference to '{}'.",
                           exe.strings.at(symbol.symbol_name));
                AEMU_CHECK(user.executable
                               || rel.type == ObjectFile::RelocationEntry::Type::R_EMU32_ABS32,
                           "Linker::link() - A relocation in {} of type {} is not supported.",
                           user.name, U32(rel.type));

                word current = 0;
                for (size_t b = 0; b < sizeof(word); b++)
                {
                    current |= word(user.bytes[rel.offset + b]) << (8 * b);
                }

                const word index = bases[i].users[e] + rel.offset;
                const word patched =
                    apply_relocation(rel.type, current, addresses.users[e] + index,
                                     symbol.symbol_value + word(rel.addend));
                for (size_t b = 0; b < sizeof(word); b++)
                {
                    exe.user_sections[e].bytes[index + b] = byte(patched >> (8 * b));
                }
            }
        }
    }
}

void Linker::tokenize_ld()
{
    basm::LexOptions options;
    options.mode = basm::LexMode::LINKER_SCRIPT;
    options.keep_newlines = false;

    const basm::SourceId source =
        m_use_default_script ? m_sources.add("<default linker script>",
                                             default_linker_script(m_obj_files))
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
