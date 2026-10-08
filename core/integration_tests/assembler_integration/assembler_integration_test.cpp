#include <gtest/gtest.h>

TEST(assembler_integration, paths_defined)
{
    EXPECT_FALSE(std::string(EMULATOR_PATH).empty());
    EXPECT_FALSE(std::string(ASSEMBLER_PATH).empty());
}