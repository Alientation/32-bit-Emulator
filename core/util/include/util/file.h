#pragma once

#include <cstdint>
#include <ios>
#include <iosfwd>
#include <string>
#include <vector>

/// Normalizes a directory path: both kinds of separators become `/`-separated segments, `.`
/// segments are dropped and a `..` removes the segment before it.
///
/// @param str the directory path
/// @return the path with `/` between the segments
std::string trim_dir_path(const std::string &str);

// TODO: add some form of thread safe locking to the file operations.
// TODO: automatically convert separators.
// TODO: add a way to compose a file path using the builder pattern, which attaches the correct
// separator.

/// A file on the file system, addressed by its directory, name and extension.
class File
{
  public:
    // Windows supports both / and \\ as separators, so use / to be compatible with other OSes.
    inline static const std::string SEPARATOR = "/";

    /// @param name the file name, without the extension
    /// @return whether the name is not empty and has none of the characters a name cannot contain
    static bool valid_name(const std::string &name)
    {
        return name.find_first_of("\\/:*?\"<>|") == std::string::npos && name.size() > 0;
    }

    /// @param extension the extension, without the dot
    /// @return whether the extension is not empty and has none of the characters it cannot contain
    static bool valid_extension(const std::string &extension)
    {
        return extension.find_first_of("\\/:*?\"<>|") == std::string::npos && extension.size() > 0;
    }

    /// @param dir the directory path
    /// @return whether the directory has none of the characters a path cannot contain
    static bool valid_dir(const std::string &dir)
    {
        return dir.find_first_of("*?\"<>|") == std::string::npos;
    }

    /// @param path the file path
    /// @return whether the path has none of the characters a path cannot contain
    static bool valid_path(const std::string &path)
    {
        return path.find_first_of("*?\"<>|") == std::string::npos;
    }

    /// Constructs a file object from its parts. An empty directory is the current directory. An
    /// invalid name, extension or directory is a fatal error.
    ///
    /// @param name the name of the file, without the extension
    /// @param extension the extension of the file, without the dot
    /// @param dir the directory of the file
    /// @param create_if_not_present whether to create the file when it does not exist
    File(const std::string &name, const std::string &extension, const std::string &dir,
         bool create_if_not_present = false);

    /// Constructs a file object with the given file path. A path without an extension, or with an
    /// invalid name, extension or directory, is a fatal error.
    ///
    /// @param path the path of the file
    /// @param create_if_not_present whether to create the file when it does not exist
    File(const std::string &path, bool create_if_not_present = false);

    /// Constructs a file object with no name, extension or directory.
    File();

    /// @return the name of the file, without the extension
    std::string get_name() const;

    /// @return the extension of the file, without the dot
    std::string get_extension() const;

    /// @return the path of the file, relative to the current directory unless the directory is
    /// absolute
    std::string get_path() const;

    /// @return the directory of the file
    std::string get_dir_str() const;

    /// @return whether the file exists
    bool exists() const;

    /// Creates the file, and the directories above it that do not exist.
    ///
    /// @return whether the file was created
    bool create();

    /// Empties the file.
    ///
    /// @return whether the file was emptied
    bool clear();

  private:
    std::string m_name;
    std::string m_extension;
    std::string m_dir;
};

/// Writes text and bytes to a file. Failing to open the file is a fatal error, and so is writing
/// after close().
class FileWriter
{
  public:
    /// Opens the file for writing with the given stream modes.
    ///
    /// @param file the file to write to
    /// @param flags the open mode of the stream, e.g. `std::ios::binary | std::ios::app`
    FileWriter(const File &file, std::ios_base::openmode flags);

    /// Closes the file.
    ~FileWriter();

    /// Same as write().
    FileWriter &operator<<(std::string);
    FileWriter &operator<<(char byte);
    FileWriter &operator<<(const char *bytes);

    /// Writes a string to the file.
    ///
    /// @param text the string to write
    void write(std::string text);

    /// Writes a byte to the file.
    ///
    /// @param byte the byte to write
    void write(char byte);

    /// Writes a null terminated byte array to the file.
    ///
    /// @param bytes the byte array to write
    void write(const char *bytes);

    /// Writes what is buffered to the file.
    void flush();

    /// Closes the file. Closing it again does nothing.
    void close();

  private:
    File m_file;
    std::ofstream *m_file_stream;
    bool m_closed;
};

/// Writes values of a given width to a FileWriter, as bytes.
class ByteWriter
{
  public:
    /// @param filewriter the writer the bytes go to, which has to outlive this
    ByteWriter(FileWriter &filewriter);

    /// A value and the number of its low bytes to write.
    struct Data
    {
        unsigned long long value = 0;
        int num_bytes;

        /// @param value the value to write
        /// @param num_bytes how many bytes of the value to write
        Data(unsigned long long value, int num_bytes);

        /// @param value the value to write
        /// @param num_bytes how many bytes of the value to write
        /// @param little_endian whether the least significant byte is written first, if not
        ///     the bytes of the value are reversed
        Data(unsigned long long value, int num_bytes, bool little_endian);
    };

    /// Writes the bytes of the value, the least significant one first.
    ///
    /// @param data the value and how many bytes of it to write
    /// @return this writer
    ByteWriter &operator<<(Data data);

  private:
    FileWriter &m_filewriter;
};

/// Reads text and bytes from a file. Failing to open the file is a fatal error.
class FileReader
{
  public:
    /// Opens the file for reading with the given stream modes.
    ///
    /// @param file the file to read
    /// @param flags the open mode of the stream, e.g. `std::ios::binary`
    FileReader(const File &file, std::ios_base::openmode flags);

    /// Closes the file.
    ~FileReader();

    /// @return the next byte, which is consumed
    char read_byte();

    /// @return whether there is a byte left to read
    bool has_next_byte();

    /// Closes the file. Closing it again does nothing.
    void close();

  private:
    File m_file;
    std::ifstream *m_file_stream;
    bool m_closed;
};

/// Reads values of a given width from a vector of bytes.
class ByteReader
{
  public:
    /// @param bytes the bytes to read, which have to outlive this
    ByteReader(std::vector<unsigned char> &bytes) :
        m_bytes(bytes){};

    /// How many bytes to read, in which order, and the value that was read.
    struct Data
    {
        unsigned long long val = 0;
        int num_bytes = 0;
        bool little_endian = true;

        /// @param num_bytes how many bytes to read, little endian
        Data(int num_bytes);

        /// @param num_bytes how many bytes to read
        /// @param little_endian whether the least significant byte comes first
        Data(int num_bytes, bool little_endian);
    };

    /// Reads `data.num_bytes` bytes into `data.val`.
    ///
    /// @param data the width and byte order, and where the value is stored
    /// @return this reader
    ByteReader &operator>>(Data &data);

    /// @return whether there is a byte left to read
    bool has_next();

    /// Each of the following reads one value of its width.
    ///
    /// @param little_endian whether the least significant byte comes first
    /// @return the value that was read
    unsigned char read_byte(bool little_endian = true);
    unsigned short read_hword(bool little_endian = true);
    unsigned int read_word(bool little_endian = true);
    unsigned long long read_dword(bool little_endian = true);

    /// Skips bytes without reading them.
    ///
    /// @param num_bytes how many bytes to skip
    void skip_bytes(int num_bytes);

  private:
    std::vector<unsigned char> &m_bytes;
    size_t m_cur_byte = 0;
};
