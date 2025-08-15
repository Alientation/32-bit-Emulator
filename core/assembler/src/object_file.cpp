#include "assembler/object_file.h"
#include "assembler/build.h"
#include "emulator32bit/emulator32bit.h"
#include "util/logger.h"
#include "util/types.h"

#include <fstream>

ObjectFile::ObjectFile()
{
    this->m_state = State::NO_STATE;
}

ObjectFile::ObjectFile(File obj_file) : ObjectFile()
{
    read_object_file(obj_file);
}

void ObjectFile::read_object_file(std::vector<byte>& bytes)
{
    // m_obj_file will not be set
    // this is way for static libraries to be decomposed into a list of object files easily
    disassemble(bytes);

    print();
}

void ObjectFile::read_object_file(File obj_file)
{
    m_obj_file = obj_file;

    FileReader file_reader(m_obj_file, std::ios::in | std::ios::binary);

    DEBUG("ObjectFile::read_object_file() - Reading bytes");
    std::vector<byte> bytes;
    while (file_reader.has_next_byte())
    {
        bytes.push_back(file_reader.read_byte());
    }

    disassemble(bytes);

    /* since errors in disassemble will early return before setting state to success, check for early return */
    if (m_state == State::DISASSEMBLING)
    {
        m_state = State::DISASSEMBLED_ERROR;
    }

    print();
}

void ObjectFile::disassemble(std::vector<byte>& bytes)
{
    DEBUG("ObjectFile::disassemble() - Disassembling");
    m_state = State::DISASSEMBLING;
    ByteReader reader(bytes);

    /* BELF Header */
    DEBUG("ObjectFile::disassemble() - Reading BELF Header");
    byte expected[4] = {'B','E','L','F'};                            /* 0-3 */
    for (unsigned long long i = 0; i < sizeof(expected)/sizeof(expected[0]); i++)
    {
        if (expected[i] != reader.read_byte())
        {
            // todo logger
            return;
        }
    }
    reader.skip_bytes(12);                                            /* 4-15 */
    file_type = reader.read_hword();                                /* 16-17 */
    target_machine = reader.read_hword();                            /* 18-19 */
    flags = reader.read_hword();                                    /* 20-21 */
    n_sections = reader.read_hword();                                /* 22-23 */

    DEBUG("ObjectFile::disassemble() - Belf Header = (filetype=%hu, target_machine=%hu, flags=%hu, n_sections=%hu)",
        file_type, target_machine, flags, n_sections);

    /* Section headers */
    DEBUG("ObjectFile::disassemble() - Reading section headers");
    ByteReader section_headers_reader(bytes);
    ByteReader section_headers_start_reader(bytes);
    section_headers_start_reader.skip_bytes(bytes.size() - 8);
    dword section_header_start = section_headers_start_reader.read_dword();
    DEBUG("ObjectFile::disassemble() - Section Header Start = %llu", section_header_start);
    section_headers_reader.skip_bytes(section_header_start);
    for (int i = 0; i < n_sections; i++)
    {
        SectionHeader section_header =
        {
            .section_name = int(section_headers_reader.read_dword()),
            .type = (SectionHeader::Type) section_headers_reader.read_word(),

            /* TODO: Why are we reading in dwords and then immediatelly casting to words? */
            .section_start = word(section_headers_reader.read_dword()),
            .section_size = word(section_headers_reader.read_dword()),
            .entry_size = word(section_headers_reader.read_dword()),

            /* TODO: Maybe should store a single bit? instead of wasting a whole byte for this. */
            .load_at_physical_address = (bool) section_headers_reader.read_byte(),
            .address = word(section_headers_reader.read_dword()),
        };

        sections.push_back(section_header);

        DEBUG("ObjectFile::disassemble() - Reading section %d (name = %d, type=%d, section_start=%u, section_size=%u, entry_size=%u)",
                i, section_header.section_name, int(section_header.type), section_header.section_start,
                section_header.section_size, section_header.entry_size);
    }

    /* Sections */
    DEBUG("ObjectFile::disassemble() - Reading %hu sections.", n_sections);
    for (hword section_i = 0; section_i < n_sections; section_i++)
    {
        SectionHeader &section_header = sections[section_i];
        switch(section_header.type)
        {
            case SectionHeader::Type::TEXT:
                DEBUG("ObjectFile::disassemble() - Disassembling Text Section");
                for (word i = 0; i < section_header.section_size; i+=4)
                {
                    text_section.push_back(reader.read_word(false));
                }
                break;
            case SectionHeader::Type::DATA:
                DEBUG("ObjectFile::disassemble() - Disassembling Data Section");
                for (word i = 0; i < section_header.section_size; i++)
                {
                    data_section.push_back(reader.read_byte());
                }
                break;
            case SectionHeader::Type::BSS:
                DEBUG("ObjectFile::disassemble() - Disassembling BSS Section");
                bss_section = reader.read_dword();
                break;
            case SectionHeader::Type::SYMTAB:
                DEBUG("ObjectFile::disassemble() - Disassembling Symbol Table Section");
                for (word i = 0; i < section_header.section_size; i += SYMBOL_TABLE_ENTRY_SIZE)
                {
                    SymbolTableEntry symbol =
                    {
                        .symbol_name = int(reader.read_dword()),
                        .symbol_value = word(reader.read_dword()),
                        .binding_info = (SymbolTableEntry::BindingInfo) reader.read_hword(),
                        .section = int(reader.read_dword()),
                    };

                    symbol_table[symbol.symbol_name] = symbol;
                    DEBUG("ObjectFile::disassemble() - Symbol entry = (symbol_name=%d, symbol_value=%d, binding_info=%d, section=%d)",
                            symbol.symbol_name, symbol.symbol_value, int(symbol.binding_info), symbol.section);
                }
                break;
            case SectionHeader::Type::REL_TEXT:
                DEBUG("ObjectFile::disassemble() - Disassembling Rel.Text Section");
                for (word i = 0; i < section_header.section_size; i+=RELOCATION_ENTRY_SIZE)
                {
                    RelocationEntry rel =
                    {
                        .offset = word(reader.read_dword()),
                        .symbol = int(reader.read_dword()),
                        .type = (RelocationEntry::Type) reader.read_word(),
                        .shift = word(reader.read_dword()),
                        .token = 0,
                    };
                    rel_text.push_back(rel);
                }
                break;
            case SectionHeader::Type::REL_DATA:
                DEBUG("ObjectFile::disassemble() - Disassembling Rel.Data Section");
                for (word i = 0; i < section_header.section_size; i+=RELOCATION_ENTRY_SIZE)
                {
                    RelocationEntry rel =
                    {
                        .offset = word(reader.read_dword()),
                        .symbol = int(reader.read_dword()),
                        .type = (RelocationEntry::Type) reader.read_word(),
                        .shift = word(reader.read_dword()),
                        .token = 0,
                    };
                    rel_data.push_back(rel);
                }
                break;
            case SectionHeader::Type::REL_BSS:
                DEBUG("ObjectFile::disassemble() - Disassembling Rel.BSS Section");
                for (word i = 0; i < section_header.section_size; i+=RELOCATION_ENTRY_SIZE)
                {
                    RelocationEntry rel =
                    {
                        .offset = word(reader.read_dword()),
                        .symbol = int(reader.read_dword()),
                        .type = (RelocationEntry::Type) reader.read_word(),
                        .shift = word(reader.read_dword()),
                        .token = 0,
                    };
                    rel_bss.push_back(rel);
                }
                break;
            case SectionHeader::Type::STRTAB: {
                DEBUG("ObjectFile::disassemble() - Disassembling String Table section");
                std::string current_string;
                for (word i = 0; i < section_header.section_size; i++)
                {
                    byte b = reader.read_byte();
                    if (b == '\0')
                    {
                        string_table[current_string] = strings.size();
                        strings.push_back(current_string);
                        current_string = "";
                    }
                    else
                    {
                        current_string += b;
                    }
                }
                break;
            }
            default:
                ERROR("ObjectFile::disassemble() - Invalid Section Type");
                return;
        }
    }

    /* Fill in section table */
    DEBUG("ObjectFile::disassemble() - Filling in Section table");
    for (size_t i = 0; i < sections.size(); i++)
    {
        section_table[strings[sections[i].section_name]] = i;
    }

    m_state = State::DISASSEMBLED_SUCCESS;
    DEBUG("ObjectFile::disassemble() - Finished disassembling");
}

int ObjectFile::add_section(const std::string& section_name, SectionHeader header)
{
    EXPECT_TRUE_SS(section_table.find(section_name) == section_table.end(), std::stringstream()
            << "ObjectFile::add_section() - Section name exists in section table");

    header.section_name = add_string(section_name);
    section_table[section_name] = sections.size();
    sections.push_back(header);
    n_sections++;
    return sections.size() - 1;
}

int ObjectFile::add_string(const std::string& string)
{
    EXPECT_TRUE_SS(string_table.find(string) == string_table.end(), std::stringstream()
            << "ObjectFile::add_string() - String name exists in string table");

    string_table[string] = string_table.size();
    strings.push_back(string);
    return strings.size()-1;
}

std::string ObjectFile::get_symbol_name(int symbol)
{
    return strings[symbol_table[symbol].symbol_name];
}

/**
 * @brief                     Adds a symbol to the symbol table
 *
 * @param symbol             symbol string
 * @param value             value of the symbol if it is defined
 * @param binding_info         visiblity of the symbol
 * @param section             section it is defined in. -1 if not defined in a section
 */
void ObjectFile::add_symbol(const std::string& symbol, word value, SymbolTableEntry::BindingInfo binding_info, int section)
{
    if (string_table.find(symbol) == string_table.end())
    {                /* If symbol does not exist yet, create it */
        string_table[symbol] = strings.size();
        strings.push_back(symbol);
        symbol_table[string_table[symbol]] = (SymbolTableEntry)
        {
            .symbol_name = string_table[symbol],
            .symbol_value = value,
            .binding_info = binding_info,
            .section = section,
        };
    }
    else
    {
        SymbolTableEntry &symbol_entry = symbol_table[string_table[symbol]];
        if (symbol_entry.section == -1 && section != -1)
        {
            symbol_entry.section = section;
            symbol_entry.symbol_value = value;
        } else if (symbol_entry.section != -1 && section != -1)
        {
            ERROR("ObjectFile::add_symbol() - Multiple definition of symbol %s at sections %s and %s",
                    symbol.c_str(), strings[sections[section].section_name].c_str(),
                    strings[sections[symbol_entry.section].section_name].c_str());
            return;
        }

        if (binding_info == SymbolTableEntry::BindingInfo::GLOBAL
                || (binding_info == SymbolTableEntry::BindingInfo::LOCAL &&
                symbol_entry.binding_info == SymbolTableEntry::BindingInfo::WEAK))
        {
            symbol_entry.binding_info = binding_info;
        }
    }
}

void ObjectFile::write_object_file(File obj_file)
{
    DEBUG("ObjectFile::write_object_file() - Writing to object file.");
    m_state = State::WRITING;
    m_obj_file = obj_file;

    // clearing object file
    std::ofstream ofs;
    ofs.open(obj_file.get_path(), std::ofstream::out | std::ofstream::trunc);
    ofs.close();

    // create writer for object file
    FileWriter m_writer = FileWriter(obj_file, std::ios::out | std::ios::binary);

    ByteWriter byte_writer(m_writer);
    int current_byte = 0;

    /* BELF Header */
    DEBUG("ObjectFile::write_objectFile() - Writing BELF header.");
    m_writer.write("BELF");                                                /* BELF magic number header */
    byte_writer << ByteWriter::Data(0, 12);                                /* Unused padding */
    byte_writer << ByteWriter::Data(file_type, 2);                        /* Object file type */
    byte_writer << ByteWriter::Data(target_machine, 2);                    /* Target machine */
    byte_writer << ByteWriter::Data(0, 2);                                /* Flags */
    byte_writer << ByteWriter::Data(sections.size(), 2);                /* Number of sections */
    current_byte += BELF_HEADER_SIZE;

    /* Text Section */
    DEBUG("ObjectFile::write_object_file() - Writing .text section.");
    for (size_t i = 0; i < text_section.size(); i++)
    {
        byte_writer << ByteWriter::Data(text_section.at(i), 4, false);
    }
    sections[section_table[".text"]].section_size = text_section.size() * 4;
    sections[section_table[".text"]].section_start = current_byte;
    current_byte += text_section.size() * 4;

    /* Data Section */
    DEBUG("ObjectFile::write_object_file() - Writing .data section.");
    for (size_t i = 0; i < data_section.size(); i++)
    {
        byte_writer << ByteWriter::Data(data_section.at(i), 1);
    }
    sections[section_table[".data"]].section_size = data_section.size();
    sections[section_table[".data"]].section_start = current_byte;
    current_byte += data_section.size();

    /* BSS Section */
    DEBUG("ObjectFile::write_object_file() - Writing .bss section. Size %u bytes.", bss_section);
    byte_writer << ByteWriter::Data(bss_section, BSS_SECTION_SIZE);
    sections[section_table[".bss"]].section_size = bss_section;
    sections[section_table[".bss"]].section_start = current_byte;
    current_byte += BSS_SECTION_SIZE;

    /* Symbol Table */
    DEBUG("ObjectFile::write_object_file() - Writing .symtab section.");
    for (std::pair<int, SymbolTableEntry> symbol : symbol_table)
    {
        byte_writer << ByteWriter::Data(symbol.second.symbol_name, 8);
        byte_writer << ByteWriter::Data(symbol.second.symbol_value, 8);
        byte_writer << ByteWriter::Data(S16(symbol.second.binding_info), 2);
        byte_writer << ByteWriter::Data(symbol.second.section, 8);

        DEBUG("ObjectFile::write_object_file() - symbol %s = %u (%d)[%d]",
                strings[symbol.second.symbol_name].c_str(), symbol.second.symbol_value,
                int(symbol.second.binding_info), symbol.second.section);
    }
    sections[section_table[".symtab"]].section_size = symbol_table.size() * SYMBOL_TABLE_ENTRY_SIZE;
    sections[section_table[".symtab"]].section_start = current_byte;
    current_byte += symbol_table.size() * SYMBOL_TABLE_ENTRY_SIZE;

    /* rel.text Section */
    DEBUG("ObjectFile::write_object_file() - Writing .rel.text section.");
    for (size_t i = 0; i < rel_text.size(); i++)
    {
        byte_writer << ByteWriter::Data(rel_text[i].offset, 8);
        byte_writer << ByteWriter::Data(rel_text[i].symbol, 8);
        byte_writer << ByteWriter::Data(int(rel_text[i].type), 4);
        byte_writer << ByteWriter::Data(rel_text[i].shift, 8);
    }
    sections[section_table[".rel.text"]].section_size = rel_text.size() * RELOCATION_ENTRY_SIZE;
    sections[section_table[".rel.text"]].section_start = current_byte;
    current_byte += rel_text.size() * RELOCATION_ENTRY_SIZE;

    /* rel.data Section */
    DEBUG("ObjectFile::write_object_file() - Writing .rel.data section.");
    for (size_t i = 0; i < rel_data.size(); i++)
    {
        byte_writer << ByteWriter::Data(rel_data[i].offset, 8);
        byte_writer << ByteWriter::Data(rel_data[i].symbol, 8);
        byte_writer << ByteWriter::Data(int(rel_data[i].type), 4);
        byte_writer << ByteWriter::Data(rel_data[i].shift, 8);
    }
    sections[section_table[".rel.data"]].section_size = rel_data.size() * RELOCATION_ENTRY_SIZE;
    sections[section_table[".rel.data"]].section_start = current_byte;
    current_byte += rel_data.size() * RELOCATION_ENTRY_SIZE;

    /* rel.bss Section */
    DEBUG("ObjectFile::write_object_file() - Writing .rel.bss section.");
    for (size_t i = 0; i < rel_bss.size(); i++)
    {
        byte_writer << ByteWriter::Data(rel_bss[i].offset, 8);
        byte_writer << ByteWriter::Data(rel_bss[i].symbol, 8);
        byte_writer << ByteWriter::Data(int(rel_bss[i].type), 4);
        byte_writer << ByteWriter::Data(rel_bss[i].shift, 8);
    }
    sections[section_table[".rel.bss"]].section_size = rel_bss.size() * RELOCATION_ENTRY_SIZE;
    sections[section_table[".rel.bss"]].section_start = current_byte;
    current_byte += rel_bss.size() * RELOCATION_ENTRY_SIZE;

    /* String Table */
    DEBUG("ObjectFile::write_object_file() - Writing .strtab section.");
    int size = 0;
    for (size_t i = 0; i < strings.size(); i++)
    {
        m_writer.write(strings[i]);
        byte_writer << ByteWriter::Data(0, 1);                            /* Null terminated string */
        size += strings[i].size() + 1;
    }
    sections[section_table[".strtab"]].section_size = size;
    sections[section_table[".strtab"]].section_start = current_byte;
    current_byte += size;

    /* Section headers */
    DEBUG("ObjectFile::write_object_file() - Writing Section headers.");
    for (size_t i = 0; i < sections.size(); i++)
    {
        byte_writer << ByteWriter::Data(sections[i].section_name, 8);
        byte_writer << ByteWriter::Data(int(sections[i].type), 4);
        byte_writer << ByteWriter::Data(sections[i].section_start, 8);
        byte_writer << ByteWriter::Data(sections[i].section_size, 8);
        byte_writer << ByteWriter::Data(sections[i].entry_size, 8);

        byte_writer << ByteWriter::Data(sections[i].load_at_physical_address, 1);
        byte_writer << ByteWriter::Data(sections[i].address, 8);
    }
    /* For easy access */
    byte_writer << ByteWriter::Data(current_byte, 8);
    current_byte += 8;
    current_byte += sections.size() * SECTION_HEADER_SIZE;

    m_writer.close();

    m_state = State::WRITING_SUCCESS;

    print();
}

void ObjectFile::print()
{
    /* Print object file */
    DEBUG("ObjectFile::print() - Printing object file.");

    /* Don't print object files that could not be disassembled */
    if (m_state != State::DISASSEMBLED_SUCCESS)
    {
        printf("ERROR: Cannot print object file. Disassembly failed.");
        return;
    }

    printf("%s:\tfile format %s\n\n", (m_obj_file.get_name() + "." + OBJECT_EXTENSION).c_str(), "belf32-littleemu32");
    printf("SYMBOL TABLE:\n");
    for (std::pair<int, SymbolTableEntry> symbol : symbol_table)
    {
        char visibility = ' ';
        if (symbol.second.binding_info == SymbolTableEntry::BindingInfo::GLOBAL)
        {
            visibility = 'g';
        }
        else if (symbol.second.binding_info == SymbolTableEntry::BindingInfo::LOCAL)
        {
            visibility = 'l';
        }

        std::string section_name = "*UND*";
        if (symbol.second.section != -1)
        {
            section_name = strings[sections[symbol.second.section].section_name];
        }

        std::cout << color_val_str(to_hex_str(dword(symbol.second.symbol_value)))
                << " " << visibility << "\t " << section_name << "\t\t " <<
                color_val_str(to_hex_str(dword(0))) << " " << strings[symbol.second.symbol_name]
                << "\n";
    }

    printf("\nContents of section .data:");
    int data_address_width = std::__bit_width(data_section.size() / 16);
    if (data_address_width < 4)
    {
        data_address_width = 4;
    }
    std::string data_address_format = "\n%." + std::to_string(data_address_width) + "hx ";
    for (size_t i = 0; i < data_section.size(); i++)
    {
        if (i % 16 == 0)
        {
            printf(data_address_format.c_str(), i);
        }
        if (i % 4 == 0 && i % 16 != 0)
        {
            printf(" ");
        }
        printf("%.2hhx", data_section[i]);
    }

    printf("\n\nDisassembly of section .text:\n");
    std::unordered_map<int, int> label_map;
    for (std::pair<int, SymbolTableEntry> symbol : symbol_table)
    {
        if (sections[symbol.second.section].type != SectionHeader::Type::TEXT ||
                strings[symbol.second.symbol_name].find("::SCOPE") != std::string::npos)
        {
            continue;
        }

        label_map[symbol.second.symbol_value] = symbol.second.symbol_name;
    }

    std::unordered_map<int, RelocationEntry> rel_text_map;
    for (size_t i = 0; i < rel_text.size(); i++)
    {
        rel_text_map[rel_text[i].offset] = rel_text[i];
    }

    if (label_map.find(0) == label_map.end())
    {
        std::cout << color_val_str(to_hex_str(dword(0))) << ":";
    }

    int text_address_width = std::__bit_width(text_section.size() / 4);
    if (text_address_width < 4)
    {
        text_address_width = 4;
    }
    std::string text_address_format = "\n%" + std::to_string(text_address_width) + "hx";
    std::string relocation_spacing = "\n%" + std::to_string(text_address_width) + "s";
    std::string current_label = "";
    for (size_t i = 0; i < text_section.size(); i++)
    {
        if (label_map.find(i*4) != label_map.end())
        {
            if (i != 0)
            {
                printf("\n\n");
            }
            current_label = strings[label_map[i*4]];
            std::cout << color_val_str(to_hex_str(dword(i*4))) << " <" << current_label << ">:";
        }
        std::string disassembly = Emulator32bit::disassemble_instr(text_section[i]);
        printf(text_address_format.c_str(), i*4);

        if (disassembly.find_first_of(' ') != std::string::npos)
        {
            std::string op = disassembly.substr(0, disassembly.find_first_of(' '));
            std::string operands = disassembly.substr(disassembly.find_first_of(' ') + 1);
            printf(":\t%.8x\t%.12s\t\t%s", text_section[i], op.c_str(), operands.c_str());
            switch (bitfield_unsigned(text_section[i], 26, 6))
            {
                case Emulator32bit::_op_b:
                case Emulator32bit::_op_bl:
                    sword offset = bitfield_signed(text_section[i], 0, 22) * 4;
                    if (offset < 0)
                    {
                        printf(" <%s-0x%hx>", current_label.c_str(), -offset);
                    }
                    else
                    {
                        printf(" <%s+0x%hx>", current_label.c_str(), offset);
                    }
            }
        }
        else
        {
            printf(":\t%.8x\t%.12s", text_section[i], disassembly.c_str());
        }

        /* Check if there is a relocation record here */
        if (rel_text_map.find(i*4) != rel_text_map.end())
        {
            printf(relocation_spacing.c_str(), "");

            RelocationEntry entry = rel_text_map[i*4];
            std::string print_str = "";
            switch (entry.type)
            {
                case RelocationEntry::Type::R_EMU32_O_LO12:
                    print_str = "R_EMU32_O_LO12";
                    break;
                case RelocationEntry::Type::R_EMU32_ADRP_HI20:
                    print_str = "R_EMU32_ADRP_HI20";
                    break;
                case RelocationEntry::Type::R_EMU32_MOV_LO19:
                    print_str = "R_EMU32_MOV_LO19";
                    break;
                case RelocationEntry::Type::R_EMU32_MOV_HI13:
                    print_str = "R_EMU32_MOV_HI13";
                    break;
                case RelocationEntry::Type::R_EMU32_B_OFFSET22:
                    print_str = "R_EMU32_B_OFFSET22";
                    break;
                case RelocationEntry::Type::UNDEFINED:
                    print_str = "<ERROR>";
                    break;
            }

            int print_str_width = 29 - std::__bit_width(i / 4);
            printf((" \t%hx: %-" + std::to_string(print_str_width) + "s").c_str(), dword(i*4), print_str.c_str());
            printf("%s", strings[symbol_table[entry.symbol].symbol_name].c_str());
        }
    }
    printf("\n");
}
