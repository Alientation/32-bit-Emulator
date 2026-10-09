#include "emulator32bit/virtual_memory.h"

#include "util/logger.h"

#include <unordered_set>

VirtualMemory::VirtualMemory(Disk *disk, word frame_lo_page, word frame_pages,
                             PhysicalPages *physical) :
    m_disk(disk),
    m_freepids(0, kMaxProcesses),
    m_frame_lo(frame_lo_page),
    m_frame_pages(frame_pages),
    m_frames(frame_lo_page, frame_pages, true),
    m_physical(physical)
{
}

VirtualMemory::~VirtualMemory()
{
    for (auto &[pid, ptable] : m_process_ptable_map)
    {
        for (auto &[vpage, entry] : ptable->entries)
        {
            delete entry;
        }
        delete ptable;
    }
}

void VirtualMemory::set_physical_pages(PhysicalPages *physical)
{
    m_physical = physical;
}

VirtualMemory::PhysicalPage &VirtualMemory::physical_page(word ppage)
{
    const auto [it, inserted] = m_physical_memory_map.try_emplace(ppage);
    if (inserted)
    {
        it->second.ppage = ppage;
    }
    return it->second;
}

VirtualMemory::PhysicalPage *VirtualMemory::find_physical_page(word ppage)
{
    const auto it = m_physical_memory_map.find(ppage);
    return it == m_physical_memory_map.end() ? nullptr : &it->second;
}

bool VirtualMemory::is_frame(word ppage) const
{
    return ppage >= m_frame_lo && U64(ppage) < U64(m_frame_lo) + m_frame_pages;
}

void VirtualMemory::release_frame(word ppage)
{
    if (is_frame(ppage))
    {
        m_frames.release(ppage);
    }
}

void VirtualMemory::invalidate_tlb(long long pid, word vpage)
{
    TLB_Entry &tlb_entry = m_tlb[vpage & (kMaxTLBSize - 1)];
    if (tlb_entry.key == tlb_key(pid, vpage))
    {
        tlb_entry.key = kNoKey;
    }

    // The page of the current process that is fetched from. A page of another process with the
    // same number does not matter, but it is not worth looking at the process.
    if (vpage == m_fetch_vpage)
    {
        drop_fetch_cache();
    }
}

void VirtualMemory::flush_tlb()
{
    for (TLB_Entry &tlb_entry : m_tlb)
    {
        tlb_entry.key = kNoKey;
    }
    drop_fetch_cache();
}

void VirtualMemory::clock_remove(PhysicalPage &page)
{
    if (!page.in_clock)
    {
        return;
    }

    // The hand goes on to the page after it. The end of the list is followed by the beginning,
    // which is what a null hand stands for.
    if (m_clock_hand == &page)
    {
        m_clock_hand = page.clock_next;
    }

    if (page.clock_prev != nullptr)
    {
        page.clock_prev->clock_next = page.clock_next;
    }
    else
    {
        m_clock_head = page.clock_next;
    }

    if (page.clock_next != nullptr)
    {
        page.clock_next->clock_prev = page.clock_prev;
    }
    else
    {
        m_clock_tail = page.clock_prev;
    }

    page.clock_prev = nullptr;
    page.clock_next = nullptr;
    page.in_clock = false;
}

void VirtualMemory::clock_add(PhysicalPage &page)
{
    clock_remove(page);

    // Just before the hand, or at the end of the list when the hand is at the beginning (null).
    PhysicalPage *const next = m_clock_hand;
    page.clock_next = next;
    page.clock_prev = next != nullptr ? next->clock_prev : m_clock_tail;
    if (page.clock_prev != nullptr)
    {
        page.clock_prev->clock_next = &page;
    }
    else
    {
        m_clock_head = &page;
    }
    if (next != nullptr)
    {
        next->clock_prev = &page;
    }
    else
    {
        m_clock_tail = &page;
    }

    page.in_clock = true;
    page.referenced = true;
}

VirtualMemory::PhysicalPage &VirtualMemory::clock_victim()
{
    // The pages on the clock can be swapped. Only the frames can hold a page that is paged in, a
    // page outside of them was mapped explicitly and stays. The first turn clears the pages that
    // were used, so the second one finds a page if the first did not, unless there is none.
    //
    // A hit on the TLB does not mark a page (that would be work in every translation), so when a
    // mark is cleared the TLB entries of the page go with it, and the next access to it, if it is
    // still in use, fills its entry again and marks it. The page that instructions are fetched
    // from is remembered in the same way, and goes with its entry (invalidate_tlb). This is the
    // clock without a cost for the accesses, the way the accessed bit of a real MMU works. Only
    // the entries of the pages that were cleared are dropped, the TLB is not wiped.
    PhysicalPage *const start = m_clock_hand != nullptr ? m_clock_hand : m_clock_head;
    if (start != nullptr)
    {
        for (int turn = 0; turn < 2; turn++)
        {
            PhysicalPage *page = start;
            do
            {
                PhysicalPage *const next =
                    page->clock_next != nullptr ? page->clock_next : m_clock_head;
                if (is_frame(page->ppage))
                {
                    if (!page->referenced)
                    {
                        m_clock_hand = next;
                        return *page;
                    }
                    page->referenced = false;
                    for (const PageTableEntry *entry : page->mapped_vpages)
                    {
                        invalidate_tlb(entry->pid, entry->vpage);
                    }
                }
                page = next;
            } while (page != start);
        }
    }

    throw VirtualMemoryException("Out of physical memory, there is no page that can be swapped.");
}

VirtualMemory::VirtualMemoryException::VirtualMemoryException(const std::string &msg) :
    message(msg)
{
}

const char *VirtualMemory::VirtualMemoryException::what() const noexcept
{
    return message.c_str();
}

namespace
{

const char *access_name(VirtualMemory::AccessType access)
{
    switch (access)
    {
    case VirtualMemory::AccessType::READ:
        return "Read";
    case VirtualMemory::AccessType::WRITE:
        return "Write";
    case VirtualMemory::AccessType::EXECUTE:
        return "Execute";
    }
    return "Access";
}

std::string fault_message(VirtualMemory::PageFaultException::Reason reason, word vpage,
                          VirtualMemory::AccessType access)
{
    using Reason = VirtualMemory::PageFaultException::Reason;
    const std::string page = " virtual page " + std::to_string(vpage);
    switch (reason)
    {
    case Reason::UNMAPPED:
        return std::string("SIGSEGV: ") + access_name(access) + " of unmapped" + page;
    case Reason::WRITE_DENIED:
        return "Write to the read-only" + page;
    case Reason::EXECUTE_DENIED:
        return "Execute permission denied for" + page;
    case Reason::KERNEL_ONLY:
        return std::string(access_name(access)) + " of kernel memory in" + page
               + " without kernel privilege";
    }
    return "Page fault";
}

} // namespace

VirtualMemory::PageFaultException::PageFaultException(Reason reason, word vpage,
                                                      AccessType access) :
    VirtualMemoryException(fault_message(reason, vpage, access)),
    reason(reason),
    vpage(vpage),
    access(access)
{
}

VirtualMemory::PageFaultException::Reason
VirtualMemory::PageFaultException::get_reason() const noexcept
{
    return reason;
}

word VirtualMemory::PageFaultException::get_vpage() const noexcept
{
    return vpage;
}

VirtualMemory::AccessType VirtualMemory::PageFaultException::get_access() const noexcept
{
    return access;
}

void VirtualMemory::throw_fault(PageFaultException::Reason reason, word vpage, AccessType access)
{
    throw PageFaultException(reason, vpage, access);
}

VirtualMemory::InvalidPIDException::InvalidPIDException(const std::string &msg,
                                                        long long invalid_pid) :
    VirtualMemoryException(msg),
    invalid_pid(invalid_pid)
{
}

long long VirtualMemory::InvalidPIDException::get_invalid_pid() const noexcept
{
    return invalid_pid;
}

VirtualMemory::VPageRemapException::VPageRemapException(const std::string &msg, word vpage,
                                                        word already_mapped_ppage,
                                                        word attempted_mapped_ppage) :
    VirtualMemoryException(msg),
    vpage(vpage),
    already_mapped_ppage(already_mapped_ppage),
    attempted_mapped_ppage(attempted_mapped_ppage)
{
}

word VirtualMemory::VPageRemapException::get_vpage() const noexcept
{
    return vpage;
}

word VirtualMemory::VPageRemapException::get_already_mapped_ppage() const noexcept
{
    return already_mapped_ppage;
}

word VirtualMemory::VPageRemapException::get_attempted_mapped_ppage() const noexcept
{
    return attempted_mapped_ppage;
}

VirtualMemory::InvalidVPageException::InvalidVPageException(const std::string &msg, word vpage) :
    VirtualMemoryException(msg),
    vpage(vpage)
{
}

word VirtualMemory::InvalidVPageException::get_vpage() const noexcept
{
    return vpage;
}

VirtualMemory::PageTableEntry::PageTableEntry(long long pid, word vpage, word diskpage, bool write,
                                              bool execute) :
    pid(pid),
    vpage(vpage),
    ppage(0),
    disk(true),
    diskpage(diskpage),
    mapped(false),
    mapped_ppage(0),
    write(write),
    execute(execute)
{
}

void VirtualMemory::set_process(long long pid)
{
    if (m_process_ptable_map.find(pid) == m_process_ptable_map.end())
    {
        throw InvalidPIDException("Cannot set memory map of process " + std::to_string(pid)
                                      + " because it doesn't exist.",
                                  pid);
    }

    m_cur_ptable = m_process_ptable_map.at(pid);
    update_swap_key();
    drop_fetch_cache();
    AEMU_DEBUG("Setting memory map to process {}.", pid);
}

long long VirtualMemory::begin_process(bool kernel_privilege)
{
    if (!m_freepids.can_fit(1))
    {
        throw VirtualMemoryException(
            "Reached the MAX_PROCESSES limit. Cannot create a new process.");
    }

    word pid = m_freepids.get_free_block(1);

    PageTable *new_pagetable = new PageTable{
        .pid = pid,
        .kernel_privilege = kernel_privilege,
    };

    m_process_ptable_map.insert(std::make_pair(pid, new_pagetable));
    m_cur_ptable = new_pagetable;
    update_swap_key();
    drop_fetch_cache();

    AEMU_DEBUG("Beginning process {}.", pid);
    return pid;
}

void VirtualMemory::end_process(long long pid)
{
    if (m_process_ptable_map.find(pid) == m_process_ptable_map.end())
    {
        throw InvalidPIDException("Cannot end process " + std::to_string(pid)
                                      + " since it does "
                                        "not exist.",
                                  pid);
    }

    /*
     * Store into a temporary array because when vpage entry is removed, it will also be removed
     * from the process PageTableEntry map. This would have caused a concurrent modification error.
     */
    std::vector<word> vpages;
    for (std::pair<const word, PageTableEntry *> &pair : m_process_ptable_map.at(pid)->entries)
    {
        vpages.push_back(pair.first);
    }

    for (word vpage : vpages)
    {
        remove_vpage(pid, vpage);
    }

    if (m_cur_ptable == m_process_ptable_map.at(pid))
    {
        m_cur_ptable = nullptr;
    }
    update_swap_key();
    drop_fetch_cache();

    delete m_process_ptable_map.at(pid);
    m_process_ptable_map.erase(pid);
    m_freepids.return_block(pid, 1);
    AEMU_DEBUG("Ending process {}.", pid);
}

long long VirtualMemory::current_process()
{
    if (m_cur_ptable == nullptr)
    {
        return -1;
    }

    return m_cur_ptable->pid;
}

void VirtualMemory::set_ppage_permissions(word ppage_begin, word ppage_end, bool swappable,
                                          bool kernel_locked)
{
    if (ppage_begin > ppage_end)
    {
        return;
    }

    // Not `i <= ppage_end` in the loop condition, the last page can be the last one there is.
    for (word i = ppage_begin;; i++)
    {
        PhysicalPage &page = physical_page(i);
        page.swappable = swappable;
        page.kernel_locked = kernel_locked;

        if (!swappable)
        {
            clock_remove(page);
        }
        else if (page.used && !page.in_clock)
        {
            clock_add(page);
        }

        if (i == ppage_end)
        {
            break;
        }
    }

    // Translations that were allowed may not be anymore.
    flush_tlb();
}

void VirtualMemory::set_vpage_permissions(long long pid, word vpage_begin, word vpage_end,
                                          bool write, bool execute)
{
    if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
    {
        throw InvalidPIDException("Cannot set vpage permissions since pid is invalid.", pid);
    }

    if (vpage_begin > vpage_end)
    {
        return;
    }

    drop_fetch_cache();
    PageTable *ptable = m_process_ptable_map.at(pid);
    for (word vpage = vpage_begin;; vpage++)
    {
        if (ptable->entries.find(vpage) == ptable->entries.end())
        {
            add_vpage(pid, vpage, 1, write, execute);
        }
        else
        {
            PageTableEntry *entry = ptable->entries.at(vpage);
            entry->write = write;
            entry->execute = execute;
        }

        if (vpage == vpage_end)
        {
            break;
        }
    }

    // The translations hold whether the page can be written to.
    flush_tlb();
}

bool VirtualMemory::has_vpage(long long pid, word vpage)
{
    if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
    {
        throw InvalidPIDException("Cannot check virtual page because pid is invalid.", pid);
    }

    const PageTable *ptable = m_process_ptable_map.at(pid);
    return ptable->entries.find(vpage) != ptable->entries.end();
}

bool VirtualMemory::can_write_vpage(long long pid, word vpage)
{
    if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
    {
        throw InvalidPIDException(
            "Cannot check write permission of virtual page because pid is invalid.", pid);
    }

    PageTable *ptable = m_process_ptable_map.at(pid);
    if (ptable->entries.find(vpage) == ptable->entries.end())
    {
        return false;
    }
    return ptable->entries.at(vpage)->write;
}

bool VirtualMemory::can_execute_vpage(long long pid, word vpage)
{
    if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
    {
        throw InvalidPIDException(
            "Cannot check execute permission of virtual page because pid is invalid.", pid);
    }

    PageTable *ptable = m_process_ptable_map.at(pid);
    if (ptable->entries.find(vpage) == ptable->entries.end())
    {
        return false;
    }
    return ptable->entries.at(vpage)->execute;
}

bool VirtualMemory::can_access_ppage(long long pid, word ppage)
{
    if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
    {
        throw InvalidPIDException(
            "Cannot check access permission of physical page because pid is invalid.", pid);
    }

    PageTable *ptable = m_process_ptable_map.at(pid);
    const PhysicalPage *page = find_physical_page(ppage);
    return page == nullptr || !page->kernel_locked || ptable->kernel_privilege;
}

void VirtualMemory::add_vpage(long long pid, word vpage, word length, bool write, bool execute)
{
    if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
    {
        throw InvalidPIDException("Cannot add virtual pages because pid is invalid.", pid);
    }

    if (length == 0)
    {
        return;
    }
    if (U64(vpage) + length > (U64(1) << (8 * sizeof(word) - kNumPageOffsetBits)))
    {
        throw InvalidVPageException(
            "Cannot add " + std::to_string(length) + " virtual pages from virtual page "
                + std::to_string(vpage) + ", they are not all in the address space.",
            vpage);
    }

    AEMU_DEBUG("Adding vpages from {} to {}.", vpage, vpage + length - 1);

    PageTable *ptable = m_process_ptable_map.at(pid);
    const word last_vpage = vpage + length - 1;
    for (;; vpage++)
    {
        if (ptable->entries.find(vpage) != ptable->entries.end())
        {
            throw InvalidVPageException("Cannot add virtual page " + std::to_string(vpage)
                                            + " because it is already mapped to process "
                                            + std::to_string(pid),
                                        vpage);
        }

        ptable->entries.insert(std::make_pair(
            vpage, new PageTableEntry(pid, vpage, m_disk->get_free_page(), write, execute)));

        AEMU_DEBUG("Adding virtual page {} to process {}.", vpage, pid);

        if (vpage == last_vpage)
        {
            break;
        }
    }
}

void VirtualMemory::map_ppage(long long pid, word vpage, word ppage)
{
    if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
    {
        throw InvalidPIDException(
            "Cannot map virtual page to physical page because pid is invalid.", pid);
    }

    PageTable *ptable = m_process_ptable_map.at(pid);
    if (ptable->entries.find(vpage) != ptable->entries.end())
    {
        throw InvalidVPageException(
            "Cannot map virtual page to physical page because virtual page has already been added.",
            vpage);
    }

    add_vpage(pid, vpage, 1, true, true);
    PageTableEntry *entry = ptable->entries.at(vpage);

    PhysicalPage &page = physical_page(ppage);
    if (page.used)
    {
        evict_ppage(page);
    }

    if (is_frame(ppage))
    {
        m_frames.take(ppage);
    }
    map_vpage_to_ppage(entry, page);

    entry->mapped = true;
    entry->mapped_ppage = ppage;
}

void VirtualMemory::remove_vpage(long long pid, word vpage)
{
    if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
    {
        throw InvalidPIDException("Cannot remove virtual page because pid is invalid.", pid);
    }

    PageTable *ptable = m_process_ptable_map.at(pid);

    if (ptable->entries.find(vpage) == ptable->entries.end())
    {
        throw InvalidVPageException(
            "Cannot remove virtual page because it is not mapped to process.", vpage);
    }

    PageTableEntry *entry = ptable->entries.at(vpage);
    ptable->entries.erase(vpage);

    invalidate_tlb(pid, vpage);

    if (entry->disk)
    {
        m_disk->return_page(entry->diskpage);

        AEMU_DEBUG("Returning disk page {} coressponding to virtual page {}.", entry->diskpage,
                   vpage);
    }
    else
    {
        PhysicalPage &physical = physical_page(entry->ppage);
        std::erase(physical.mapped_vpages, entry);

        /* The physical page is only free once no virtual page maps to it anymore. */
        if (physical.mapped_vpages.empty())
        {
            physical.used = false;
            clock_remove(physical);

            /* add back to free list */
            release_frame(entry->ppage);

            AEMU_DEBUG("Returning physical page {} corresponding to virtual page {}.", entry->ppage,
                       vpage);
        }
    }

    delete entry;
}

void VirtualMemory::check_vm()
{
    for (const auto &[ppage, page] : m_physical_memory_map)
    {
        AEMU_CHECK(ppage == page.ppage, "Expected physical memory to match");

        if (page.mapped_vpages.size() > 0)
        {
            word diskpage = page.mapped_vpages.at(0)->diskpage;
            for (PageTableEntry *entry : page.mapped_vpages)
            {
                AEMU_CHECK(entry->diskpage == diskpage,
                           "Expected all virtual pages mapped to the "
                           "physical page to have same diskpage location.");
            }
        }
    }

    for (std::pair<long long, PageTable *> pair : m_process_ptable_map)
    {
        AEMU_DEBUG("Checking process {}.", pair.first);
        AEMU_CHECK(pair.second->pid == pair.first, "Expected Process ID to match");
        for (std::pair<word, PageTableEntry *> entry : pair.second->entries)
        {
            AEMU_DEBUG("Checking page entry at vpage {}.", entry.first);

            AEMU_CHECK(entry.second->vpage == entry.first, "Expected virtual memory to match");
        }
    }
}

void VirtualMemory::evict_ppage(PhysicalPage &evicted_ppage)
{
    const word ppage = evicted_ppage.ppage;
    AEMU_DEBUG("Evicting physical page {} to disk.", ppage);

    if (evicted_ppage.mapped_vpages.empty())
    {
        throw VirtualMemoryException("Cannot evict physical page " + std::to_string(ppage)
                                     + " because no virtual page is mapped to it.");
    }

    /* Every virtual page that maps to the physical page shares one disk page. The page is saved
       before the pages are changed, if that fails they are still in memory. */
    const word diskpage = m_disk->get_free_page();
    if (m_physical != nullptr)
    {
        if (const byte *const direct = m_physical->direct_page(ppage))
        {
            m_disk->write_page(diskpage, direct);
        }
        else
        {
            m_physical->read_page(ppage, m_page_buffer.data());
            m_disk->write_page(diskpage, m_page_buffer.data());
        }
    }
    for (PageTableEntry *removed_entry : evicted_ppage.mapped_vpages)
    {
        removed_entry->disk = true;
        removed_entry->diskpage = diskpage;
        invalidate_tlb(removed_entry->pid, removed_entry->vpage);
    }
    evicted_ppage.mapped_vpages.clear();
    evicted_ppage.used = false;
    clock_remove(evicted_ppage);

    release_frame(ppage);
}

void VirtualMemory::map_vpage_to_ppage(PageTableEntry *entry, PhysicalPage &mapped_ppage)
{
    const word ppage = mapped_ppage.ppage;
    AEMU_DEBUG("Disk Fetch from page {} to physical page {}.", entry->diskpage, ppage);

    // The contents are in place before the page is, a page that could not be read is still on disk.
    if (m_physical != nullptr)
    {
        if (byte *const direct = m_physical->direct_page(ppage))
        {
            m_disk->read_page(entry->diskpage, direct);
        }
        else
        {
            m_disk->read_page(entry->diskpage, m_page_buffer.data());
            m_physical->write_page(ppage, m_page_buffer.data());
        }
    }
    m_disk->return_page(entry->diskpage);

    entry->ppage = ppage;
    entry->disk = false;

    mapped_ppage.mapped_vpages.push_back(entry);
    mapped_ppage.used = true;

    if (mapped_ppage.swappable && !mapped_ppage.in_clock)
    {
        clock_add(mapped_ppage);
    }
    mark_referenced(mapped_ppage);
}

void VirtualMemory::ensure_physical_page_mapping(long long pid, word vpage, word ppage)
{
    if (UNLIKELY(!m_enabled))
    {
        return;
    }

    if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
    {
        throw InvalidPIDException("Invalid Process ID " + std::to_string(pid), pid);
    }

    PageTable *ptable = m_process_ptable_map.at(pid);

    /*
     * It is likely that the virtual page has already been mapped since this is a temporary
     * way to allow the emulator to load a program at a specific physical address.
     */
    if (LIKELY(ptable->entries.find(vpage) != ptable->entries.end()))
    {
        const PageTableEntry *entry = ptable->entries.at(vpage);

        /*
         * It is likely that the virtual page maps to the same physical page. A page that is on
         * disk is still mapped to its physical page if it was mapped to it explicitly, otherwise it
         * has none (its ppage is not valid).
         */
        if (LIKELY(entry->disk ? (entry->mapped && entry->mapped_ppage == ppage)
                               : entry->ppage == ppage))
        {
            return;
        }

        const word mapped_to = entry->mapped ? entry->mapped_ppage : entry->ppage;
        throw VPageRemapException("Virtual page " + std::to_string(vpage)
                                      + " is already "
                                        "mapped to a different physical page "
                                      + std::to_string(mapped_to) + " of process "
                                      + std::to_string(pid),
                                  vpage, mapped_to, ppage);
    }

    AEMU_DEBUG("Mapping physical page {} to virtual page {}.", ppage, vpage);

    map_ppage(pid, vpage, ppage);
}

word VirtualMemory::access_vpage_slow(PageTable *ptable, word vpage, AccessType access)
{
    const auto it = ptable->entries.find(vpage);
    if (UNLIKELY(it == ptable->entries.end()))
    {
        throw_fault(PageFaultException::Reason::UNMAPPED, vpage, access);
    }
    PageTableEntry *entry = it->second;

    if (UNLIKELY(access == AccessType::WRITE && !entry->write))
    {
        throw_fault(PageFaultException::Reason::WRITE_DENIED, vpage, access);
    }

    /*
     * Likely that the virtual page being accessed has not been evicted to the disk.
     */
    // The physical page that the virtual page is in, which is looked up once: it is known when
    // the page is brought in here, and found by its number when it is in memory already.
    PhysicalPage *page_in = nullptr;
    if (UNLIKELY(entry->disk))
    {
        /*
         * Unlikely that the virtual page has been forcibly mapped to a physical page.
         *
         * Maintains any explicit mappings of virtual page to physical page, like
         * writing/reading from memory mapped I/O or ports.
         */
        if (UNLIKELY(entry->mapped))
        {
            /*
             * Since the virtual page is mapped to a physical page on disk, we can assume it was
             * evicted and some other page may be in use at the spot.
             */
            PhysicalPage &target = physical_page(entry->mapped_ppage);
            if (target.used)
            {
                evict_ppage(target);
            }

            if (is_frame(entry->mapped_ppage))
            {
                m_frames.take(entry->mapped_ppage);
            }
            map_vpage_to_ppage(entry, target);
            page_in = &target;
        }
        else
        {
            /*
             * Unlikely that all physical pages are in use.
             */
            if (UNLIKELY(!m_frames.any_free()))
            {
                evict_ppage(clock_victim());
            }

            PhysicalPage &target = physical_page(m_frames.allocate());
            map_vpage_to_ppage(entry, target);
            page_in = &target;
        }
    }

    PhysicalPage &page = page_in != nullptr ? *page_in : physical_page(entry->ppage);
    if (UNLIKELY(page.kernel_locked && !ptable->kernel_privilege))
    {
        throw_fault(PageFaultException::Reason::KERNEL_ONLY, vpage, access);
    }

    /* Update the TLB with the result of the translation of virtual page to physical page. */
    TLB_Entry &tlb = m_tlb[vpage & (kMaxTLBSize - 1)];
    tlb.key = tlb_key(ptable->pid, vpage);
    tlb.ppage = entry->ppage;
    tlb.write = entry->write;

    // Only the access that fills the entry marks the page, a hit on it does not (see m_tlb).
    mark_referenced(page);
    return entry->ppage;
}

void VirtualMemory::check_clock()
{
    AEMU_DEBUG("Checking the clock");

    std::unordered_set<const PhysicalPage *> listed;
    const PhysicalPage *previous = nullptr;
    for (const PhysicalPage *page = m_clock_head; page != nullptr; page = page->clock_next)
    {
        AEMU_CHECK(page->clock_prev == previous, "Expected the previous page to be linked back");
        AEMU_CHECK(page->in_clock, "Expected a page on the list to be marked as on it");
        AEMU_CHECK(page->swappable, "Expected a page on the list to be swappable");
        AEMU_CHECK(listed.insert(page).second, "Expected a page to be on the list once");
        previous = page;
    }
    AEMU_CHECK(previous == m_clock_tail, "Expected the list to end at the tail");
    AEMU_CHECK(m_clock_hand == nullptr || listed.find(m_clock_hand) != listed.end(),
               "Expected the hand to point to a page on the list");

    for (const auto &[ppage, page] : m_physical_memory_map)
    {
        AEMU_CHECK(page.in_clock == (listed.find(&page) != listed.end()),
                   "Expected the marked pages to be the ones on the list, page {}", ppage);
    }
}

void VirtualMemory::set_walk_enabled(const bool enabled)
{
    m_walk = enabled;
    update_swap_key();
    invalidate_translations();
}

void VirtualMemory::set_page_table_base(const word base)
{
    m_ptbr = base & kPteFrameMask;
    invalidate_translations();
}

void VirtualMemory::invalidate_translations()
{
    for (WalkEntry &entry : m_walk_tlb)
    {
        entry.valid = false;
    }
    drop_fetch_cache();
}

void VirtualMemory::invalidate_translation(const word address)
{
    const word vpage = address >> kNumPageOffsetBits;
    WalkEntry &entry = m_walk_tlb[vpage & (kMaxTLBSize - 1)];
    if (entry.valid && entry.vpage == vpage)
    {
        entry.valid = false;
    }
    // The page that is fetched from stands for the entry of the TLB that has it, so it goes with
    // it, and only with it (the entry of another page in the same slot stays, and so does it).
    if (vpage == m_fetch_vpage)
    {
        drop_fetch_cache();
    }
}

word VirtualMemory::translate_address_slow(const word address, const AccessType access)
{
    const word vpage = address >> kNumPageOffsetBits;

    if (m_walk)
    {
        const WalkEntry &entry = m_walk_tlb[vpage & (kMaxTLBSize - 1)];
        if (LIKELY(entry.valid && entry.vpage == vpage && walk_allows(entry, access)))
        {
            return (entry.ppage << kNumPageOffsetBits) | (address & (kPageSize - 1));
        }
        return walk_translate(address, access);
    }

    // The devices are always where they are, so a program without page tables can use them.
    if (m_cur_ptable == nullptr || !m_enabled || address >= kDeviceBase)
    {
        return address;
    }

    // Not in the TLB, or in it without the permission to write (which is looked at again).
    const word ppage = access_vpage_slow(m_cur_ptable, vpage, access);
    return (ppage << kNumPageOffsetBits) | (address & (kPageSize - 1));
}

word VirtualMemory::translate_fetch_slow(const word address)
{
    const word vpage = address >> kNumPageOffsetBits;

    // The page tables say whether a page can be executed; the swapping memory has its own list,
    // which is looked at after the translation, since that is where the page is brought in.
    const word physical =
        translate_address(address, m_walk ? AccessType::EXECUTE : AccessType::READ);
    if (UNLIKELY(!m_walk && !can_execute_address(address)))
    {
        throw_fault(PageFaultException::Reason::EXECUTE_DENIED, vpage, AccessType::EXECUTE);
    }

    // Only now: the translation can have dropped it (the walk fills the TLB, a page that is
    // brought in evicts another).
    m_fetch_vpage = vpage;
    m_fetch_ppage = physical >> kNumPageOffsetBits;
    return physical;
}

// The walk: first level entry (10 bits of the address), second level entry (10 bits), page.
// Nothing is cached unless the access is allowed.
word VirtualMemory::walk_translate(const word address, const AccessType access)
{
    const word vpage = address >> kNumPageOffsetBits;
    const auto fault = [&](const PageFaultException::Reason reason)
    { throw_fault(reason, vpage, access); };

    word first = 0;
    if (!m_physical->read_physical_word(m_ptbr + ((address >> 22) << 2), first)
        || !(first & kPteValid))
    {
        fault(PageFaultException::Reason::UNMAPPED);
    }

    const word entry_address =
        (first & kPteFrameMask) + (((address >> kNumPageOffsetBits) & 0x3FF) << 2);
    word entry = 0;
    if (!m_physical->read_physical_word(entry_address, entry) || !(entry & kPteValid))
    {
        fault(PageFaultException::Reason::UNMAPPED);
    }

    if (m_user && !(entry & kPteUser))
    {
        fault(PageFaultException::Reason::KERNEL_ONLY);
    }
    if (access == AccessType::WRITE && !(entry & kPteWrite))
    {
        fault(PageFaultException::Reason::WRITE_DENIED);
    }
    // The kernel does not run code that a user process can write.
    if (access == AccessType::EXECUTE
        && (!(entry & kPteExecute) || (!m_user && (entry & kPteUser))))
    {
        fault(PageFaultException::Reason::EXECUTE_DENIED);
    }

    const word marked = entry | kPteAccessed | (access == AccessType::WRITE ? kPteDirty : 0);
    if (marked != entry && !m_physical->write_physical_word(entry_address, marked))
    {
        fault(PageFaultException::Reason::UNMAPPED);
    }

    // The slot may have held the entry of the page that is fetched from. The visible TLB keeps a
    // stale entry only as long as it is there, so what stands for it goes as well.
    drop_fetch_cache();
    WalkEntry &cached = m_walk_tlb[vpage & (kMaxTLBSize - 1)];
    cached = {.valid = true,
              .vpage = vpage,
              .ppage = marked >> kNumPageOffsetBits,
              .write = bool(marked & kPteWrite),
              .execute = bool(marked & kPteExecute),
              .user = bool(marked & kPteUser),
              .dirty = bool(marked & kPteDirty)};
    return (cached.ppage << kNumPageOffsetBits) | (address & (kPageSize - 1));
}
