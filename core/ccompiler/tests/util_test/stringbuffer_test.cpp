#include <ccompiler_test/ccompiler_test.h>

TEST (stringbuffer, stringbuffer_buffer_null_buffer_ptr)
{
    stringbuffer_t sb{};
    stringbuffer_init (&sb);
    stringbuffer_free (&sb);

    EXPECT_EQ (sb.buf, nullptr);
}

TEST (stringbuffer, stringbuffer_append)
{
    stringbuffer_t sb{};
    stringbuffer_init (&sb);

    const char *str1 = "Hello World!";
    const size_t str1_len = strlen (str1);

    const size_t initial_capacity = sb.capacity;
    stringbuffer_append (&sb, str1);

    EXPECT_EQ (sb.length, str1_len);
    EXPECT_EQ (sb.buf[str1_len], '\0');
    EXPECT_STREQ (sb.buf, str1);

    const char *str2 = " This is Bob.";
    const size_t str2_len = strlen (str2);
    stringbuffer_append (&sb, str2);

    EXPECT_GT (sb.capacity, initial_capacity);
    EXPECT_GE (sb.capacity, str1_len + str2_len + 1);
    EXPECT_EQ (sb.length, str1_len + str2_len);
    EXPECT_EQ (sb.buf[str1_len + str2_len], '\0');
    EXPECT_STREQ (sb.buf, "Hello World! This is Bob.");

    stringbuffer_free (&sb);
    EXPECT_EQ (sb.buf, nullptr);
    EXPECT_EQ (sb.length, 0);
}

TEST (stringbuffer, stringbuffer_appendf)
{
    stringbuffer_t sb{};
    stringbuffer_init (&sb);

    const char *target_str = "Hello World!. Lucky number is 11.";
    const size_t target_str_len = strlen (target_str);
    stringbuffer_appendf (&sb, "Hello %s. Lucky number is %d.", "World!", 11);

    EXPECT_EQ (sb.length, target_str_len);
    EXPECT_GT (sb.capacity, sb.length);
    EXPECT_EQ (sb.buf[target_str_len], '\0');
    EXPECT_STREQ (sb.buf, target_str);

    stringbuffer_free (&sb);
}

TEST (stringbuffer, stringbuffer_appendl)
{
    stringbuffer_t sb{};
    stringbuffer_init (&sb);

    const size_t limit = 5;
    stringbuffer_appendl (&sb, "Hello World!", limit);

    EXPECT_EQ (sb.length, limit);
    EXPECT_GT (sb.capacity, sb.length);
    EXPECT_EQ (sb.buf[limit], '\0');
    EXPECT_STREQ (sb.buf, "Hello");

    stringbuffer_free (&sb);
}

TEST (stringbuffer, stringbuffer_appendsb)
{
    stringbuffer_t sb1{};
    stringbuffer_t sb2{};
    stringbuffer_init (&sb1);
    stringbuffer_init (&sb2);

    const char *str1 = "Hello World!";
    const char *str2 = " I am Bob.";
    const size_t str1_len = strlen (str1);
    const size_t str2_len = strlen (str2);
    stringbuffer_append (&sb1, str1);
    stringbuffer_append (&sb2, str2);

    EXPECT_EQ (sb1.length, str1_len);
    EXPECT_GT (sb1.capacity, sb1.length);
    EXPECT_EQ (sb1.buf[str1_len], '\0');
    EXPECT_STREQ (sb1.buf, str1);

    EXPECT_EQ (sb2.length, str2_len);
    EXPECT_GT (sb2.capacity, sb2.length);
    EXPECT_EQ (sb2.buf[str2_len], '\0');
    EXPECT_STREQ (sb2.buf, str2);

    stringbuffer_appendsb (&sb1, &sb2);

    EXPECT_EQ (sb1.length, str1_len + str2_len);
    EXPECT_GT (sb1.capacity, sb1.length);
    EXPECT_EQ (sb1.buf[str1_len + str2_len], '\0');
    EXPECT_STREQ (sb1.buf, "Hello World! I am Bob.");

    stringbuffer_free (&sb1);
    stringbuffer_free (&sb2);
}

TEST (stringbuffer, stringbuffer_appendsb_empty)
{
    stringbuffer_t sb1{};
    stringbuffer_t sb2{};
    stringbuffer_init (&sb1);
    stringbuffer_init (&sb2);

    const char *str1 = "Hello World!";
    const size_t str1_len = strlen (str1);
    stringbuffer_append (&sb1, str1);

    EXPECT_EQ (sb1.length, str1_len);
    EXPECT_GT (sb1.capacity, sb1.length);
    EXPECT_EQ (sb1.buf[str1_len], '\0');
    EXPECT_STREQ (sb1.buf, str1);

    EXPECT_EQ (sb2.length, 0);

    stringbuffer_appendsb (&sb1, &sb2);

    EXPECT_EQ (sb1.length, str1_len);
    EXPECT_GT (sb1.capacity, sb1.length);
    EXPECT_EQ (sb1.buf[str1_len], '\0');
    EXPECT_STREQ (sb1.buf, str1);

    stringbuffer_free (&sb1);
    stringbuffer_free (&sb2);
}

TEST (stringbuffer, stringbuffer_clear)
{
    stringbuffer_t sb{};
    stringbuffer_init (&sb);

    const char *str = "Hello World!";
    const size_t str_len = strlen (str);
    stringbuffer_append (&sb, str);

    EXPECT_EQ (sb.length, str_len);
    EXPECT_GT (sb.capacity, sb.length);
    EXPECT_EQ (sb.buf[str_len], '\0');
    EXPECT_STREQ (sb.buf, str);

    stringbuffer_clear (&sb);
    EXPECT_EQ (sb.length, 0);
    EXPECT_EQ (sb.buf[0], '\0');

    stringbuffer_free (&sb);
}

TEST (stringbuffer, stringbuffer_clear_empty)
{
    stringbuffer_t sb{};
    stringbuffer_init (&sb);

    stringbuffer_clear (&sb);
    EXPECT_EQ (sb.length, 0);

    stringbuffer_free (&sb);
}