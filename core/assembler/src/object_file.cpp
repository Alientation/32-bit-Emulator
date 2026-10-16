#include "assembler/object_file.h"

#include "assembler/build.h"
#include "emulator32bit/emulator32bit.h"
#include "util/logger.h"
#include "util/types.h"

#include <fstream>

/// Fails unless the file has a section with the name and the type.
static void require_section(const ObjectFile &obj, const std::string &path, const char *name,
                            const ObjectFile::SectionHeader::Type type)
{
    const ObjectFile::NameIndex::const_iterator section = obj.section_table.find(name);
    AEMU_CHECK(section != obj.section_table.end() && obj.sections[section->second].type == type,
               "'{}' is not an object file that can be used, it has no {} section.", path, name);
}

/// Fails unless every relocation is for a symbol of the file, has a type that exists and is
/// inside the section (`size` bytes long) with `width` bytes to patch.
static void check_relocations(const ObjectFile &obj, const std::string &path,
                              const std::vector<ObjectFile::RelocationEntry> &relocations,
                              const char *section, const size_t size, const size_t width)
{
    using Type = ObjectFile::RelocationEntry::Type;
    for (const ObjectFile::RelocationEntry &rel : relocations)
    {
        AEMU_CHECK(obj.symbol_table.count(rel.symbol) != 0,
                   "'{}' is corrupt, a relocation of {} is for the symbol {}, which does not "
                   "exist.",
                   path, section, rel.symbol);
        AEMU_CHECK(rel.type > Type::UNDEFINED && rel.type <= Type::R_EMU32_ADR_PCREL21,
                   "'{}' is corrupt, a relocation of {} has the invalid type {}.", path, section,
                   U32(rel.type));
        AEMU_CHECK(U64(rel.offset) + width <= size,
                   "'{}' is corrupt, a relocation of {} is at {}, outside of the section.", path,
                   section, rel.offset);
    }
}

/// Writes the relocations of a section where the last write stopped, and gives its header the
/// size and the place.
static void write_relocations(ByteWriter &byte_writer, size_t &current_byte,
                              ObjectFile::SectionHeader &header, const char *name,
                              const std::vector<ObjectFile::RelocationEntry> &relocations)
{
    AEMU_DEBUG("Writing {} section.", name);
    for (const ObjectFile::RelocationEntry &rel : relocations)
    {
        byte_writer << ByteWriter::Data(rel.offset, sizeof(dword));
        byte_writer << ByteWriter::Data(rel.symbol, sizeof(dword));
        byte_writer << ByteWriter::Data(U32(rel.type), sizeof(word));
        byte_writer << ByteWriter::Data(S64(rel.addend), sizeof(dword));
    }
    header.section_size = relocations.size() * ObjectFile::kRelocationEntrySize;
    header.section_start = current_byte;
    current_byte += relocations.size() * ObjectFile::kRelocationEntrySize;
}

ObjectFile::ObjectFile() = default;

const ObjectFile::ByteSections &ObjectFile::byte_sections()
{
    using Type = SectionHeader::Type;
    static const ByteSections sections = {
        {.name = ".data",
         .rel_name = ".rel.data",
         .type = Type::DATA,
         .rel_type = Type::REL_DATA,
         .bytes = &ObjectFile::data_section,
         .relocations = &ObjectFile::rel_data,
         .writable = true,
         .alignment = 1},
        {.name = ".rodata",
         .rel_name = ".rel.rodata",
         .type = Type::RODATA,
         .rel_type = Type::REL_RODATA,
         .bytes = &ObjectFile::rodata_section,
         .relocations = &ObjectFile::rel_rodata,
         .writable = false,
         .alignment = 1},
        {.name = ".init_array",
         .rel_name = ".rel.init_array",
         .type = Type::INIT_ARRAY,
         .rel_type = Type::REL_INIT_ARRAY,
         .bytes = &ObjectFile::init_array_section,
         .relocations = &ObjectFile::rel_init_array,
         .writable = false,
         .alignment = 4},
        {.name = ".fini_array",
         .rel_name = ".rel.fini_array",
         .type = Type::FINI_ARRAY,
         .rel_type = Type::REL_FINI_ARRAY,
         .bytes = &ObjectFile::fini_array_section,
         .relocations = &ObjectFile::rel_fini_array,
         .writable = false,
         .alignment = 4},
    };
    return sections;
}

const ObjectFile::ByteSection *ObjectFile::byte_section_of(const SectionHeader::Type type)
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

ObjectFile::SectionHeader::Type
ObjectFile::user_section_type(const bool writable, const bool executable, const bool nobits)
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

const ObjectFile::UserSection *ObjectFile::user_section_at(const U32 header_index) const
{
    for (const UserSection &section : user_sections)
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

ObjectFile::ObjectFile(const File &obj_file) :
    ObjectFile()
{
    read_object_file(obj_file);
}

void ObjectFile::read_object_file(const std::vector<byte> &bytes)
{
    // m_obj_file is not set.
    // This is how a static library is decomposed into a list of object files.
    deserialize_checked(bytes);
}

void ObjectFile::read_object_file(const File &object_file)
{
    m_obj_file = object_file;
    deserialize_checked(read_file_bytes(m_obj_file));
}

void ObjectFile::deserialize_checked(const std::vector<byte> &bytes)
{
    try
    {
        deserialize(bytes);
    }
    catch (const std::out_of_range &e)
    {
        // ByteReader and the section/string lookups are bounds checked.
        AEMU_FATAL("'{}' is truncated or corrupt: {}", m_obj_file.get_path(), e.what());
    }
}

void ObjectFile::deserialize(const std::vector<byte> &bytes)
{
    AEMU_DEBUG("Disassembling");
    ByteReader reader(bytes);

    // Read the BELF header.
    AEMU_DEBUG("Reading BELF Header");
    if (bytes.size() < kBELFHeaderSize + sizeof(dword))
    {
        AEMU_FATAL("'{}' is too small ({} bytes) to be an object file.", m_obj_file.get_path(),
                   bytes.size());
        return;
    }

    const byte expected[4] = {'B', 'E', 'L', 'F'}; // 0-3
    for (size_t i = 0; i < sizeof(expected) / sizeof(expected[0]); i++)
    {
        if (expected[i] != reader.read_byte())
        {
            AEMU_FATAL("'{}' is not an object file, bad magic number.", m_obj_file.get_path());
            return;
        }
    }
    reader.skip_bytes(kBELFHeaderPadding); // 4-15
    file_type = reader.read_hword();       // 16-17
    target_machine = reader.read_hword();  // 18-19
    flags = reader.read_hword();           // 20-21
    n_sections = reader.read_hword();      // 22-23

    AEMU_DEBUG("Belf Header = (filetype={}, target_machine={}, "
               "flags={}, n_sections={})",
               file_type, target_machine, flags, n_sections);

    // Read the section headers.
    AEMU_DEBUG("Reading section headers");
    ByteReader section_headers_reader(bytes);
    ByteReader section_headers_start_reader(bytes);
    section_headers_start_reader.skip_bytes(bytes.size() - sizeof(dword));
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
    for (size_t i = 0; i < n_sections; i++)
    {
        SectionHeader section_header = {
            .section_name = U32(section_headers_reader.read_dword()),
            .type = (SectionHeader::Type) section_headers_reader.read_word(),

            // TODO: Why are we reading in dwords and then immediately casting to words?
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
            for (word i = 0; i < section_header.section_size; i += kTextEntrySize)
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
                const RelocationEntry rel = {
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
                const byte b = reader.read_byte();
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

    // Fill in the section table.
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

/// @param[in] type The type of a section.
/// @return The name that a section of the type has (the linker finds it by that name), or null for
///     a section of the program's own.
static const char *fixed_name(const ObjectFile::SectionHeader::Type type)
{
    using Type = ObjectFile::SectionHeader::Type;
    switch (type)
    {
    case Type::TEXT:
        return ".text";
    case Type::BSS:
        return ".bss";
    case Type::SYMTAB:
        return ".symtab";
    case Type::STRTAB:
        return ".strtab";
    case Type::REL_TEXT:
        return ".rel.text";
    case Type::REL_BSS:
        return ".rel.bss";
    default:
        break;
    }
    for (const ObjectFile::ByteSection &section : ObjectFile::byte_sections())
    {
        if (type == section.type) return section.name;
        if (type == section.rel_type) return section.rel_name;
    }
    return nullptr;
}

/// @param[in] type The type of a section.
/// @return Whether a symbol can be defined in it: code, bytes or zeros, not a table.
static bool can_hold_symbols(const ObjectFile::SectionHeader::Type type)
{
    using Type = ObjectFile::SectionHeader::Type;
    return type == Type::TEXT || type == Type::BSS || ObjectFile::byte_section_of(type) != nullptr
           || ObjectFile::is_user_section_type(type);
}

void ObjectFile::validate() const
{
    const std::string &path = m_obj_file.get_path();

    // Each section has a name of its own: the linker finds them by name.
    AEMU_CHECK(section_table.size() == sections.size(),
               "'{}' is corrupt, two of its sections have the same name.", path);

    // The sections that everything else is looked up in.
    require_section(*this, path, ".text", SectionHeader::Type::TEXT);
    for (const ByteSection &section : byte_sections())
    {
        require_section(*this, path, section.name, section.type);
    }
    require_section(*this, path, ".bss", SectionHeader::Type::BSS);
    require_section(*this, path, ".symtab", SectionHeader::Type::SYMTAB);
    require_section(*this, path, ".strtab", SectionHeader::Type::STRTAB);

    for (const SectionHeader &header : sections)
    {
        const std::string &name = strings[header.section_name];
        AEMU_CHECK(header.alignment < kAlignmentLimit,
                   "'{}' is corrupt, the section {} is aligned to {} bytes.", path, name,
                   header.alignment);
        const char *fixed = fixed_name(header.type);
        AEMU_CHECK(fixed == nullptr || name == fixed,
                   "'{}' is corrupt, the section {} has the type of {}.", path, name,
                   fixed == nullptr ? "" : fixed);
    }

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
        AEMU_CHECK(symbol.section == U32(-1) || can_hold_symbols(sections[symbol.section].type),
                   "'{}' is corrupt, the symbol '{}' is in the section {}, which has no place for "
                   "one.",
                   path, strings[symbol.symbol_name],
                   strings[sections[symbol.section].section_name]);
        AEMU_CHECK(symbol.binding_info == SymbolTableEntry::BindingInfo::LOCAL
                       || symbol.binding_info == SymbolTableEntry::BindingInfo::GLOBAL
                       || symbol.binding_info == SymbolTableEntry::BindingInfo::WEAK
                       || symbol.binding_info == SymbolTableEntry::BindingInfo::WEAK_DECLARED,
                   "'{}' is corrupt, the symbol '{}' has the invalid "
                   "binding {}.",
                   path, strings[symbol.symbol_name], U32(symbol.binding_info));
    }

    // Where the relocation is, in the section that it is for.
    check_relocations(*this, path, rel_text, ".text", text_section.size() * kTextEntrySize,
                      sizeof(word));
    for (const ByteSection &section : byte_sections())
    {
        check_relocations(*this, path, this->*section.relocations, section.name,
                          (this->*section.bytes).size(), sizeof(word));
    }
    check_relocations(*this, path, rel_bss, ".bss", 0, 0);
    for (const UserSection &user : user_sections)
    {
        check_relocations(*this, path, user.relocations, user.name.c_str(), user.bytes.size(),
                          sizeof(word));
    }

    for (const RelocationEntry &rel : rel_text)
    {
        AEMU_CHECK(rel.offset % kTextEntrySize == 0,
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
        AEMU_CHECK(!user.executable || user.bytes.size() % kTextEntrySize == 0,
                   "'{}' is corrupt, the code of {} is not made of "
                   "whole instructions.",
                   path, user.name);
        for (const RelocationEntry &rel : user.relocations)
        {
            AEMU_CHECK(!user.executable || rel.offset % kTextEntrySize == 0,
                       "'{}' is corrupt, a relocation of {} is at {}, "
                       "which is not the start of an instruction.",
                       path, user.name, rel.offset);
        }
    }
}

U32 ObjectFile::add_section(const std::string &section_name, const SectionHeader::Type type)
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
    const NameIndex::const_iterator found = string_table.find(string);
    if (found != string_table.end()) return found->second;

    string_table[string] = U32(strings.size());
    strings.push_back(string);
    return strings.size() - 1;
}

const ObjectFile::SymbolTableEntry *ObjectFile::find_symbol(const std::string &name) const
{
    const NameIndex::const_iterator string = string_table.find(name);
    if (string == string_table.end()) return nullptr;
    const SymbolTable::const_iterator symbol = symbol_table.find(string->second);
    return symbol == symbol_table.end() ? nullptr : &symbol->second;
}

ObjectFile::SymbolTableEntry *ObjectFile::find_symbol(const std::string &name)
{
    const NameIndex::const_iterator string = string_table.find(name);
    if (string == string_table.end()) return nullptr;
    const SymbolTable::iterator symbol = symbol_table.find(string->second);
    return symbol == symbol_table.end() ? nullptr : &symbol->second;
}

std::string ObjectFile::get_symbol_name(const U32 symbol) const
{
    return strings.at(symbol_table.at(symbol).symbol_name);
}

void ObjectFile::add_symbol(const std::string &symbol, const word value,
                            const SymbolTableEntry::BindingInfo binding_info, const U32 section)
{
    // If symbol does not exist yet, create it. Its name can be in the string table already, as the
    // name of a section.
    SymbolTableEntry *const existing = find_symbol(symbol);
    if (existing == nullptr)
    {
        const U32 name = add_string(symbol);
        symbol_table[name] = {
            .symbol_name = name,
            .symbol_value = value,
            .binding_info = binding_info,
            .section = section,
        };
    }
    else
    {
        SymbolTableEntry &symbol_entry = *existing;
        if (symbol_entry.section == U32(-1) && section != U32(-1))
        {
            symbol_entry.section = section;
            symbol_entry.symbol_value = value;
        }
        else if (symbol_entry.section != U32(-1) && section != U32(-1))
        {
            AEMU_FATAL("Multiple definition of symbol {} at sections {} and {}", symbol,
                       strings[sections[section].section_name],
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

void ObjectFile::write_object_file(const File &object_file)
{
    AEMU_DEBUG("Writing to object file.");
    m_obj_file = object_file;

    // Clear the object file.
    std::ofstream ofs;
    ofs.open(object_file.get_path(), std::ofstream::out | std::ofstream::trunc);
    ofs.close();

    // Create the writer for the object file.
    FileWriter m_writer = FileWriter(object_file, std::ios::out | std::ios::binary);

    ByteWriter byte_writer(m_writer);
    size_t current_byte = 0;

    // Write the BELF header.
    AEMU_DEBUG("Writing BELF header.");
    m_writer.write("BELF");                                          // BELF magic number header
    byte_writer << ByteWriter::Data(0, kBELFHeaderPadding);          // Unused padding
    byte_writer << ByteWriter::Data(file_type, sizeof(hword));       // Object file type
    byte_writer << ByteWriter::Data(target_machine, sizeof(hword));  // Target machine
    byte_writer << ByteWriter::Data(flags, sizeof(hword));           // Flags
    byte_writer << ByteWriter::Data(sections.size(), sizeof(hword)); // Number of sections
    current_byte += kBELFHeaderSize;

    // Write the text section.
    AEMU_DEBUG("Writing .text section.");
    for (size_t i = 0; i < text_section.size(); i++)
    {
        byte_writer << ByteWriter::Data(text_section.at(i), sizeof(word));
    }
    sections[section_table[".text"]].section_size = text_section.size() * kTextEntrySize;
    sections[section_table[".text"]].section_start = current_byte;
    current_byte += text_section.size() * kTextEntrySize;

    // Write the data, rodata and the arrays of functions.
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

    // Write the BSS section.
    AEMU_DEBUG("Writing .bss section. Size {} bytes.", bss_section);
    byte_writer << ByteWriter::Data(bss_section, kBSSSectionSize);
    sections[section_table[".bss"]].section_size = bss_section;
    sections[section_table[".bss"]].section_start = current_byte;
    current_byte += kBSSSectionSize;

    // Write the symbol table.
    AEMU_DEBUG("Writing .symtab section.");
    for (const auto &[key, symbol] : symbol_table)
    {
        byte_writer << ByteWriter::Data(symbol.symbol_name, sizeof(dword));
        byte_writer << ByteWriter::Data(symbol.symbol_value, sizeof(dword));
        byte_writer << ByteWriter::Data(S16(symbol.binding_info), sizeof(hword));
        byte_writer << ByteWriter::Data(symbol.section, sizeof(dword));

        AEMU_DEBUG("symbol {} = {} ({})[{}]", strings[symbol.symbol_name], symbol.symbol_value,
                   U32(symbol.binding_info), symbol.section);
    }
    sections[section_table[".symtab"]].section_size = symbol_table.size() * kSymbolTableEntrySize;
    sections[section_table[".symtab"]].section_start = current_byte;
    current_byte += symbol_table.size() * kSymbolTableEntrySize;

    // Write the relocation sections.
    write_relocations(byte_writer, current_byte, sections[section_table[".rel.text"]], ".rel.text",
                      rel_text);
    for (const ByteSection &section : byte_sections())
    {
        write_relocations(byte_writer, current_byte, sections[section_table[section.rel_name]],
                          section.rel_name, this->*section.relocations);
    }
    write_relocations(byte_writer, current_byte, sections[section_table[".rel.bss"]], ".rel.bss",
                      rel_bss);

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

        SectionHeader &rel_header = sections[user.header_index + 1];
        write_relocations(byte_writer, current_byte, rel_header,
                          strings[rel_header.section_name].c_str(), user.relocations);
    }

    // Write the string table.
    AEMU_DEBUG("Writing .strtab section.");
    size_t size = 0;
    for (size_t i = 0; i < strings.size(); i++)
    {
        m_writer.write(strings[i]);
        byte_writer << ByteWriter::Data(0, 1); // Null terminated string
        size += strings[i].size() + 1;
    }
    sections[section_table[".strtab"]].section_size = size;
    sections[section_table[".strtab"]].section_start = current_byte;
    current_byte += size;

    // Write the section headers.
    AEMU_DEBUG("Writing Section headers.");
    for (size_t i = 0; i < sections.size(); i++)
    {
        byte_writer << ByteWriter::Data(sections[i].section_name, sizeof(dword));
        byte_writer << ByteWriter::Data(U32(sections[i].type), sizeof(word));
        byte_writer << ByteWriter::Data(sections[i].section_start, sizeof(dword));
        byte_writer << ByteWriter::Data(sections[i].section_size, sizeof(dword));
        byte_writer << ByteWriter::Data(sections[i].entry_size, sizeof(dword));

        byte_writer << ByteWriter::Data(sections[i].load_at_physical_address, 1);
        byte_writer << ByteWriter::Data(sections[i].address, sizeof(dword));
        byte_writer << ByteWriter::Data(sections[i].alignment, sizeof(dword));
    }
    // The offset of the section headers, in the last 8 bytes for easy access.
    byte_writer << ByteWriter::Data(current_byte, sizeof(dword));
    current_byte += sizeof(dword);
    current_byte += sections.size() * kSectionHeaderSize;

    m_writer.close();
}

word ObjectFile::get_text_section_size() const
{
    return text_section.size() * kTextEntrySize;
}

word ObjectFile::get_data_section_size() const
{
    return data_section.size();
}

word ObjectFile::get_bss_section_size() const
{
    return bss_section;
}

word ObjectFile::get_section_size(const U32 section) const
{
    AEMU_CHECK(section < sections.size(), "No section {}.", section);
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
