#include "util/file.h"

#include <gtest/gtest.h>

#include <filesystem>
#include <fstream>
#include <iterator>

TEST(ByteReaderTest, reading_into_the_same_data_again_replaces_the_value)
{
    std::vector<unsigned char> bytes = {0x34, 0x12, 0x78, 0x56};
    ByteReader reader(bytes);
    ByteReader::Data data(2);

    reader >> data;
    EXPECT_EQ(data.val, 0x1234u);
    reader >> data;
    EXPECT_EQ(data.val, 0x5678u);
}

TEST(ByteReaderTest, a_value_is_little_endian_unless_asked_otherwise)
{
    std::vector<unsigned char> bytes = {0x01, 0x02, 0x03, 0x04, 0x01, 0x02, 0x03, 0x04};
    ByteReader reader(bytes);

    EXPECT_EQ(reader.read_word(), 0x04030201u);
    EXPECT_EQ(reader.read_word(false), 0x01020304u);
    EXPECT_FALSE(reader.has_next());
    EXPECT_THROW(reader.read_byte(), std::out_of_range);
}

TEST(ByteWriterTest, bytes_past_the_width_of_the_value_are_zero)
{
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "aemu_byte_writer";
    std::filesystem::create_directories(dir);
    const std::string path = (dir / "bytes.bin").string();
    {
        FileWriter writer(File(path), std::ios::out | std::ios::binary);
        ByteWriter bytes(writer);
        bytes << ByteWriter::Data(0x0102030405060708ull, 10);
        writer.close();
    }

    std::ifstream in(path, std::ios::binary);
    const std::string written((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
    EXPECT_EQ(written, std::string("\x08\x07\x06\x05\x04\x03\x02\x01\0\0", 10));
    std::filesystem::remove_all(dir);
}

TEST(TrimDirPathTest, dots_are_dropped_and_dot_dot_removes_the_segment_before_it)
{
    EXPECT_EQ(trim_dir_path("a/./b"), "a/b");
    EXPECT_EQ(trim_dir_path("a\\b/../c"), "a/c");
    EXPECT_EQ(trim_dir_path("../a"), "../a");
}

TEST(FileTest, clear_empties_a_file_and_fails_for_a_directory_that_does_not_exist)
{
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "aemu_file_test";
    std::filesystem::create_directories(dir);
    const std::string path = (dir / "contents.txt").string();
    {
        std::ofstream(path) << "some text";
    }

    File file(path);
    EXPECT_TRUE(file.clear());
    EXPECT_EQ(std::filesystem::file_size(path), 0u);

    File missing((dir / "no_such_directory" / "contents.txt").string());
    EXPECT_FALSE(missing.clear());
    std::filesystem::remove_all(dir);
}
