#include "util/file.h"

#include "util/logger.h"
#include "util/types.h"

#include <filesystem>
#include <fstream>

std::string trim_dir_path(const std::string &str)
{
    std::vector<std::string> segments;
    size_t i = 0;
    while (i < str.size())
    {
        size_t end = str.find("\\", i);
        if (end == std::string::npos)
        {
            end = str.size();
        }
        size_t other_separator_end = str.find("/", i);
        if (other_separator_end == std::string::npos)
        {
            other_separator_end = str.size();
        }
        end = end < other_separator_end ? end : other_separator_end;

        segments.push_back(str.substr(i, end - i));
        i = end + 1;

        if (segments.back() == ".")
        {
            segments.pop_back();
        }
        else if (segments.back() == "..")
        {
            if (segments.size() > 1)
            {
                segments.pop_back();
                segments.pop_back();
            }
        }
    }

    std::string res;
    for (size_t i = 0; i < segments.size(); i++)
    {
        res += segments[i];
        if (i + 1 < segments.size())
        {
            res += File::SEPARATOR;
        }
    }
    return res;
}

File::File(const std::string &name, const std::string &extension, const std::string &dir,
           const bool create_if_not_present) :
    m_name(name),
    m_extension(extension)
{
    if (dir.empty())
    {
        m_dir = std::filesystem::current_path().string();
    }
    else
    {
        m_dir = trim_dir_path(dir);
    }

    if (!valid_name(name))
    {
        AEMU_FATAL("Invalid file name: '{}'", name);
    }
    else if (!valid_extension(extension))
    {
        AEMU_FATAL("Invalid file extension: '{}'", extension);
    }
    else if (!valid_dir(dir))
    {
        AEMU_FATAL("Invalid file directory: '{}'", dir);
    }

    if (create_if_not_present && !exists())
    {
        create();
    }
}

File::File(const std::string &path, const bool create_if_not_present)
{
    // The extension is after the last dot of the last part of the path: a dot in a directory
    // (`v1.2/program`) is not one. Both separators are taken, as trim_dir_path does.
    const size_t last_separator = path.find_last_of("/\\");
    const bool has_dir = last_separator != std::string::npos;
    const std::string name_and_extension = has_dir ? path.substr(last_separator + 1) : path;
    const size_t dot = name_and_extension.find_last_of('.');
    if (dot == std::string::npos)
    {
        AEMU_FATAL("File path does not contain an extension: {}", path);
        return;
    }

    m_name = name_and_extension.substr(0, dot);
    m_extension = name_and_extension.substr(dot + 1);
    m_dir = has_dir ? trim_dir_path(path.substr(0, last_separator)) : "";

    if (!valid_name(m_name))
    {
        AEMU_FATAL("Invalid file name: '{}'", m_name);
    }
    else if (!valid_extension(m_extension))
    {
        AEMU_FATAL("Invalid file extension: '{}'", m_extension);
    }
    else if (!valid_dir(m_dir))
    {
        AEMU_FATAL("Invalid file directory: '{}'", m_dir);
    }

    if (create_if_not_present && !exists())
    {
        create();
    }
}

File::File() :
    m_name(""),
    m_extension(""),
    m_dir("")
{
}

std::string File::get_name() const
{
    return m_name;
}

std::string File::get_extension() const
{
    return m_extension;
}

std::string File::get_path() const
{
    if (m_dir.size() == 0)
    {
        return m_name + "." + m_extension;
    }
    return m_dir + SEPARATOR + m_name + "." + m_extension;
}

std::string File::get_dir_str() const
{
    return m_dir;
}

bool File::exists() const
{
    return std::filesystem::exists(this->get_path());
}

bool File::create()
{
    const std::filesystem::path fs_path(get_path());

    // Create all necessary directories. A bare file name has no parent directory, and
    // create_directories returns false when the directories already exist, so check the error
    // code instead of the return value.
    if (fs_path.has_parent_path())
    {
        std::error_code ec;
        std::filesystem::create_directories(fs_path.parent_path(), ec);
        if (ec)
        {
            return false;
        }
    }

    std::ofstream file(get_path());
    return bool(file);
}

bool File::clear()
{
    std::ofstream ofs;
    ofs.open(get_path(), std::ofstream::out | std::ofstream::trunc);

    // A file that cannot be opened (no directory above it) sets failbit, not badbit.
    if (!ofs)
    {
        return false;
    }

    ofs.close();
    return true;
}

FileWriter::FileWriter(const File &file, const std::ios_base::openmode flags) :
    m_file(file)
{
    m_file_stream = new std::ofstream(file.get_path(), flags);
    m_closed = false;

    if (!m_file_stream->good())
    {
        AEMU_FATAL("Failed to open file: '{}'", file.get_path());
    }
}

FileWriter::~FileWriter()
{
    this->close();
}

FileWriter &FileWriter::operator<<(const std::string &str)
{
    this->write(str);
    return *this;
}

FileWriter &FileWriter::operator<<(const char byte)
{
    this->write(byte);
    return *this;
}

FileWriter &FileWriter::operator<<(const char *str)
{
    this->write(str);
    return *this;
}

void FileWriter::write(const std::string &text)
{
    AEMU_CHECK(!m_closed, "The file is closed.");

    (*m_file_stream) << text;
}

ByteWriter::Data::Data(const U64 value, const size_t num_bytes) :
    value(value),
    num_bytes(num_bytes)
{
}

ByteWriter::Data::Data(U64 value, const size_t num_bytes, const bool little_endian)
{
    if (little_endian)
    {
        this->value = value;
    }
    else
    {
        for (size_t i = 0; i < num_bytes; i++)
        {
            this->value <<= kNumByteBits;
            this->value += byte(value);
            value >>= kNumByteBits;
        }
    }
    this->num_bytes = num_bytes;
}

ByteWriter::ByteWriter(FileWriter &filewriter) :
    m_filewriter(filewriter)
{
}

ByteWriter &ByteWriter::operator<<(const Data &data)
{
    for (size_t i = 0; i < data.num_bytes; i++)
    {
        // Bytes past the width of the value are zero.
        const bool in_value = i < sizeof(data.value);
        m_filewriter.write(in_value ? byte(data.value >> (i * kNumByteBits)) : byte(0));
    }
    return (*this);
}

void FileWriter::write(const char byte)
{
    AEMU_CHECK(!m_closed, "The file is closed.");

    (*m_file_stream) << byte;
}

void FileWriter::write(const char *bytes)
{
    AEMU_CHECK(!m_closed, "The file is closed.");

    (*m_file_stream) << bytes;
}

void FileWriter::flush()
{
    AEMU_CHECK(!m_closed, "The file is closed.");

    m_file_stream->flush();
}

void FileWriter::close()
{
    if (!m_closed)
    {
        m_closed = true;
        delete m_file_stream;
    }
}

ByteReader::Data::Data(const size_t num_bytes) :
    num_bytes(num_bytes){};
ByteReader::Data::Data(const size_t num_bytes, const bool little_endian) :
    num_bytes(num_bytes),
    little_endian(little_endian){};

ByteReader &ByteReader::operator>>(ByteReader::Data &data)
{
    data.val = 0;
    if (data.little_endian)
    {
        for (size_t i = data.num_bytes - 1; i != U64(-1); i--)
        {
            data.val <<= kNumByteBits;
            data.val += m_bytes.at(m_cur_byte + i);
        }
        m_cur_byte += data.num_bytes;
    }
    else
    {
        for (size_t i = data.num_bytes - 1; i != U64(-1); i--)
        {
            data.val += U64(m_bytes.at(m_cur_byte)) << (kNumByteBits * i);
            m_cur_byte++;
        }
    }

    return (*this);
}

bool ByteReader::has_next()
{
    return m_cur_byte < m_bytes.size();
}

U8 ByteReader::read_byte(const bool little_endian)
{
    ByteReader::Data data(1, little_endian);
    (*this) >> data;
    return data.val;
}

U16 ByteReader::read_hword(const bool little_endian)
{
    ByteReader::Data data(sizeof(hword), little_endian);
    (*this) >> data;
    return data.val;
}

U32 ByteReader::read_word(const bool little_endian)
{
    ByteReader::Data data(sizeof(word), little_endian);
    (*this) >> data;
    return data.val;
}

U64 ByteReader::read_dword(const bool little_endian)
{
    ByteReader::Data data(sizeof(dword), little_endian);
    (*this) >> data;
    return data.val;
}

void ByteReader::skip_bytes(const size_t num_bytes)
{
    m_cur_byte += num_bytes;
}

std::vector<byte> read_file_bytes(const File &file)
{
    std::ifstream in(file.get_path(), std::ios::binary);
    if (!in)
    {
        AEMU_FATAL("Failed to open file: '{}'.", file.get_path());
        return {};
    }

    in.seekg(0, std::ios::end);
    const std::streamoff size = in.tellg();
    in.seekg(0, std::ios::beg);
    if (size < 0)
    {
        AEMU_FATAL("Failed to read file: '{}'.", file.get_path());
        return {};
    }

    std::vector<byte> bytes(size_t(size), 0);
    in.read(reinterpret_cast<char *>(bytes.data()), size);
    if (in.gcount() != size)
    {
        AEMU_FATAL("Failed to read file: '{}'.", file.get_path());
        return {};
    }
    return bytes;
}
