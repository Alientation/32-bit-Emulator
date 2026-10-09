#include "emulator32bit_test/emulator32bit_test.h"

#include "emulator32bit/disk.h"
#include "emulator32bit/virtual_memory.h"

#include <map>
#include <optional>
#include <set>

namespace
{

using Fault = VirtualMemory::PageFaultException;
using Access = VirtualMemory::AccessType;

word vaddr(word vpage, word offset = 0)
{
    return (vpage << kNumPageOffsetBits) | offset;
}

/// The physical memory behind a virtual memory: pages that start as zeros. Keeps what the virtual
/// memory read from it and wrote to it.
class FakePhysicalPages : public VirtualMemory::PhysicalPages
{
  public:
    std::map<word, std::vector<byte>> pages;
    std::vector<word> read;
    std::vector<word> written;

    std::vector<byte> &page(word ppage)
    {
        auto &contents = pages[ppage];
        contents.resize(kPageSize, 0);
        return contents;
    }

    void read_page(word ppage, byte *out) override
    {
        read.push_back(ppage);
        std::copy(page(ppage).begin(), page(ppage).end(), out);
    }

    void write_page(word ppage, const byte *data) override
    {
        written.push_back(ppage);
        page(ppage).assign(data, data + kPageSize);
    }

    bool read_physical_word(word address, word &out) override
    {
        const auto &contents = page(address >> kNumPageOffsetBits);
        const word offset = address & (kPageSize - 1);
        out = contents[offset] | (contents[offset + 1] << 8) | (contents[offset + 2] << 16)
              | (word(contents[offset + 3]) << 24);
        return true;
    }

    bool write_physical_word(word address, word value) override
    {
        auto &contents = page(address >> kNumPageOffsetBits);
        const word offset = address & (kPageSize - 1);
        for (word i = 0; i < 4; i++)
        {
            contents[offset + i] = byte(value >> (8 * i));
        }
        return true;
    }
};

/// A virtual memory over a fake physical memory and a disk in memory.
struct Machine
{
    MockDisk disk;
    FakePhysicalPages physical;
    VirtualMemory vm;

    explicit Machine(word frame_lo = 0, word frame_pages = kMaxPhysicalPages) :
        vm(&disk, frame_lo, frame_pages)
    {
        vm.set_physical_pages(&physical);
    }
};

/// The physical page that the virtual page is in, bringing it into memory if it is not.
word ppage_of(VirtualMemory &vm, word vpage, Access access = Access::READ)
{
    return vm.translate_address(vaddr(vpage), access) >> kNumPageOffsetBits;
}

/// What the fault of an access is, or nothing if there is none.
std::optional<Fault::Reason> fault_of(VirtualMemory &vm, word vpage, Access access)
{
    try
    {
        ppage_of(vm, vpage, access);
    }
    catch (const Fault &fault)
    {
        EXPECT_EQ(fault.get_vpage(), vpage);
        EXPECT_EQ(fault.get_access(), access);
        return fault.get_reason();
    }
    return std::nullopt;
}

/// What the fault of fetching an instruction from the page is, or nothing if there is none.
std::optional<Fault::Reason> fetch_fault_of(VirtualMemory &vm, word vpage)
{
    try
    {
        vm.translate_fetch(vaddr(vpage));
    }
    catch (const Fault &fault)
    {
        return fault.get_reason();
    }
    return std::nullopt;
}

/// The physical page that an instruction at the start of the virtual page is fetched from.
word fetch_ppage_of(VirtualMemory &vm, word vpage)
{
    return vm.translate_fetch(vaddr(vpage)) >> kNumPageOffsetBits;
}

std::vector<byte> filled(byte value)
{
    return std::vector<byte>(kPageSize, value);
}

/// Page tables in the fake physical memory: the first level table is in physical page 1.
constexpr word kTableBase = word(1) << kNumPageOffsetBits;

/// Maps the virtual page to the physical page with the bits (VirtualMemory::kPte...). The second
/// level table that holds the entry is in physical page `table`.
void set_pte(Machine &m, word vpage, word ppage, word bits, word table)
{
    constexpr word first_level_bits = 10;
    m.physical.write_physical_word(kTableBase + ((vpage >> first_level_bits) << 2),
                                   (table << kNumPageOffsetBits) | VirtualMemory::kPteValid);
    m.physical.write_physical_word((table << kNumPageOffsetBits)
                                       + ((vpage & ((1u << first_level_bits) - 1)) << 2),
                                   (ppage << kNumPageOffsetBits) | VirtualMemory::kPteValid | bits);
}

/// A machine whose virtual memory translates with the page tables.
struct WalkMachine : Machine
{
    WalkMachine()
    {
        vm.set_page_table_base(kTableBase);
        vm.set_walk_enabled(true);
    }
};

} // namespace

TEST(virtual_memory, force_mapping_maps_vpage_to_requested_ppage)
{
    Machine m;
    const long long pid = m.vm.begin_process();

    m.vm.ensure_physical_page_mapping(pid, 5, 3);

    EXPECT_TRUE(m.vm.has_vpage(pid, 5));
    EXPECT_EQ(m.physical.written, std::vector<word>{3}) << "the page is brought into page 3";
    EXPECT_EQ(m.vm.translate_address(vaddr(5, 0x10)), vaddr(3, 0x10));
}

TEST(virtual_memory, force_mapping_to_a_used_ppage_evicts_the_old_page)
{
    Machine m;
    const long long pid = m.vm.begin_process();

    m.vm.ensure_physical_page_mapping(pid, 5, 3);
    m.physical.page(3) = filled(0xA5);
    m.physical.read.clear();

    m.vm.ensure_physical_page_mapping(pid, 6, 3);

    EXPECT_EQ(m.physical.read, std::vector<word>{3}) << "the old page is saved";
    EXPECT_EQ(m.physical.page(3), filled(0)) << "and the new one is in its place";
    EXPECT_EQ(m.vm.translate_address(vaddr(6, 0x4)), vaddr(3, 0x4));
}

TEST(virtual_memory, evicted_vpage_is_paged_back_in_on_access)
{
    Machine m;
    const long long pid = m.vm.begin_process();

    m.vm.ensure_physical_page_mapping(pid, 5, 3);
    m.physical.page(3) = filled(0xA5);
    m.vm.ensure_physical_page_mapping(pid, 6, 3);
    m.physical.page(3) = filled(0x6B);

    // vpage 5 was evicted. It is force mapped to ppage 3, so touching it again evicts vpage 6 and
    // fetches vpage 5 back into ppage 3.
    EXPECT_EQ(m.vm.translate_address(vaddr(5, 0x8)), vaddr(3, 0x8));
    EXPECT_EQ(m.physical.page(3), filled(0xA5)) << "with what it had";

    EXPECT_EQ(m.vm.translate_address(vaddr(6, 0x8)), vaddr(3, 0x8));
    EXPECT_EQ(m.physical.page(3), filled(0x6B));
}

TEST(virtual_memory, ending_a_process_releases_its_pages_and_translations)
{
    Machine m;

    const long long first = m.vm.begin_process();
    m.vm.ensure_physical_page_mapping(first, 5, 3);
    m.vm.translate_address(vaddr(5, 0x10)); // fills the TLB
    m.vm.end_process(first);

    // The pid is recycled. A stale TLB entry would still translate vpage 5 to ppage 3.
    const long long second = m.vm.begin_process();
    EXPECT_EQ(second, first);
    m.vm.ensure_physical_page_mapping(second, 5, 7);

    EXPECT_EQ(m.vm.translate_address(vaddr(5, 0x10)), vaddr(7, 0x10));

    // ppage 3 was returned, so it can be mapped again.
    EXPECT_NO_THROW(m.vm.ensure_physical_page_mapping(second, 6, 3));
}

TEST(virtual_memory, unmapped_vpage_access_throws)
{
    Machine m;
    m.vm.begin_process();

    EXPECT_THROW(m.vm.translate_address(vaddr(9)), VirtualMemory::VirtualMemoryException);
}

TEST(virtual_memory, an_unmapped_access_is_a_page_fault_that_says_so)
{
    Machine m;
    m.vm.begin_process();

    EXPECT_EQ(fault_of(m.vm, 9, Access::READ), Fault::Reason::UNMAPPED);
    EXPECT_EQ(fault_of(m.vm, 9, Access::WRITE), Fault::Reason::UNMAPPED);
}

TEST(virtual_memory, a_page_that_is_not_writable_cannot_be_written)
{
    Machine m;
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 5, 1, false, false);

    EXPECT_EQ(fault_of(m.vm, 5, Access::WRITE), Fault::Reason::WRITE_DENIED);
    EXPECT_EQ(fault_of(m.vm, 5, Access::READ), std::nullopt);
    // The translation of the read is in the TLB now, and it does not allow the write either.
    EXPECT_EQ(fault_of(m.vm, 5, Access::WRITE), Fault::Reason::WRITE_DENIED);

    m.vm.set_vpage_permissions(pid, 5, 5, true, false);
    EXPECT_EQ(fault_of(m.vm, 5, Access::WRITE), std::nullopt);
    m.vm.set_vpage_permissions(pid, 5, 5, false, false);
    EXPECT_EQ(fault_of(m.vm, 5, Access::WRITE), Fault::Reason::WRITE_DENIED)
        << "taking the permission away has to reach the translation that is cached";
}

TEST(virtual_memory, kernel_memory_needs_kernel_privilege)
{
    Machine m(4, 2);
    const long long user = m.vm.begin_process(false);
    m.vm.add_vpage(user, 10, 1, true, false);
    m.vm.set_ppage_permissions(4, 5, true, true);

    EXPECT_EQ(fault_of(m.vm, 10, Access::READ), Fault::Reason::KERNEL_ONLY);
    EXPECT_EQ(fault_of(m.vm, 10, Access::WRITE), Fault::Reason::KERNEL_ONLY);

    const long long kernel = m.vm.begin_process(true);
    m.vm.add_vpage(kernel, 11, 1, true, false);
    EXPECT_EQ(fault_of(m.vm, 11, Access::WRITE), std::nullopt);

    // The page can be opened up again for everyone.
    m.vm.set_process(user);
    m.vm.set_ppage_permissions(4, 5, true, false);
    EXPECT_EQ(fault_of(m.vm, 10, Access::READ), std::nullopt);
}

TEST(virtual_memory, virtual_pages_are_only_put_in_the_frames)
{
    Machine m(4, 2);
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 4, true, false);

    for (word vpage = 10; vpage < 14; vpage++)
    {
        const word ppage = ppage_of(m.vm, vpage);
        EXPECT_GE(ppage, 4u) << "vpage " << vpage;
        EXPECT_LT(ppage, 6u) << "vpage " << vpage;
    }
}

// The pages that can be swapped are on a clock: an access marks its page, and the hand that looks
// for a page to evict passes over the marked ones (clearing the mark) until it finds one that was
// not used since it was last there.
TEST(virtual_memory, when_every_page_was_used_the_one_brought_in_first_is_evicted)
{
    Machine m(4, 2);
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 3, true, false);

    const word first = ppage_of(m.vm, 10);
    const word second = ppage_of(m.vm, 11);
    ASSERT_NE(first, second);

    // Both were used when they were brought in, and 10 again after that. The first turn of the
    // hand clears the marks, the second one finds 10 first.
    ppage_of(m.vm, 10);
    m.physical.read.clear();
    EXPECT_EQ(ppage_of(m.vm, 12), first);
    EXPECT_EQ(m.physical.read, std::vector<word>{first}) << "10 was saved";
}

TEST(virtual_memory, a_page_used_since_the_hand_passed_it_is_not_evicted)
{
    Machine m(4, 3);
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 5, true, false);

    const word first = ppage_of(m.vm, 10);
    const word second = ppage_of(m.vm, 11);
    const word third = ppage_of(m.vm, 12);

    // All three are marked, the hand clears them and 10 goes. 11 and 12 are not marked now.
    EXPECT_EQ(ppage_of(m.vm, 13), first);

    // 11 is used again (a translation that is in the TLB counts as well). The hand is at 11, so
    // it passes it and 12, which was not used, is the one that goes.
    ppage_of(m.vm, 11);
    m.physical.read.clear();
    EXPECT_EQ(ppage_of(m.vm, 14), third);
    EXPECT_EQ(m.physical.read, std::vector<word>{third}) << "12 was saved";

    // 11 is still in memory, it keeps its page.
    EXPECT_EQ(ppage_of(m.vm, 11), second);
}

// The information about the frames is kept in chunks of 512 pages that are made when they are first
// needed, and the pages outside of the frames are kept apart: none of that should show.
TEST(virtual_memory, frames_in_several_chunks_of_page_information_and_pages_outside_work_alike)
{
    constexpr word kLo = 1000;
    constexpr word kFrames = 1100; // 1000..2099: three chunks, the borders are at 1512 and 2024
    Machine m(kLo, kFrames);
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, kFrames + 200, true, false);

    std::set<word> seen;
    for (word i = 0; i < kFrames; i++)
    {
        const word ppage = ppage_of(m.vm, 10 + i);
        ASSERT_EQ(ppage, kLo + i) << "the lowest free frame comes first";
        seen.insert(ppage);
    }
    EXPECT_EQ(seen.size(), size_t(kFrames));

    // A frame in the second chunk and one in the third cannot be swapped out.
    const word pinned_a = 1600;
    const word pinned_b = 2050;
    m.vm.set_ppage_permissions(pinned_a, pinned_a, false, false);
    m.vm.set_ppage_permissions(pinned_b, pinned_b, false, false);

    // 200 more pages than there are frames: every one evicts a page, never a pinned one.
    for (word i = 0; i < 200; i++)
    {
        const word ppage = ppage_of(m.vm, 10 + kFrames + i);
        ASSERT_GE(ppage, kLo);
        ASSERT_LT(ppage, kLo + kFrames);
        ASSERT_NE(ppage, pinned_a);
        ASSERT_NE(ppage, pinned_b);
    }
    EXPECT_EQ(ppage_of(m.vm, 10 + (pinned_a - kLo)), pinned_a);
    EXPECT_EQ(ppage_of(m.vm, 10 + (pinned_b - kLo)), pinned_b);

    // Physical pages below and above the frames (a device, the ROM) are mapped where they are.
    m.vm.ensure_physical_page_mapping(pid, 5000, 100);
    m.vm.ensure_physical_page_mapping(pid, 5001, kLo + kFrames + 7);
    EXPECT_EQ(ppage_of(m.vm, 5000), 100u);
    EXPECT_EQ(ppage_of(m.vm, 5001), kLo + kFrames + 7);
    EXPECT_EQ(ppage_of(m.vm, 5000), 100u) << "and again, from the page that was made";
}

TEST(virtual_memory, the_clock_keeps_working_when_pages_are_released)
{
    Machine m(4, 3);
    const long long first = m.vm.begin_process();
    m.vm.add_vpage(first, 10, 3, true, false);
    for (word vpage = 10; vpage < 13; vpage++)
    {
        ppage_of(m.vm, vpage);
    }

    // The second process uses the frames, which puts the hand on a page of the first one.
    const long long second = m.vm.begin_process();
    m.vm.add_vpage(second, 10, 5, true, false);
    ppage_of(m.vm, 10);
    ppage_of(m.vm, 11);

    // Ending the first process releases the pages, the one under the hand as well.
    m.vm.end_process(first);
    for (word round = 0; round < 4; round++)
    {
        for (word vpage = 10; vpage < 15; vpage++)
        {
            EXPECT_NO_THROW(ppage_of(m.vm, vpage)) << "vpage " << vpage;
        }
    }
}

TEST(virtual_memory, a_page_that_is_not_swappable_is_not_evicted)
{
    Machine m(4, 2);
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 3, true, false);

    const word pinned = ppage_of(m.vm, 10);
    const word other = ppage_of(m.vm, 11);
    m.vm.set_ppage_permissions(pinned, pinned, false, false);

    // 10 is the oldest, but 11 is the one that goes.
    EXPECT_EQ(ppage_of(m.vm, 12), other);
    EXPECT_EQ(ppage_of(m.vm, 10), pinned);
}

TEST(virtual_memory, running_out_of_pages_that_can_be_swapped_is_an_error_and_not_a_crash)
{
    Machine m(4, 1);
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 2, true, false);

    const word only = ppage_of(m.vm, 10);
    m.vm.set_ppage_permissions(only, only, false, false);
    EXPECT_THROW(ppage_of(m.vm, 11), VirtualMemory::VirtualMemoryException);
}

TEST(virtual_memory, a_page_keeps_its_contents_when_it_is_swapped_out_and_in)
{
    Machine m(4, 1); // one frame, so the pages take turns
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 2, true, false);

    EXPECT_EQ(ppage_of(m.vm, 10), 4u);
    m.physical.page(4) = filled(0x11);

    EXPECT_EQ(ppage_of(m.vm, 11), 4u);
    EXPECT_EQ(m.physical.page(4), filled(0)) << "a page that was never used is zeros";
    m.physical.page(4) = filled(0x22);

    EXPECT_EQ(ppage_of(m.vm, 10), 4u);
    EXPECT_EQ(m.physical.page(4), filled(0x11));
    EXPECT_EQ(ppage_of(m.vm, 11), 4u);
    EXPECT_EQ(m.physical.page(4), filled(0x22));
}

TEST(virtual_memory, explicit_mappings_outside_of_the_frames_do_not_use_up_frames)
{
    Machine m(4, 2);
    const long long pid = m.vm.begin_process();

    // Like a program that is loaded into ROM.
    m.vm.ensure_physical_page_mapping(pid, 20, 40);
    EXPECT_EQ(ppage_of(m.vm, 20), 40u);

    m.vm.add_vpage(pid, 10, 3, true, false);
    ppage_of(m.vm, 10);
    ppage_of(m.vm, 11);
    ppage_of(m.vm, 12); // a third page makes one of the frames go
    EXPECT_EQ(ppage_of(m.vm, 20), 40u)
        << "the page outside of the frames is never the one that goes";
}

// A page that was mapped to a physical page explicitly gets it back when it is used again. The page
// was given to somebody else meanwhile, and nobody else may get it while it is in use.
TEST(virtual_memory, a_physical_page_in_use_is_not_given_out_again)
{
    Machine m(0, 2);
    const long long pid = m.vm.begin_process();

    m.vm.ensure_physical_page_mapping(pid, 5, 0); // 5 is in frame 0
    m.vm.add_vpage(pid, 6, 3, true, false);
    EXPECT_EQ(ppage_of(m.vm, 6), 1u);
    EXPECT_EQ(ppage_of(m.vm, 7), 0u) << "5 is the oldest, so it goes";

    // 5 takes its page back from 7.
    EXPECT_EQ(ppage_of(m.vm, 5), 0u);
    const word eight = ppage_of(m.vm, 8);
    EXPECT_EQ(eight, 1u) << "6 is older than 5 now, and 0 is in use";
    EXPECT_EQ(ppage_of(m.vm, 5), 0u);
}

TEST(virtual_memory, a_page_on_disk_is_not_mapped_to_page_0_by_accident)
{
    Machine m;
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 5, 1, true, false); // not in memory, so it has no physical page

    EXPECT_THROW(m.vm.ensure_physical_page_mapping(pid, 5, 0), VirtualMemory::VPageRemapException);
}

TEST(virtual_memory, permissions_can_be_set_up_to_the_last_page)
{
    Machine m;
    const long long pid = m.vm.begin_process();

    // The loops that walk over the pages must stop at the last page there is.
    m.vm.set_vpage_permissions(pid, 0xFFFFE, 0xFFFFF, true, false);
    EXPECT_TRUE(m.vm.has_vpage(pid, 0xFFFFF));
    m.vm.set_ppage_permissions(0xFFFFE, 0xFFFFF, true, false);
    EXPECT_NO_THROW(m.vm.add_vpage(pid, 0xFFFFD, 0, true, false)) << "no pages is nothing to add";
    EXPECT_THROW(m.vm.add_vpage(pid, 0xFFFFF, 2, true, false),
                 VirtualMemory::InvalidVPageException);
}

TEST(virtual_memory, a_virtual_memory_is_cheap_to_create)
{
    // It used to allocate a record for each of the million physical pages.
    MockDisk disk;
    for (int i = 0; i < 200; i++)
    {
        VirtualMemory vm(&disk);
    }
}

// ---------------------------------------------------------------------------------------------
// The translation of the pages of the current process is looked up in the TLB by one key, the
// process and the virtual page. Whatever changes which process that is, or whether its pages are
// translated at all, has to change the key that is looked up with.
// ---------------------------------------------------------------------------------------------

TEST(virtual_memory_tlb, the_pages_of_two_processes_do_not_get_mixed_up)
{
    Machine m(4, 4);
    const long long first = m.vm.begin_process();
    m.vm.add_vpage(first, 10, 1, true, false);
    const word first_ppage = ppage_of(m.vm, 10);

    const long long second = m.vm.begin_process();
    m.vm.add_vpage(second, 10, 1, true, false);
    const word second_ppage = ppage_of(m.vm, 10);
    ASSERT_NE(first_ppage, second_ppage);

    // The same virtual page of both is in the TLB, in the same slot, and each is found for its
    // process.
    for (int round = 0; round < 3; round++)
    {
        m.vm.set_process(first);
        EXPECT_EQ(ppage_of(m.vm, 10), first_ppage);
        m.vm.set_process(second);
        EXPECT_EQ(ppage_of(m.vm, 10), second_ppage);
    }
}

TEST(virtual_memory_tlb, a_page_of_another_process_is_not_translated_by_the_current_one)
{
    Machine m;
    const long long first = m.vm.begin_process();
    m.vm.add_vpage(first, 10, 1, true, false);
    ppage_of(m.vm, 10);

    const long long second = m.vm.begin_process();
    EXPECT_EQ(fault_of(m.vm, 10, Access::READ), Fault::Reason::UNMAPPED);
    m.vm.set_process(first);
    EXPECT_EQ(fault_of(m.vm, 10, Access::READ), std::nullopt);
    m.vm.end_process(second);
}

TEST(virtual_memory_tlb, addresses_are_physical_without_a_process_and_when_the_memory_is_off)
{
    Machine m(4, 2);
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 1, true, false);
    const word mapped = ppage_of(m.vm, 10);
    ASSERT_NE(mapped, 10u);

    m.vm.set_enabled(false);
    EXPECT_EQ(ppage_of(m.vm, 10), 10u);
    m.vm.set_enabled(true);
    EXPECT_EQ(ppage_of(m.vm, 10), mapped);

    m.vm.end_process(pid);
    EXPECT_EQ(ppage_of(m.vm, 10), 10u) << "there is no process whose pages could be translated";
}

TEST(virtual_memory_tlb, the_devices_are_where_they_are_for_a_process)
{
    Machine m(4, 2);
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 1, true, false);
    ppage_of(m.vm, 10);

    EXPECT_EQ(m.vm.translate_address(kDeviceBase + 0x24), kDeviceBase + 0x24);
    EXPECT_EQ(m.vm.translate_address(kDeviceBase + 0x24, Access::WRITE), kDeviceBase + 0x24);
}

TEST(virtual_memory_tlb, the_page_tables_and_the_swapping_memory_take_over_from_each_other)
{
    Machine m(4, 2);
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 5, 1, true, false);
    const word mapped = ppage_of(m.vm, 5); // the entry of the TLB of the swapping memory
    EXPECT_EQ(ppage_of(m.vm, 5), mapped);

    // The page tables are on: they translate, and the entry of the process is not used.
    set_pte(m, 5, 8, VirtualMemory::kPteExecute | VirtualMemory::kPteWrite, 2);
    m.vm.set_page_table_base(kTableBase);
    m.vm.set_walk_enabled(true);
    ASSERT_NE(mapped, 8u);
    EXPECT_EQ(ppage_of(m.vm, 5), 8u);
    EXPECT_EQ(m.vm.translate_address(vaddr(5, 4)), vaddr(8, 4));

    // And off again: the process translates.
    m.vm.set_walk_enabled(false);
    EXPECT_EQ(ppage_of(m.vm, 5), mapped);
}

TEST(virtual_memory_tlb, a_translation_that_was_made_for_reading_does_not_allow_a_write)
{
    Machine m;
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 1, false, false);

    EXPECT_EQ(fault_of(m.vm, 10, Access::READ), std::nullopt);
    EXPECT_EQ(fault_of(m.vm, 10, Access::READ), std::nullopt) << "from the TLB";
    EXPECT_EQ(fault_of(m.vm, 10, Access::WRITE), Fault::Reason::WRITE_DENIED);
    EXPECT_EQ(fault_of(m.vm, 10, Access::READ), std::nullopt);
}

TEST(virtual_memory_tlb, a_page_that_stops_being_writable_cannot_be_written_to)
{
    Machine m;
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 1, true, false);

    EXPECT_EQ(fault_of(m.vm, 10, Access::WRITE), std::nullopt);
    EXPECT_EQ(fault_of(m.vm, 10, Access::WRITE), std::nullopt) << "from the TLB";
    m.vm.set_vpage_permissions(pid, 10, 10, false, false);
    EXPECT_EQ(fault_of(m.vm, 10, Access::WRITE), Fault::Reason::WRITE_DENIED);
}

// ---------------------------------------------------------------------------------------------
// The page that instructions are fetched from is remembered (translate_fetch), and has to be
// forgotten whenever the answer could change.
// ---------------------------------------------------------------------------------------------

TEST(virtual_memory_fetch, a_page_is_fetched_from_where_it_is_and_the_address_is_kept)
{
    Machine m(4, 2);
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 2, true, true);

    const word ppage = fetch_ppage_of(m.vm, 10);
    EXPECT_EQ(ppage, ppage_of(m.vm, 10));
    EXPECT_EQ(m.vm.translate_fetch(vaddr(10, 0x24)), vaddr(ppage, 0x24))
        << "from the page it keeps";
    EXPECT_EQ(m.vm.translate_fetch(vaddr(10, 0xFFC)), vaddr(ppage, 0xFFC));
    EXPECT_NE(fetch_ppage_of(m.vm, 11), ppage) << "another page is another page";
    EXPECT_EQ(fetch_ppage_of(m.vm, 10), ppage);
}

// The page that is fetched from is remembered, which skips the translation that marks a page as
// used. When the clock clears the mark the remembered page goes with it, or code that runs would
// look unused and be evicted.
TEST(virtual_memory_fetch, the_page_that_is_fetched_from_counts_as_used_for_the_clock)
{
    Machine m(4, 3);
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 5, true, true);

    const word first = ppage_of(m.vm, 10);
    const word second = fetch_ppage_of(m.vm, 11); // the page that is remembered
    const word third = ppage_of(m.vm, 12);

    // All three are marked, the hand clears them and 10 goes.
    EXPECT_EQ(ppage_of(m.vm, 13), first);

    // The code goes on in 11. That has to mark it again, 12 is the one that is not used.
    EXPECT_EQ(fetch_ppage_of(m.vm, 11), second);
    EXPECT_EQ(ppage_of(m.vm, 14), third);
    EXPECT_EQ(fetch_ppage_of(m.vm, 11), second) << "11 kept its page";
}

TEST(virtual_memory_fetch, a_page_that_is_not_executable_cannot_be_fetched_from)
{
    Machine m;
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 1, true, false);

    // Not remembered either, so it is not allowed the second time.
    EXPECT_EQ(fetch_fault_of(m.vm, 10), Fault::Reason::EXECUTE_DENIED);
    EXPECT_EQ(fetch_fault_of(m.vm, 10), Fault::Reason::EXECUTE_DENIED);
    EXPECT_EQ(fetch_fault_of(m.vm, 11), Fault::Reason::UNMAPPED);
}

TEST(virtual_memory_fetch, a_page_that_stops_being_executable_cannot_be_fetched_from_anymore)
{
    Machine m;
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 1, true, true);

    EXPECT_EQ(fetch_fault_of(m.vm, 10), std::nullopt);
    EXPECT_EQ(fetch_fault_of(m.vm, 10), std::nullopt) << "from the page it keeps";
    m.vm.set_vpage_permissions(pid, 10, 10, true, false);
    EXPECT_EQ(fetch_fault_of(m.vm, 10), Fault::Reason::EXECUTE_DENIED);
}

TEST(virtual_memory_fetch, the_fetch_follows_the_process)
{
    Machine m;
    const long long first = m.vm.begin_process();
    m.vm.add_vpage(first, 10, 1, true, true);
    EXPECT_EQ(fetch_fault_of(m.vm, 10), std::nullopt);

    const long long second = m.vm.begin_process();
    EXPECT_EQ(fetch_fault_of(m.vm, 10), Fault::Reason::UNMAPPED)
        << "10 is not a page of the second";

    m.vm.set_process(first);
    EXPECT_EQ(fetch_fault_of(m.vm, 10), std::nullopt);
    m.vm.end_process(first);
    m.vm.set_process(second);
    EXPECT_EQ(fetch_fault_of(m.vm, 10), Fault::Reason::UNMAPPED);
}

TEST(virtual_memory_fetch, a_page_that_a_data_access_swapped_out_is_brought_back_in)
{
    Machine m(4, 1); // one frame, so the pages take turns
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 2, true, true);

    const word frame = fetch_ppage_of(m.vm, 10);

    // The only frame is page 11's now. If the fetch still used what it kept, it would run the
    // code of page 11.
    ppage_of(m.vm, 11);
    m.physical.read.clear();
    EXPECT_EQ(fetch_ppage_of(m.vm, 10), frame);
    EXPECT_EQ(m.physical.read, std::vector<word>{frame}) << "11 was saved to make room for 10";
}

// The page can also be evicted without the hand: an explicit mapping takes its physical page.
TEST(virtual_memory_fetch, a_page_that_a_mapping_took_the_physical_page_of_is_brought_back_in)
{
    Machine m(4, 3);
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 1, true, true);

    const word frame = fetch_ppage_of(m.vm, 10);
    m.vm.ensure_physical_page_mapping(pid, 20, frame);

    // 10 was saved to the disk, and the frame is 20's. If the fetch still used what it kept, it
    // would run what is in the frame.
    EXPECT_NE(fetch_ppage_of(m.vm, 10), frame);
    EXPECT_EQ(m.vm.translate_address(vaddr(20)), vaddr(frame));
}

TEST(virtual_memory_fetch, addresses_are_physical_when_the_virtual_memory_is_off)
{
    Machine m(4, 2);
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 1, true, true);

    const word mapped = fetch_ppage_of(m.vm, 10);
    ASSERT_NE(mapped, 10u);
    m.vm.set_enabled(false);
    EXPECT_EQ(fetch_ppage_of(m.vm, 10), 10u);
    m.vm.set_enabled(true);
    EXPECT_EQ(fetch_ppage_of(m.vm, 10), mapped);
}

// A page that only instructions are fetched from has nothing that marks it as used while it is
// remembered, so the hand of the clock that clears its mark has to make it forget it.
TEST(virtual_memory_fetch,
     a_page_that_is_only_fetched_from_is_marked_again_after_the_hand_cleared_it)
{
    Machine m(4, 3);
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 6, true, true);

    const word data1 = ppage_of(m.vm, 11);
    const word data2 = ppage_of(m.vm, 12);
    const word code = fetch_ppage_of(m.vm, 10);

    // All three are marked. The first turn of the hand clears them (the code page too, and the
    // fetch forgets it) and 11, which was first, goes.
    EXPECT_EQ(ppage_of(m.vm, 13), data1);

    // Instructions are fetched again and 12 is used again: both are marked.
    EXPECT_EQ(m.vm.translate_fetch(vaddr(10, 4)), vaddr(code, 4));
    ppage_of(m.vm, 12);

    // The hand passes both, and then every page is cleared and 12, where it started, goes. If
    // the fetch had not marked the code page it would be this one.
    EXPECT_EQ(ppage_of(m.vm, 14), data2);
    EXPECT_EQ(fetch_ppage_of(m.vm, 10), code);
}

// With the page tables the TLB can be seen by the program: an entry stays as it was until
// tlbi, or until another page replaces it.
TEST(virtual_memory_fetch, the_page_tables_are_read_again_after_tlbi_and_not_before)
{
    WalkMachine m;
    set_pte(m, 5, 8, VirtualMemory::kPteExecute, 2);
    EXPECT_EQ(fetch_ppage_of(m.vm, 5), 8u);
    EXPECT_EQ(m.vm.translate_fetch(vaddr(5, 0x24)), vaddr(8, 0x24));

    set_pte(m, 5, 9, VirtualMemory::kPteExecute, 2);
    EXPECT_EQ(fetch_ppage_of(m.vm, 5), 8u) << "the entry of the TLB is as it was";

    m.vm.invalidate_translation(vaddr(6)); // another page does not matter
    EXPECT_EQ(fetch_ppage_of(m.vm, 5), 8u);
    m.vm.invalidate_translation(vaddr(5));
    EXPECT_EQ(fetch_ppage_of(m.vm, 5), 9u);
}

TEST(virtual_memory_fetch, a_page_that_the_tables_do_not_allow_to_execute_anymore_faults_after_tlbi)
{
    WalkMachine m;
    set_pte(m, 5, 8, VirtualMemory::kPteExecute, 2);
    EXPECT_EQ(fetch_fault_of(m.vm, 5), std::nullopt);

    set_pte(m, 5, 8, 0, 2);
    EXPECT_EQ(fetch_fault_of(m.vm, 5), std::nullopt) << "the entry of the TLB is as it was";
    m.vm.invalidate_translations();
    EXPECT_EQ(fetch_fault_of(m.vm, 5), Fault::Reason::EXECUTE_DENIED);
}

TEST(virtual_memory_fetch, a_page_replaced_in_the_tlb_is_read_from_the_tables_again)
{
    WalkMachine m;
    // 5 and 4101 are the same slot of the TLB.
    set_pte(m, 5, 8, VirtualMemory::kPteExecute, 2);
    set_pte(m, 4101, 9, 0, 3);
    EXPECT_EQ(fetch_ppage_of(m.vm, 5), 8u);

    m.vm.translate_address(vaddr(4101), Access::READ);

    // The entry of 5 is not in the TLB anymore, so the change is seen without tlbi.
    set_pte(m, 5, 10, VirtualMemory::kPteExecute, 2);
    EXPECT_EQ(fetch_ppage_of(m.vm, 5), 10u);
}

TEST(virtual_memory_fetch, the_mode_decides_whether_a_user_page_can_be_fetched_from)
{
    WalkMachine m;
    set_pte(m, 5, 8, VirtualMemory::kPteExecute | VirtualMemory::kPteUser, 2);

    m.vm.set_user_mode(true);
    EXPECT_EQ(fetch_fault_of(m.vm, 5), std::nullopt);
    m.vm.set_user_mode(false);
    EXPECT_EQ(fetch_fault_of(m.vm, 5), Fault::Reason::EXECUTE_DENIED)
        << "the kernel does not run code of a user process";
    m.vm.set_user_mode(true);
    EXPECT_EQ(fetch_fault_of(m.vm, 5), std::nullopt);

    // And a kernel page cannot be run in user mode.
    set_pte(m, 6, 9, VirtualMemory::kPteExecute, 2);
    m.vm.set_user_mode(false);
    EXPECT_EQ(fetch_fault_of(m.vm, 6), std::nullopt);
    m.vm.set_user_mode(true);
    EXPECT_EQ(fetch_fault_of(m.vm, 6), Fault::Reason::KERNEL_ONLY);
}

TEST(virtual_memory_fetch, a_new_table_or_switching_back_to_swapping_forgets_the_page)
{
    WalkMachine m;
    set_pte(m, 5, 8, VirtualMemory::kPteExecute, 2);
    EXPECT_EQ(fetch_ppage_of(m.vm, 5), 8u);

    // Another first level table: page 1's content is moved to page 12.
    m.vm.set_page_table_base(word(12) << kNumPageOffsetBits);
    EXPECT_EQ(fetch_fault_of(m.vm, 5), Fault::Reason::UNMAPPED);

    m.vm.set_page_table_base(kTableBase);
    EXPECT_EQ(fetch_ppage_of(m.vm, 5), 8u);
    m.vm.set_walk_enabled(false);
    EXPECT_EQ(fetch_fault_of(m.vm, 5), std::nullopt) << "without a process everything can run";
    EXPECT_EQ(fetch_ppage_of(m.vm, 5), 5u) << "and addresses are physical";
}
