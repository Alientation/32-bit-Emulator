#include "emulator32bit_test/emulator32bit_test.h"

#include "emulator32bit/disk.h"
#include "emulator32bit/virtual_memory.h"

#include <map>
#include <optional>

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

std::vector<byte> filled(byte value)
{
    return std::vector<byte>(kPageSize, value);
}

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

TEST(virtual_memory, the_page_used_the_longest_ago_is_evicted)
{
    Machine m(4, 2);
    const long long pid = m.vm.begin_process();
    m.vm.add_vpage(pid, 10, 3, true, false);

    const word first = ppage_of(m.vm, 10);
    const word second = ppage_of(m.vm, 11);
    ASSERT_NE(first, second);

    // 10 is used again, so 11 is the one that was not used for the longest time.
    ppage_of(m.vm, 10);
    ppage_of(m.vm, 10); // a translation that is in the TLB counts as well
    m.physical.read.clear();
    EXPECT_EQ(ppage_of(m.vm, 12), second);
    EXPECT_EQ(m.physical.read, std::vector<word>{second}) << "11 was saved";

    // Now 10 is older than 12, and 11 is not in memory.
    m.physical.read.clear();
    ppage_of(m.vm, 11);
    EXPECT_EQ(m.physical.read, std::vector<word>{first}) << "10 was the oldest";
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
