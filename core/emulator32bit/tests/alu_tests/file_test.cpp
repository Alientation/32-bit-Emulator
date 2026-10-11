#include "util/file.h"

#include <gtest/gtest.h>

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

TEST(TrimDirPathTest, dots_are_dropped_and_dot_dot_removes_the_segment_before_it)
{
    EXPECT_EQ(trim_dir_path("a/./b"), "a/b");
    EXPECT_EQ(trim_dir_path("a\\b/../c"), "a/c");
    EXPECT_EQ(trim_dir_path("../a"), "../a");
}
