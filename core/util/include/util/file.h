#pragma once

#include "util/types.h"

#include <ios>
#include <iosfwd>
#include <string>
#include <vector>

/// Normalizes a directory path: `\` and `/` both separate segments and the result is joined with
/// `/`. A `.` segment is dropped and a `..` removes the segment before it (a `..` that has none
/// before it stays).
///
/// @param[in] str The directory path.
/// @return The path with `/` between the segments.
std::string trim_dir_path(const std::string &str);

// TODO: add some form of thread safe locking to the file operations.
// TODO: automatically convert separators.
// TODO: add a way to compose a file path using the builder pattern, which attaches the correct
// separator.

/// A file on the file system, addressed by its directory, name and extension.
class File
{
  public:
    /// The separator of the parts of a path. Windows supports both / and \\, so / is used to be
    /// compatible with other OSes.
    inline static const std::string SEPARATOR = "/";

    /// @param[in] name The file name, without the extension.
    /// @return Whether the name is not empty and has none of the characters a name cannot contain.
    static bool valid_name(const std::string &name)
    {
        return name.find_first_of("\\/:*?\"<>|") == std::string::npos && name.size() > 0;
    }

    /// @param[in] extension The extension, without the dot.
    /// @return Whether the extension is not empty and has none of the characters it cannot contain.
    static bool valid_extension(const std::string &extension)
    {
        return extension.find_first_of("\\/:*?\"<>|") == std::string::npos && extension.size() > 0;
    }

    /// @param[in] dir The directory path.
    /// @return Whether the directory has none of the characters a path cannot contain.
    static bool valid_dir(const std::string &dir)
    {
        return dir.find_first_of("*?\"<>|") == std::string::npos;
    }

    /// @param[in] path The file path.
    /// @return Whether the path has none of the characters a path cannot contain.
    static bool valid_path(const std::string &path)
    {
        return path.find_first_of("*?\"<>|") == std::string::npos;
    }

    /// Constructs a file object from its parts. An empty directory is the current directory. An
    /// invalid name, extension or directory is a fatal error.
    ///
    /// @param[in] name The name of the file, without the extension.
    /// @param[in] extension The extension of the file, without the dot.
    /// @param[in] dir The directory of the file.
    /// @param[in] create_if_not_present Whether to create the file when it does not exist.
    File(const std::string &name, const std::string &extension, const std::string &dir,
         bool create_if_not_present = false);

    /// Constructs a file object with the given file path. The extension is what follows the last
    /// dot of the file name, a dot in a directory is part of the directory. A file name without an
    /// extension, or an invalid name, extension or directory, is a fatal error.
    ///
    /// @param[in] path The path of the file.
    /// @param[in] create_if_not_present Whether to create the file when it does not exist.
    File(const std::string &path, bool create_if_not_present = false);

    /// Constructs a file object with no name, extension or directory.
    File();

    /// @return The name of the file, without the extension.
    std::string get_name() const;

    /// @return The extension of the file, without the dot.
    std::string get_extension() const;

    /// @return The path of the file, relative to the current directory unless the directory is
    /// absolute.
    std::string get_path() const;

    /// @return The directory of the file.
    std::string get_dir_str() const;

    /// @return Whether the file exists.
    bool exists() const;

    /// Creates the file, and the directories above it that do not exist. A file that exists is
    /// emptied.
    ///
    /// @return `false` if the directories above the file or the file could not be created.
    bool create();

    /// Empties the file. A file that does not exist is created empty.
    ///
    /// @return `false` if the file could not be written.
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
    /// @param[in] file The file to write to.
    /// @param[in] flags The open mode of the stream, e.g. `std::ios::binary | std::ios::app`
    FileWriter(const File &file, std::ios_base::openmode flags);

    /// Closes the file.
    ~FileWriter();

    /// Same as write().
    FileWriter &operator<<(const std::string &str);
    FileWriter &operator<<(char byte);
    FileWriter &operator<<(const char *str);

    /// Writes a string to the file.
    ///
    /// @param[in] text The string to write.
    void write(const std::string &text);

    /// Writes a byte to the file.
    ///
    /// @param[in] byte The byte to write.
    void write(char byte);

    /// Writes a null terminated byte array to the file.
    ///
    /// @param[in] bytes The byte array to write.
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
    /// @param[in] filewriter The writer the bytes go to, which has to outlive this.
    ByteWriter(FileWriter &filewriter);

    /// A value and the number of its low bytes to write.
    struct Data
    {
        U64 value = 0;
        size_t num_bytes;

        /// @param[in] value The value to write.
        /// @param[in] num_bytes How many bytes of the value to write.
        Data(U64 value, size_t num_bytes);

        /// @param[in] value The value to write.
        /// @param[in] num_bytes How many bytes of the value to write.
        /// @param[in] little_endian Whether the least significant byte is written first, if not
        ///     the bytes of the value are reversed.
        Data(U64 value, size_t num_bytes, bool little_endian);
    };

    /// Writes the bytes of the value, the least significant one first.
    ///
    /// @param[in] data The value and how many bytes of it to write.
    /// @return This writer.
    ByteWriter &operator<<(const Data &data);

  private:
    FileWriter &m_filewriter;
};

/// Reads a whole file at once. A file that cannot be opened or read is a fatal error.
///
/// @param[in] file The file to read.
/// @return The bytes of the file.
std::vector<byte> read_file_bytes(const File &file);

/// Reads values of a given width from a vector of bytes.
class ByteReader
{
  public:
    /// Reading past the last byte throws `std::out_of_range`.
    ///
    /// @param[in] bytes The bytes to read, which have to outlive this.
    ByteReader(const std::vector<U8> &bytes) :
        m_bytes(bytes){};

    /// How many bytes to read, in which order, and the value that was read.
    struct Data
    {
        U64 val = 0;
        size_t num_bytes = 0;
        bool little_endian = true;

        /// @param[in] num_bytes How many bytes to read, little endian.
        Data(size_t num_bytes);

        /// @param[in] num_bytes How many bytes to read.
        /// @param[in] little_endian Whether the least significant byte comes first.
        Data(size_t num_bytes, bool little_endian);
    };

    /// Reads `data.num_bytes` bytes into `data.val`.
    ///
    /// @param[in,out] data The width and byte order, and where the value is stored.
    /// @return This reader.
    ByteReader &operator>>(Data &data);

    /// @return Whether there is a byte left to read.
    bool has_next();

    /// Each of the following reads one value of its width.
    ///
    /// @param[in] little_endian Whether the least significant byte comes first.
    /// @return The value that was read.
    U8 read_byte(bool little_endian = true);
    U16 read_hword(bool little_endian = true);
    U32 read_word(bool little_endian = true);
    U64 read_dword(bool little_endian = true);

    /// Skips bytes without reading them.
    ///
    /// @param[in] num_bytes How many bytes to skip.
    void skip_bytes(size_t num_bytes);

  private:
    const std::vector<U8> &m_bytes;
    size_t m_cur_byte = 0;
};
