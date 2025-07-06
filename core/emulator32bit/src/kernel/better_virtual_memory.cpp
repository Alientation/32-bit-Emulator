#include "emulator32bit/kernel/better_virtual_memory.h"

MMU::MMU(Emulator32bit *processor, word user_low_page, word user_high_page,
         word kernel_low_page, word kernel_high_page)
    : processor(processor), user_low_page(user_low_page), user_high_page(user_high_page),
    kernel_low_page(kernel_low_page), kernel_high_page(kernel_high_page),
    free_user_ppages(processor->ram->data, user_low_page,
        user_high_page - user_low_page + 1, PAGE_SIZE),
    free_kernel_ppages(processor->ram->data, kernel_low_page,
        kernel_high_page - kernel_low_page + 1, PAGE_SIZE)
{
    EXPECT_TRUE(processor->ram->in_bounds(user_low_page), "User page not in ram.");
    EXPECT_TRUE(processor->ram->in_bounds(user_high_page), "User page not in ram.");
    EXPECT_TRUE(processor->ram->in_bounds(kernel_low_page), "Kernel page not in ram.");
    EXPECT_TRUE(processor->ram->in_bounds(kernel_high_page), "Kernel page not in ram.");
}

void MMU::create_pagedir()
{

}

void MMU::add_vpage(word vpage, bool kernel, bool write,
               bool execute, bool copy_on_write)
{
    (void) (vpage);
    (void) (kernel);
    (void) (write);
    (void) (execute);
    (void) (copy_on_write);
}

void MMU::remove_vpage(word vpage)
{
    (void) (vpage);
}

// todo fix for shared pages
void MMU::remove_pagedir()
{
    struct PageTableEntry *pagetable = (struct PageTableEntry *)
                &processor->ram->data[processor->_pagedir];

    word end_page = N_VPAGES;
    for (word page = 0; page <= end_page; page++)
    {
        struct PageTableEntry *entry = &pagetable[page];
        entry->reference_count--;
        if (!entry->valid || entry->reference_count > 0)
        {
            continue;
        }

        if (entry->disk)
        {
            processor->disk->return_page(entry->disk);
        }
        else
        {
            free_user_ppages.return_block(entry->ppage);
        }
    }

    processor->_pagedir = 0;
}