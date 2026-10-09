#include "util/file.h"

#include "util/logger.h"
#include "util/types.h"

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
           bool create_if_not_present) :
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
        AEMU_FATAL("File::File() - Invalid file name: '{}'", name);
    }
    else if (!valid_extension(extension))
    {
        AEMU_FATAL("File::File() - Invalid file extension: '{}'", extension);
    }
    else if (!valid_dir(dir))
    {
        AEMU_FATAL("File::File() - Invalid file directory: '{}'", dir);
    }

    if (create_if_not_present && !exists())
    {
        create();
    }
}

File::File(const std::string &path, bool create_if_not_present)
{
    std::size_t extension_separator_index = path.find_last_of(".");
    if (extension_separator_index == std::string::npos)
    {
        AEMU_FATAL("File::File() - File path does not contain an extension: {}", path);
    }

    bool has_dir = path.find_last_of(SEPARATOR) == std::string::npos;
    std::string name_and_extension = has_dir ? path : path.substr(path.find_last_of(SEPARATOR) + 1);
    m_name = name_and_extension.substr(0, name_and_extension.find_last_of("."));
    m_extension = name_and_extension.substr(name_and_extension.find_last_of(".") + 1);
    m_dir = has_dir ? "" : trim_dir_path(path.substr(0, path.find_last_of(SEPARATOR)));

    if (!valid_name(m_name))
    {
        AEMU_FATAL("File::File() - Invalid file name: '{}'", m_name);
    }
    else if (!valid_extension(m_extension))
    {
        AEMU_FATAL("File::File() - Invalid file extension: '{}'", m_extension);
    }
    else if (!valid_dir(m_dir))
    {
        AEMU_FATAL("File::File() - Invalid file directory: '{}'", m_dir);
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

std::string File::get_abs_path() const
{
    return std::filesystem::absolute(get_path()).string();
}

std::string File::get_dir_str() const
{
    return m_dir;
}

int File::get_size() const
{
    return std::filesystem::file_size(this->get_path());
}

bool File::exists() const
{
    return std::filesystem::exists(this->get_path());
}

bool File::create()
{
    std::filesystem::path fs_path(get_path());

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
    file.close();
    return true;
}

bool File::clear()
{
    std::ofstream ofs;
    ofs.open(get_path(), std::ofstream::out | std::ofstream::trunc);

    if (ofs.bad())
    {
        return false;
    }

    ofs.close();
    return true;
}

FileWriter::FileWriter(const File &file) :
    m_file(file)
{
    m_file_stream = new std::ofstream(file.get_path(), std::ifstream::out);
    m_closed = false;

    if (!m_file_stream->good())
    {
        AEMU_FATAL("FileWriter::FileWriter() - Failed to open file: '{}'", file.get_path());
    }
}

FileWriter::FileWriter(const File &file, std::_Ios_Openmode flags) :
    m_file(file)
{
    m_file_stream = new std::ofstream(file.get_path(), flags);
    m_closed = false;

    if (!m_file_stream->good())
    {
        AEMU_FATAL("FileWriter::FileWriter() - Failed to open file: '{}'", file.get_path());
    }
}

FileWriter::~FileWriter()
{
    this->close();
}

FileWriter &FileWriter::operator<<(std::string str)
{
    this->write(str);
    return *this;
}

FileWriter &FileWriter::operator<<(char byte)
{
    this->write(byte);
    return *this;
}

FileWriter &FileWriter::operator<<(const char *str)
{
    this->write(str);
    return *this;
}

void FileWriter::write(const std::string text)
{
    AEMU_CHECK(!m_closed, "FileWriter::write() - The file is closed.");

    (*m_file_stream) << text;
}

ByteWriter::Data::Data(unsigned long long value, int num_bytes) :
    value(value),
    num_bytes(num_bytes)
{
}

ByteWriter::Data::Data(unsigned long long value, int num_bytes, bool little_endian)
{
    if (little_endian)
    {
        this->value = value;
    }
    else
    {
        for (int i = 0; i < num_bytes; i++)
        {
            this->value <<= 8;
            this->value += value & 0xFF;
            value >>= 8;
        }
    }
    this->num_bytes = num_bytes;
}

ByteWriter::ByteWriter(FileWriter &filewriter) :
    m_filewriter(filewriter)
{
}

ByteWriter &ByteWriter::operator<<(Data data)
{
    for (int i = 0; i < data.num_bytes; i++)
    {
        m_filewriter.write(data.value & 0xFF);
        data.value >>= 8;
    }
    return (*this);
}

void FileWriter::write(const char byte)
{
    AEMU_CHECK(!m_closed, "FileWriter::write() - The file is closed.");

    (*m_file_stream) << byte;
}

void FileWriter::write(const char *bytes)
{
    AEMU_CHECK(!m_closed, "FileWriter::write() - The file is closed.");

    (*m_file_stream) << bytes;
}

void FileWriter::flush()
{
    AEMU_CHECK(!m_closed, "FileWriter::flush() - The file is closed.");

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

ByteReader::Data::Data(int num_bytes) :
    num_bytes(num_bytes){};
ByteReader::Data::Data(int num_bytes, bool little_endian) :
    num_bytes(num_bytes),
    little_endian(little_endian){};

ByteReader &ByteReader::operator>>(ByteReader::Data &data)
{
    if (data.little_endian)
    {
        for (int i = data.num_bytes - 1; i >= 0; i--)
        {
            data.val <<= 8;
            data.val += m_bytes.at(m_cur_byte + i);
        }
        m_cur_byte += data.num_bytes;
    }
    else
    {
        for (int i = data.num_bytes - 1; i >= 0; i--)
        {
            data.val += U64(m_bytes.at(m_cur_byte)) << (8 * i);
            m_cur_byte++;
        }
    }

    return (*this);
}

bool ByteReader::has_next()
{
    return m_cur_byte < m_bytes.size();
}

unsigned char ByteReader::read_byte(bool little_endian)
{
    ByteReader::Data data(1, little_endian);
    (*this) >> data;
    return data.val;
}

unsigned short ByteReader::read_hword(bool little_endian)
{
    ByteReader::Data data(2, little_endian);
    (*this) >> data;
    return data.val;
}

unsigned int ByteReader::read_word(bool little_endian)
{
    ByteReader::Data data(4, little_endian);
    (*this) >> data;
    return data.val;
}

unsigned long long ByteReader::read_dword(bool little_endian)
{
    ByteReader::Data data(8, little_endian);
    (*this) >> data;
    return data.val;
}

void ByteReader::skip_bytes(int num_bytes)
{
    m_cur_byte += num_bytes;
}

FileReader::FileReader(const File &file) :
    m_file(file)
{
    m_file_stream = new std::ifstream(m_file.get_path(), std::ifstream::in);
    m_closed = false;

    if (!m_file_stream->good())
    {
        AEMU_FATAL("FileReader::FileReader() - Failed to open file: '{}'.", m_file.get_path());
    }
}

FileReader::FileReader(const File &file, std::_Ios_Openmode flags) :
    m_file(file)
{
    m_file_stream = new std::ifstream(m_file.get_path(), flags);
    m_closed = false;

    if (!m_file_stream->good())
    {
        AEMU_FATAL("FileReader::FileReader() - Failed to open file: '{}'.", m_file.get_path());
    }
}

FileReader::~FileReader()
{
    this->close();
}

std::string FileReader::read_all()
{
    std::string fileContents;
    while (m_file_stream->peek() != EOF)
    {
        fileContents += m_file_stream->get();
    }
    close();
    return fileContents;
}

char FileReader::read_byte()
{
    return m_file_stream->get();
    ;
}

char FileReader::peek_byte()
{
    return m_file_stream->peek();
}

char *FileReader::read_bytes(const unsigned int num_bytes)
{
    char *bytes = new char[num_bytes];
    m_file_stream->read(bytes, num_bytes);

    if (m_file_stream->fail())
    {
        AEMU_FATAL("FileReader::readBytes() - Failed to read {} bytes from file: '{}'.", num_bytes,
                   m_file.get_path());
    }

    return bytes;
}

char *
FileReader::read_token(const char token_delimiter) // TODO: make this take in a regex separator
{
    std::string token = "";
    while (m_file_stream->peek() != token_delimiter && m_file_stream->peek() != EOF)
    {
        token += m_file_stream->get();
    }

    return (char *) token.c_str();
}

bool FileReader::has_next_byte()
{
    return m_file_stream->peek() != EOF;
}

void FileReader::close()
{
    if (!m_closed)
    {
        delete m_file_stream;
        m_closed = true;
    }
}
