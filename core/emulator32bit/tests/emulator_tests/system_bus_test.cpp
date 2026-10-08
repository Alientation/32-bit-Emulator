#include "emulator32bit_test/emulator32bit_test.h"

#include "emulator32bit/emulator32bit.h"

#include <vector>

namespace
{

word vaddr(word vpage, word offset = 0)
{
    return (vpage << kNumPageOffsetBits) | offset;
}

/// 4 pages of RAM at 0, 2 pages of ROM at page 8 and a disk that is in memory.
std::unique_ptr<SystemBus> make_bus()
{
    return std::make_unique<SystemBus>(std::make_unique<RAM>(4, 0), std::make_unique<ROM>(2, 8));
}

std::vector<byte> pattern(size_t size)
{
    std::vector<byte> bytes(size);
    for (size_t i = 0; i < size; i++)
    {
        bytes[i] = byte(i * 7 + 1);
    }
    return bytes;
}

/// A CPU whose process has the virtual pages 10 and 11 mapped to the physical pages 3 and 7, which
/// are not next to each other.
struct MemoryPortTest : testing::Test
{
    Emulator32bit cpu{std::make_unique<RAM>(16, 0), std::make_unique<ROM>(16, 16),
                      std::make_unique<MockDisk>()};
    long long pid = cpu.mmu->begin_process();

    MemoryPortTest()
    {
        cpu.mmu->ensure_physical_page_mapping(pid, 10, 3);
        cpu.mmu->ensure_physical_page_mapping(pid, 11, 7);
    }

    byte physical(word ppage, word offset)
    {
        return cpu.system_bus->ram->read_byte(vaddr(ppage, offset));
    }
};

} // namespace

// ---------------------------------------------------------------------------------------------
// The system bus: physical addresses only
// ---------------------------------------------------------------------------------------------

TEST(system_bus, an_address_goes_to_the_memory_or_device_that_has_it)
{
    auto bus = make_bus();
    EXPECT_EQ(bus->find_memory(0), bus->ram.get());
    EXPECT_EQ(bus->find_memory(vaddr(3, 0xFFF)), bus->ram.get());
    EXPECT_EQ(bus->find_memory(vaddr(8)), bus->rom.get());
    EXPECT_EQ(bus->find_memory(kIntcBase), &bus->intc);
    EXPECT_EQ(bus->find_memory(kTimerBase + 4), &bus->timer);
    EXPECT_EQ(bus->find_memory(kConsoleBase), &bus->console);
    EXPECT_EQ(bus->find_memory(kBlockBase), &bus->block);

    EXPECT_EQ(bus->find_memory(vaddr(4)), nullptr) << "between the RAM and the ROM";
    EXPECT_EQ(bus->find_memory(kBlockBase + kPageSize), nullptr) << "after the last device";
    EXPECT_EQ(&bus->route_memory(vaddr(9)), bus->rom.get());
}

TEST(system_bus, an_address_with_nothing_behind_it_is_an_error)
{
    auto bus = make_bus();
    EXPECT_THROW(bus->route_memory(vaddr(4)), SystemBus::Exception);
    EXPECT_THROW(bus->route_memory(0xFFFFFFFF), SystemBus::Exception);
}

TEST(system_bus, a_device_is_not_storage)
{
    auto bus = make_bus();
    EXPECT_EQ(bus->find_storage(vaddr(2)), bus->ram.get());
    EXPECT_EQ(bus->find_storage(vaddr(8)), bus->rom.get());
    EXPECT_EQ(bus->find_storage(kConsoleBase), nullptr);
}

TEST(system_bus, a_block_is_written_across_the_pages_it_covers)
{
    auto bus = make_bus();
    const std::vector<byte> data = pattern(kPageSize + 100);
    const word start = vaddr(1, kPageSize - 40);

    bus->write_block(start, data.data(), data.size());
    for (word i = 0; i < data.size(); i++)
    {
        ASSERT_EQ(bus->ram->read_byte(start + i), data[i]) << "byte " << i;
    }
    EXPECT_EQ(bus->ram->read_byte(start - 1), 0) << "nothing before it";
    EXPECT_EQ(bus->ram->read_byte(start + data.size()), 0) << "nothing after it";
}

TEST(system_bus, a_block_that_runs_into_nothing_is_an_error)
{
    auto bus = make_bus();
    const std::vector<byte> data = pattern(100);
    EXPECT_THROW(bus->write_block(vaddr(4) - 10, data.data(), data.size()), SystemBus::Exception);
}

TEST(system_bus, the_page_table_walker_reads_storage_and_not_devices)
{
    auto bus = make_bus();
    bus->ram->write_word(8, 0xAABBCCDD);
    bus->rom->write_word(vaddr(8, 4), 0x01020304);

    word out = 0;
    EXPECT_TRUE(bus->read_physical_word(8, out));
    EXPECT_EQ(out, 0xAABBCCDDu);
    EXPECT_TRUE(bus->read_physical_word(vaddr(8, 4), out)) << "a table can be in the ROM";
    EXPECT_EQ(out, 0x01020304u);

    EXPECT_FALSE(bus->read_physical_word(9, out)) << "not aligned";
    EXPECT_FALSE(bus->read_physical_word(vaddr(4), out)) << "no memory";
    EXPECT_FALSE(bus->read_physical_word(kConsoleBase, out)) << "a device could have an effect";
}

TEST(system_bus, the_page_table_walker_writes_to_the_ram_only)
{
    auto bus = make_bus();
    EXPECT_TRUE(bus->write_physical_word(12, 7));
    EXPECT_EQ(bus->ram->read_word(12), 7u);

    EXPECT_FALSE(bus->write_physical_word(13, 7)) << "not aligned";
    EXPECT_FALSE(bus->write_physical_word(vaddr(8), 7)) << "the ROM";
    EXPECT_FALSE(bus->write_physical_word(kConsoleBase, 7)) << "a device";
}

TEST(system_bus, pages_are_copied_to_and_from_the_memory_that_has_them)
{
    auto bus = make_bus();
    const std::vector<byte> data = pattern(kPageSize);
    bus->write_page(2, data.data());
    EXPECT_EQ(bus->ram->read_byte(vaddr(2, 5)), data[5]);

    std::vector<byte> back(kPageSize);
    bus->read_page(2, back.data());
    EXPECT_EQ(back, data);
    EXPECT_THROW(bus->read_page(5, back.data()), SystemBus::Exception) << "no memory there";
}

// ---------------------------------------------------------------------------------------------
// The memory port: virtual addresses, translated by the MMU, on the bus
// ---------------------------------------------------------------------------------------------

TEST_F(MemoryPortTest, an_access_goes_to_the_physical_page_of_the_virtual_page)
{
    cpu.memory.write_word(vaddr(10, 0x10), 0x11223344);
    EXPECT_EQ(cpu.system_bus->ram->read_word(vaddr(3, 0x10)), 0x11223344u);
    EXPECT_EQ(cpu.memory.read_word(vaddr(10, 0x10)), 0x11223344u);
    EXPECT_EQ(cpu.memory.read_hword(vaddr(10, 0x12)), 0x1122u);
    EXPECT_EQ(cpu.memory.read_byte(vaddr(10, 0x10)), 0x44u);

    cpu.memory.write_byte(vaddr(11, 1), 0xAB);
    cpu.memory.write_hword(vaddr(11, 2), 0xCDEF);
    EXPECT_EQ(physical(7, 1), 0xAB);
    EXPECT_EQ(cpu.system_bus->ram->read_hword(vaddr(7, 2)), 0xCDEFu);
}

TEST_F(MemoryPortTest, a_word_that_crosses_a_page_is_put_together_from_both_pages)
{
    cpu.memory.write_word(vaddr(10, kPageSize - 2), 0x11223344);
    EXPECT_EQ(physical(3, kPageSize - 2), 0x44);
    EXPECT_EQ(physical(3, kPageSize - 1), 0x33);
    EXPECT_EQ(physical(7, 0), 0x22) << "the page after 10 is 11, which is in physical page 7";
    EXPECT_EQ(physical(7, 1), 0x11);

    EXPECT_EQ(cpu.memory.read_word(vaddr(10, kPageSize - 2)), 0x11223344u);
    EXPECT_EQ(cpu.memory.read_hword(vaddr(10, kPageSize - 1)), 0x2233u);
    EXPECT_EQ(cpu.memory.read_hword(vaddr(10, kPageSize - 1)), 0x2233u);
}

TEST_F(MemoryPortTest, a_store_into_a_page_that_cannot_be_written_changes_nothing)
{
    cpu.mmu->set_vpage_permissions(pid, 11, 11, false, false);

    EXPECT_THROW(cpu.memory.write_word(vaddr(10, kPageSize - 2), 0x11223344),
                 VirtualMemory::PageFaultException);
    EXPECT_EQ(physical(3, kPageSize - 2), 0) << "the part in the page that can be written either";
    EXPECT_EQ(physical(3, kPageSize - 1), 0);
}

TEST_F(MemoryPortTest, a_block_is_written_with_a_translation_for_each_page)
{
    const std::vector<byte> data = pattern(600);
    const word start = vaddr(10, kPageSize - 256);

    cpu.memory.write_block(start, data.data(), data.size());
    for (word i = 0; i < data.size(); i++)
    {
        const bool second_page = i >= 256;
        const byte stored = second_page ? physical(7, i - 256) : physical(3, kPageSize - 256 + i);
        ASSERT_EQ(stored, data[i]) << "byte " << i;
    }
    EXPECT_EQ(physical(7, 600 - 256), 0) << "nothing after it";
}

TEST_F(MemoryPortTest, a_block_stops_at_a_page_that_cannot_be_written)
{
    cpu.mmu->set_vpage_permissions(pid, 11, 11, false, false);
    const std::vector<byte> data = pattern(600);

    EXPECT_THROW(cpu.memory.write_block(vaddr(10, kPageSize - 256), data.data(), data.size()),
                 VirtualMemory::PageFaultException);
    EXPECT_EQ(physical(3, kPageSize - 256), data[0]) << "the page before it was written";
    EXPECT_EQ(physical(7, 0), 0);
}

TEST_F(MemoryPortTest, an_instruction_is_fetched_through_the_mmu)
{
    cpu.memory.write_word(vaddr(10, 8), 0xCAFEF00D);
    EXPECT_EQ(cpu.memory.fetch_instruction(vaddr(10, 8)), 0xCAFEF00Du);
}

TEST_F(MemoryPortTest, instructions_can_be_fetched_from_the_rom_and_not_from_where_there_is_nothing)
{
    cpu.mmu->set_enabled(false); // addresses are physical
    cpu.system_bus->rom->write_word(vaddr(16, 4), 0x12345678);
    EXPECT_EQ(cpu.memory.fetch_instruction(vaddr(16, 4)), 0x12345678u);

    EXPECT_THROW(cpu.memory.fetch_instruction(0x7FFF0000), SystemBus::Exception);
}
