#include "assembler/linker.h"
#include "emulator32bit/fbl.h"
#include "util/logger.h"

#include <optional>
#include <span>

Linker::Linker(std::vector<ObjectFile> obj_files, File exe_file) :
    m_obj_files(obj_files),
    m_exe_file(exe_file),
    m_ld_file(File("default_linker", "ld", std::string(AEMU_PROJECT_ROOT_DIR) + "/assembler/src"))
{
    link();
}

Linker::Linker(std::vector<ObjectFile> obj_files, File exe_file, File ld_file) :
    m_obj_files(obj_files),
    m_exe_file(exe_file),
    m_ld_file(ld_file)
{
    link();
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

    ObjectFile exe_obj_file;

    exe_obj_file.file_type = ObjectFile::kExecutableFileType;
    exe_obj_file.target_machine = ObjectFile::kEMU32MachineId;
    exe_obj_file.flags = 0;

    exe_obj_file.add_section(".text", ObjectFile::SectionHeader::Type::TEXT);
    exe_obj_file.add_section(".data", ObjectFile::SectionHeader::Type::DATA);
    exe_obj_file.add_section(".bss", ObjectFile::SectionHeader::Type::BSS);
    exe_obj_file.add_section(".symtab", ObjectFile::SectionHeader::Type::SYMTAB);
    exe_obj_file.add_section(".rel.text", ObjectFile::SectionHeader::Type::REL_TEXT);
    exe_obj_file.add_section(".rel.data", ObjectFile::SectionHeader::Type::REL_DATA);
    exe_obj_file.add_section(".rel.bss", ObjectFile::SectionHeader::Type::REL_BSS);
    exe_obj_file.add_section(".strtab", ObjectFile::SectionHeader::Type::STRTAB);

    /* Add all .text section together. order of obj files in list is the order they will be in memory */
    for (ObjectFile &obj_file : m_obj_files)
    {
        exe_obj_file.text_section.insert(exe_obj_file.text_section.end(),
                                         obj_file.text_section.begin(),
                                         obj_file.text_section.end());
    }

    /* .data section */
    for (ObjectFile &obj_file : m_obj_files)
    {
        exe_obj_file.data_section.insert(exe_obj_file.data_section.end(),
                                         obj_file.data_section.begin(),
                                         obj_file.data_section.end());
    }

    /* .bss section */
    for (ObjectFile &obj_file : m_obj_files)
    {
        exe_obj_file.bss_section += obj_file.bss_section;
    }

    // todo, add to freeblocklist a way to remove blocks of a certain range so we can mark the address space of these sections as taken
    // FreeBlockList free_vm_address_space(0, 1 << (sizeof(word) * 8));
    word address = 0;
    word offset_text = 0;
    word offset_data = 0;
    word offset_bss = 0;
    for (SectionAddress &section : m_sections)
    {
        ObjectFile::SectionHeader *section_header = nullptr;
        word section_size = 0;
        if (section.type == SectionAddress::Type::TEXT)
        {
            section_header = &exe_obj_file.sections[exe_obj_file.section_table.at(".text")];
            section_size = exe_obj_file.text_section.size() * 4;
            offset_text = section.set_address ? section.address : address;
        }
        else if (section.type == SectionAddress::Type::DATA)
        {
            section_header = &exe_obj_file.sections[exe_obj_file.section_table.at(".data")];
            section_size = exe_obj_file.data_section.size(); // bytes, unlike .text
            offset_data = section.set_address ? section.address : address;
        }
        else if (section.type == SectionAddress::Type::BSS)
        {
            section_header = &exe_obj_file.sections[exe_obj_file.section_table.at(".bss")];
            section_size = exe_obj_file.bss_section;
            offset_bss = section.set_address ? section.address : address;
        }

        section_header->load_at_physical_address = section.physical;
        section_header->address = section.set_address ? section.address : address;
        address = section_header->address + section_size;
    }

    /* .symtab section */
    word text_section_size = 0;
    word data_section_size = 0;
    word bss_section_size = 0;
    for (size_t i = 0; i < m_obj_files.size(); i++)
    {
        ObjectFile &obj_file = m_obj_files.at(i);
        for (auto &pair : obj_file.symbol_table)
        {
            std::string symbol_name = obj_file.strings[pair.first];

            if (pair.second.binding_info == ObjectFile::SymbolTableEntry::BindingInfo::LOCAL)
            {
                symbol_name += ":LOCAL:" + std::to_string(i);
            }

            word val = pair.second.symbol_value;
            if (pair.second.section == obj_file.section_table.at(".text"))
            {
                val += offset_text + text_section_size;
            }
            else if (pair.second.section == obj_file.section_table.at(".data"))
            {
                val += offset_data + data_section_size;
            }
            else if (pair.second.section == obj_file.section_table.at(".bss"))
            {
                val += offset_bss + bss_section_size;
            }

            exe_obj_file.add_symbol(symbol_name, val, pair.second.binding_info,
                                    pair.second.section);
            /* Updated current obj file symbol table (pair is passed as reference), this will be used to assist with
                relocation by mapping this symbol to the corresponding symbol in the exe file */
            pair.second.symbol_name = exe_obj_file.string_table.at(symbol_name);
            pair.second.symbol_value = val;
            pair.second.binding_info =
                exe_obj_file.symbol_table[pair.second.symbol_name].binding_info;
        }

        text_section_size += obj_file.text_section.size() * 4;
        data_section_size += obj_file.data_section.size();
        bss_section_size += obj_file.bss_section;
    }

    /* .rel.text section */
    text_section_size = 0;
    for (ObjectFile &obj_file : m_obj_files)
    {
        for (ObjectFile::RelocationEntry &rel : obj_file.rel_text)
        {
            /* exe file's symbol table contains the most recent updated version of the symbol across all obj files.
                Since all obj file symbols have been converted to point towards the exe file symbol table, we have to find the symbol located
                in this obj file which the symbol name will be the index into the combined string table. */
            ObjectFile::SymbolTableEntry symbol_entry =
                exe_obj_file.symbol_table.at(obj_file.symbol_table.at(rel.symbol).symbol_name);

            /* all symbols should have a corresponding definition */
            if (symbol_entry.binding_info == ObjectFile::SymbolTableEntry::BindingInfo::WEAK)
            {
                AEMU_FATAL("Linker::link() - Error, symbol definition is not found.");
                continue;
            }

            word instr_i = (offset_text + text_section_size + rel.offset) / 4;

            /* Only fill in relocations that are relative offsets since we do not know where the exe file will be in memory */
            switch (rel.type)
            {
            case ObjectFile::RelocationEntry::Type::R_EMU32_O_LO12:
                // exe_obj_file.text_section[instr_i] = mask_0(obj_file.text_section[rel.offset/4], 0, 14) + bitfield_unsigned(symbol_entry.symbol_value, 0, 12);
            case ObjectFile::RelocationEntry::Type::R_EMU32_ADRP_HI20:
                // exe_obj_file.text_section[instr_i] = mask_0(obj_file.text_section[rel.offset/4], 0, 20) + bitfield_unsigned(symbol_entry.symbol_value, 12, 20);
            case ObjectFile::RelocationEntry::Type::R_EMU32_MOV_LO19:
                // exe_obj_file.text_section[instr_i] = mask_0(obj_file.text_section[rel.offset/4], 0, 19) + bitfield_unsigned(symbol_entry.symbol_value, 0, 19);
            case ObjectFile::RelocationEntry::Type::R_EMU32_MOV_HI13:
                // exe_obj_file.text_section[instr_i] = mask_0(obj_file.text_section[rel.offset/4], 0, 19) + bitfield_unsigned(symbol_entry.symbol_value, 19, 13);
                break;
            case ObjectFile::RelocationEntry::Type::R_EMU32_B_OFFSET22:
                AEMU_CHECK((symbol_entry.symbol_value & 0b11) == 0,
                           "Linker::fill_local() - Expected relocation value for "
                           "R_EMU32_B_OFFSET22 to be 4 byte aligned. Got {}",
                           symbol_entry.symbol_value);
                exe_obj_file.text_section[instr_i] =
                    mask_0(obj_file.text_section[rel.offset / 4], 0, 22)
                    + bitfield_unsigned(bitfield_signed(symbol_entry.symbol_value, 2, 22) - instr_i,
                                        0, 22);
                continue;
            case ObjectFile::RelocationEntry::Type::UNDEFINED:
            default:
                AEMU_FATAL("Linker::fill_local() - Unknown relocation entry type.");
            }

            /* relocation is not a relative offset, add to exe file relocation to be resolved when the exe file is loaded into memory */
            exe_obj_file.rel_text.push_back({
                .offset = rel.offset + offset_text + text_section_size,
                .symbol = obj_file.symbol_table.at(rel.symbol).symbol_name,
                .type = rel.type,
                .shift = rel.shift,
                .token = 0,
            });
        }

        text_section_size += 4 * obj_file.text_section.size();
    }

    // offset_data = 0;
    // offset_bss = 0;
    /* .rel.data section */
    /* .rel.bss section */

    // offset_data += obj_file.data_section.size();
    // offset_bss += obj_file.bss_section;

    exe_obj_file.write_object_file(m_exe_file);
}

void Linker::tokenize_ld()
{
    basm::LexOptions options;
    options.mode = basm::LexMode::LINKER_SCRIPT;
    options.keep_newlines = false;

    const basm::SourceId source = m_sources.add_file(m_ld_file.get_path());
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