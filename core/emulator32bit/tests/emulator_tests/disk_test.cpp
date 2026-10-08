#include "emulator32bit_test/emulator32bit_test.h"

#include "emulator32bit/disk.h"

#include <filesystem>
#include <fstream>

class DiskTest : public ::testing::Test
{
  protected:
    void SetUp() override
    {
        const auto *info = ::testing::UnitTest::GetInstance()->current_test_info();
        m_dir = std::filesystem::temp_directory_path()
                / (std::string("aemu_disk_test_") + info->name());
        std::filesystem::remove_all(m_dir);
        std::filesystem::create_directories(m_dir);
    }

    void TearDown() override
    {
        std::filesystem::remove_all(m_dir);
    }

    File disk_file() const
    {
        return File((m_dir / "disk.img").string(), true);
    }

    std::filesystem::path m_dir;
};

TEST_F(DiskTest, read_page_returns_exactly_one_page_with_the_written_data)
{
    Disk disk(disk_file(), 4, 0);

    std::vector<byte> page(kPageSize);
    for (word i = 0; i < kPageSize; i++) page[i] = byte(i * 7 + 1);
    disk.write_page(2, page);

    const std::vector<byte> back = disk.read_page(2);
    ASSERT_EQ(back.size(), kPageSize);
    EXPECT_EQ(back, page);
}

TEST_F(DiskTest, pages_survive_cache_eviction)
{
    // More pages than cache slots, so every page has to go through the file.
    const word npages = kDiskCacheSize * 2;
    Disk disk(disk_file(), npages, 0);

    for (word p = 0; p < npages; p++)
    {
        disk.write_page(p, std::vector<byte>(kPageSize, byte(p + 1)));
    }
    for (word p = 0; p < npages; p++)
    {
        const std::vector<byte> back = disk.read_page(p);
        ASSERT_EQ(back.size(), kPageSize);
        EXPECT_EQ(back.front(), byte(p + 1)) << "page " << p;
        EXPECT_EQ(back.back(), byte(p + 1)) << "page " << p;
    }
}

TEST_F(DiskTest, word_access_across_a_page_boundary)
{
    Disk disk(disk_file(), 4, 0);

    const word address = kPageSize - 2;
    disk.write_word(address, 0xDEADBEEF);
    EXPECT_EQ(disk.read_word(address), 0xDEADBEEFu);

    const std::vector<byte> first = disk.read_page(0);
    const std::vector<byte> second = disk.read_page(1);
    EXPECT_EQ(first[kPageSize - 2], 0xEF);
    EXPECT_EQ(first[kPageSize - 1], 0xBE);
    EXPECT_EQ(second[0], 0xAD);
    EXPECT_EQ(second[1], 0xDE);
}

TEST_F(DiskTest, a_page_that_is_not_on_the_disk_is_an_error)
{
    Disk disk(disk_file(), 4, 0);

    EXPECT_THROW(disk.read_page(4), Disk::DiskReadException);
    EXPECT_THROW(disk.write_page(100, std::vector<byte>(kPageSize)), Disk::DiskReadException);
    EXPECT_THROW(disk.read_word(4 * kPageSize), Disk::DiskReadException);
}

TEST_F(DiskTest, a_page_that_was_not_written_to_is_not_written_back)
{
    // Pages 0 and 32 share the slot of the cache.
    const word other = kDiskCacheSize;
    Disk disk(disk_file(), other * 2, 0);
    const auto set_file_page = [&](word page, byte value)
    {
        std::fstream file(disk_file().get_path(), std::ios::binary | std::ios::in | std::ios::out);
        file.seekp(std::streamoff(page) * kPageSize);
        file.write(std::string(kPageSize, char(value)).data(), kPageSize);
    };

    disk.write_page(0, std::vector<byte>(kPageSize, 0xA0));
    disk.write_page(other, std::vector<byte>(kPageSize, 0xA1)); // page 0 goes to the file
    set_file_page(0, 0xB0);
    EXPECT_EQ(disk.read_page(0).front(), 0xB0);     // page 0 is loaded, nothing was written to it
    set_file_page(0, 0xC0);
    EXPECT_EQ(disk.read_page(other).front(), 0xA1); // page 0 goes, and it is clean
    EXPECT_EQ(disk.read_page(0).front(), 0xC0) << "a page that was only read was written back";
}

TEST_F(DiskTest, a_new_page_is_zero_whatever_the_page_was_used_for_before)
{
    Disk disk(disk_file(), 4, 0);

    std::vector<word> pages;
    for (int i = 0; i < 4; i++)
    {
        pages.push_back(disk.get_free_page());
        disk.write_page(pages.back(), std::vector<byte>(kPageSize, 0xEE));
    }
    for (const word page : pages)
    {
        disk.return_page(page);
    }

    for (int i = 0; i < 4; i++)
    {
        const std::vector<byte> page = disk.read_page(disk.get_free_page());
        EXPECT_EQ(page, std::vector<byte>(kPageSize, 0));
    }
}

TEST_F(DiskTest, saved_pages_are_there_after_reopening_the_disk)
{
    {
        Disk disk(disk_file(), 4, 0);
        const word page = disk.get_free_page();
        disk.write_page(page, std::vector<byte>(kPageSize, 0x5A));
        disk.save();
    }

    Disk reopened(disk_file(), 4, 0);
    EXPECT_EQ(reopened.read_page(0).front(), 0x5A);
    EXPECT_EQ(reopened.get_free_page(), 1u) << "page 0 is still taken";
}

TEST_F(DiskTest, byte_addresses_are_relative_to_the_disk_start_page)
{
    const word lo_page = 100;
    Disk disk(disk_file(), 4, lo_page);

    disk.write_word((lo_page << kNumPageOffsetBits) + 8, 0x11223344);
    EXPECT_EQ(disk.read_word((lo_page << kNumPageOffsetBits) + 8), 0x11223344u);

    // The same bytes are page 0 of the disk when read as a page.
    const std::vector<byte> page = disk.read_page(0);
    EXPECT_EQ(page[8], 0x44);
    EXPECT_EQ(page[11], 0x11);
}

// The mock disk is where the tests keep pages when there is no file. It used to throw away what
// was written, so a page that was swapped out came back as zeros.
TEST(mock_disk, keeps_what_is_written)
{
    MockDisk disk;
    const word first = disk.get_free_page();
    const word second = disk.get_free_page();
    ASSERT_NE(first, second);

    disk.write_page(first, std::vector<byte>(kPageSize, 0x11));
    disk.write_page(second, std::vector<byte>(kPageSize, 0x22));
    EXPECT_EQ(disk.read_page(first), std::vector<byte>(kPageSize, 0x11));
    EXPECT_EQ(disk.read_page(second), std::vector<byte>(kPageSize, 0x22));
}

TEST(mock_disk, a_page_that_was_not_written_is_zeros)
{
    MockDisk disk;
    EXPECT_EQ(disk.read_page(disk.get_free_page()), std::vector<byte>(kPageSize, 0));
}

TEST(mock_disk, a_returned_page_is_given_out_again_as_zeros)
{
    MockDisk disk;
    const word page = disk.get_free_page();
    disk.write_page(page, std::vector<byte>(kPageSize, 0x7E));
    disk.return_page(page);
    disk.return_page(page); // twice is the same as once

    const word again = disk.get_free_page();
    EXPECT_EQ(again, page);
    EXPECT_EQ(disk.read_page(again), std::vector<byte>(kPageSize, 0));
    EXPECT_NE(disk.get_free_page(), again) << "the page was only returned once";
}

TEST(mock_disk, only_whole_pages_can_be_written)
{
    MockDisk disk;
    EXPECT_THROW(disk.write_page(disk.get_free_page(), std::vector<byte>(10)),
                 Disk::DiskWriteException);
}
