#pragma once

#include "emulator32bit/emulator32bit_util.h"

/**
 * @brief             The physical address space as the virtual memory sees it: whole pages that it
 *                    copies in and out when it swaps, and the words of the page tables that it
 *                    reads and writes when it walks them. The SystemBus is what implements it.
 */
class PhysicalPages
{
  public:
    virtual ~PhysicalPages() = default;

    /// Copies the physical page to `out`, which has room for kPageSize bytes.
    virtual void read_page(word ppage, byte *out) = 0;

    /// Copies kPageSize bytes to the physical page.
    virtual void write_page(word ppage, const byte *data) = 0;

    /// The kPageSize bytes of the physical page themselves, if they are plain memory that can be
    /// copied to and from directly (the RAM), else null. Swapping uses it to copy between the page
    /// and the disk without a buffer in between; when it is null it goes through read_page and
    /// write_page.
    virtual byte *direct_page(word ppage)
    {
        (void)ppage;
        return nullptr;
    }

    /// Reads the word at a physical address (the page table walker uses this). Returns
    /// false if no memory is there.
    virtual bool read_physical_word(word address, word &out) = 0;

    /// Writes a word to a physical address. Returns false if no memory is there.
    virtual bool write_physical_word(word address, word value) = 0;
};
