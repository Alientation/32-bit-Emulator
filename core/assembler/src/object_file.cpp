#include "assembler/object_file.h"

#include "assembler/build.h"
#include "emulator32bit/emulator32bit.h"
#include "util/logger.h"
#include "util/types.h"

#include <fstream>

ObjectFile::ObjectFile() = default;

const std::array<ObjectFile::ByteSection, 4> &ObjectFile::byte_sections()
{
    using Type = SectionHeader::Type;
    static const std::array<ByteSection, 4> sections = {{
        {".data", ".rel.data", Type::DATA, Type::REL_DATA, &ObjectFile::data_section,
         &ObjectFile::rel_data, true, 1},
        {".rodata", ".rel.rodata", Type::RODATA, Type::REL_RODATA, &ObjectFile::rodata_section,
         &ObjectFile::rel_rodata, false, 1},
        {".init_array", ".rel.init_array", Type::INIT_ARRAY, Type::REL_INIT_ARRAY,
         &ObjectFile::init_array_section, &ObjectFile::rel_init_array, false, 4},
        {".fini_array", ".rel.fini_array", Type::FINI_ARRAY, Type::REL_FINI_ARRAY,
         &ObjectFile::fini_array_section, &ObjectFile::rel_fini_array, false, 4},
    }};
    return sections;
}

const ObjectFile::ByteSection *ObjectFile::byte_section_of(SectionHeader::Type type)
{
    for (const ByteSection &section : byte_sections())
    {
        if (section.type == type || section.rel_type == type)
        {
            return &section;
        }
    }
    return nullptr;
}

ObjectFile::SectionHeader::Type ObjectFile::user_section_type(const bool writable,
                                                              const bool executable,
                                                              const bool nobits)
{
    using Type = SectionHeader::Type;
    if (nobits) return Type::USER_BSS;
    return executable ? Type::USER_RX : writable ? Type::USER_RW : Type::USER_R;
}

bool ObjectFile::is_user_section_type(const SectionHeader::Type type)
{
    using Type = SectionHeader::Type;
    return type == Type::USER_R || type == Type::USER_RW || type == Type::USER_RX
           || type == Type::USER_BSS;
}

ObjectFile::UserSection *ObjectFile::find_user_section(const std::string &name)
{
    for (UserSection &section : user_sections)
    {
        if (section.name == name) return &section;
    }
    return nullptr;
}

const ObjectFile::UserSection *ObjectFile::find_user_section(const std::string &name) const
{
    for (const UserSection &section : user_sections)
    {
        if (section.name == name) return &section;
    }
    return nullptr;
}

ObjectFile::UserSection *ObjectFile::user_section_at(const U32 header_index)
{
    for (UserSection &section : user_sections)
    {
        if (section.header_index == header_index) return &section;
    }
    return nullptr;
}

U32 ObjectFile::add_user_section(const std::string &name, const bool writable,
                                 const bool executable, const bool nobits)
{
    AEMU_CHECK(!(writable && executable),
               "The section {} would be both writable and "
               "executable.",
               name);
    AEMU_CHECK(!nobits || (writable && !executable),
               "The section {} is nobits, which is writable and "
               "not executable.",
               name);

    const U32 header = add_section(name, user_section_type(writable, executable, nobits));
    add_section(".rel" + name, SectionHeader::Type::REL_USER);

    UserSection section;
    section.name = name;
    section.writable = writable;
    section.executable = executable;
    section.nobits = nobits;
    section.header_index = header;
    user_sections.push_back(std::move(section));

    // Code is made of words.
    if (executable) sections[header].alignment = kTextEntrySize;
    return header;
}

ObjectFile::ObjectFile(File obj_file) :
    ObjectFile()
{
    read_object_file(obj_file);
}

void ObjectFile::read_object_file(std::vector<byte> &bytes)
{
    // m_obj_file will not be set
    // this is way for static libraries to be decomposed into a list of object files easily
    deserialize_checked(bytes);
}

void ObjectFile::read_object_file(File obj_file)
{
    m_obj_file = obj_file;

    FileReader file_reader(m_obj_file, std::ios::in | std::ios::binary);

    AEMU_DEBUG("Reading bytes");
    std::vector<byte> bytes;
    while (file_reader.has_next_byte())
    {
        bytes.push_back(file_reader.read_byte());
    }

    deserialize_checked(bytes);
}

void ObjectFile::deserialize_checked(std::vector<byte> &bytes)
{
    try
    {
        deserialize(bytes);
    }
    catch (const std::out_of_range &e)
    {
        // ByteReader and the section/string lookups are bounds checked.
        AEMU_FATAL("'{}' is truncated or corrupt: {}",
                   m_obj_file.get_path(), e.what());
    }
}

void ObjectFile::deserialize(std::vector<byte> &bytes)
{
    AEMU_DEBUG("Disassembling");
    ByteReader reader(bytes);

    // BELF Header
    AEMU_DEBUG("Reading BELF Header");
    if (bytes.size() < kBELFHeaderSize + 8)
    {
        AEMU_FATAL("'{}' is too small ({} bytes) to be an object file.",
                   m_obj_file.get_path(), bytes.size());
        return;
    }

    byte expected[4] = {'B', 'E', 'L', 'F'}; // 0-3
    for (unsigned long long i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
    {
        if (expected[i] != reader.read_byte())
        {
            AEMU_FATAL("'{}' is not an object file, bad magic number.",
                       m_obj_file.get_path());
            return;
        }
    }
    reader.skip_bytes(12); // 4-15
    file_type = reader.read_hword(); // 16-17
    target_machine = reader.read_hword(); // 18-19
    flags = reader.read_hword(); // 20-21
    n_sections = reader.read_hword(); // 22-23

    AEMU_DEBUG("Belf Header = (filetype={}, target_machine={}, "
               "flags={}, n_sections={})",
               file_type, target_machine, flags, n_sections);

    // Section headers
    AEMU_DEBUG("Reading section headers");
    ByteReader section_headers_reader(bytes);
    ByteReader section_headers_start_reader(bytes);
    section_headers_start_reader.skip_bytes(bytes.size() - 8);
    dword section_header_start = section_headers_start_reader.read_dword();
    AEMU_DEBUG("Section Header Start = {}", section_header_start);
    if (section_header_start >= bytes.size())
    {
        AEMU_FATAL("'{}' is corrupt, the section headers start at {} "
                   "but the file has {} bytes.",
                   m_obj_file.get_path(), section_header_start, bytes.size());
        return;
    }
    section_headers_reader.skip_bytes(section_header_start);
    for (int i = 0; i < n_sections; i++)
    {
        SectionHeader section_header = {
            .section_name = U32(section_headers_reader.read_dword()),
            .type = (SectionHeader::Type) section_headers_reader.read_word(),

            // TODO: Why are we reading in dwords and then immediatelly casting to words?
            .section_start = word(section_headers_reader.read_dword()),
            .section_size = word(section_headers_reader.read_dword()),
            .entry_size = word(section_headers_reader.read_dword()),

            // TODO: Maybe should store a single bit? instead of wasting a whole byte for this.
            .load_at_physical_address = (bool) section_headers_reader.read_byte(),
            .address = word(section_headers_reader.read_dword()),
            .alignment = word(section_headers_reader.read_dword()),
        };

        sections.push_back(section_header);

        AEMU_DEBUG("Reading section {} (name = {}, type={}, "
                   "section_start={}, section_size={}, entry_size={})",
                   i, section_header.section_name, U32(section_header.type),
                   section_header.section_start, section_header.section_size,
                   section_header.entry_size);
    }

    // Sections. Each is read from where its header says it is.
    AEMU_DEBUG("Reading {} sections.", n_sections);
    for (hword section_i = 0; section_i < n_sections; section_i++)
    {
        SectionHeader &section_header = sections[section_i];
        // .bss and the sections like it have their size in the header and 8 bytes in the file.
        if (section_header.type != SectionHeader::Type::BSS
            && section_header.type != SectionHeader::Type::USER_BSS
            && U64(section_header.section_start) + section_header.section_size > bytes.size())
        {
            AEMU_FATAL("'{}' is corrupt, section {} is at {} with "
                       "{} bytes but the file has {} bytes.",
                       m_obj_file.get_path(), section_i, section_header.section_start,
                       section_header.section_size, bytes.size());
            return;
        }

        ByteReader reader(bytes);
        reader.skip_bytes(section_header.section_start);

        switch (section_header.type)
        {
        case SectionHeader::Type::TEXT:
            AEMU_DEBUG("Disassembling Text Section");
            for (word i = 0; i < section_header.section_size; i += 4)
            {
                text_section.push_back(reader.read_word());
            }
            break;
        case SectionHeader::Type::DATA:
        case SectionHeader::Type::RODATA:
        case SectionHeader::Type::INIT_ARRAY:
        case SectionHeader::Type::FINI_ARRAY:
        {
            AEMU_DEBUG("Disassembling a byte section");
            std::vector<byte> &bytes_of_section =
                this->*byte_section_of(section_header.type)->bytes;
            for (word i = 0; i < section_header.section_size; i++)
            {
                bytes_of_section.push_back(reader.read_byte());
            }
            break;
        }
        case SectionHeader::Type::USER_R:
        case SectionHeader::Type::USER_RW:
        case SectionHeader::Type::USER_RX:
        {
            AEMU_DEBUG("Reading a user section");
            UserSection user;
            user.writable = section_header.type == SectionHeader::Type::USER_RW;
            user.executable = section_header.type == SectionHeader::Type::USER_RX;
            user.header_index = section_i;
            for (word i = 0; i < section_header.section_size; i++)
            {
                user.bytes.push_back(reader.read_byte());
            }
            user_sections.push_back(std::move(user));
            break;
        }
        case SectionHeader::Type::USER_BSS:
        {
            AEMU_DEBUG("Reading a nobits user section");
            UserSection user;
            user.writable = true;
            user.nobits = true;
            user.header_index = section_i;
            user.zero_size = word(reader.read_dword());
            user_sections.push_back(std::move(user));
            break;
        }
        case SectionHeader::Type::REL_USER:
        {
            UserSection *user = section_i == 0 ? nullptr : user_section_at(section_i - 1);
            if (user == nullptr)
            {
                AEMU_FATAL("'{}' is corrupt, the relocation section {} "
                           "does not follow the section it is for.",
                           m_obj_file.get_path(), section_i);
                return;
            }
            for (word i = 0; i < section_header.section_size; i += kRelocationEntrySize)
            {
                user->relocations.push_back({
                    .offset = word(reader.read_dword()),
                    .symbol = U32(reader.read_dword()),
                    .type = (RelocationEntry::Type) reader.read_word(),
                    .addend = sword(word(reader.read_dword())),
                    .token = 0,
                });
            }
            break;
        }
        case SectionHeader::Type::BSS:
            AEMU_DEBUG("Disassembling BSS Section");
            bss_section = reader.read_dword();
            break;
        case SectionHeader::Type::SYMTAB:
            AEMU_DEBUG("Disassembling Symbol Table Section");
            for (word i = 0; i < section_header.section_size; i += kSymbolTableEntrySize)
            {
                SymbolTableEntry symbol = {
                    .symbol_name = U32(reader.read_dword()),
                    .symbol_value = word(reader.read_dword()),
                    .binding_info = (SymbolTableEntry::BindingInfo) reader.read_hword(),
                    .section = U32(reader.read_dword()),
                };

                symbol_table[symbol.symbol_name] = symbol;
                AEMU_DEBUG("Symbol entry = (symbol_name={}, "
                           "symbol_value={}, binding_info={}, section={})",
                           symbol.symbol_name, symbol.symbol_value, U32(symbol.binding_info),
                           symbol.section);
            }
            break;
        case SectionHeader::Type::REL_TEXT:
        case SectionHeader::Type::REL_DATA:
        case SectionHeader::Type::REL_BSS:
        case SectionHeader::Type::REL_RODATA:
        case SectionHeader::Type::REL_INIT_ARRAY:
        case SectionHeader::Type::REL_FINI_ARRAY:
        {
            AEMU_DEBUG("Disassembling a relocation section");
            const ByteSection *of_bytes = byte_section_of(section_header.type);
            std::vector<RelocationEntry> &relocations =
                section_header.type == SectionHeader::Type::REL_TEXT ? rel_text
                : of_bytes != nullptr                                ? this->*of_bytes->relocations
                                                                     : rel_bss;
            for (word i = 0; i < section_header.section_size; i += kRelocationEntrySize)
            {
                RelocationEntry rel = {
                    .offset = word(reader.read_dword()),
                    .symbol = U32(reader.read_dword()),
                    .type = (RelocationEntry::Type) reader.read_word(),
                    .addend = sword(word(reader.read_dword())),
                    .token = 0,
                };
                relocations.push_back(rel);
            }
            break;
        }
        case SectionHeader::Type::STRTAB:
        {
            AEMU_DEBUG("Disassembling String Table section");
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
            AEMU_FATAL("Invalid Section Type");
            return;
        }
    }

    // Fill in section table
    AEMU_DEBUG("Filling in Section table");
    for (size_t i = 0; i < sections.size(); i++)
    {
        if (sections[i].section_name >= strings.size())
        {
            AEMU_FATAL("'{}' is corrupt, section {} has the invalid "
                       "name index {}.",
                       m_obj_file.get_path(), i, sections[i].section_name);
            return;
        }
        section_table[strings[sections[i].section_name]] = i;
    }
    for (UserSection &user : user_sections)
    {
        user.name = strings[sections[user.header_index].section_name];
    }

    validate();

    AEMU_DEBUG("Finished disassembling");
}

void ObjectFile::validate() const
{
    const std::string &path = m_obj_file.get_path();

    // The sections that everything else is looked up in.
    const auto require = [&](const char *name, SectionHeader::Type type)
    {
        const auto section = section_table.find(name);
        AEMU_CHECK(
            section != section_table.end() && sections[section->second].type == type,
            "'{}' is not an object file that can be used, it has "
            "no {} section.",
            path, name);
    };
    require(".text", SectionHeader::Type::TEXT);
    for (const ByteSection &section : byte_sections())
    {
        require(section.name, section.type);
    }
    require(".bss", SectionHeader::Type::BSS);
    require(".symtab", SectionHeader::Type::SYMTAB);
    require(".strtab", SectionHeader::Type::STRTAB);

    for (const auto &[key, symbol] : symbol_table)
    {
        AEMU_CHECK(symbol.symbol_name == key && symbol.symbol_name < strings.size(),
                   "'{}' is corrupt, a symbol has the invalid name "
                   "index {}.",
                   path, symbol.symbol_name);
        AEMU_CHECK(symbol.section == U32(-1) || symbol.section < sections.size(),
                   "'{}' is corrupt, the symbol '{}' is in the invalid "
                   "section {}.",
                   path, strings[symbol.symbol_name], symbol.section);
        AEMU_CHECK(symbol.binding_info == SymbolTableEntry::BindingInfo::LOCAL
                       || symbol.binding_info == SymbolTableEntry::BindingInfo::GLOBAL
                       || symbol.binding_info == SymbolTableEntry::BindingInfo::WEAK
                       || symbol.binding_info == SymbolTableEntry::BindingInfo::WEAK_DECLARED,
                   "'{}' is corrupt, the symbol '{}' has the invalid "
                   "binding {}.",
                   path, strings[symbol.symbol_name], U32(symbol.binding_info));
    }

    // Where the relocation is, in the section that it is for.
    const auto check_relocations = [&](const std::vector<RelocationEntry> &relocations,
                                       const char *section, size_t size, size_t width)
    {
        for (const RelocationEntry &rel : relocations)
        {
            AEMU_CHECK(symbol_table.count(rel.symbol) != 0,
                       "'{}' is corrupt, a relocation of {} is for the "
                       "symbol {}, which does not exist.",
                       path, section, rel.symbol);
            AEMU_CHECK(rel.type > RelocationEntry::Type::UNDEFINED
                           && rel.type <= RelocationEntry::Type::R_EMU32_ADR_PCREL21,
                       "'{}' is corrupt, a relocation of {} has the "
                       "invalid type {}.",
                       path, section, U32(rel.type));
            AEMU_CHECK(U64(rel.offset) + width <= size,
                       "'{}' is corrupt, a relocation of {} is at {}, "
                       "outside of the section.",
                       path, section, rel.offset);
        }
    };
    check_relocations(rel_text, ".text", text_section.size() * 4, 4);
    for (const ByteSection &section : byte_sections())
    {
        check_relocations(this->*section.relocations, section.name, (this->*section.bytes).size(),
                          4);
    }
    check_relocations(rel_bss, ".bss", 0, 0);
    for (const UserSection &user : user_sections)
    {
        check_relocations(user.relocations, user.name.c_str(), user.bytes.size(), 4);
    }

    for (const RelocationEntry &rel : rel_text)
    {
        AEMU_CHECK(rel.offset % 4 == 0,
                   "'{}' is corrupt, a relocation of .text is at {}, "
                   "which is not the start of an instruction.",
                   path, rel.offset);
    }

    for (const UserSection &user : user_sections)
    {
        AEMU_CHECK(!(user.writable && user.executable),
                   "'{}' is corrupt, the section {} is both writable "
                   "and executable.",
                   path, user.name);
        AEMU_CHECK(!user.executable || user.bytes.size() % 4 == 0,
                   "'{}' is corrupt, the code of {} is not made of "
                   "whole instructions.",
                   path, user.name);
        for (const RelocationEntry &rel : user.relocations)
        {
            AEMU_CHECK(!user.executable || rel.offset % 4 == 0,
                       "'{}' is corrupt, a relocation of {} is at {}, "
                       "which is not the start of an instruction.",
                       path, user.name, rel.offset);
        }
    }
}

U32 ObjectFile::add_section(const std::string &section_name, SectionHeader::Type type)
{
    AEMU_CHECK(section_table.find(section_name) == section_table.end(),
               "Section name exists in section table");

    SectionHeader header = {
        .section_name = U32(-1),
        .type = type,
        .section_start = 0,
        .section_size = 0,
        .entry_size = 0,
    };

    switch (type)
    {
    case SectionHeader::Type::TEXT:
        header.entry_size = kTextEntrySize;
        header.alignment = kTextEntrySize;
        break;
    case SectionHeader::Type::SYMTAB:
        header.entry_size = kSymbolTableEntrySize;
        break;
    case SectionHeader::Type::REL_TEXT:
    case SectionHeader::Type::REL_DATA:
    case SectionHeader::Type::REL_BSS:
    case SectionHeader::Type::REL_RODATA:
    case SectionHeader::Type::REL_INIT_ARRAY:
    case SectionHeader::Type::REL_FINI_ARRAY:
    case SectionHeader::Type::REL_USER:
        header.entry_size = kRelocationEntrySize;
        break;
    case SectionHeader::Type::USER_R:
    case SectionHeader::Type::USER_RW:
    case SectionHeader::Type::USER_RX:
    case SectionHeader::Type::USER_BSS:
        header.entry_size = 0;
        break;
    case SectionHeader::Type::INIT_ARRAY:
    case SectionHeader::Type::FINI_ARRAY:
        header.alignment = byte_section_of(type)->alignment;
        break;
    case SectionHeader::Type::DEBUG:
        AEMU_FATAL("Cannot add section of type DEBUG. Not implemented yet");
        return U32(-1);
        break;
    case SectionHeader::Type::BSS:
    case SectionHeader::Type::DATA:
    case SectionHeader::Type::RODATA:
    case SectionHeader::Type::STRTAB:
        header.entry_size = 0;
        break;
    case SectionHeader::Type::UNDEFINED:
        AEMU_FATAL("Cannot add section of type UNDEFINED");
        return U32(-1);
        break;
    }

    header.section_name = add_string(section_name);
    section_table[section_name] = U32(sections.size());
    sections.push_back(header);
    n_sections++;
    return n_sections - 1;
}

U32 ObjectFile::add_string(const std::string &string)
{
    AEMU_CHECK(string_table.find(string) == string_table.end(),
               "String name exists in string table");

    string_table[string] = U32(string_table.size());
    strings.push_back(string);
    return strings.size() - 1;
}

std::string ObjectFile::get_symbol_name(U32 symbol)
{
    return strings.at(symbol_table.at(symbol).symbol_name);
}

void ObjectFile::add_symbol(const std::string &symbol, word value,
                            SymbolTableEntry::BindingInfo binding_info, U32 section)
{
    // If symbol does not exist yet, create it.
    if (string_table.find(symbol) == string_table.end())
    {
        string_table[symbol] = U32(strings.size());
        strings.push_back(symbol);
        symbol_table[string_table[symbol]] = {
            .symbol_name = string_table[symbol],
            .symbol_value = value,
            .binding_info = binding_info,
            .section = section,
        };
    }
    else
    {
        SymbolTableEntry &symbol_entry = symbol_table[string_table[symbol]];
        if (symbol_entry.section == U32(-1) && section != U32(-1))
        {
            symbol_entry.section = section;
            symbol_entry.symbol_value = value;
        }
        else if (symbol_entry.section != U32(-1) && section != U32(-1))
        {
            AEMU_FATAL(
                "Multiple definition of symbol {} at sections {} and {}",
                symbol, strings[sections[section].section_name],
                strings[sections[symbol_entry.section].section_name]);
            return;
        }

        if (binding_info == SymbolTableEntry::BindingInfo::GLOBAL
            || binding_info == SymbolTableEntry::BindingInfo::WEAK_DECLARED
            || (binding_info == SymbolTableEntry::BindingInfo::LOCAL
                && symbol_entry.binding_info == SymbolTableEntry::BindingInfo::WEAK))
        {
            symbol_entry.binding_info = binding_info;
        }
    }
}

void ObjectFile::write_object_file(File obj_file)
{
    AEMU_DEBUG("Writing to object file.");
    m_obj_file = obj_file;

    // clearing object file
    std::ofstream ofs;
    ofs.open(obj_file.get_path(), std::ofstream::out | std::ofstream::trunc);
    ofs.close();

    // create writer for object file
    FileWriter m_writer = FileWriter(obj_file, std::ios::out | std::ios::binary);

    ByteWriter byte_writer(m_writer);
    int current_byte = 0;

    // BELF Header
    AEMU_DEBUG("Writing BELF header.");
    m_writer.write("BELF"); // BELF magic number header
    byte_writer << ByteWriter::Data(0, 12); // Unused padding
    byte_writer << ByteWriter::Data(file_type, 2); // Object file type
    byte_writer << ByteWriter::Data(target_machine, 2); // Target machine
    byte_writer << ByteWriter::Data(flags, 2); // Flags
    byte_writer << ByteWriter::Data(sections.size(), 2); // Number of sections
    current_byte += kBELFHeaderSize;

    // Text Section
    AEMU_DEBUG("Writing .text section.");
    for (size_t i = 0; i < text_section.size(); i++)
    {
        byte_writer << ByteWriter::Data(text_section.at(i), 4);
    }
    sections[section_table[".text"]].section_size = text_section.size() * 4;
    sections[section_table[".text"]].section_start = current_byte;
    current_byte += text_section.size() * 4;

    // Data, rodata and the arrays of functions
    for (const ByteSection &section : byte_sections())
    {
        AEMU_DEBUG("Writing {} section.", section.name);
        const std::vector<byte> &bytes_of_section = this->*section.bytes;
        for (const byte b : bytes_of_section)
        {
            byte_writer << ByteWriter::Data(b, 1);
        }
        sections[section_table[section.name]].section_size = bytes_of_section.size();
        sections[section_table[section.name]].section_start = current_byte;
        current_byte += bytes_of_section.size();
    }

    // BSS Section
    AEMU_DEBUG("Writing .bss section. Size {} bytes.",
               bss_section);
    byte_writer << ByteWriter::Data(bss_section, kBSSSectionSize);
    sections[section_table[".bss"]].section_size = bss_section;
    sections[section_table[".bss"]].section_start = current_byte;
    current_byte += kBSSSectionSize;

    // Symbol Table
    AEMU_DEBUG("Writing .symtab section.");
    for (const auto &[key, symbol] : symbol_table)
    {
        byte_writer << ByteWriter::Data(symbol.symbol_name, 8);
        byte_writer << ByteWriter::Data(symbol.symbol_value, 8);
        byte_writer << ByteWriter::Data(S16(symbol.binding_info), 2);
        byte_writer << ByteWriter::Data(symbol.section, 8);

        AEMU_DEBUG("symbol {} = {} ({})[{}]",
                   strings[symbol.symbol_name], symbol.symbol_value, U32(symbol.binding_info),
                   symbol.section);
    }
    sections[section_table[".symtab"]].section_size = symbol_table.size() * kSymbolTableEntrySize;
    sections[section_table[".symtab"]].section_start = current_byte;
    current_byte += symbol_table.size() * kSymbolTableEntrySize;

    // Relocation sections
    const auto write_relocations =
        [&](const char *name, const std::vector<RelocationEntry> &relocations)
    {
        AEMU_DEBUG("Writing {} section.", name);
        for (const RelocationEntry &rel : relocations)
        {
            byte_writer << ByteWriter::Data(rel.offset, 8);
            byte_writer << ByteWriter::Data(rel.symbol, 8);
            byte_writer << ByteWriter::Data(U32(rel.type), 4);
            byte_writer << ByteWriter::Data(S64(rel.addend), 8);
        }
        sections[section_table[name]].section_size = relocations.size() * kRelocationEntrySize;
        sections[section_table[name]].section_start = current_byte;
        current_byte += relocations.size() * kRelocationEntrySize;
    };
    write_relocations(".rel.text", rel_text);
    for (const ByteSection &section : byte_sections())
    {
        write_relocations(section.rel_name, this->*section.relocations);
    }
    write_relocations(".rel.bss", rel_bss);

    // User sections: the bytes and the relocations of each, in the order of their headers.
    for (const UserSection &user : user_sections)
    {
        AEMU_DEBUG("Writing the section {}.", user.name);
        SectionHeader &header = sections[user.header_index];
        header.section_size = user.size();
        header.section_start = current_byte;
        if (user.nobits)
        {
            // Like .bss, the size is all there is.
            byte_writer << ByteWriter::Data(user.zero_size, kBSSSectionSize);
            current_byte += kBSSSectionSize;
        }
        else
        {
            for (const byte b : user.bytes)
            {
                byte_writer << ByteWriter::Data(b, 1);
            }
            current_byte += user.bytes.size();
        }

        write_relocations(strings[sections[user.header_index + 1].section_name].c_str(),
                          user.relocations);
    }

    // String Table
    AEMU_DEBUG("Writing .strtab section.");
    int size = 0;
    for (size_t i = 0; i < strings.size(); i++)
    {
        m_writer.write(strings[i]);
        byte_writer << ByteWriter::Data(0, 1); // Null terminated string
        size += strings[i].size() + 1;
    }
    sections[section_table[".strtab"]].section_size = size;
    sections[section_table[".strtab"]].section_start = current_byte;
    current_byte += size;

    // Section headers
    AEMU_DEBUG("Writing Section headers.");
    for (size_t i = 0; i < sections.size(); i++)
    {
        byte_writer << ByteWriter::Data(sections[i].section_name, 8);
        byte_writer << ByteWriter::Data(U32(sections[i].type), 4);
        byte_writer << ByteWriter::Data(sections[i].section_start, 8);
        byte_writer << ByteWriter::Data(sections[i].section_size, 8);
        byte_writer << ByteWriter::Data(sections[i].entry_size, 8);

        byte_writer << ByteWriter::Data(sections[i].load_at_physical_address, 1);
        byte_writer << ByteWriter::Data(sections[i].address, 8);
        byte_writer << ByteWriter::Data(sections[i].alignment, 8);
    }
    // For easy access
    byte_writer << ByteWriter::Data(current_byte, 8);
    current_byte += 8;
    current_byte += sections.size() * kSectionHeaderSize;

    m_writer.close();
}

word ObjectFile::get_text_section_size()
{
    return text_section.size() * 4;
}

word ObjectFile::get_data_section_size()
{
    return data_section.size();
}

word ObjectFile::get_bss_section_size()
{
    return bss_section;
}

word ObjectFile::get_section_size(U32 section)
{
    AEMU_CHECK(section < sections.size(), "No section {}.",
               section);
    switch (sections[section].type)
    {
    case SectionHeader::Type::TEXT:
        return get_text_section_size();
    case SectionHeader::Type::DATA:
    case SectionHeader::Type::RODATA:
    case SectionHeader::Type::INIT_ARRAY:
    case SectionHeader::Type::FINI_ARRAY:
        return word((this->*byte_section_of(sections[section].type)->bytes).size());
    case SectionHeader::Type::BSS:
        return get_bss_section_size();
    case SectionHeader::Type::USER_R:
    case SectionHeader::Type::USER_RW:
    case SectionHeader::Type::USER_RX:
    case SectionHeader::Type::USER_BSS:
        return user_section_at(section)->size();
    default:
        AEMU_FATAL("Section {} holds no code or data.", section);
    }
}
