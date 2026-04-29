#include <ccompiler_test/ccompiler_test.h>

TEST (stringbuffer, stringbuffer_buffer_null_buffer_ptr)
{
    stringbuffer_t sb{};
    sb_init (&sb);

    sb_append (&sb, "");

    sb_free (&sb);

    EXPECT_EQ (sb.buf, nullptr);
}

TEST (stringbuffer, sb_append)
{
    stringbuffer_t sb{};
    sb_init (&sb);

    const char *str1 = "Hello World!";
    const size_t str1_len = strlen (str1);

    const size_t initial_capacity = sb.capacity;
    sb_append (&sb, str1);

    EXPECT_EQ (sb.length, str1_len);
    EXPECT_EQ (sb.buf[str1_len], '\0');
    EXPECT_STREQ (sb.buf, str1);

    const char *str2 = " This is Bob.";
    const size_t str2_len = strlen (str2);
    sb_append (&sb, str2);

    EXPECT_GT (sb.capacity, initial_capacity);
    EXPECT_GE (sb.capacity, str1_len + str2_len + 1);
    EXPECT_EQ (sb.length, str1_len + str2_len);
    EXPECT_EQ (sb.buf[str1_len + str2_len], '\0');
    EXPECT_STREQ (sb.buf, "Hello World! This is Bob.");

    sb_free (&sb);
    EXPECT_EQ (sb.buf, nullptr);
    EXPECT_EQ (sb.length, 0);
}

TEST (stringbuffer, sb_appendf)
{
    stringbuffer_t sb{};
    sb_init (&sb);

    const char *target_str = "Hello World!. Lucky number is 11.";
    const size_t target_str_len = strlen (target_str);
    sb_appendf (&sb, "Hello %s. Lucky number is %d.", "World!", 11);

    EXPECT_EQ (sb.length, target_str_len);
    EXPECT_GT (sb.capacity, sb.length);
    EXPECT_EQ (sb.buf[target_str_len], '\0');
    EXPECT_STREQ (sb.buf, target_str);

    sb_free (&sb);
}

TEST (stringbuffer, stringbuffer_appendl)
{
    stringbuffer_t sb{};
    sb_init (&sb);

    const size_t limit = 5;
    sb_appendl (&sb, "Hello World!", limit);

    EXPECT_EQ (sb.length, limit);
    EXPECT_GT (sb.capacity, sb.length);
    EXPECT_EQ (sb.buf[limit], '\0');
    EXPECT_STREQ (sb.buf, "Hello");

    sb_free (&sb);
}

TEST (stringbuffer, sb_appendsb)
{
    stringbuffer_t sb1{};
    stringbuffer_t sb2{};
    sb_init (&sb1);
    sb_init (&sb2);

    const char *str1 = "Hello World!";
    const char *str2 = " I am Bob.";
    const size_t str1_len = strlen (str1);
    const size_t str2_len = strlen (str2);
    sb_append (&sb1, str1);
    sb_append (&sb2, str2);

    EXPECT_EQ (sb1.length, str1_len);
    EXPECT_GT (sb1.capacity, sb1.length);
    EXPECT_EQ (sb1.buf[str1_len], '\0');
    EXPECT_STREQ (sb1.buf, str1);

    EXPECT_EQ (sb2.length, str2_len);
    EXPECT_GT (sb2.capacity, sb2.length);
    EXPECT_EQ (sb2.buf[str2_len], '\0');
    EXPECT_STREQ (sb2.buf, str2);

    sb_appendsb (&sb1, &sb2);

    EXPECT_EQ (sb1.length, str1_len + str2_len);
    EXPECT_GT (sb1.capacity, sb1.length);
    EXPECT_EQ (sb1.buf[str1_len + str2_len], '\0');
    EXPECT_STREQ (sb1.buf, "Hello World! I am Bob.");

    sb_free (&sb1);
    sb_free (&sb2);
}

TEST (stringbuffer, stringbuffer_appendsb_empty)
{
    stringbuffer_t sb1{};
    stringbuffer_t sb2{};
    sb_init (&sb1);
    sb_init (&sb2);

    const char *str1 = "Hello World!";
    const size_t str1_len = strlen (str1);
    sb_append (&sb1, str1);

    EXPECT_EQ (sb1.length, str1_len);
    EXPECT_GT (sb1.capacity, sb1.length);
    EXPECT_EQ (sb1.buf[str1_len], '\0');
    EXPECT_STREQ (sb1.buf, str1);

    EXPECT_EQ (sb2.length, 0);

    sb_appendsb (&sb1, &sb2);

    EXPECT_EQ (sb1.length, str1_len);
    EXPECT_GT (sb1.capacity, sb1.length);
    EXPECT_EQ (sb1.buf[str1_len], '\0');
    EXPECT_STREQ (sb1.buf, str1);

    sb_free (&sb1);
    sb_free (&sb2);
}

TEST (stringbuffer, sb_clear)
{
    stringbuffer_t sb{};
    sb_init (&sb);

    const char *str = "Hello World!";
    const size_t str_len = strlen (str);
    sb_append (&sb, str);

    EXPECT_EQ (sb.length, str_len);
    EXPECT_GT (sb.capacity, sb.length);
    EXPECT_EQ (sb.buf[str_len], '\0');
    EXPECT_STREQ (sb.buf, str);

    sb_clear (&sb);
    EXPECT_EQ (sb.length, 0);
    EXPECT_EQ (sb.buf[0], '\0');

    sb_free (&sb);
}

TEST (stringbuffer, stringbuffer_clear_empty)
{
    stringbuffer_t sb{};
    sb_init (&sb);

    sb_clear (&sb);
    EXPECT_EQ (sb.length, 0);

    sb_free (&sb);
}