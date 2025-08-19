#pragma once

#include "emulator32bit/emulator32bit.h"
#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/kernel/fbl_inmemory.h"

#include "util/logger.h"

#include <string>

#define N_VPAGES (1 << 20)

// todo, figure out how to implement shared pages. might have to use
// dynamically allocated memory on kernel heap to do so.

class MMU
{
  public:
    MMU (Emulator32bit *processor, word user_low_page, word user_high_page, word kernel_low_page,
         word kernel_high_page);

    enum AccessMode
    {
        READ_ACCESSMODE,
        WRITE_ACCESSMODE,
        EXECUTE_ACCESSMODE,
    };

    void create_pagedir ();
    void add_vpage (word vpage, bool kernel, bool write, bool execute, bool copy_on_write);

    void remove_vpage (word vpage);
    void remove_pagedir ();

    inline word map_address (word address, AccessMode mode)
    {
        /*
                Null page directory or processor in real more implies no virtual
                memory.
            */
        if (UNLIKELY (!m_processor->pagedir
                      || m_processor->get_flag (Emulator32bit::kRealModeFlagBit)))
        {
            return address;
        }

        /* Check for valid page directory. */
        RAM *ram = m_processor->system_bus->ram;
        if (UNLIKELY (!ram->in_bounds (m_processor->pagedir))
            || !ram->in_bounds (m_processor->pagedir + (kPageSize * sizeof (struct PageTableEntry))
                                - 1))
        {
            throw Emulator32bit::Exception (Emulator32bit::InterruptType::BAD_PAGEDIR,
                                            "Page directory is not in RAM.");
        }

        word vpage = address >> kNumPageOffsetBits;

        /*
                Kernel memory is directly mapped, not rerouting. Just check
                for permissions of process accessing.
            */
        if (UNLIKELY (vpage >= m_kernel_low_page && vpage <= m_kernel_high_page))
        {
            if (UNLIKELY (m_processor->get_flag (Emulator32bit::kUserModeFlagBit)))
            {
                throw Emulator32bit::Exception (Emulator32bit::InterruptType::PAGEFAULT,
                                                "User tried accessing kernel page.");
            }
            return address;
        }

        struct PageTableEntry *pagetable =
            (struct PageTableEntry *) &ram->m_data[m_processor->pagedir];
        struct PageTableEntry *entry = &pagetable[vpage];

        /* Check for access permissions. */
        if (UNLIKELY (!entry->valid))
        {
            throw Emulator32bit::Exception (Emulator32bit::InterruptType::PAGEFAULT,
                                            "Unmapped memory accessed.");
        }
        else if (UNLIKELY (entry->kernel
                           && m_processor->get_flag (Emulator32bit::kUserModeFlagBit)))
        {
            throw Emulator32bit::Exception (Emulator32bit::InterruptType::PAGEFAULT,
                                            "User tried accessing kernel page.");
        }
        else if (UNLIKELY (mode == WRITE_ACCESSMODE && !entry->write))
        {
            throw Emulator32bit::Exception (Emulator32bit::InterruptType::PAGEFAULT,
                                            "Page has no write permissions.");
        }
        else if (UNLIKELY (mode == EXECUTE_ACCESSMODE && !entry->execute))
        {
            throw Emulator32bit::Exception (Emulator32bit::InterruptType::PAGEFAULT,
                                            "Page has no execute permissions.");
        }

        entry->clock = 1;

        /* Read from disk and write to memory. */
        if (UNLIKELY (entry->disk))
        {
            Disk *disk = m_processor->system_bus->disk;
            std::vector<byte> disk_page = disk->read_page (entry->disk);
            disk->return_page (entry->disk);

            EXPECT_TRUE (disk_page.size () == kPageSize, "Page size does not match.");

            word free_ppage = get_free_ppage ();

            entry->disk = false;
            entry->ppage = free_ppage;

            word mapped_address = (free_ppage << kNumPageOffsetBits) + (address & (kPageSize - 1));
            memcpy (&ram->m_data[mapped_address], disk_page.data (), kPageSize);
            return mapped_address;
        }

        return (entry->ppage << kNumPageOffsetBits) + (address & (kPageSize - 1));
    }

  private:
    struct PageTableEntry
    {
        word ppage;           /* Physical page mapped to */
        word reference_count; /* How many virtual pages map to this */
        word diskpage;        /* Disk page stored in */
        bool valid;           /* Valid entry in table */
        bool disk;            /* Page is stored on disk */
        bool dirty;           /* Page has been written to */
        bool clock;           /* Clock based LRU replacement */

        bool kernel;          /* Kernel page */
        bool write;           /* Write access */
        bool execute;         /* Execute access */
        bool copy_on_write;   /* Copies and maps new page on write */
    };

    Emulator32bit *m_processor;
    word m_user_low_page;
    word m_user_high_page;
    word m_kernel_low_page;
    word m_kernel_high_page;

    FBL_InMemory m_free_user_ppages;
    FBL_InMemory m_free_kernel_ppages;
    word m_clock_hand = 0;

    inline word get_free_ppage ()
    {
        if (UNLIKELY (m_free_user_ppages.empty ()))
        {
            return evict_ppage ();
        }

        return m_free_user_ppages.get_free_block () >> kNumPageOffsetBits;
    }

    inline word evict_ppage ()
    {
        return 0;
    }
};
