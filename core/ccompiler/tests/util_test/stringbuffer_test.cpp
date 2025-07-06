#include <ccompiler_test/ccompiler_test.h>

TEST (stringbuffer, stringbuffer_buffer_null_buffer_ptr)
{
    stringbuffer_t stringbuffer{};
    stringbuffer_init (&stringbuffer);
    stringbuffer_free (&stringbuffer);

    EXPECT_EQ(stringbuffer.buf, nullptr);
}

TEST (stringbuffer, stringbuffer_append)
{
    stringbuffer_t stringbuffer{};
    stringbuffer_init (&stringbuffer);

    const char *str1 = "Hello World!";
    const size_t str1_len = strlen(str1);

    const size_t initial_capacity = stringbuffer.capacity;
    stringbuffer_append (&stringbuffer, str1);

    EXPECT_EQ(stringbuffer.length, str1_len);
    EXPECT_EQ(stringbuffer.buf[str1_len], '\0');
    EXPECT_STREQ(stringbuffer.buf, str1);

    const char *str2 = " This is Bob.";
    const size_t str2_len = strlen(str2);
    stringbuffer_append (&stringbuffer, str2);

    EXPECT_GT(stringbuffer.capacity, initial_capacity);
    EXPECT_GE(stringbuffer.capacity, str1_len + str2_len + 1);
    EXPECT_EQ(stringbuffer.length, str1_len + str2_len);
    EXPECT_EQ(stringbuffer.buf[str1_len + str2_len], '\0');
    EXPECT_STREQ(stringbuffer.buf, "Hello World! This is Bob.");

    stringbuffer_free (&stringbuffer);
}
