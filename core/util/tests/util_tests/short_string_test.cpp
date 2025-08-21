#include <gtest/gtest.h>

#include "util/short_string.h"

TEST (short_string, test_constructor)
{
    // Test default constructor.
    {
        const ShortString<> short_string;

        EXPECT_EQ (short_string.len (), 0);
        EXPECT_STREQ (short_string.str (), "");
    }

    // Test smallest buffer size.
    {
        const ShortString<0> short_string;

        EXPECT_EQ (short_string.len (), 0);
        EXPECT_STREQ (short_string.str (), "");
    }

    // Test constructor truncating input string to fit inside buffer.
    {
        const ShortString<0> short_string ("I");

        EXPECT_EQ (short_string.len (), 0);
        EXPECT_STREQ (short_string.str (), "");
    }

    // Test regular case.
    {
        const char *str = "Hello World!";
        const ShortString<> short_string (str);

        EXPECT_EQ (short_string.len (), strlen (str));
        EXPECT_STREQ (short_string.str (), str);
    }

    // Test regular case.
    {
        const std::string str = "Hello World!";
        const ShortString<> short_string (str.c_str ());

        EXPECT_EQ (short_string.len (), str.length ());
        EXPECT_STREQ (short_string.str (), str.c_str ());
    }
}

TEST (short_string, test_addition)
{
    // Test simple addition.
    {
        const std::string str_1 = "Hello";
        ShortString<24> short_string_1 (str_1.c_str ());

        const std::string str_2 = " World!";
        const ShortString<32> short_string_2 (str_2.c_str ());

        const std::string str_final = str_1 + str_2;

        const ShortString short_string_final = short_string_1 + short_string_2;

        EXPECT_EQ (short_string_final.len (), str_final.length ());
        EXPECT_STREQ (short_string_final.str (), str_final.c_str ());

        EXPECT_EQ (short_string_1.len (), str_1.length ());
        EXPECT_STREQ (short_string_1.str (), str_1.c_str ());

        short_string_1 += short_string_2;

        EXPECT_EQ (short_string_1.len (), str_final.length ());
        EXPECT_STREQ (short_string_1.str (), str_final.c_str ());

        EXPECT_EQ (short_string_2.len (), str_2.length ());
        EXPECT_STREQ (short_string_2.str (), str_2.c_str ());
    }

    // Test compound addition.
    {
        const char *str_1 = "a";
        const char *str_2 = "b";
        const char *str_3 = "c";

        ShortString<3> short_string_1 (str_1);
        short_string_1 += ShortString (str_2) + str_3;
        EXPECT_EQ (short_string_1.len (), 3);
        EXPECT_STREQ (short_string_1.str (), "abc");

        // TODO: investigate why this compiles? I removed the operator overload for const char * on the LHS.
        const ShortString<3> short_string_2 = str_1 + ShortString (str_2) + str_3;
        EXPECT_EQ (short_string_2.len (), 3);
        EXPECT_STREQ (short_string_2.str (), "abc");

        const ShortString<3> short_string_3 = str_1 + ShortString (str_2) + ShortString (str_3);
        EXPECT_EQ (short_string_3.len (), 3);
        EXPECT_STREQ (short_string_3.str (), "abc");

        const ShortString<3> short_string_4 (str_1);
        const ShortString short_string_5 = short_string_4 + str_2 + ShortString (str_3);
        EXPECT_EQ (short_string_5.len (), 3);
        EXPECT_STREQ (short_string_5.str (), "abc");
    }

    // Test exceed storage limit.
    {
        const char *str_1 = "12";
        const char *str_2 = "34";
        const char *str_3 = "5";

        ShortString<4> short_string_1 (str_1);
        short_string_1 += ShortString (str_2) + ShortString (str_3);
        EXPECT_EQ (short_string_1.len (), 4);
        EXPECT_STREQ (short_string_1.str (), "1234");

        ShortString<3> short_string_2 (str_1);
        short_string_2 += ShortString (str_2) + ShortString (str_3);
        EXPECT_EQ (short_string_2.len (), 3);
        EXPECT_STREQ (short_string_2.str (), "123");

        ShortString<4> short_string_3 (str_1);
        short_string_3 += ShortString (str_2) + str_3;
        EXPECT_EQ (short_string_3.len (), 4);
        EXPECT_STREQ (short_string_3.str (), "1234");
    }
}

TEST (short_string, test_repeat)
{
    // Simple repeat.
    {
        const char *str_1 = "1";
        const char *str_2 = "2";

        ShortString short_string_1 (str_1);
        short_string_1 *= 3;
        short_string_1 += str_2;
        short_string_1 *= 2;
        EXPECT_EQ (short_string_1.len (), 8);
        EXPECT_STREQ (short_string_1.str (), "11121112");

        ShortString short_string_2 (str_1);
        short_string_2 *= 3;
        short_string_2 += ShortString (str_2) * 2;
        short_string_2 *= 3;
        EXPECT_EQ (short_string_2.len (), 15);
        EXPECT_STREQ (short_string_2.str (), "111221112211122");

        ShortString short_string_3 (str_1);
        const ShortString short_string_4 (str_2);
        short_string_3 += short_string_4 * 2;
        short_string_3 *= 3;
        EXPECT_EQ (short_string_3.len (), 9);
        EXPECT_STREQ (short_string_3.str (), "122122122");
    }

    // Edge cases
    {
        const char *str_1 = "1";
        const char *str_2 = "2";

        ShortString short_string_1 (str_1);
        short_string_1 *= 0;
        EXPECT_EQ (short_string_1.len (), 0);
        EXPECT_STREQ (short_string_1.str (), "");

        ShortString<2> short_string_2 (str_1);
        short_string_2 *= 3;
        EXPECT_EQ (short_string_2.len (), 2);
        EXPECT_STREQ (short_string_2.str (), "11");

        ShortString<3> short_string_3 (str_1);
        short_string_3 += ShortString (str_2) * 2;
        short_string_3 *= 2;
        EXPECT_EQ (short_string_3.len (), 3);
        EXPECT_STREQ (short_string_3.str (), "122");

        ShortString<7> short_string_4 (str_1);
        short_string_4 += ShortString (str_2) * 2;
        short_string_4 *= 3;
        EXPECT_EQ (short_string_4.len (), 7);
        EXPECT_STREQ (short_string_4.str (), "1221221");
    }
}