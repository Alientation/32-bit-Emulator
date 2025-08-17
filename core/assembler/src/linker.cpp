#include "assembler/linker.h"
#include "emulator32bit/fbl.h"
#include "util/logger.h"

#include <regex>

Linker::Linker (std::vector<ObjectFile> obj_files, File exe_file) :
    m_obj_files (obj_files),
    m_exe_file (exe_file),
    m_ld_file (File ("default_linker", "ld", AEMU_PROJECT_ROOT_DIR + "core/assembler/src"))
{
    link ();
}

Linker::Linker (std::vector<ObjectFile> obj_files, File exe_file, File ld_file) :
    m_obj_files (obj_files),
    m_exe_file (exe_file),
    m_ld_file (ld_file)
{
    link ();
}

void Linker::_entry (size_t &tok_i)
{
    consume (tok_i);
    skip_tokens (tok_i, {Token::Type::WHITESPACE});
    consume (tok_i, {Token::Type::OPEN_PARENTHESIS},
             "Expected open parenthesis after ENTRY command. Got " + m_tokens[tok_i].val);
    skip_tokens (tok_i, {Token::Type::WHITESPACE});
    m_entry_symbol = consume (tok_i, {Token::Type::SYMBOL},
                              "Expected symbol to follow ENTRY command. Got " + m_tokens[tok_i].val)
                         .val;

    skip_tokens (tok_i, {Token::Type::WHITESPACE});
    consume (tok_i, {Token::Type::CLOSE_PARENTHESIS},
             "Expected close parenthesis after ENTRY command. Got " + m_tokens[tok_i].val);
}

void Linker::_sections (size_t &tok_i)
{
    consume (tok_i);
    skip_tokens (tok_i, {Token::Type::WHITESPACE});
    consume (tok_i, {Token::Type::OPEN_PARENTHESIS},
             "Expected open parenthesis after SECTIONS command. Got " + m_tokens[tok_i].val);
    skip_tokens (tok_i, {Token::Type::WHITESPACE});

    while (!is_token (tok_i, {Token::Type::CLOSE_PARENTHESIS}))
    {
        if (m_tokens[tok_i].type == Token::Type::AT)
        {
            consume (tok_i);
            skip_tokens (tok_i, {Token::Type::WHITESPACE});
            std::string tag =
                consume (tok_i, {Token::Token::Type::SYMBOL},
                         "Expected symbol tag to follow @. Got " + m_tokens[tok_i].val)
                    .val;
            if (tag == "P")
            {
                m_physical = true;
            }
            else if (tag == "V")
            {
                m_physical = false;
            }

            skip_tokens (tok_i, {Token::Type::WHITESPACE});
            consume (tok_i, {Token::Token::Type::SEMI_COLON},
                     "Expected semicolon to end statement. Got " + m_tokens[tok_i].val);
            skip_tokens (tok_i, {Token::Type::WHITESPACE});

            continue;
        }

        switch (m_tokens[tok_i].type)
        {
        case Token::Type::TEXT:
            consume (tok_i);
            m_sections.push_back (
                (SectionAddress) {.type = SectionAddress::Type::TEXT, .physical = m_physical});
            break;
        case Token::Type::DATA:
            consume (tok_i);
            m_sections.push_back (
                (SectionAddress) {.type = SectionAddress::Type::DATA, .physical = m_physical});
            break;
        case Token::Type::BSS:
            consume (tok_i);
            m_sections.push_back (
                (SectionAddress) {.type = SectionAddress::Type::BSS, .physical = m_physical});
            break;
        default:
            ERROR ("Invalid token %s in SECTIONS command.", m_tokens[tok_i].val.c_str ());
        }

        skip_tokens (tok_i, {Token::Type::WHITESPACE});
        if (is_token (tok_i, {Token::Type::EQUAL}))
        {
            consume (tok_i, {Token::Type::EQUAL},
                     "Expected equal symbol to follow section. Got " + m_tokens[tok_i].val);
            skip_tokens (tok_i, {Token::Type::WHITESPACE});

            m_sections.back ().set_address = true;
            m_sections.back ().address = parse_value (tok_i);
            skip_tokens (tok_i, {Token::Type::WHITESPACE});
        }

        consume (tok_i, {Token::Type::SEMI_COLON},
                 "Expected semi colon to follow section definition. Got " + m_tokens[tok_i].val);
        skip_tokens (tok_i, {Token::Type::WHITESPACE});
    }
    consume (tok_i);
}

void Linker::parse_ld ()
{
    for (size_t i = 0; i < m_tokens.size ();)
    {
        switch (m_tokens[i].type)
        {
        case Token::Type::WHITESPACE:
            consume (i);
            continue;
        case Token::Type::ENTRY:
            _entry (i);
            break;
        case Token::Type::SECTIONS:
            _sections (i);
            break;
        default:
            ERROR ("Invalid token %s", m_tokens[i].val.c_str ());
        }
    }
}

void Linker::link ()
{
    tokenize_ld ();
    parse_ld ();

    ObjectFile exe_obj_file;

    exe_obj_file.file_type = ObjectFile::kExecutableFileType;
    exe_obj_file.target_machine = ObjectFile::kEMU32MachineId;
    exe_obj_file.flags = 0;

    exe_obj_file.add_section (".text", ObjectFile::SectionHeader::Type::TEXT);
    exe_obj_file.add_section (".data", ObjectFile::SectionHeader::Type::DATA);
    exe_obj_file.add_section (".bss", ObjectFile::SectionHeader::Type::BSS);
    exe_obj_file.add_section (".symtab", ObjectFile::SectionHeader::Type::SYMTAB);
    exe_obj_file.add_section (".rel.text", ObjectFile::SectionHeader::Type::REL_TEXT);
    exe_obj_file.add_section (".rel.data", ObjectFile::SectionHeader::Type::REL_DATA);
    exe_obj_file.add_section (".rel.bss", ObjectFile::SectionHeader::Type::REL_BSS);
    exe_obj_file.add_section (".strtab", ObjectFile::SectionHeader::Type::STRTAB);

    /* Add all .text section together. order of obj files in list is the order they will be in memory */
    for (ObjectFile &obj_file : m_obj_files)
    {
        exe_obj_file.text_section.insert (exe_obj_file.text_section.end (),
                                          obj_file.text_section.begin (),
                                          obj_file.text_section.end ());
    }

    /* .data section */
    for (ObjectFile &obj_file : m_obj_files)
    {
        exe_obj_file.data_section.insert (exe_obj_file.data_section.end (),
                                          obj_file.data_section.begin (),
                                          obj_file.data_section.end ());
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
            section_header = &exe_obj_file.sections[exe_obj_file.section_table.at (".text")];
            section_size = exe_obj_file.text_section.size () * 4;
            offset_text = section.set_address ? section.address : address;
        }
        else if (section.type == SectionAddress::Type::DATA)
        {
            section_header = &exe_obj_file.sections[exe_obj_file.section_table.at (".data")];
            section_size = exe_obj_file.data_section.size () * 4;
            offset_data = section.set_address ? section.address : address;
        }
        else if (section.type == SectionAddress::Type::BSS)
        {
            section_header = &exe_obj_file.sections[exe_obj_file.section_table.at (".bss")];
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
    for (size_t i = 0; i < m_obj_files.size (); i++)
    {
        ObjectFile &obj_file = m_obj_files.at (i);
        for (auto &pair : obj_file.symbol_table)
        {
            std::string symbol_name = obj_file.strings[pair.first];

            if (pair.second.binding_info == ObjectFile::SymbolTableEntry::BindingInfo::LOCAL)
            {
                symbol_name += ":LOCAL:" + std::to_string (i);
            }

            word val = pair.second.symbol_value;
            if (pair.second.section == obj_file.section_table.at (".text"))
            {
                val += offset_text + text_section_size;
            }
            else if (pair.second.section == obj_file.section_table.at (".data"))
            {
                val += offset_data + data_section_size;
            }
            else if (pair.second.section == obj_file.section_table.at (".bss"))
            {
                val += offset_bss + bss_section_size;
            }

            exe_obj_file.add_symbol (symbol_name, val, pair.second.binding_info,
                                     pair.second.section);
            /* Updated current obj file symbol table (pair is passed as reference), this will be used to assist with
                relocation by mapping this symbol to the corresponding symbol in the exe file */
            pair.second.symbol_name = exe_obj_file.string_table.at (symbol_name);
            pair.second.symbol_value = val;
            pair.second.binding_info =
                exe_obj_file.symbol_table[pair.second.symbol_name].binding_info;
        }

        text_section_size += obj_file.text_section.size () * 4;
        data_section_size += obj_file.data_section.size ();
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
                exe_obj_file.symbol_table.at (obj_file.symbol_table.at (rel.symbol).symbol_name);

            /* all symbols should have a corresponding definition */
            if (symbol_entry.binding_info == ObjectFile::SymbolTableEntry::BindingInfo::WEAK)
            {
                ERROR ("Linker::link() - Error, symbol definition is not found.");
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
                EXPECT_TRUE_SS ((symbol_entry.symbol_value & 0b11) == 0,
                                std::stringstream ()
                                    << "Linker::fill_local() - Expected relocation value for "
                                       "R_EMU32_B_OFFSET22 to be 4 byte aligned. Got "
                                    << symbol_entry.symbol_value);
                exe_obj_file.text_section[instr_i] =
                    mask_0 (obj_file.text_section[rel.offset / 4], 0, 22)
                    + bitfield_unsigned (
                        bitfield_signed (symbol_entry.symbol_value, 2, 22) - instr_i, 0, 22);
                continue;
            case ObjectFile::RelocationEntry::Type::UNDEFINED:
            default:
                ERROR ("Linker::fill_local() - Unknown relocation entry type.");
            }

            /* relocation is not a relative offset, add to exe file relocation to be resolved when the exe file is loaded into memory */
            exe_obj_file.rel_text.push_back ((ObjectFile::RelocationEntry) {
                .offset = rel.offset + offset_text + text_section_size,
                .symbol = obj_file.symbol_table.at (rel.symbol).symbol_name,
                .type = rel.type,
                .shift = rel.shift,
                .token = 0,
            });
        }

        text_section_size += 4 * obj_file.text_section.size ();
    }

    // offset_data = 0;
    // offset_bss = 0;
    /* .rel.data section */
    /* .rel.bss section */

    // offset_data += obj_file.data_section.size();
    // offset_bss += obj_file.bss_section;

    exe_obj_file.write_object_file (m_exe_file);
}

void Linker::tokenize_ld ()
{
    FileReader reader (m_ld_file);

    // append a new line to the end to allow regex matching to match an ending whitespace
    std::string source_code = reader.read_all () + "\n";
    reader.close ();

    while (source_code.size () > 0)
    {
        // try to match regex
        bool matched = false;
        for (std::pair<std::string, Linker::Token::Type> regexPair : kTokenSpec)
        {
            std::string regex = regexPair.first;
            Linker::Token::Type type = regexPair.second;
            std::regex token_regex (regex);
            std::smatch match;
            if (std::regex_search (source_code, match, token_regex))
            {
                // matched regex
                std::string token_value = match.str ();
                m_tokens.push_back (Linker::Token (type, token_value));
                source_code = match.suffix ();
                matched = true;

                break;
            }
        }

        // check if regex matched
        EXPECT_TRUE_SS (matched,
                        std::stringstream ()
                            << "Linker::tokenize() - Could not match regex to source code: "
                            << source_code);
    }
}

Linker::Token::Token (Type type, std::string val) :
    type (type),
    val (val)
{
}

const std::vector<std::pair<std::string, Linker::Token::Type>> Linker::kTokenSpec = {
    {"^[^\\S]+", Linker::Token::Type::WHITESPACE},
    {"^/\\*[\\s\\S]*?\\*/", Linker::Token::Type::WHITESPACE},
    {"^//.*", Linker::Token::Type::WHITESPACE},
    {"^ENTRY\\b", Linker::Token::Type::ENTRY},
    {"^SECTIONS\\b", Linker::Token::Type::SECTIONS},
    {"^\\.text\\b", Linker::Token::Type::TEXT},
    {"^\\.data\\b", Linker::Token::Type::DATA},
    {"^\\.bss\\b", Linker::Token::Type::BSS},

    {"^0b[0-1]+", Linker::Token::Type::LITERAL_NUMBER_BINARY},
    {"^0x[0-9a-fA-F]+", Linker::Token::Type::LITERAL_NUMBER_HEXADECIMAL},
    {"^[0-9]+", Linker::Token::Type::LITERAL_NUMBER_DECIMAL},

    {"^\\.", Linker::Token::Type::SECTION_COUNTER},
    {"^\\(", Linker::Token::Type::OPEN_PARENTHESIS},
    {"^\\)", Linker::Token::Type::CLOSE_PARENTHESIS},
    {"^;", Linker::Token::Type::SEMI_COLON},
    {"^,", Linker::Token::Type::COMMA},
    {"^=", Linker::Token::Type::EQUAL},
    {"^@", Linker::Token::Type::AT},
    {"^[a-zA-Z_][a-zA-Z0-9_]*", Linker::Token::Type::SYMBOL},
};

word Linker::parse_value (size_t &tok_i)
{
    Token tok = consume (tok_i);
    std::string val_part = tok.val.size () >= 2 ? tok.val.substr (2) : "";
    word val = 0;
    switch (tok.type)
    {
    case Token::Type::LITERAL_NUMBER_BINARY:
        for (char c : val_part)
        {
            val = (val * 2) + (c - '0');
        }
        break;
    case Token::Type::LITERAL_NUMBER_DECIMAL:
        for (char c : val_part)
        {
            val = (val * 10) + (c - '0');
        }
        break;
    case Token::Type::LITERAL_NUMBER_HEXADECIMAL:
    {
        auto hex_to_decimal = [] (char c)
        {
            if (c >= '0' && c <= '9')
            {
                return c - '0';
            }
            else if (c >= 'a' && c <= 'f')
            {
                return c - 'a';
            }
            else if (c >= 'A' && c <= 'F')
            {
                return c - 'A';
            }
            else
            {
                ERROR ("Invalid hexadecimal digit %c", c);
                return 0;
            }
        };
        for (char c : val_part)
        {
            val = (val * 16) + hex_to_decimal (c);
        }
        break;
    }
    default:
        ERROR ("Expected numeric token but got %s", tok.val.c_str ());
    }
    return val;
}

/**
 * Skips tokens that match the given regex.
 *
 * @param regex matches tokens to skip.
 * @param tok_i the index of the current token.
 */
void Linker::skip_tokens (size_t &tok_i, const std::string &regex)
{
    while (in_bounds (tok_i) && std::regex_match (m_tokens[tok_i].val, std::regex (regex)))
    {
        tok_i++;
    }
}

/**
 * Skips tokens that match the given types.
 *
 * @param tok_i the index of the current token.
 * @param tokenTypes the types to match.
 */
void Linker::skip_tokens (size_t &tok_i, const std::set<Token::Type> &tokenTypes)
{
    while (in_bounds (tok_i) && tokenTypes.find (m_tokens[tok_i].type) != tokenTypes.end ())
    {
        tok_i++;
    }
}

/**
 * Expects the current token to exist.
 *
 * @param tok_i the index of the expected token.
 * @param errorMsg the error message to throw if the token does not exist.
 */
bool Linker::expect_token (size_t tok_i, const std::string &errorMsg)
{
    EXPECT_TRUE_SS (in_bounds (tok_i), std::stringstream (errorMsg));
    return true;
}

bool Linker::expect_token (size_t tok_i, const std::set<Token::Type> &expectedTypes,
                           const std::string &errorMsg)
{
    EXPECT_TRUE_SS (in_bounds (tok_i), std::stringstream (errorMsg));
    EXPECT_TRUE_SS (expectedTypes.find (m_tokens[tok_i].type) != expectedTypes.end (),
                    std::stringstream (errorMsg));
    return true;
}

/**
 * Returns whether the current token matches the given types.
 *
 * @param tok_i the index of the current token.
 * @param tokenTypes the types to match.
 *
 * @return true if the current token matches the given types.
 */
bool Linker::is_token (size_t tok_i, const std::set<Token::Type> &tokenTypes,
                       const std::string &errorMsg)
{
    expect_token (tok_i, errorMsg);
    return tokenTypes.find (m_tokens[tok_i].type) != tokenTypes.end ();
}

/**
 * Returns whether the current token index is within the bounds of the tokens list.
 *
 * @param tok_i the index of the current token
 *
 * @return true if the token index is within the bounds of the tokens list.
 */
bool Linker::in_bounds (size_t tok_i)
{
    return tok_i < m_tokens.size ();
}

/**
 * Consumes the current token.
 *
 * @param tok_i the index of the current token.
 * @param errorMsg the error message to throw if the token does not exist.
 *
 * @returns the value of the consumed token.
 */
Linker::Token &Linker::consume (size_t &tok_i, const std::string &errorMsg)
{
    expect_token (tok_i, errorMsg);
    return m_tokens[tok_i++];
}

/**
 * Consumes the current token and checks it matches the given types.
 *
 * @param tok_i the index of the current token.
 * @param expectedTypes the expected types of the token.
 * @param errorMsg the error message to throw if the token does not have the expected type.
 *
 * @returns the value of the consumed token.
 */
Linker::Token &Linker::consume (size_t &tok_i, const std::set<Linker::Token::Type> &expectedTypes,
                                const std::string &errorMsg)
{
    expect_token (tok_i, errorMsg);
    EXPECT_TRUE_SS (expectedTypes.find (m_tokens[tok_i].type) != expectedTypes.end (),
                    std::stringstream () << errorMsg << " - Unexpected end of file.");
    return m_tokens.at (tok_i++);
}