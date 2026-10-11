
#include "emulator32bit/fbl.h"

#include <algorithm>
#include <cstdio>

FreeBlockList::FreeBlockList(const word begin, const word len, const bool init) :
    m_begin(begin),
    m_len(len)
{
    if (init)
    {
        // Only initializes all blocks to be free if specified.
        m_head = new FreeBlock{
            .addr = begin,
            .len = len,
        };
    }
}

FreeBlockList::~FreeBlockList()
{
    const FreeBlock *cur = m_head;
    while (cur != nullptr)
    {
        const FreeBlock *next = cur->next;
        delete (cur);
        cur = next;
    }
}

FreeBlockList::Error::Error(const std::string &msg) :
    message(msg)
{
}

const char *FreeBlockList::Error::what() const noexcept
{
    return message.c_str();
}

word FreeBlockList::get_free_block(const word length)
{
    FreeBlock *freeblock = find(length);

    if (!freeblock)
    {
        throw Error("Not enough space to allocate free block " + std::to_string(length));
        return 0;
    }

    // Split the block: the front part is the free block that is returned.
    const word addr = freeblock->addr;
    freeblock->len -= length;
    freeblock->addr += length;

    // Remove the block if empty.
    if (freeblock->len == 0)
    {
        remove(freeblock);
    }

    return addr;
}

void FreeBlockList::remove_block(const word addr, const word length)
{
    FreeBlock *cur = m_head;
    while (cur != nullptr && cur->addr + cur->len <= addr)
    {
        cur = cur->next;
    }

    if (cur == nullptr || cur->addr > addr || cur->addr + cur->len < addr + length)
    {
        throw Error("Invalid returned block " + std::to_string(addr) + " - "
                    + std::to_string(length) + ".");
        return;
    }

    const word remaining_before = addr - cur->addr;
    const word remaining_after = cur->addr + cur->len - (addr + length);
    const word block_addr = cur->addr;

    remove(cur);

    if (remaining_before > 0)
    {
        return_block(block_addr, remaining_before);
    }

    if (remaining_after > 0)
    {
        return_block(addr + length, remaining_after);
    }
}

FreeBlockList::FreeBlock *FreeBlockList::insert(const word addr, const word length)
{
    if (!m_head || addr < m_head->addr)
    {
        m_head = new FreeBlock{
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

    FreeBlock *next = new FreeBlock{
        .addr = addr,
        .len = length,
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

bool FreeBlockList::in_range(const word addr, const word length) const
{
    return addr >= m_begin && U64(addr) + length <= U64(m_begin) + m_len;
}

void FreeBlockList::return_block(const word addr, const word length)
{
    if (!in_range(addr, length))
    {
        throw Error("Invalid returned block " + std::to_string(addr) + " - "
                    + std::to_string(length) + ".");
        return;
    }

    FreeBlock *ret_block = insert(addr, length);

    const bool intersect_prev =
        ret_block->prev && ret_block->prev->addr + ret_block->prev->len > addr;
    const bool intersect_next = ret_block->next && ret_block->next->addr < addr + length;
    if (intersect_prev || intersect_next)
    {
        // Undo state change so that the caller can cleanly handle the exception.
        remove(ret_block);
        throw Error("Invalid returned block " + std::to_string(addr) + " - "
                    + std::to_string(length) + ".");
    }

    coalesce(ret_block);
    coalesce(ret_block->prev);
}

void FreeBlockList::force_return_block(const word addr, const word length)
{
    if (!in_range(addr, length))
    {
        throw Error("Invalid returned block " + std::to_string(addr) + " - "
                    + std::to_string(length) + ".");
    }
    if (length == 0)
    {
        return;
    }

    // The free blocks that overlap or touch the returned block become part of it. Ends are
    // exclusive, and fit in 64 bits where a block ending at the top of the range does not.
    U64 lo = addr;
    U64 hi = U64(addr) + length;
    for (FreeBlock *cur = m_head; cur != nullptr;)
    {
        FreeBlock *next = cur->next;
        const U64 cur_hi = U64(cur->addr) + cur->len;
        if (cur->addr <= hi && cur_hi >= lo)
        {
            if (cur->addr < lo) lo = cur->addr;
            if (cur_hi > hi) hi = cur_hi;
            remove(cur);
        }
        cur = next;
    }

    insert(word(lo), word(hi - lo));
}

void FreeBlockList::return_all()
{
    const FreeBlock *cur = m_head;
    while (cur)
    {
        const FreeBlock *next = cur;
        cur = cur->next;
        delete next;
    }

    // A range without anything in it has no free block.
    m_head = m_len == 0 ? nullptr
                        : new FreeBlock{
                              .addr = m_begin,
                              .len = m_len,
                          };
}

std::vector<std::pair<word, word>> FreeBlockList::get_blocks() const
{
    std::vector<std::pair<word, word>> blocks;
    for (const FreeBlock *cur = m_head; cur; cur = cur->next)
    {
        blocks.push_back(std::pair<word, word>(cur->addr, cur->len));
    }
    return blocks;
}

bool FreeBlockList::can_fit(const word length) const
{
    return find(length) != nullptr;
}

bool FreeBlockList::empty() const
{
    return size() == 0;
}

word FreeBlockList::size() const
{
    word size = 0;
    for (const FreeBlock *cur = m_head; cur; cur = cur->next)
    {
        size += cur->len;
    }
    return size;
}

FreeBlockList::FreeBlock *FreeBlockList::find(const word length) const
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

void FreeBlockList::remove(FreeBlock *node)
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

void FreeBlockList::coalesce(FreeBlock *first)
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
    const FreeBlock *cur = first->next;
    first->next = first->next->next;

    if (first->next)
    {
        first->next->prev = first;
    }
    delete cur;
}
