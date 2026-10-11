#pragma once

#include "emulator32bit/disk.h"
#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/fbl.h"
#include "emulator32bit/frame_allocator.h"
#include "emulator32bit/physical_pages.h"

#include <memory>
#include <unordered_map>

/// The number of bits of the virtual page that pick an entry of the TLB.
constexpr U8 kNumTLBBits = 12;

/// The number of entries of the TLB.
constexpr U32 kMaxTLBSize = 1 << kNumTLBBits;

/// The number of processes. Their numbers are below it.
constexpr U32 kMaxProcesses = 1024;

/// The number of pages of the physical address space.
constexpr U32 kMaxPhysicalPages = 1 << (8 * sizeof(word) - kNumPageOffsetBits);

/// The MMU: translates the virtual addresses of a program to physical addresses, in one of two
/// ways (docs/mmu.md).
///
/// By default the memory of each process is paged: the virtual pages of a process are brought
/// into the frames (the pages of the RAM) when they are used and swapped out to the Disk when the
/// frames are full, evicting a page with a clock that approximates least recently used. Every
/// virtual page has read/write/execute permissions, and an access that is not allowed throws a
/// PageFault. A virtual page can also be mapped to a specific physical page (RAM, ROM, a
/// device), which is then never swapped.
///
/// With `SCTLR.M` set (set_walk_enabled), the addresses are translated by walking the two level
/// page table that the operating system keeps in physical memory, and the processes and the
/// swapping are not used.
class VirtualMemory
{
  public:
    /// @param[in] disk Where pages that are not in physical memory are kept.
    /// @param[in] frame_lo_page First physical page that can hold a virtual page that is paged in.
    ///     These are the pages of RAM. A page outside of this range (like ROM) can only be used by
    ///     mapping a virtual page to it explicitly.
    /// @param[in] frame_pages Number of such pages.
    /// @param[in] physical Where the pages that are paged in are, see set_physical_pages. Without
    ///     it the pages have no contents, which is enough to look at the mappings.
    VirtualMemory(Disk &disk, word frame_lo_page = 0, word frame_pages = kMaxPhysicalPages,
                  PhysicalPages *physical = nullptr);
    ~VirtualMemory();
    VirtualMemory(const VirtualMemory &) = delete;
    VirtualMemory &operator=(const VirtualMemory &) = delete;

    /// The disk that the pages are swapped to.
    Disk &m_disk;

    /// @return Whether addresses are mapped by the page tables of the processes. Off, they are
    ///     physical.
    bool enabled() const
    {
        return m_enabled;
    }

    /// @param[in] enabled Whether addresses are mapped by the processes, or physical.
    void set_enabled(const bool enabled)
    {
        m_enabled = enabled;
        update_swap_key();
        drop_fetch_cache();
    }

    /// What a memory access does, which decides the permission it needs.
    enum class AccessType : U8
    {
        READ,
        WRITE,
        EXECUTE,
    };

    /// Any error of the virtual memory.
    class Error : public std::exception
    {
      protected:
        std::string message;

      public:
        /// @param[in] msg What is wrong.
        Error(const std::string &msg);

        /// @return The message.
        const char *what() const noexcept override;
    };

    /// An access that the page tables do not allow. The access did not happen.
    class PageFault : public Error
    {
      public:
        /// Why the access was refused.
        enum class Reason : U8
        {
            UNMAPPED,       ///< The virtual page is not part of the process.
            WRITE_DENIED,   ///< Write to a page that is not writable.
            EXECUTE_DENIED, ///< Execute of a page that is not executable.
            KERNEL_ONLY,    ///< Access to a physical page that needs kernel privilege.
        };

        /// @param[in] reason Why the access was refused.
        /// @param[in] vpage The virtual page that was accessed.
        /// @param[in] access What the access was.
        PageFault(Reason reason, word vpage, AccessType access);

        /// @return Why the access was refused.
        Reason get_reason() const noexcept;

        /// @return The virtual page that was accessed.
        word get_vpage() const noexcept;

        /// @return What the access was.
        AccessType get_access() const noexcept;

      protected:
        Reason reason;
        word vpage;
        AccessType access;
    };

    /// A process id that is not a process.
    class InvalidPid : public Error
    {
      protected:
        long long invalid_pid;

      public:
        /// @param[in] msg What is wrong.
        /// @param[in] invalid_pid The process id that is not a process.
        InvalidPid(const std::string &msg, long long invalid_pid);

        /// @return The process id that is not a process.
        long long get_invalid_pid() const noexcept;
    };

    /// A virtual page that is mapped to a physical page was mapped to another physical page.
    class VPageAlreadyMapped : public Error
    {
      protected:
        word vpage;
        word already_mapped_ppage;
        word attempted_mapped_ppage;

      public:
        /// @param[in] msg What is wrong.
        /// @param[in] vpage The virtual page.
        /// @param[in] already_mapped_ppage The physical page that it is mapped to.
        /// @param[in] attempted_mapped_ppage The physical page that it was to be mapped to.
        VPageAlreadyMapped(const std::string &msg, word vpage, word already_mapped_ppage,
                           word attempted_mapped_ppage);

        /// @return The virtual page.
        word get_vpage() const noexcept;

        /// @return The physical page that the virtual page is mapped to.
        word get_already_mapped_ppage() const noexcept;

        /// @return The physical page that it was to be mapped to.
        word get_attempted_mapped_ppage() const noexcept;
    };

    /// A virtual page that was added to a process twice, or that was never added to it.
    class InvalidVPage : public Error
    {
      protected:
        word vpage;

      public:
        /// @param[in] msg What is wrong.
        /// @param[in] vpage The virtual page.
        InvalidVPage(const std::string &msg, word vpage);

        /// @return The virtual page.
        word get_vpage() const noexcept;
    };

    /// The physical memory that holds the pages that are paged in. The virtual memory copies a page
    /// to it when the page is brought in from the disk, and from it when the page is swapped out.
    using PhysicalPages = ::PhysicalPages;

    /// @name Page tables in memory
    /// When the walk is enabled (`SCTLR.M`), virtual addresses are translated by walking a two
    /// level table in physical memory that the operating system owns. Processes, the swapping and
    /// `begin_process` are not used. See docs/mmu.md.
    /// @{

    /// The bits of a page table entry. An entry of the first level only uses valid and the frame.
    static constexpr word kPteValid = 1u << 0;
    static constexpr word kPteWrite = 1u << 1;
    static constexpr word kPteExecute = 1u << 2;
    static constexpr word kPteUser = 1u << 3;
    static constexpr word kPteAccessed = 1u << 4; ///< Set by the hardware on any access.
    static constexpr word kPteDirty = 1u << 5;    ///< Set by the hardware on a write.
    /// The bits of an entry that are the physical page.
    static constexpr word kPteFrameMask = ~word(kPageSize - 1);

    /// Whether addresses are translated by the page tables. Drops all cached translations.
    ///
    /// @param[in] enabled `true` to walk the page tables, false for the swapping memory.
    void set_walk_enabled(bool enabled);

    /// @return Whether addresses are translated by the page tables.
    bool walk_enabled() const
    {
        return m_walk;
    }

    /// Sets the first level table to walk, and drops the cached translations.
    ///
    /// @param[in] base The physical address of the first level table (page aligned).
    void set_page_table_base(word base);

    /// @return The physical address of the first level table.
    word page_table_base() const
    {
        return m_ptbr;
    }

    /// Sets whether accesses are made in user mode, which may only use pages marked user.
    ///
    /// @param[in] user `true` for user mode.
    void set_user_mode(const bool user)
    {
        if (user != m_user)
        {
            m_user = user;
            drop_fetch_cache();
        }
    }

    /// `TLBI xn`: forgets the cached translation of the page that holds `address`.
    ///
    /// @param[in] address A virtual address.
    void invalidate_translation(word address);

    /// `TLBI`: forgets all the cached translations.
    void invalidate_translations();
    /// @}

    /// Sets the physical memory that holds the pages that are paged in. Without it the pages have
    /// no contents, which is enough to look at the mappings.
    ///
    /// @param[in] physical The physical memory, which must outlive this.
    void set_physical_pages(PhysicalPages *physical);

    /// Sets the current process, which the calls without a pid change the mappings of.
    ///
    /// @param[in] pid Process id.
    /// @throws InvalidPid When the pid is not a valid process.
    void set_process(long long pid);

    /// Starts a new process with its own virtual memory address space.
    ///
    /// @throws Error When MAX_PROCESSES limit is reached.
    /// @param[in] kernel_privilege Whether the process has the kernel level access
    ///     privilege.
    /// @return New process id.
    long long begin_process(bool kernel_privilege = false);

    /// Ends a specified process.
    ///
    /// @throws InvalidPid When the pid is not a valid process.
    /// @param[in] pid Process id.
    void end_process(long long pid);

    /// Gets the current process identifier.
    ///
    /// @return Current process ID, -1 if no current active process.
    long long current_process();

    /// Checks that the page tables, the physical pages and the clock agree with each other. A
    /// check that fails is an AEMU_CHECK, so it is meant for tests and debugging, not for a run.
    void check_consistency()
    {
        check_vm();
        check_clock();
    }

    /// Sets the access permissions of physical memory. Used by the kernel
    /// to set up memory mapped regions for I/O.
    ///
    /// @param[in] ppage_begin First physical page to set the permissions to.
    /// @param[in] ppage_end Last physical page to set the permissions to.
    /// @param[in] swappable Whether the pages in this region should be swappable. A page
    ///     that is not swappable is never chosen to make room for another page.
    /// @param[in] kernel_locked Whether the pages in this region require kernel level
    ///     privilege to access.
    void set_ppage_permissions(word ppage_begin, word ppage_end, bool swappable,
                               bool kernel_locked);

    /// Set the access permissions of virtual memory specific to a process.
    ///
    /// @throws InvalidPid If the pid is invalid.
    /// @param[in] pid Process to set the access permissions.
    /// @param[in] vpage_begin First virtual page to update permissions.
    /// @param[in] vpage_end Last virtual page to update permissions.
    /// @param[in] write Virtual page write permissions.
    /// @param[in] execute Virtual page execute permissions.
    void set_vpage_permissions(long long pid, word vpage_begin, word vpage_end, bool write,
                               bool execute);

    /// Checks whether the virtual page has been added to the process.
    ///
    /// @throws InvalidPid When pid is invalid.
    /// @param[in] pid Process identifier.
    /// @param[in] vpage Virtual page to check.
    /// @return Whether the virtual page is mapped for the process.
    bool has_vpage(long long pid, word vpage);

    /// Adds new virtual pages to the specified process.
    ///
    /// @throws InvalidPid When pid is invalid.
    /// @throws InvalidVPage When a virtual page has already been mapped to
    ///     the process.
    /// @param[in] pid ID of the process to add virtual pages to.
    /// @param[in] vpage First virtual page to add.
    /// @param[in] length Number of pages to add.
    /// @param[in] write Whether the pages can be written to.
    /// @param[in] execute Whether code in the pages can be executed.
    void add_vpage(long long pid, word vpage, word length, bool write, bool execute);

    /// Checks whether code at the virtual address of the current process may be
    /// executed. Without an active process or with virtual memory disabled,
    /// everything can be executed. Unmapped pages cannot.
    ///
    /// @param[in] address Virtual address to check.
    /// @return Whether the code can be executed.
    inline bool can_execute_address(const word address)
    {
        if (UNLIKELY(m_cur_ptable == nullptr || !m_enabled))
        {
            return true;
        }

        const auto it = m_cur_ptable->entries.find(address >> kNumPageOffsetBits);
        return it != m_cur_ptable->entries.end() && it->second->execute;
    }

    /// Converts the virtual address of an instruction into a physical address,
    /// and checks that the page may be executed (the permission that
    /// `translate_address` does not look at in the swapping memory).
    ///
    /// The page of the last instruction fetched is remembered, and fetching from
    /// it again is the translation of a few bits. The page is forgotten
    /// whenever something that could change the answer happens: the mappings,
    /// the permissions, the process, the mode or the page tables change, the TLB
    /// entry of the page is dropped or replaced, or (in the swapping memory) the
    /// hand of the clock clears the mark of the page, which is how a page that
    /// is used by the fetch alone stays in memory.
    ///
    /// @throws PageFault If the page may not be executed.
    /// @param[in] address Virtual address of the instruction.
    /// @return Physical address of the instruction.
    inline word translate_fetch(const word address)
    {
        if (LIKELY((address >> kNumPageOffsetBits) == m_fetch_vpage))
        {
            return (m_fetch_ppage << kNumPageOffsetBits) | (address & (kPageSize - 1));
        }
        return translate_fetch_slow(address);
    }

    /// Throws the fault for an access that is not allowed.
    ///
    /// @param[in] reason Why the access is not allowed.
    /// @param[in] vpage The virtual page of the access.
    /// @param[in] access What the page is accessed for.
    [[noreturn]] static void throw_fault(PageFault::Reason reason, word vpage, AccessType access);

    /// Converts a virtual address into a physical address of the process
    /// specified by the process id if virtual memory
    /// is enabled, otherwise the virtual address is equivalent to the physical
    /// address.
    ///
    /// @param[in] pid ID of the process to translate virtual address.
    /// @param[in] address Virtual address to translate.
    /// @param[in] access What the address is accessed for.
    /// @return Physical address corresponding to the virtual address.
    /// @throws PageFault If the process may not make the access.
    /// @throws Error If the pid is not a process (and the virtual memory is enabled).
    inline word translate_address(const long long pid, const word address,
                                  const AccessType access = AccessType::READ)
    {
        if (UNLIKELY(!m_enabled))
        {
            return address;
        }

        if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
        {
            throw Error("Invalid Process ID: " + std::to_string(pid));
        }

        return translate_address(*m_process_ptable_map.at(pid), address, access);
    }

    /// Converts a virtual address into a physical address if virtual memory
    /// is enabled, otherwise the virtual address is equivalent to the physical
    /// address.
    ///
    /// @throws PageFault If the process may not make the access.
    /// @param[in] address Virtual address to translate.
    /// @param[in] access What the address is accessed for.
    /// @return Physical address corresponding to the virtual address.
    inline word translate_address(const word address, const AccessType access = AccessType::READ)
    {
        // The translation of a page of the current process, in the swapping memory, is what
        // nearly every access finds. It is one comparison: when the virtual memory is off, or the
        // page tables are used, the key never matches and translate_address_slow decides.
        const word vpage = address >> kNumPageOffsetBits;
        const TLB_Entry &tlb = m_tlb[vpage & (kMaxTLBSize - 1)];
        if (LIKELY(tlb.key == (m_swap_key | vpage) && (access != AccessType::WRITE || tlb.write)))
        {
            return (tlb.ppage << kNumPageOffsetBits) | (address & (kPageSize - 1));
        }

        // The page tables are what the key of the swapping memory never matches for, and their TLB
        // is just as hot.
        if (UNLIKELY(m_walk))
        {
            const WalkEntry &entry = m_walk_tlb[vpage & (kMaxTLBSize - 1)];
            if (LIKELY(entry.valid && entry.vpage == vpage && walk_allows(entry, access)))
            {
                return (entry.ppage << kNumPageOffsetBits) | (address & (kPageSize - 1));
            }
        }
        return translate_address_slow(address, access);
    }

    /// Maps the virtual page to the physical page, as a forced mapping, if it is not mapped yet.
    ///
    /// @throws InvalidPid If pid is invalid.
    /// @param[in] pid Process identifier.
    /// @param[in] vpage Virtual page to force map to physical page if not yet mapped.
    /// @param[in] ppage Physical page to force map to.
    void ensure_physical_page_mapping(long long pid, word vpage, word ppage);

  private:
    /// The mapping of one virtual page of a process: whether it is in a physical page or on the
    /// disk, and what the process may do with it.
    struct PageTableEntry
    {
        /// Makes the entry of a virtual page that is on the disk.
        ///
        /// @param[in] pid Process ID.
        /// @param[in] vpage Virtual page of this mapping.
        /// @param[in] diskpage Disk page where the virtual page resides.
        /// @param[in] write Whether virtual page can be written to.
        /// @param[in] execute Whether code can be executed from the virtual page.
        PageTableEntry(long long pid, word vpage, word diskpage, bool write, bool execute);

        long long pid; ///< Process that has this mapping.
        word vpage; ///< Virtual page.
        word ppage; ///< Mapped physical page if not on disk.
        bool disk; ///< Whether the virtual page is on disk.
        word diskpage; ///< Corresponding disk page where the virtual page resides.
        bool mapped; ///< Whether this is a mapped virtual page with a permanent physical page.
        word mapped_ppage; ///< Corresponding physical page as mentioned above.

        bool write; ///< Whether this virtual page can be written to.
        bool execute; ///< Whether this virtual page contains code to execute.
    };

    /// A physical page, and the virtual pages that are mapped to it.
    struct PhysicalPage
    {
        std::vector<PageTableEntry *> mapped_vpages;
        word ppage = 0;

        bool used = false;

        bool swappable = true; ///< Whether this physical page can be evicted/swapped.
        bool kernel_locked = false; ///< Whether this physical page requires kernel privilege.

        /// The pages in use that can be swapped are on the clock, a ring in the order they were
        /// brought in (a list whose end is followed by its beginning).
        PhysicalPage *clock_prev = nullptr;
        PhysicalPage *clock_next = nullptr;
        bool in_clock = false;

        /// Set when a translation of the page is filled into the TLB, cleared when the hand of the
        /// clock passes it (which also drops the TLB, so that the next access sets it again). A
        /// page that is not set when the hand gets to it was not used for a whole turn.
        bool referenced = false;
    };

    /// Contains information about the memory mapping of a specific process.
    struct PageTable
    {
        long long pid = 0; ///< Process ID.

        /// Mapping of virtual page address to the corresponding PageTableEntry.
        std::unordered_map<word, PageTableEntry *> entries =
            std::unordered_map<word, PageTableEntry *>();

        bool kernel_privilege;
    };

    /// What an entry of the TLB is looked up by: the process in the upper half, the virtual page
    /// in the lower, so that the lookup is one comparison. No entry has the key of a process that
    /// does not exist (processes are numbered below kMaxProcesses).
    ///
    /// @param[in] pid The process.
    /// @param[in] vpage The virtual page.
    /// @return The key.
    static constexpr U64 tlb_key(const long long pid, const word vpage)
    {
        return (U64(pid) << kNumWordBits) | vpage;
    }

    /// The key of an entry that has no translation, which no lookup has.
    static constexpr U64 kNoKey = ~U64(0);

    /// The key that is looked up when no process has its pages translated through the TLB (the
    /// virtual memory is off, the page tables are used, or there is no process). No entry has it,
    /// and it is not kNoKey either, so the lookup fails and the slow path decides.
    static constexpr U64 kNoProcessKey = U64(1) << 62;

    /// An entry of the TLB.
    struct TLB_Entry
    {
        U64 key = kNoKey; ///< The process and the virtual page of the translation, see tlb_key.
        word ppage = 0; ///< Resulting physical page address of the translation.
        bool write = false; ///< Whether the translation can be used to write.
    };

    /// The translation lookaside buffer: the recent translations of a virtual page to a physical
    /// page. An entry is only filled when the
    /// process may access the page, and has the permission to write that the
    /// page had then.
    ///
    /// Filling an entry marks the physical page as used (for the clock), and a
    /// hit does not. That is what the hand of the clock makes up for: when it
    /// clears the marks it drops the whole TLB, and the next access to a page
    /// that is still in use fills its entry and marks it again.
    TLB_Entry m_tlb[kMaxTLBSize];

    /// The upper half of the key of the entries of the current process, or kNoProcessKey if the
    /// pages of no process are translated through m_tlb. Kept up to date by update_swap_key.
    U64 m_swap_key = kNoProcessKey;

    /// Called when the current process changes, or whether the virtual memory is on, or whether
    /// the page tables are used.
    inline void update_swap_key()
    {
        m_swap_key = (m_cur_ptable != nullptr && m_enabled && !m_walk)
                         ? tlb_key(m_cur_ptable->pid, 0)
                         : kNoProcessKey;
    }

    /// A cached translation of the page table walk. Keeps the bits of the entry, so a later access
    /// of another kind (a write to a page that is not dirty yet) takes the slow path.
    struct WalkEntry
    {
        bool valid = false;
        word vpage = 0;
        word ppage = 0;
        bool write = false;
        bool execute = false;
        bool user = false;
        bool dirty = false;
    };

    WalkEntry m_walk_tlb[kMaxTLBSize];
    bool m_walk = false;
    bool m_user = false;
    word m_ptbr = 0;

    /// Whether the cached translation allows the access in the current mode.
    ///
    /// @param[in] entry The cached translation.
    /// @param[in] access The kind of access.
    /// @return Whether it is allowed.
    inline bool walk_allows(const WalkEntry &entry, const AccessType access) const
    {
        if (m_user ? !entry.user : (access == AccessType::EXECUTE && entry.user))
        {
            return false;
        }
        switch (access)
        {
        case AccessType::WRITE:
            return entry.write && entry.dirty;
        case AccessType::EXECUTE:
            return entry.execute;
        default:
            return true;
        }
    }

    /// Walks the tables: first level entry (10 bits of the address), second level entry (10 bits),
    /// page. Checks the entry, sets accessed and dirty, and fills the cache. Nothing is cached
    /// unless the access is allowed.
    ///
    /// @param[in] address The virtual address.
    /// @param[in] access What the address is accessed for.
    /// @return The physical address.
    /// @throws PageFault If the access is not allowed.
    word walk_translate(word address, AccessType access);

    /// Free PIDs not in use by any process.
    FreeBlockList m_freepids;

    /// Map of PID to the corresponding page table of the process.
    std::unordered_map<long long, PageTable *> m_process_ptable_map;

    /// Map of the physical pages that are not frames to the information about
    /// them (an explicit mapping can be to any page). A page has an entry once
    /// something is known about it (it is used, or its permissions are set), so
    /// the map is as big as the memory in use and not as the address space. The
    /// values do not move, the lists link them by pointer. The frames are in
    /// m_frame_chunks, a lookup in the map is a hash and a division.
    std::unordered_map<word, PhysicalPage> m_physical_memory_map;

    /// The physical pages that hold virtual pages that are paged in.
    word m_frame_lo;
    word m_frame_pages;

    /// The information about the frames, in chunks of kFrameChunkPages that are made when one of
    /// their pages is first looked at (a machine with 4 GB of RAM has a million frames, and most
    /// of a program's memory is never touched). The pages do not move, the lists link them by
    /// pointer.
    static constexpr word kFrameChunkBits = 9;
    static constexpr word kFrameChunkPages = word(1) << kFrameChunkBits;
    std::vector<std::unique_ptr<PhysicalPage[]>> m_frame_chunks;

    /// Free physical pages (within the frames) that new virtual pages can map to.
    FrameAllocator m_frames;

    /// Where the contents of the pages that are paged in are.
    PhysicalPages *m_physical = nullptr;

    /// A page on its way between the physical memory and the disk, so that swapping does not
    /// allocate one for every page.
    std::vector<byte> m_page_buffer = std::vector<byte>(kPageSize);

    /// The page table of the current process, which the calls without a pid use.
    PageTable *m_cur_ptable = nullptr;

    bool m_enabled = true;

    /// The page that the last instruction was fetched from, with the physical page
    /// it is in, once the fetch was allowed. See translate_fetch. The virtual
    /// page is kNoPage (which no address has) when there is none.
    static constexpr word kNoPage = ~word(0);
    word m_fetch_vpage = kNoPage;
    word m_fetch_ppage = 0;

    /// Forgets the page that instructions are fetched from. Everything that changes whether a
    /// fetch is allowed, or where the page is, calls this.
    inline void drop_fetch_cache()
    {
        m_fetch_vpage = kNoPage;
    }

    /// translate_fetch for a page that is not remembered: translates, checks and remembers it.
    ///
    /// @param[in] address The virtual address of the instruction.
    /// @return The physical address.
    word translate_fetch_slow(word address);

    /// The first and the last page of the list that makes up the clock, the pages that can be
    /// swapped. The last is followed by the first.
    PhysicalPage *m_clock_head = nullptr;
    PhysicalPage *m_clock_tail = nullptr;

    /// The page that the hand of the clock points to, the next one to look at when
    /// a page has to be evicted. Null stands for the beginning of the list.
    PhysicalPage *m_clock_hand = nullptr;

    /// The information about a physical page, which is added if there is none.
    ///
    /// @param[in] ppage The physical page.
    /// @return The information.
    PhysicalPage &physical_page(const word ppage)
    {
        // A frame whose chunk is made: two loads. The index wraps for a page below the frames.
        const U64 index = U64(ppage) - m_frame_lo;
        if (LIKELY(index < m_frame_pages))
        {
            const std::unique_ptr<PhysicalPage[]> &chunk = m_frame_chunks[index >> kFrameChunkBits];
            if (LIKELY(chunk != nullptr))
            {
                return chunk[index & (kFrameChunkPages - 1)];
            }
        }
        return physical_page_slow(ppage);
    }

    /// physical_page () for a frame whose chunk is not made yet, and for a page that is not a
    /// frame.
    ///
    /// @param[in] ppage The physical page.
    /// @return The information.
    PhysicalPage &physical_page_slow(word ppage);

    /// Every physical page that there is information about, with its number (the consistency
    /// checks use it).
    ///
    /// @return Each page and its information.
    std::vector<std::pair<word, const PhysicalPage *>> all_physical_pages() const;

    /// Whether the physical page is one of the frames, which the pages that are paged in go to.
    ///
    /// @param[in] ppage The physical page.
    /// @return Whether it is.
    bool is_frame(word ppage) const;

    /// Gives a physical page back to the free pages, if it is one of the frames.
    ///
    /// @param[in] ppage The physical page.
    void release_frame(word ppage);

    /// Drops the cached translation of the virtual page of a process, if any.
    ///
    /// @param[in] pid The process.
    /// @param[in] vpage The virtual page.
    void invalidate_tlb(long long pid, word vpage);

    /// Drops all cached translations.
    void flush_tlb();

    /// Checks that the page tables and the mappings agree (see check_consistency ()).
    void check_vm();

    /// Records that a physical page was just used, so that the clock gives it
    /// another turn. A translation that fills the TLB does it, a hit on the
    /// TLB does not (see m_tlb), which is why the clock costs a translation
    /// nothing.
    ///
    /// @param[out] page The page.
    static inline void mark_referenced(PhysicalPage &page)
    {
        page.referenced = true;
    }

    /// Puts a physical page on the clock, just behind the hand (so that it is
    /// the last page the hand gets to), as used. A page that is on it already
    /// is moved.
    ///
    /// @param[in,out] page The page.
    void clock_add(PhysicalPage &page);

    /// Takes a physical page off the clock, if it is on it.
    ///
    /// @param[in,out] page The page.
    void clock_remove(PhysicalPage &page);

    /// The page to evict to make room. The hand goes round: a page that was used
    /// since the hand was last there is passed over and marked as not used, the
    /// first page that is not marked and is a frame and can be swapped is it.
    ///
    /// @return The page.
    /// @throws Error If there is none.
    PhysicalPage &clock_victim();

    /// Checks that the list of the clock is linked both ways and holds the pages that are on it.
    void check_clock();

    /// Writes the contents of the physical page back to the disk and frees the page for another
    /// virtual page to map to.
    ///
    /// @param[in,out] page Physical page to evict.
    void evict_ppage(PhysicalPage &page);

    /// Brings a virtual page that is on the disk into a physical page, which is
    /// free. The callers already have the entry and the page, and looking up either costs a hash.
    ///
    /// @param[in,out] entry The virtual page, of a process that exists.
    /// @param[in,out] page Physical page to map to.
    void map_vpage_to_ppage(PageTableEntry &entry, PhysicalPage &page);

    /// Maps a new virtual page to a physical page of the specified process.
    /// Note this forces the virtual page to always map to the physical page.
    ///
    /// @throws InvalidPid If pid is invalid.
    /// @throws InvalidVPage If virtual page has already been added.
    /// @param[in] pid Process id to map virtual page.
    /// @param[in] vpage Virtual page to map.
    /// @param[in] ppage Physical page to map to.
    void map_ppage(long long pid, word vpage, word ppage);

    /// Removes the virtual page from the process with the pid.
    ///
    /// @throws InvalidPid If pid is invalid.
    /// @throws InvalidVPage If virtual page is not mapped to process.
    /// @param[in] pid Process id.
    /// @param[in] vpage Virtual page to remove.
    void remove_vpage(long long pid, word vpage);

    /// Translates a virtual address to a physical address. These are full addresses, not page
    /// numbers.
    ///
    /// @param[in] ptable Page table of the process to access.
    /// @param[in] address Virtual space address.
    /// @param[in] access What the address is accessed for.
    /// @return Physical space address corresponding to the virtual space address of
    ///     this process.
    inline word translate_address(const PageTable &ptable, const word address,
                                  const AccessType access)
    {
        const word vpage = address >> kNumPageOffsetBits;
        const word ppage = access_vpage(ptable, vpage, access);

        return (ppage << kNumPageOffsetBits) + (address & (kPageSize - 1));
    }

    /// Accesses a virtual page, performing the translation to the physical page
    /// according to the page table.
    ///
    /// The TLB (Translation Lookaside Buffer) helps avoid expensive calls to the
    /// entry mapping. Recently accessed virtual pages will have the translation
    /// stored in the buffer.
    ///
    /// @param[in] ptable Page table of the process containing the virtual address mappings.
    /// @param[in] vpage Virtual page to map.
    /// @param[in] access What the page is accessed for.
    /// @return Physical page address.
    inline word access_vpage(const PageTable &ptable, const word vpage, const AccessType access)
    {
        const TLB_Entry &tlb = m_tlb[vpage & (kMaxTLBSize - 1)];
        if (LIKELY(tlb.key == tlb_key(ptable.pid, vpage)
                   && (access != AccessType::WRITE || tlb.write)))
        {
            return tlb.ppage;
        }

        return access_vpage_slow(ptable, vpage, access);
    }

    /// Translates a virtual page that has no translation in the TLB, or that the
    /// translation does not allow to write: checks the permissions, brings the
    /// page in from disk if it is not in memory, and fills in the TLB.
    ///
    /// @param[in] ptable The page table of the process.
    /// @param[in] vpage The virtual page.
    /// @param[in] access The kind of access.
    /// @return The physical page, as a page number.
    word access_vpage_slow(const PageTable &ptable, word vpage, AccessType access);

    /// translate_address for everything that is not in the TLB of the current
    /// process: the page tables, addresses that are physical (virtual memory
    /// off, no process, the devices) and the first access to a page.
    ///
    /// @param[in] address The virtual address.
    /// @param[in] access The kind of access.
    /// @return The physical address.
    word translate_address_slow(word address, AccessType access);
};
