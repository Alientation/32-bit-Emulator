#include <gtest/gtest.h>

#include "util/short_string.h"

TEST (short_string, test_default_constructor)
{
    // Test default constructor.
    {
        ShortString<> short_string;

        EXPECT_EQ (short_string.len (), 0);
        EXPECT_STREQ (short_string.str (), "");
    }

    // Test smallest buffer size.
    {
        ShortString<0> short_string;

        EXPECT_EQ (short_string.len (), 0);
        EXPECT_STREQ (short_string.str (), "");
    }

    // Test constructor truncating input string to fit inside buffer.
    {
        ShortString<0> short_string ("I");

        EXPECT_EQ (short_string.len (), 0);
        EXPECT_STREQ (short_string.str (), "");
    }

    // Test constructor truncating input string to fit inside buffer.
    {
        ShortString<0> short_string (std::string ("I"));

        EXPECT_EQ (short_string.len (), 0);
        EXPECT_STREQ (short_string.str (), "");
    }

    // Test regular case.
    {
        const char *str = "Hello World!";
        ShortString<> short_string (str);

        EXPECT_EQ (short_string.len (), strlen (str));
        EXPECT_STREQ (short_string.str (), str);
    }

    // Test regular case.
    {
        const std::string str = "Hello World!";
        ShortString<> short_string (str);

        EXPECT_EQ (short_string.len (), str.length ());
        EXPECT_STREQ (short_string.str (), str.c_str ());
    }
}