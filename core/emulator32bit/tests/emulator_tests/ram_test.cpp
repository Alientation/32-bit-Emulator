#include "emulator32bit_test/emulator32bit_test.h"
#include "emulator32bit/memory.h"

#include <filesystem>
#include <fstream>

TEST(ram, words_and_half_words_are_little_endian_at_any_address)
{
    RAM ram(2, 0);
    ram.write_word(0, 0x11223344);
    EXPECT_EQ(ram.read_byte(0), 0x44);
    EXPECT_EQ(ram.read_byte(3), 0x11);
    EXPECT_EQ(ram.read_hword(0), 0x3344);
    EXPECT_EQ(ram.read_hword(2), 0x1122);

    // Addresses that are not aligned to the size of the access.
    for (word address = 1; address < 8; address++)
    {
        ram.write_word(address, 0xA1B2C3D4 + address);
        EXPECT_EQ(ram.read_word(address), 0xA1B2C3D4 + address) << address;
        EXPECT_EQ(ram.read_byte(address), byte(0xD4 + address)) << address;

        ram.write_hword(address, 0x1234 + address);
        EXPECT_EQ(ram.read_hword(address), 0x1234 + address) << address;
    }
}

TEST(ram, a_new_memory_is_zero)
{
    RAM ram(1, 0);
    for (word i = 0; i < kPageSize; i += 4)
    {
        ASSERT_EQ(ram.read_word(i), 0u) << i;
    }
}

TEST(ram, addresses_are_absolute)
{
    RAM ram(2, 10);
    const word start = 10 * kPageSize;
    ram.write_word(start + 8, 0xCAFEF00D);
    EXPECT_EQ(ram.read_word(start + 8), 0xCAFEF00Du);
    EXPECT_EQ(ram.read_byte(start + 8), 0x0D);
}

TEST(ram, bounds_are_the_pages_of_the_memory)
{
    RAM ram(2, 10);
    EXPECT_FALSE(ram.in_bounds(10 * kPageSize - 1));
    EXPECT_TRUE(ram.in_bounds(10 * kPageSize));
    EXPECT_TRUE(ram.in_bounds(12 * kPageSize - 1));
    EXPECT_FALSE(ram.in_bounds(12 * kPageSize));
}

TEST(ram, memory_at_the_top_of_the_address_space_has_bounds)
{
    // Its end is 2^32, which is not an address, and used to wrap around to 0.
    RAM ram(2, 0xFFFFE);
    EXPECT_FALSE(ram.in_bounds(0xFFFFDFFF));
    EXPECT_TRUE(ram.in_bounds(0xFFFFE000));
    EXPECT_TRUE(ram.in_bounds(0xFFFFFFFF));
    EXPECT_FALSE(ram.in_bounds(0));
}

TEST(ram, memory_without_pages_has_no_addresses)
{
    RAM none(0, 0);
    EXPECT_FALSE(none.in_bounds(0));
    EXPECT_FALSE(none.in_bounds(0xFFFFFFFF));
}

TEST(ram, memories_overlap_when_they_share_a_page)
{
    RAM a(4, 0);
    RAM b(4, 3);
    RAM c(4, 4);
    RAM empty(0, 1);
    EXPECT_TRUE(a.overlap(b));
    EXPECT_FALSE(a.overlap(c));
    EXPECT_FALSE(a.overlap(empty));
}

TEST(rom, an_image_file_is_loaded_and_never_written_back)
{
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "aemu_rom_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const std::string path = (dir / "rom.img").string();
    {
        std::ofstream image(path, std::ios::binary);
        const char bytes[] = {0x11, 0x22, 0x33, 0x44};
        image.write(bytes, sizeof(bytes));
    }

    {
        ROM rom(File(path), 1, 0);
        EXPECT_EQ(rom.read_word(0), 0x44332211u);
        EXPECT_EQ(rom.read_word(4), 0u);

        // Physical loads write the ROM, and that stays in memory.
        rom.write_word(0, 0xDEADBEEF);
    }

    EXPECT_EQ(std::filesystem::file_size(path), 4u) << "the file was rewritten";
    std::ifstream image(path, std::ios::binary);
    char read[4] = {};
    image.read(read, sizeof(read));
    EXPECT_EQ(read[0], 0x11);
    EXPECT_EQ(read[3], 0x44);

    std::filesystem::remove_all(dir);
}

TEST(rom, an_image_larger_than_the_rom_is_an_error)
{
    const std::filesystem::path dir = std::filesystem::temp_directory_path() / "aemu_rom_big_test";
    std::filesystem::remove_all(dir);
    std::filesystem::create_directories(dir);
    const std::string path = (dir / "rom.img").string();
    {
        std::ofstream image(path, std::ios::binary);
        image << std::string(kPageSize + 1, 'x');
    }
    EXPECT_THROW(ROM(File(path), 1, 0), ROM::ROM_Exception);
    std::filesystem::remove_all(dir);
}
