#include "util/console_color.h"
#include "util/types.h"

#include <gtest/gtest.h>

TEST(TypesTest, a_hexadecimal_string_has_the_prefix_and_the_digits)
{
    EXPECT_EQ(to_hex_str(U32(0xAB)), "0x000000ab");
    EXPECT_EQ(to_hex_str(U64(0xAB), 4), "0x00ab");
    EXPECT_EQ(to_hex_str(U64(0x12345), 2), "0x12345");
}

TEST(TypesTest, the_leading_zeros_and_the_prefix_are_gray_and_the_rest_is_bold)
{
    const std::string gray = ccolor::GRAY;
    const std::string bold = ccolor::BOLD_WHITE;
    const std::string reset = ccolor::RESET;

    EXPECT_EQ(color_val_str("0x00ab"), gray + "0x00" + bold + "ab" + reset);
    EXPECT_EQ(color_val_str("0x1234"), gray + "0x" + bold + "1234" + reset);

    // The last digit stays bold even when it is a zero.
    EXPECT_EQ(color_val_str("0x0000"), gray + "0x000" + bold + "0" + reset);
}
