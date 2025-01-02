#pragma once
#ifndef BETTER_VIRTUAL_MEMORY_H
#define BETTER_VIRTUAL_MEMORY_H

#include "emulator32bit/emulator32bit.h"
#include "emulator32bit/emulator32bit_util.h"
#include "emulator32bit/kernel/fbl_inmemory.h"

#include <string>

#define N_VPAGES (1<<20)

// todo, figure out how to implement shared pages. might have to use
// dynamically allocated memory on kernel heap to do so.

class MMU
{
    public:
        MMU(Emulator32bit *processor, word user_low_page, word user_high_page,
            word kernel_low_page, word kernel_high_page);

        enum AccessMode
        {
            READ_ACCESSMODE,
            WRITE_ACCESSMODE,
            EXECUTE_ACCESSMODE,
        };

        void create_pagedir();
        void add_vpage(word vpage, bool kernel, bool write,
                       bool execute, bool copy_on_write);

        void remove_vpage(word vpage);
        void remove_pagedir();

        inline word map_address(word address, AccessMode mode)
        {
            /*
                Null page directory or processor in real more implies no virtual
                memory.
            */
            if (UNLIKELY(!processor->_pagedir ||
                processor->get_flag(REAL_FLAG)))
            {
                return address;
            }

            /* Check for valid page directory. */
            if (UNLIKELY(!processor->ram->in_bounds(processor->_pagedir)) ||
                !processor->ram->in_bounds(processor->_pagedir +
                (PAGE_SIZE * sizeof(struct PageTableEntry)) - 1))
            {
                throw Emulator32bit::Exception(Emulator32bit::BAD_PAGEDIR,
                    "Page directory is not in RAM.");
            }

            word vpage = address >> PAGE_PSIZE;

            /*
                Kernel memory is directly mapped, not rerouting. Just check
                for permissions of process accessing.
            */
            if (UNLIKELY(vpage >= kernel_low_page && vpage <= kernel_high_page))
            {
                if (UNLIKELY(processor->get_flag(USER_FLAG)))
                {
                    throw Emulator32bit::Exception(Emulator32bit::PAGEFAULT,
                        "User tried accessing kernel page.");
                }
                return address;
            }

            struct PageTableEntry *pagetable = (struct PageTableEntry *)
                &processor->ram->data[processor->_pagedir];
            struct PageTableEntry *entry = &pagetable[vpage];

            /* Check for access permissions. */
            if (UNLIKELY(!entry->valid))
            {
                throw Emulator32bit::Exception(Emulator32bit::PAGEFAULT,
                    "Unmapped memory accessed.");
            }
            else if (UNLIKELY(entry->kernel && processor->get_flag(USER_FLAG)))
            {
                throw Emulator32bit::Exception(Emulator32bit::PAGEFAULT,
                    "User tried accessing kernel page.");
            }
            else if (UNLIKELY(mode == WRITE_ACCESSMODE && !entry->write))
            {
                throw Emulator32bit::Exception(Emulator32bit::PAGEFAULT,
                    "Page has no write permissions.");
            }
            else if (UNLIKELY(mode == EXECUTE_ACCESSMODE && !entry->execute))
            {
                throw Emulator32bit::Exception(Emulator32bit::PAGEFAULT,
                    "Page has no execute permissions.");
            }

            entry->clock = 1;

            /* Read from disk and write to memory. */
            if (UNLIKELY(entry->disk))
            {
                std::vector<byte> disk_page =
                        processor->disk->read_page(entry->disk);
                processor->disk->return_page(entry->disk);

                EXPECT_TRUE(disk_page.size() == PAGE_SIZE,
                    "Page size does not match.");

                word free_ppage = get_free_ppage();

                entry->disk = false;
                entry->ppage = free_ppage;

                word mapped_address = (free_ppage << PAGE_PSIZE) +
                    (address & (PAGE_SIZE - 1));
                memcpy(&processor->ram->data[mapped_address], disk_page.data(),
                    PAGE_SIZE);
                return mapped_address;
            }

            return (entry->ppage << PAGE_PSIZE) + (address & (PAGE_SIZE - 1));
        }

    private:
        struct PageTableEntry
        {
            word ppage;                 /* Physical page mapped to */
            word reference_count;       /* How many virtual pages map to this */
            word diskpage;              /* Disk page stored in */
            bool valid;                 /* Valid entry in table */
            bool disk;                  /* Page is stored on disk */
            bool dirty;                 /* Page has been written to */
            bool clock;                 /* Clock based LRU replacement */

            bool kernel;                /* Kernel page */
            bool write;                 /* Write access */
            bool execute;               /* Execute access */
            bool copy_on_write;         /* Copies and maps new page on write */
        };

        Emulator32bit *processor;
        word user_low_page;
        word user_high_page;
        word kernel_low_page;
        word kernel_high_page;

        FBL_InMemory free_user_ppages;
        FBL_InMemory free_kernel_ppages;
        word clock_hand = 0;

        inline word get_free_ppage()
        {
            if (UNLIKELY(free_user_ppages.empty()))
            {
                return evict_ppage();
            }

            return free_user_ppages.get_free_block() >> PAGE_PSIZE;
        }

        inline word evict_ppage()
        {
            return 0;
        }
};




#endif /* BETTER_VIRTUAL_MEMORY */