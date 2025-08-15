
#include "emulator32bit/fbl.h"

FreeBlockList::FreeBlockList (word begin, word len, bool init) :
    m_begin (begin),
    m_len (len)
{
    if (init)
    {
        /* Only initializes all blocks to be free if specified. */
        m_head = new FreeBlock
        {
            .addr = begin,
            .len = len,
        };
    }
    // DEBUG("Initializing Free Block List");
}

FreeBlockList::~FreeBlockList ()
{
    FreeBlock *cur = m_head;
    while (cur != nullptr)
    {
        FreeBlock *next = cur->next;
        delete (cur);
        cur = next;
    }
    // DEBUG("Destroying Free Block List");
}

FreeBlockList::FreeBlockListException::FreeBlockListException (const std::string& msg) :
    message (msg)
{

}

const char *FreeBlockList::FreeBlockListException::what () const noexcept
{
    return message.c_str ();
}

word FreeBlockList::get_free_block (word length)
{
    FreeBlock *freeblock = find (length);

    if (!freeblock)
    {
        throw FreeBlockListException ("Not enough space to allocate free block " +
                std::to_string (length));
        return 0;
    }

    /*
     * Split the block, front part is the returned free block.
     */
    word addr = freeblock->addr;
    freeblock->len -= length;
    freeblock->addr += length;

    /* Remove the block if empty. */
    if (freeblock->len == 0)
    {
        remove (freeblock);
    }

    return addr;
}

void FreeBlockList::remove_block (word addr, word length)
{
    FreeBlock *cur = m_head;
    while (cur->addr + cur->len <= addr)
    {
        cur = cur->next;
    }

    if (cur->addr > addr || cur->addr + cur->len < addr + length)
    {
        throw FreeBlockListException ("Invalid returned block " +
                std::to_string (addr) + " - " + std::to_string (length) + ".");
        return;
    }

    word remaining_before = addr - cur->addr;
    word remaining_after = cur->addr + cur->len - (addr + length);

    remove (cur);

    if (remaining_before > 0)
    {
        return_block (addr, remaining_before);
    }

    if (remaining_after > 0)
    {
        return_block (addr + length, remaining_after);
    }
}

FreeBlockList::FreeBlock *FreeBlockList::insert (word addr, word length)
{
    if (!m_head || addr < m_head->addr)
    {
        m_head = new FreeBlock
        {
            .addr = addr,
            .len = length,
            .next = m_head,
        };

        if (m_head->next)
        {
            m_head->next->prev = m_head;
        }

        return m_head;
    }

    FreeBlock *cur = m_head;
    while (cur->next != nullptr && cur->next->addr < addr)
    {
        cur = cur->next;
    }

    FreeBlock *next = new FreeBlock
    {
        .addr = addr,
        .len = 1,
        .next = cur->next,
        .prev = cur,
    };

    if (cur->next)
    {
        cur->next->prev = next;
    }
    cur->next = next;
    return next;
}

void FreeBlockList::return_block (word addr, word length)
{
    if (addr < m_begin || addr + length > m_begin + m_len)
    {
        throw FreeBlockListException ("Invalid returned block " +
                std::to_string (addr) + " - " + std::to_string (length) + ".");
        return;
    }

    FreeBlock *ret_block = insert (addr, length);

    bool intersect_prev = ret_block->prev && ret_block->prev->addr + ret_block->prev->len > addr;
    bool intersect_next = ret_block->next && ret_block->next->addr < addr + length;
    if (intersect_prev || intersect_next)
    {
        throw FreeBlockListException ("Invalid returned block " +
                std::to_string (addr) + " - " + std::to_string (length) + ".");

        /* Undo state change so that the caller can cleanly handle the exception. */
        remove (ret_block);
        return;
    }

    coalesce (ret_block);
    coalesce (ret_block->prev);
}

// NO CONFIDENCE THAT THIS WORKS.. HAS TO BE TESTED
void FreeBlockList::force_return_block (word addr, word length)
{
    if (addr < m_begin || addr + length > m_begin + m_len)
    {
        throw FreeBlockListException ("Invalid returned block " + std::to_string (addr) +
                " - " + std::to_string (length) + ".");
        return;
    }

    FreeBlock *ret_block = insert (addr, length);

    while (ret_block->prev && ret_block->prev->addr + ret_block->prev->len > ret_block->addr)
    {
        if (ret_block->prev->addr + ret_block->prev->len > ret_block->addr + ret_block->len)
        {
            ret_block->len += ret_block->prev->addr + ret_block->prev->len -
                    (ret_block->addr + ret_block->len);
        }

        ret_block->len += ret_block->addr - ret_block->prev->addr;
        ret_block->addr = ret_block->prev->addr;

        remove (ret_block->prev);
    }

    while (ret_block->next && ret_block->next->addr < ret_block->addr + ret_block->len)
    {
        ret_block->len += ret_block->next->addr + ret_block->next->len -
                (ret_block->addr + ret_block->len);

        remove (ret_block->next);
    }
}


void FreeBlockList::return_all ()
{
    FreeBlock *cur = m_head;
    while (cur)
    {
        FreeBlock *next = cur;
        cur = cur->next;
        delete next;
    }

    m_head = new FreeBlock
    {
        .addr = m_begin,
        .len = m_len,
    };
}

std::vector<std::pair<word,word>> FreeBlockList::get_blocks ()
{
    std::vector<std::pair<word,word>> blocks;
    for (FreeBlock *cur = m_head; cur; cur = cur->next)
    {
        blocks.push_back (std::pair<word,word> (cur->addr, cur->len));
    }
    return blocks;
}

void FreeBlockList::print_blocks()
{
    std::vector<std::pair<word,word>> blocks = get_blocks ();
    printf ("Printing FBL List\n");
    for (auto pair : blocks)
    {
        printf ("Block {addr=%x, len=%x}\n", pair.first, pair.second);
    }
}

bool FreeBlockList::can_fit (word length)
{
    return find (length) != nullptr;
}

bool FreeBlockList::empty ()
{
    return size () == 0;
}

word FreeBlockList::size ()
{
    word size = 0;
    for (FreeBlock *cur = m_head; cur; cur = cur->next)
    {
        size += cur->len;
    }
    return size;
}

FreeBlockList::FreeBlock *FreeBlockList::find (word length)
{
    for (FreeBlock *cur = m_head; cur; cur = cur->next)
    {
        if (cur->len >= length)
        {
            return cur;
        }
    }
    return nullptr;
}

void FreeBlockList::remove (FreeBlock *node)
{
    if (node->prev)
    {
        node->prev->next = node->next;
    }
    else
    {
        m_head = node->next;
    }

    if (node->next)
    {
        node->next->prev = node->prev;
    }

    delete node;
}

void FreeBlockList::coalesce (FreeBlock *first)
{
    if (!first || !first->next)
    {
        return;
    }

    if (first->next->addr != first->addr + first->len)
    {
        return;
    }

    first->len += first->next->len;
    FreeBlock *cur = first->next;
    first->next = first->next->next;

    if (first->next)
    {
        first->next->prev = first;
    }
    delete cur;
}
