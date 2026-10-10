#pragma once

#include "emulator32bit/disk.h"
#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/fbl.h"
#include "emulator32bit/frame_allocator.h"
#include "emulator32bit/physical_pages.h"

#include <memory>
#include <unordered_map>

constexpr U8 kNumTLBBits = 12;
constexpr U32 kMaxTLBSize = 1 << kNumTLBBits;
constexpr U32 kMaxProcesses = 1024;
constexpr U32 kMaxPhysicalPages = 1 << (8 * sizeof(word) - kNumPageOffsetBits);

/// The MMU: translates the virtual addresses of a program to physical addresses, in one of two
/// ways (docs/mmu.md).
///
/// By default the memory of each process is paged: the virtual pages of a process are brought
/// into the frames (the pages of the RAM) when they are used and swapped out to the Disk when the
/// frames are full, evicting a page with a clock that approximates least recently used. Every
/// virtual page has read/write/execute permissions, and an access that is not allowed throws a
/// PageFaultException. A virtual page can also be mapped to a specific physical page (RAM, ROM, a
/// device), which is then never swapped.
///
/// With `SCTLR.M` set (set_walk_enabled), the addresses are translated by walking the two level
/// page table that the operating system keeps in physical memory, and the processes and the
/// swapping are not used.
class VirtualMemory
{
  public:
    /// @param disk Where pages that are not in physical memory are kept.
    /// @param frame_lo_page First physical page that can hold a virtual page that is paged in.
    ///     These are the pages of RAM. A page outside of this range (like ROM) can only be used by
    ///     mapping a virtual page to it explicitly.
    /// @param frame_pages Number of such pages.
    /// @param physical Where the pages that are paged in are, see set_physical_pages. Without it
    ///     the pages have no contents, which is enough to look at the mappings.
    VirtualMemory(Disk *disk, word frame_lo_page = 0, word frame_pages = kMaxPhysicalPages,
                  PhysicalPages *physical = nullptr);
    ~VirtualMemory();
    VirtualMemory(const VirtualMemory &) = delete;
    VirtualMemory &operator=(const VirtualMemory &) = delete;

    Disk *m_disk;

    /// @return whether addresses are mapped by the page tables of the processes. Off, they are
    ///     physical.
    bool enabled() const
    {
        return m_enabled;
    }

    /// @param enabled whether addresses are mapped by the processes, or physical
    void set_enabled(bool enabled)
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
    class VirtualMemoryException : public std::exception
    {
      protected:
        std::string message;

      public:
        VirtualMemoryException(const std::string &msg);

        const char *what() const noexcept override;
    };

    /// An access that the page tables do not allow. The access did not happen.
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

    /// A process id that is not a process.
    class InvalidPIDException : public VirtualMemoryException
    {
      protected:
        long long invalid_pid;

      public:
        InvalidPIDException(const std::string &msg, long long invalid_pid);

        long long get_invalid_pid() const noexcept;
    };

    /// A virtual page that is mapped to a physical page was mapped to another one.
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

    /// A virtual page that is already part of the process, or is not.
    class InvalidVPageException : public VirtualMemoryException
    {
      protected:
        word vpage;

      public:
        InvalidVPageException(const std::string &msg, word vpage);

        word get_vpage() const noexcept;
    };

    /// The physical memory that holds the pages that are paged in. The virtual memory copies a page
    /// to it when the page is brought in from the disk, and from it when the page is swapped out.
    using PhysicalPages = ::PhysicalPages;

    /// @name Page tables in memory
    /// When the walk is enabled (`SCTLR.M`), virtual addresses are translated by walking a two
    /// level table in physical memory that the operating system owns, `process`es, the swapping
    /// and `begin_process` are not used. See docs/mmu.md.
    /// @{

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
    ///
    /// @param enabled true to walk the page tables, false for the swapping memory
    void set_walk_enabled(bool enabled);

    /// @return whether addresses are translated by the page tables
    bool walk_enabled() const
    {
        return m_walk;
    }

    /// Drops cached translations.
    ///
    /// @param base the physical address of the first level table (page aligned)
    void set_page_table_base(word base);

    /// @return the physical address of the first level table
    word page_table_base() const
    {
        return m_ptbr;
    }

    /// Whether accesses are made in user mode, which may only use pages marked user.
    ///
    /// @param user true for user mode
    void set_user_mode(bool user)
    {
        if (user != m_user)
        {
            m_user = user;
            drop_fetch_cache();
        }
    }

    /// `TLBI xn`: forgets the cached translation of the page that holds `address`.
    ///
    /// @param address a virtual address
    void invalidate_translation(word address);

    /// `TLBI`: forgets all the cached translations.
    void invalidate_translations();
    /// @}

    /// Sets where the pages that are paged in are. Without it the pages have no
    /// contents, which is enough to look at the mappings.
    ///
    /// @param physical the physical memory, which must outlive this
    void set_physical_pages(PhysicalPages *physical);

    /// Sets the current proccess to change the virtual space mappings.
    ///
    /// @throws InvalidPIDException when the pid is not a valid process.
    /// @param pid Process id.
    void set_process(long long pid);

    /// Starts a new process with it's own virtual memory address space.
    ///
    /// @throws VirtualMemoryException when MAX_PROCESSES limit is reached.
    /// @param kernel_privilege Whether the process has the kernel level access
    ///     privilege
    /// @return New process id.
    long long begin_process(bool kernel_privilege = false);

    /// Ends a specified process.
    ///
    /// @throws InvalidPIDException when the pid is not a valid process.
    /// @param pid Process id.
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

    /// Set the the access permissions of physical memory. Used by the kernel
    /// to set up memory mapped regions for I/O.
    ///
    /// @param ppage_begin First physical page to set the permissions to.
    /// @param ppage_end Last physical page to set the permissions to.
    /// @param swappable Whether the pages in this region should be swappable. A page
    ///     that is not swappable is never chosen to make room for another page.
    /// @param kernel_locked Whether the pages in this region require kernel level
    ///     privilege to access.
    void set_ppage_permissions(word ppage_begin, word ppage_end, bool swappable,
                               bool kernel_locked);

    /// Set the access permissions of virtual memory specific to a process.
    ///
    /// @throws InvalidPIDException if the pid is invalid.
    /// @param pid Process to set the access permissions.
    /// @param vpage_begin First virtual page to update permissions.
    /// @param vpage_end Last virtual page to update permissions.
    /// @param write Virtual page write permissions.
    /// @param execute Virtual page execute permissions.
    void set_vpage_permissions(long long pid, word vpage_begin, word vpage_end, bool write,
                               bool execute);

    /// Checks whether the virtual page has been added to the process.
    ///
    /// @throws InvalidPIDException when pid is invalid.
    /// @param pid Process identifier.
    /// @param vpage Virtual page to check.
    /// @return Whether the virtual page is mapped for the process.
    bool has_vpage(long long pid, word vpage);

    /// Adds new virtual pages to the specified process.
    ///
    /// @throws InvalidPIDException when pid is invalid.
    /// @throws InvalidVPageException when a virtual page has already been mapped to
    ///     the process.
    /// @param pid ID of the process to add virtual pages to.
    /// @param vpage First virtual page to add.
    /// @param length Number of pages to add.
    /// @param write Whether the pages can be written to.
    /// @param execute Whether code in the pages can be executed.
    void add_vpage(long long pid, word vpage, word length, bool write, bool execute);

    /// Checks whether code at the virtual address of the current process may be
    /// executed. Without an active process or with virtual memory disabled,
    /// everything can be executed. Unmapped pages cannot.
    ///
    /// @param address Virtual address to check.
    /// @return Whether the code can be executed.
    inline bool can_execute_address(word address)
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
    /// @throws PageFaultException if the page may not be executed.
    /// @param address Virtual address of the instruction.
    /// @return Physical address of the instruction.
    inline word translate_fetch(word address)
    {
        if (LIKELY((address >> kNumPageOffsetBits) == m_fetch_vpage))
        {
            return (m_fetch_ppage << kNumPageOffsetBits) | (address & (kPageSize - 1));
        }
        return translate_fetch_slow(address);
    }

    /// Throws the fault for an access that is not allowed.
    ///
    /// @param reason Why the access is not allowed.
    /// @param vpage The virtual page of the access.
    /// @param access What the page is accessed for.
    [[noreturn]] static void throw_fault(PageFaultException::Reason reason, word vpage,
                                         AccessType access);

    /// Converts a virtual address into a physical address of the process
    /// specified by the process id if virtual memory
    /// is enabled, otherwise the virtual address is equivalent to the physical
    /// address.
    ///
    /// @throws PageFaultException if the process may not make the access.
    /// @param pid ID of the process to translate virtual address.
    /// @param address Virtual address to translate.
    /// @param access What the address is accessed for.
    /// @return Physical address corresponding to the virtual address.
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

    /// Converts a virtual address into a physical address if virtual memory
    /// is enabled, otherwise the virtual address is equivalent to the physical
    /// address.
    ///
    /// @throws PageFaultException if the process may not make the access.
    /// @param address Virtual address to translate.
    /// @param access What the address is accessed for.
    /// @return Physical address corresponding to the virtual address.
    inline word translate_address(word address, AccessType access = AccessType::READ)
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

    /// Makes a force mapping of the virtual page to the physical page if it
    /// is not yet mapped.
    ///
    /// @throws InvalidPIDException if pid is invalid.
    /// @param pid Process identifier.
    /// @param vpage Virtual page to force map to physical page if not yet mapped.
    /// @param ppage Physical page to force map to.
    void ensure_physical_page_mapping(long long pid, word vpage, word ppage);

  private:
    /// Contains information about a physical page and what virtual pages
    /// map to it.
    struct PageTableEntry
    {
        /// Construct a new Page Table Entry object.
        ///
        /// @param pid Process ID.
        /// @param vpage Virtual page of this mapping.
        /// @param diskpage Disk page where the virtual page resides.
        /// @param write Whether virtual page can be written to.
        /// @param execute Whether code can be executed from the virtual page.
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
    static constexpr U64 tlb_key(long long pid, word vpage)
    {
        return (U64(pid) << 32) | vpage;
    }

    /// The key of an entry that has no translation, which no lookup has.
    static constexpr U64 kNoKey = ~U64(0);

    /// What is looked up with when there is no process whose pages are translated (virtual memory
    /// is off, the page tables are used, or there is no process): not a key that an entry has,
    /// or that is kNoKey, so that the lookup fails and the slow path decides.
    static constexpr U64 kNoProcessKey = U64(1) << 62;

    /// An entry of the TLB.
    struct TLB_Entry
    {
        U64 key = kNoKey; ///< The process and the virtual page of the translation, see tlb_key.
        word ppage = 0; ///< Resulting physical page address of the translation.
        bool write = false; ///< Whether the translation can be used to write.
    };

    /// Translation Lookaside Buffer. Contains the recently translated virtual
    /// page address to physical page address. An entry is only filled when the
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

    /// To be called when the current process, whether the virtual memory is on or the use of the
    /// page tables changes.
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

    /// Walks the tables: first level entry (10 bits of the address), second level entry (10 bits),
    /// page. Checks the entry, sets accessed and dirty, and fills the cache. Nothing is cached
    /// unless the access is allowed.
    ///
    /// @param address the virtual address
    /// @param access what the address is accessed for
    /// @return the physical address
    /// @throws PageFaultException if the access is not allowed
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

    /// Current active process in which all calls to the virtual memory to
    /// manipulate/use mappings without a supplied PID refers to.
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
    word translate_fetch_slow(word address);

    /// Beginning and end of the list that makes up the clock, the pages that can be
    /// swapped. The end is followed by the beginning.
    PhysicalPage *m_clock_head = nullptr;
    PhysicalPage *m_clock_tail = nullptr;

    /// The page that the hand of the clock points to, the next one to look at when
    /// a page has to be evicted. Null stands for the beginning of the list.
    PhysicalPage *m_clock_hand = nullptr;

    /// The information about a physical page, which is added if there is none.
    PhysicalPage &physical_page(word ppage)
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
    PhysicalPage &physical_page_slow(word ppage);

    /// Every physical page that there is information about, with its number (the consistency
    /// checks use it).
    std::vector<std::pair<word, const PhysicalPage *>> all_physical_pages() const;

    /// Whether a virtual page that is paged in can be put in the physical page.
    bool is_frame(word ppage) const;

    /// Gives a physical page back to the free pages, if it is one of the frames.
    void release_frame(word ppage);

    /// Drops the cached translation of the virtual page of a process, if any.
    void invalidate_tlb(long long pid, word vpage);

    /// Drops all cached translations.
    void flush_tlb();

    /// Ensures that the virtual memory page tables memory mappings are valid.
    void check_vm();

    /// Records that a physical page was just used, so that the clock gives it
    /// another turn. A translation that fills the TLB does it, a hit on the
    /// TLB does not (see m_tlb), which is why the clock costs a translation
    /// nothing.
    static inline void mark_referenced(PhysicalPage &page)
    {
        page.referenced = true;
    }

    /// Puts a physical page on the clock, just behind the hand (so that it is
    /// the last page the hand gets to), as used. A page that is on it already
    /// is moved.
    void clock_add(PhysicalPage &page);

    /// Takes a physical page off the clock, if it is on it.
    void clock_remove(PhysicalPage &page);

    /// The page to evict to make room. The hand goes round: a page that was used
    /// since the hand was last there is passed over and marked as not used, the
    /// first page that is not marked and is a frame and can be swapped is it.
    ///
    /// @throws VirtualMemoryException if there is none.
    PhysicalPage &clock_victim();

    /// Ensures the clock of the in use physical pages is valid.
    void check_clock();

    /// Removes the physical page and writes it back to disk, freeing up a
    /// location for another virtual page to map to.
    ///
    /// @param page Physical page to evict.
    void evict_ppage(PhysicalPage &page);

    /// Brings a virtual page that is on the disk into a physical page, which is
    /// free. The callers have both already (a lookup of either costs a hash).
    ///
    /// @param entry The virtual page, of a process that exists.
    /// @param page Physical page to map to.
    void map_vpage_to_ppage(PageTableEntry *entry, PhysicalPage &page);

    /// Maps a new virtual page to a physical page of the specified process.
    /// Note this forces the virtual page to always map to the physical page.
    ///
    /// @throws InvalidPIDException if pid is invalid.
    /// @throws InvalidVPageException if virtual page has already been added.
    /// @param pid Process id to map virtual page.
    /// @param vpage Virtual page to map.
    /// @param ppage Physical page to map to.
    void map_ppage(long long pid, word vpage, word ppage);

    /// Removes the virtual page from a process referenced by it's pid.
    ///
    /// @throws InvalidPIDException if pid is invalid.
    /// @throws InvalidVPageException if virtual page is not mapped to process.
    /// @param pid Process id.
    /// @param vpage Virtual page to remove.
    void remove_vpage(long long pid, word vpage);

    /// Translates a virtual space address to a physical space address. Note these
    /// are not page addresses, but full memory address in the 0 to 2^31 - 1
    /// range.
    ///
    /// @param ptable Page table of the process to access.
    /// @param address Virtual space address.
    /// @param access What the address is accessed for.
    /// @return Physical space address corresponding to the virtual space address of
    ///     this process.
    inline word translate_address(PageTable *ptable, word address, AccessType access)
    {
        word vpage = address >> kNumPageOffsetBits;
        word ppage = access_vpage(ptable, vpage, access);

        return (ppage << kNumPageOffsetBits) + (address & (kPageSize - 1));
    }

    /// Accesses a virtual page, performing the translation to the physical page
    /// according to the page table.
    ///
    /// The TLB (Translation Lookaside Buffer) helps avoid expensive calls to the
    /// entry mapping. Recently accessed virtual pages will have the translation
    /// stored in the buffer.
    ///
    /// @param ptable Page table of the process containing the virtual address mappings.
    /// @param vpage Virtual page to map.
    /// @param access What the page is accessed for.
    /// @return Physical page address.
    inline word access_vpage(PageTable *ptable, word vpage, AccessType access)
    {
        const TLB_Entry &tlb = m_tlb[vpage & (kMaxTLBSize - 1)];
        if (LIKELY(tlb.key == tlb_key(ptable->pid, vpage)
                   && (access != AccessType::WRITE || tlb.write)))
        {
            return tlb.ppage;
        }

        return access_vpage_slow(ptable, vpage, access);
    }

    /// Translates a virtual page that has no translation in the TLB, or that the
    /// translation does not allow to write: checks the permissions, brings the
    /// page in from disk if it is not in memory, and fills in the TLB.
    word access_vpage_slow(PageTable *ptable, word vpage, AccessType access);

    /// translate_address for everything that is not in the TLB of the current
    /// process: the page tables, addresses that are physical (virtual memory
    /// off, no process, the devices) and the first access to a page.
    word translate_address_slow(word address, AccessType access);
};
