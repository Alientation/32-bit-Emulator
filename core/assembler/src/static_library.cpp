#include "assembler/static_library.h"

#include "util/logger.h"

#include <fstream>
#include <set>

namespace
{

/// The start of a static library, and the version of the format after it.
constexpr const char *kMagic = "BALB";
constexpr unsigned short kVersion = 1;

/// Longer than any name of a file, to refuse a file that is cut or damaged before reading it.
constexpr unsigned long long kMaxNameLength = 4096;

} // namespace

void write_static_library(const std::vector<File> &objects, File out)
{
    // The whole library is read before the old one is touched.
    std::vector<std::pair<std::string, std::vector<byte>>> members;
    for (const File &file : objects)
    {
        FileReader reader(file, std::ios::in | std::ios::binary);
        std::vector<byte> bytes;
        while (reader.has_next_byte())
        {
            bytes.push_back(reader.read_byte());
        }
        reader.close();

        // Only a file that is an object file can be a member.
        ObjectFile check;
        check.read_object_file(bytes);
        members.emplace_back(file.get_name() + "." + file.get_extension(), std::move(bytes));
    }

    // clearing library file
    std::ofstream ofs;
    ofs.open(out.get_path(), std::ofstream::out | std::ofstream::trunc);
    ofs.close();

    FileWriter writer = FileWriter(out, std::ios::out | std::ios::binary);
    ByteWriter b_writer(writer);

    writer.write(kMagic);
    b_writer << ByteWriter::Data(kVersion, 2);
    b_writer << ByteWriter::Data(members.size(), 8);
    for (const auto &[name, bytes] : members)
    {
        b_writer << ByteWriter::Data(name.size(), 8);
        writer.write(name);
        b_writer << ByteWriter::Data(bytes.size(), 8);
        for (const byte b : bytes)
        {
            b_writer << ByteWriter::Data(b, 1);
        }
    }

    writer.close();
}

std::vector<LibraryMember> read_static_library(File in)
{
    FileReader reader = FileReader(in, std::ios::in | std::ios::binary);

    std::vector<byte> bytes;
    while (reader.has_next_byte())
    {
        bytes.push_back(reader.read_byte());
    }
    reader.close();

    const std::string &path = in.get_path();
    const size_t header = std::char_traits<char>::length(kMagic) + 2 + 8;
    AEMU_CHECK(bytes.size() >= header && std::equal(kMagic, kMagic + 4, bytes.begin()),
               "read_static_library() - '{}' is not a static library.", path);

    ByteReader b_reader(bytes);
    b_reader.skip_bytes(4);
    const unsigned short version = b_reader.read_hword();
    AEMU_CHECK(version == kVersion,
               "read_static_library() - '{}' is version {} of the format, version {} is read.",
               path, version, kVersion);

    std::vector<LibraryMember> members;
    try
    {
        const unsigned long long count = b_reader.read_dword();
        for (unsigned long long i = 0; i < count; i++)
        {
            const unsigned long long name_length = b_reader.read_dword();
            AEMU_CHECK(name_length <= kMaxNameLength,
                       "read_static_library() - '{}' is corrupt, a member has a name of {} bytes.",
                       path, name_length);
            std::string name;
            for (unsigned long long b = 0; b < name_length; b++)
            {
                name += char(b_reader.read_byte());
            }

            const unsigned long long size = b_reader.read_dword();
            std::vector<byte> data;
            for (unsigned long long b = 0; b < size; b++)
            {
                data.push_back(b_reader.read_byte());
            }

            LibraryMember member{.name = name, .object = ObjectFile()};
            member.object.read_object_file(data);
            members.push_back(std::move(member));
        }
    }
    catch (const std::out_of_range &error)
    {
        AEMU_FATAL("read_static_library() - '{}' is cut short: {}", path, error.what());
    }
    return members;
}

std::vector<ObjectFile> select_library_members(const std::vector<ObjectFile> &objects,
                                               const std::vector<LibraryMember> &members)
{
    std::vector<ObjectFile> selected = objects;

    // The symbols that the objects define, and the ones that they use and do not define. A symbol
    // that is local to an object is not seen by the others.
    std::set<std::string> defined;
    std::set<std::string> wanted;
    const auto add = [&](const ObjectFile &object)
    {
        for (const auto &[key, symbol] : object.symbol_table)
        {
            if (symbol.binding_info == ObjectFile::SymbolTableEntry::BindingInfo::LOCAL)
            {
                continue;
            }

            // A weak reference does not need a definition, so it does not pull a member in.
            if (symbol.section == U32(-1)
                && symbol.binding_info == ObjectFile::SymbolTableEntry::BindingInfo::WEAK_DECLARED)
            {
                continue;
            }

            const std::string &name = object.strings[symbol.symbol_name];
            (symbol.section == U32(-1) ? wanted : defined).insert(name);
        }
    };
    for (const ObjectFile &object : objects)
    {
        add(object);
    }

    // A member brings the symbols of its own that it needs, so go on while members are added.
    std::vector<bool> taken(members.size(), false);
    for (bool added = true; added;)
    {
        added = false;
        for (size_t i = 0; i < members.size(); i++)
        {
            if (taken[i])
            {
                continue;
            }

            bool needed = false;
            for (const auto &[key, symbol] : members[i].object.symbol_table)
            {
                const std::string &name = members[i].object.strings[symbol.symbol_name];
                if (symbol.binding_info != ObjectFile::SymbolTableEntry::BindingInfo::LOCAL
                    && symbol.section != U32(-1) && wanted.count(name) != 0
                    && defined.count(name) == 0)
                {
                    needed = true;
                    break;
                }
            }

            if (needed)
            {
                AEMU_DEBUG("select_library_members() - Linking the member {}.", members[i].name);
                taken[i] = true;
                selected.push_back(members[i].object);
                add(members[i].object);
                added = true;
            }
        }
    }
    return selected;
}
