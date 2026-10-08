#pragma once

#include "emulator32bit/disk.h"
#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/fbl.h"

#include <unordered_map>

constexpr U8 kNumTLBBits = 12;
constexpr U32 kMaxTLBSize = 1 << kNumTLBBits;
constexpr U32 kMaxProcesses = 1024;
constexpr U32 kMaxPhysicalPages = 1 << (8 * sizeof(word) - kNumPageOffsetBits);

/*
    idea

    mark virtual pages with read/write/execute permissions
    fix the current way multiple virtual addresses can map to the same physical address, instead,
    check the physical page if it is swappable, if not, then that means multiple virtual addresses
    will map to the same physical address. otherwise the physical page will be swapped out.

    allow multiple virtual addresses to map to a specific physical address
    we want to be able to map virtual addresses to ram, rom, disk, i/o, ports, etc (memory mapped)
        -> The physical address space needs to encompass all memory addresses, not just ram..
    Throw a segfault whenever a virtual address page is accessed but not mapped
    Manually map new virtual address pages to a specific range of addresses (possibly supplied with the memory object reference)
        -> this will map a specific virtual address page to an unused physical page. would also mean we need to be able to evict pages
        -> FBL's need to be updated to support this if it doesn't yet
    Manually map a specific virtual address page to a specific physical page
        -> this won't require a unique virtual address page mapping to physical page, multiple other virtual address pages can map to the same phyiscal page
        -> to prevent this from breaking evictions, should we just update the PageTableEntry to support multiple vpages
*/

class VirtualMemory
{
  public:
    /**
     * @param disk          Where pages that are not in physical memory are kept.
     * @param frame_lo_page First physical page that can hold a virtual page that is paged in. These
     *                      are the pages of RAM. A page outside of this range (like ROM) can only
     *                      be used by mapping a virtual page to it explicitly.
     * @param frame_pages   Number of such pages.
     */
    VirtualMemory(Disk *disk, word frame_lo_page = 0, word frame_pages = kMaxPhysicalPages);
    ~VirtualMemory();
    VirtualMemory(const VirtualMemory &) = delete;
    VirtualMemory &operator=(const VirtualMemory &) = delete;

    Disk *m_disk;

    /// Whether addresses are mapped by the page tables of the processes. Off, they are physical.
    bool enabled() const
    {
        return m_enabled;
    }

    void set_enabled(bool enabled)
    {
        m_enabled = enabled;
        drop_fetch_cache();
    }

    /// What a memory access does, which decides the permission it needs.
    enum class AccessType : U8
    {
        READ,
        WRITE,
        EXECUTE,
    };

    class VirtualMemoryException : public std::exception
    {
      protected:
        std::string message;

      public:
        VirtualMemoryException(const std::string &msg);

        const char *what() const noexcept override;
    };

    /**
     * @brief             An access that the page tables do not allow. The access did not happen.
     */
    class PageFaultException : public VirtualMemoryException
    {
      public:
        enum class Reason : U8
        {
            UNMAPPED,       ///< The virtual page is not part of the process.
            WRITE_DENIED,   ///< Write to a page that is not writable.
            EXECUTE_DENIED, ///< Execute of a page that is not executable.
            KERNEL_ONLY,    ///< Access to a physical page that needs kernel privilege.
        };

        PageFaultException(Reason reason, word vpage, AccessType access);

        Reason get_reason() const noexcept;
        word get_vpage() const noexcept;
        AccessType get_access() const noexcept;

      protected:
        Reason reason;
        word vpage;
        AccessType access;
    };

    class InvalidPIDException : public VirtualMemoryException
    {
      protected:
        long long invalid_pid;

      public:
        InvalidPIDException(const std::string &msg, long long invalid_pid);

        long long get_invalid_pid() const noexcept;
    };

    class VPageRemapException : public VirtualMemoryException
    {
      protected:
        word vpage;
        word already_mapped_ppage;
        word attempted_mapped_ppage;

      public:
        VPageRemapException(const std::string &msg, word vpage, word already_mapped_ppage,
                            word attempted_mapped_ppage);

        word get_vpage() const noexcept;
        word get_already_mapped_ppage() const noexcept;
        word get_attempted_mapped_ppage() const noexcept;
    };

    class InvalidVPageException : public VirtualMemoryException
    {
      protected:
        word vpage;

      public:
        InvalidVPageException(const std::string &msg, word vpage);

        word get_vpage() const noexcept;
    };

    /**
     * @brief             The physical memory that holds the pages that are paged in. The virtual
     *                    memory copies a page to it when the page is brought in from the disk, and
     *                    from it when the page is swapped out.
     */
    class PhysicalPages
    {
      public:
        virtual ~PhysicalPages() = default;

        /// Copies the physical page to `out`, which has room for kPageSize bytes.
        virtual void read_page(word ppage, byte *out) = 0;

        /// Copies kPageSize bytes to the physical page.
        virtual void write_page(word ppage, const byte *data) = 0;

        /// Reads the word at a physical address (the page table walker uses this). Returns
        /// false if no memory is there.
        virtual bool read_physical_word(word address, word &out) = 0;

        /// Writes a word to a physical address. Returns false if no memory is there.
        virtual bool write_physical_word(word address, word value) = 0;
    };

    /// @name Page tables in memory
    /// When the walk is enabled (`SCTLR.M`), virtual addresses are translated by walking a two
    /// level table in physical memory that the operating system owns, `process`es, the swapping
    /// and `begin_process` are not used. See docs/mmu.md.
    ///@{

    /// Bits of a page table entry (the last level, and the first level only needs
    /// valid and the frame).
    static constexpr word kPteValid = 1u << 0;
    static constexpr word kPteWrite = 1u << 1;
    static constexpr word kPteExecute = 1u << 2;
    static constexpr word kPteUser = 1u << 3;
    static constexpr word kPteAccessed = 1u << 4; ///< set by the hardware on any access
    static constexpr word kPteDirty = 1u << 5;    ///< set by the hardware on a write
    static constexpr word kPteFrameMask = ~word(kPageSize - 1);

    /// Whether addresses are translated by the page tables. Drops all cached translations.
    void set_walk_enabled(bool enabled);

    bool walk_enabled() const
    {
        return m_walk;
    }

    /// The physical address of the first level table (page aligned). Drops cached translations.
    void set_page_table_base(word base);

    word page_table_base() const
    {
        return m_ptbr;
    }

    /// Whether accesses are made in user mode, which may only use pages marked user.
    void set_user_mode(bool user)
    {
        if (user != m_user)
        {
            m_user = user;
            drop_fetch_cache();
        }
    }

    /// `TLBI`: forget the cached translation of the page that holds `address`, or all of them.
    void invalidate_translation(word address);
    void invalidate_translations();
    ///@}

    /**
     * @brief             Sets where the pages that are paged in are. Without it the pages have no
     *                    contents, which is enough to look at the mappings.
     */
    void set_physical_pages(PhysicalPages *physical);

    /**
     * @brief             Sets the current proccess to change the virtual space mappings.
     *
     * @throws            InvalidPIDException when the pid is not a valid process.
     * @param pid         Process id.
     */
    void set_process(long long pid);

    /**
     * @brief             Starts a new process with it's own virtual memory address space.
     *
     * @throws             VirtualMemoryException when MAX_PROCESSES limit is reached.
     * @param            kernel_privilege: Whether the process has the kernel level access
     *                     privilege
     * @return            New process id.
     */
    long long begin_process(bool kernel_privilege = false);

    /**
     * @brief             Ends a specified process.
     *
     * @throws             InvalidPIDException when the pid is not a valid process.
     * @param pid        Process id.
     */
    void end_process(long long pid);

    /**
     * @brief             Gets the current process identifier.
     *
     * @return             Current process ID, -1 if no current active process.
     */
    long long current_process();

    /**
     * @brief             Set the the access permissions of physical memory. Used by the kernel
     *                     to set up memory mapped regions for I/O.
     *
     * @param             ppage_begin: First physical page to set the permissions to.
     * @param             ppage_end: Last physical page to set the permissions to.
     * @param             swappable: Whether the pages in this region should be swappable. A page
     *                     that is not swappable is never chosen to make room for another page.
     * @param             kernel_locked: Whether the pages in this region require kernel level
     *                     privilege to access.
     */
    void set_ppage_permissions(word ppage_begin, word ppage_end, bool swappable,
                               bool kernel_locked);

    /**
     * @brief             Set the access permissions of virtual memory specific to a process.
     *
     * @throws            InvalidPIDException if the pid is invalid.
     * @param             pid: Process to set the access permissions.
     * @param             vpage_begin: First virtual page to update permissions.
     * @param             vpage_end: Last virtual page to update permissions.
     * @param             write: Virtual page write permissions.
     * @param             execute: Virtual page execute permissions.
     */
    void set_vpage_permissions(long long pid, word vpage_begin, word vpage_end, bool write,
                               bool execute);

    /**
     * @brief             Checks whether the virtual page has been added to the process.
     *
     * @throw            InvalidPIDException when pid is invalid.
     * @param             pid: Process identifier.
     * @param             vpage: Virtual page to check.
     * @return             Whether the virtual page is mapped for the process.
     */
    bool has_vpage(long long pid, word vpage);

    /**
     * @brief             Checks the write permissions of the virtual page by the process.
     *
     * @throw            InvalidPIDException when pid is invalid.
     * @param             pid: Process identifier.
     * @param             vpage: Virtual page to check.
     * @return             Whether the virtual page can be written to.
     */
    bool can_write_vpage(long long pid, word vpage);

    /**
     * @brief            Checks the execute permissions of the virtual page by the process.
     *
     * @throw            InvalidPIDException when pid is invalid.
     * @param             pid: Process identifier.
     * @param             vpage: Virtual page to check.
     * @return             Whether code in the virtual page can be executed.
     */
    bool can_execute_vpage(long long pid, word vpage);

    /**
     * @brief            Checks the access permissions of the physical page by the process.
     *
     * @throw            InvalidPIDException when pid is invalid.
     * @param             pid: Process identifier.
     * @param             ppage: Physical page to check.
     * @return             Whether the physical page can be accessed by a process.
     */
    bool can_access_ppage(long long pid, word ppage);

    /**
     * @brief            Adds a new virtual page to the specified process.
     *
     * @throws            InvalidPIDException when pid is invalid.
     * @throws            InvalidVPageException when the virtual page has already been mapped to
     *                     the process.
     * @param             pid: ID of the process to add a virtual page to.
     * @param             vpage: Virtual page to add.
     */
    void add_vpage(long long pid, word vpage, word length, bool write, bool execute);

    /**
     * @brief             Checks whether code at the virtual address of the current process may be
     *                     executed. Without an active process or with virtual memory disabled,
     *                     everything can be executed. Unmapped pages cannot.
     *
     * @param             address: Virtual address to check.
     */
    inline bool can_execute_address(word address)
    {
        if (UNLIKELY(m_cur_ptable == nullptr || !m_enabled))
        {
            return true;
        }

        const auto it = m_cur_ptable->entries.find(address >> kNumPageOffsetBits);
        return it != m_cur_ptable->entries.end() && it->second->execute;
    }

    /**
     * @brief             Converts the virtual address of an instruction into a physical address,
     *                     and checks that the page may be executed (the permission that
     *                     `translate_address` does not look at in the swapping memory).
     *
     * @details           The page of the last instruction fetched is remembered, and fetching from
     *                     it again is the translation of a few bits. The page is forgotten
     *                     whenever something that could change the answer happens: the mappings,
     *                     the permissions, the process, the mode or the page tables change, the TLB
     *                     entry of the page is dropped or replaced, or (in the swapping memory) the
     *                     hand of the clock clears the mark of the page, which is how a page that
     *                     is used by the fetch alone stays in memory.
     *
     * @throws            PageFaultException if the page may not be executed.
     * @param             address: Virtual address of the instruction.
     * @return            Physical address of the instruction.
     */
    inline word translate_fetch(word address)
    {
        if (LIKELY((address >> kNumPageOffsetBits) == m_fetch_vpage))
        {
            return (m_fetch_ppage << kNumPageOffsetBits) | (address & (kPageSize - 1));
        }
        return translate_fetch_slow(address);
    }

    /**
     * @brief             Throws the fault for an access that is not allowed.
     */
    [[noreturn]] static void throw_fault(PageFaultException::Reason reason, word vpage,
                                         AccessType access);

    /**
     * @brief             Converts a virtual address into a physical address of the process
     *                     specified by the process id if virtual memory
     *                     is enabled, otherwise the virtual address is equivalent to the physical
     *                     address.
     *
     * @throws            PageFaultException if the process may not make the access.
     * @param             pid: ID of the process to translate virtual address.
     * @param             address: Virtual address to translate.
     * @param             access: What the address is accessed for.
     * @return             Physical address corresponding to the virtual address.
     */
    inline word translate_address(long long pid, word address, AccessType access = AccessType::READ)
    {
        if (UNLIKELY(!m_enabled))
        {
            return address;
        }

        if (UNLIKELY(m_process_ptable_map.find(pid) == m_process_ptable_map.end()))
        {
            throw VirtualMemoryException("Invalid Process ID: " + std::to_string(pid));
        }

        return translate_address(m_process_ptable_map.at(pid), address, access);
    }

    /**
     * @brief             Converts a virtual address into a physical address if virtual memory
     *                     is enabled, otherwise the virtual address is equivalent to the physical
     *                     address.
     *
     * @throws            PageFaultException if the process may not make the access.
     * @param             address: Virtual address to translate.
     * @param             access: What the address is accessed for.
     * @return             Physical address corresponding to the virtual address.
     */
    inline word translate_address(word address, AccessType access = AccessType::READ)
    {
        if (UNLIKELY(m_walk))
        {
            const word vpage = address >> kNumPageOffsetBits;
            const WalkEntry &entry = m_walk_tlb[vpage & (kMaxTLBSize - 1)];
            if (LIKELY(entry.valid && entry.vpage == vpage && walk_allows(entry, access)))
            {
                return (entry.ppage << kNumPageOffsetBits) | (address & (kPageSize - 1));
            }
            return walk_translate(address, access);
        }

        // The devices are always where they are, so a program without page tables can use them.
        if (UNLIKELY(m_cur_ptable == nullptr || !m_enabled || address >= kDeviceBase))
        {
            return address;
        }

        return translate_address(m_cur_ptable, address, access);
    }

    /**
     * @brief             Makes a force mapping of the virtual page to the physical page if it
     *                     is not yet mapped.
     *
     * @throws            InvalidPIDException if pid is invalid.
     * @param             pid: Process identifier.
     * @param             vpage: Virtual page to force map to physical page if not yet mapped.
     * @param             ppage: Physical page to force map to.
     */
    void ensure_physical_page_mapping(long long pid, word vpage, word ppage);

  private:
    /**
     * @brief            Contains information about a physical page and what virtual pages
     *                     map to it.
     */
    struct PageTableEntry
    {
        /**
         * @brief         Construct a new Page Table Entry object.
         *
         * @param        pid: Process ID.
         * @param         vpage: Virtual page of this mapping.
         * @param         diskpage: Disk page where the virtual page resides.
         * @param        write: Whether virtual page can be written to.
         * @param        execute: Whether code can be executed from the virtual page.
         */
        PageTableEntry(long long pid, word vpage, word diskpage, bool write, bool execute);

        long long pid; /* Process that has this mapping. */
        word vpage;    /* Virtual page. */
        word ppage;    /* Mapped physical page if not on disk. */
        bool disk;     /* Whether the virtual page is on disk. */
        word diskpage; /* Corresponding disk page where the virtual page resides. */
        bool mapped;   /* Whether this is a mapped virtual page with a permanent physical page. */
        word mapped_ppage; /* Corresponding physical page as mentioned above. */

        bool write;        /* Whether this virtual page can be written to. */
        bool execute;      /* Whether this virtual page contains code to execute. */
    };

    struct PhysicalPage
    {
        std::vector<PageTableEntry *> mapped_vpages;
        word ppage = 0;

        bool used = false;

        bool swappable = true; /* Whether this physical page can be evicted/swapped. */
        bool kernel_locked =
            false; /* Whether this physical page requires kernel level permission to access. */

        /* The pages in use that can be swapped are on the clock, a ring in the order they were
           brought in (a list whose end is followed by its beginning). */
        PhysicalPage *clock_prev = nullptr;
        PhysicalPage *clock_next = nullptr;
        bool in_clock = false;

        /* Set by every access to the page, cleared when the hand of the clock passes it. A page
           that is not set when the hand gets to it was not used for a whole turn. */
        bool referenced = false;
    };

    /**
     * @brief            Contains information about the memory mapping of a specific process.
     */
    struct PageTable
    {
        long long pid = 0; /* Process ID. */

        /* Mapping of virtual page address to the corresponding PageTableEntry. */
        std::unordered_map<word, PageTableEntry *> entries =
            std::unordered_map<word, PageTableEntry *>();

        bool kernel_privilege;
    };

    /**
     * @brief            TBL Entry.
     */
    struct TLB_Entry
    {
        bool valid = false;           /* Whether the TLB_Entry is a valid translation. */
        long long pid = -1;           /* Corresponding process of the translation. */
        word vpage = 0;               /* Virtual page address of the translation. */
        word ppage = 0;               /* Resulting physical page address of the translation. */
        bool write = false;           /* Whether the translation can be used to write. */
        PhysicalPage *page = nullptr; /* The physical page, to record that it is used. */
    };

    /**
     * @brief             Translation Lookaside Buffer. Contains the recently translated virtual
     *                     page address to physical page address. An entry is only filled when the
     *                     process may access the page, and has the permission to write that the
     *                     page had then.
     */
    TLB_Entry m_tlb[kMaxTLBSize];

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
    inline bool walk_allows(const WalkEntry &entry, AccessType access) const
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

    /// Walks the tables: checks the entry, sets accessed and dirty, fills the cache. Throws
    /// PageFaultException.
    word walk_translate(word address, AccessType access);

    /**
     * @brief            Free PIDs not in use by any process.
     */
    FreeBlockList m_freepids;

    /**
     * @brief            Map of PID to the corresponding page table of the process.
     */
    std::unordered_map<long long, PageTable *> m_process_ptable_map;

    /**
     * @brief              Map of physical pages to the information about them. A page has an entry
     *                     once something is known about it (it is used, or its permissions are
     *                     set), so the map is as big as the memory in use and not as the address
     *                     space. The values do not move, the lists link them by pointer.
     */
    std::unordered_map<word, PhysicalPage> m_physical_memory_map;

    /// The physical pages that hold virtual pages that are paged in.
    word m_frame_lo;
    word m_frame_pages;

    /**
     * @brief            Free physical pages (within the frames) that new virtual pages can map to.
     */
    FreeBlockList m_freelist;

    /// Where the contents of the pages that are paged in are.
    PhysicalPages *m_physical = nullptr;

    /**
     * @brief             Current active process in which all calls to the virtual memory to
     *                     manipulate/use mappings without a supplied PID refers to.
     */
    PageTable *m_cur_ptable = nullptr;

    bool m_enabled = true;

    /**
     * @brief             The page that the last instruction was fetched from, with the physical page
     *                     it is in, once the fetch was allowed. See translate_fetch. The virtual
     *                     page is kNoPage (which no address has) when there is none.
     */
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
    word translate_fetch_slow(word address);

    /**
     * @brief            Beginning and end of the list that makes up the clock, the pages that can be
     *                     swapped. The end is followed by the beginning.
     */
    PhysicalPage *m_clock_head = nullptr;
    PhysicalPage *m_clock_tail = nullptr;

    /**
     * @brief            The page that the hand of the clock points to, the next one to look at when
     *                     a page has to be evicted. Null stands for the beginning of the list.
     */
    PhysicalPage *m_clock_hand = nullptr;

    /**
     * @brief             The information about a physical page, which is added if there is none.
     */
    PhysicalPage &physical_page(word ppage);

    /**
     * @brief             The information about a physical page, or null if there is none.
     */
    PhysicalPage *find_physical_page(word ppage);

    /**
     * @brief             Whether a virtual page that is paged in can be put in the physical page.
     */
    bool is_frame(word ppage) const;

    /**
     * @brief             Gives a physical page back to the free pages, if it is one of the frames.
     */
    void release_frame(word ppage);

    /**
     * @brief             Drops the cached translation of the virtual page of a process, if any.
     */
    void invalidate_tlb(long long pid, word vpage);

    /**
     * @brief             Drops all cached translations.
     */
    void flush_tlb();

    /**
     * @brief             Ensures that the virtual memory page tables memory mappings are valid.
     */
    void check_vm();

    /**
     * @brief             Records that a physical page was just used, so that the clock gives it
     *                     another turn. This is all an access costs, it is the reason for using
     *                     the clock and not a list that is reordered by every access.
     *
     * @details           The mark is only written when it is not set. It stays set until the hand
     *                     passes, so nearly every access just reads it, and the store that would
     *                     make the compiler reload the state of the emulator (it may alias any
     *                     byte) is left out of the translation of every instruction.
     */
    static inline void mark_referenced(PhysicalPage &page)
    {
        if (UNLIKELY(!page.referenced))
        {
            page.referenced = true;
        }
    }

    /**
     * @brief             Puts a physical page on the clock, just behind the hand (so that it is
     *                     the last page the hand gets to), as used. A page that is on it already
     *                     is moved.
     */
    void clock_add(PhysicalPage &page);

    /**
     * @brief             Takes a physical page off the clock, if it is on it.
     */
    void clock_remove(PhysicalPage &page);

    /**
     * @brief             The page to evict to make room. The hand goes round: a page that was used
     *                     since the hand was last there is passed over and marked as not used, the
     *                     first page that is not marked and is a frame and can be swapped is it.
     *
     * @throws            VirtualMemoryException if there is none.
     */
    word clock_victim();

    /**
     * @brief            Ensures the clock of the in use physical pages is valid.
     */
    void check_clock();

    /**
     * @brief             Removes the physical page and writes it back to disk, freeing up a
     *                     location for another virtual page to map to.
     *
     * @param             ppage: Physical page to evict.
     */
    void evict_ppage(word ppage);

    /**
     * @brief             Maps a virtual page to a specific physical page of the process
     *                     corresponding to the given pid.
     *
     * @throw             InvalidPIDException is pid is invalid.
     * @param             pid: ID of the process to map a virtual page.
     * @param             vpage: Virtual page to map.
     * @param             ppage: Physical page to map to.
     */
    void map_vpage_to_ppage(long long pid, word vpage, word ppage);

    /**
     * @brief             Maps a new virtual page to a physical page of the specified process.
     *                     Note this forces the virtual page to always map to the physical page.
     *
     * @throws            InvalidPIDException if pid is invalid.
     * @throws             InvalidVPageException if virtual page has already been added.
     * @param             pid: Process id to map virtual page.
     * @param             vpage: Virtual page to map.
     * @param             ppage: Physical page to map to.
     */
    void map_ppage(long long pid, word vpage, word ppage);

    /**
     * @brief             Removes the virtual page from a process referenced by it's pid.
     *
     * @throws            InvalidPIDException if pid is invalid.
     * @throws             InvalidVPageException if virtual page is not mapped to process.
     * @param             pid: Process id.
     * @param             vpage: Virtual page to remove.
     */
    void remove_vpage(long long pid, word vpage);

    /**
     * @brief            Translates a virtual space address to a physical space address. Note these
     *                     are not page addresses, but full memory address in the 0 to 2^31 - 1
     *                     range.
     *
     * @param             ptable: Page table of the process to access.
     * @param             address: Virtual space address.
     * @param             access: What the address is accessed for.
     * @return             Physical space address corresponding to the virtual space address of
     *                     this process.
     */
    inline word translate_address(PageTable *ptable, word address, AccessType access)
    {
        word vpage = address >> kNumPageOffsetBits;
        word ppage = access_vpage(ptable, vpage, access);

        return (ppage << kNumPageOffsetBits) + (address & (kPageSize - 1));
    }

    /**
     * @brief             Accesses a virtual page, performing the translation to the physical page
     *                     according to the page table.
     *
     * @details           The TLB (Translation Lookaside Buffer) helps avoid expensive calls to the
     *                     entry mapping. Recently accessed virtual pages will have the translation
     *                     stored in the buffer.
     *
     * @param             ptable: Page table of the process containing the virtual address mappings.
     * @param             vpage: Virtual page to map.
     * @param             access: What the page is accessed for.
     * @return             Physical page address.
     */
    inline word access_vpage(PageTable *ptable, word vpage, AccessType access)
    {
        const TLB_Entry &tlb = m_tlb[vpage & (kMaxTLBSize - 1)];
        if (LIKELY(tlb.valid && tlb.pid == ptable->pid && tlb.vpage == vpage))
        {
            if (UNLIKELY(access == AccessType::WRITE && !tlb.write))
            {
                throw_fault(PageFaultException::Reason::WRITE_DENIED, vpage, access);
            }
            mark_referenced(*tlb.page);
            return tlb.ppage;
        }

        return access_vpage_slow(ptable, vpage, access);
    }

    /**
     * @brief             Translates a virtual page that has no translation in the TLB: checks the
     *                     permissions, brings the page in from disk if it is not in memory, and
     *                     fills in the TLB.
     */
    word access_vpage_slow(PageTable *ptable, word vpage, AccessType access);
};
