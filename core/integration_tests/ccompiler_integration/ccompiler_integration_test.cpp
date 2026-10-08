#include <gtest/gtest.h>

TEST(compiler_integration, paths_defined)
{
    EXPECT_FALSE(std::string(EMULATOR_PATH).empty());
    EXPECT_FALSE(std::string(ASSEMBLER_PATH).empty());
    EXPECT_FALSE(std::string(CCOMPILER_PATH).empty());
}