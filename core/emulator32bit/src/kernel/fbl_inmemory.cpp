#include "emulator32bit/kernel/fbl_inmemory.h"

#include "util/logger.h"

FBL_InMemory::FBL_InMemory (byte *mem, word mem_start, word mem_size, word block_size) :
    m_mem (mem),
    m_mem_start (mem_start),
    m_mem_size (mem_size),
    m_block_size (block_size)
{
    EXPECT_TRUE ((mem_size - mem_start) % block_size == 0, "Block size must divide memory space.");
    EXPECT_TRUE (block_size >= sizeof (struct FreeBlock), "Block size is too small");

    m_head = (struct FreeBlock *) (mem + mem_start);
    m_head->len = (mem_size - mem_start) / block_size;
    m_head->prev = nullptr;
    m_head->next = nullptr;
}

FBL_InMemory::Exception::Exception (const std::string &msg) :
    message (msg)
{
}

const char *FBL_InMemory::Exception::what () const noexcept
{
    return message.c_str ();
}

word FBL_InMemory::get_free_block ()
{
    if (m_head == nullptr)
    {
        throw Exception ("Free list is empty.");
    }

    struct FreeBlock *free = m_head;
    if (m_head->len > 1)
    {
        m_head = ((struct FreeBlock *) ((byte *) m_head + m_block_size));
        m_head->len = free->len - 1;
        m_head->next = free->next;
        m_head->prev = nullptr;
    }
    else
    {
        m_head = m_head->next;
        if (m_head)
        {
            m_head->prev = nullptr;
        }
    }

    return ptr_to_mem_index (free);
}

void FBL_InMemory::return_block (word block)
{
    EXPECT_TRUE ((block - m_mem_start) % m_block_size == 0, "Block size must divide memory space.");

    struct FreeBlock *ret_block = insert (block);
    coalesce (ret_block);
    coalesce (ret_block->prev);
}

bool FBL_InMemory::empty ()
{
    return m_head == nullptr;
}

int FBL_InMemory::nfree ()
{
    int count = 0;
    struct FreeBlock *cur = m_head;
    while (cur != nullptr)
    {
        count += cur->len;
        cur = cur->next;
    }
    return count;
}

int FBL_InMemory::nnodes ()
{
    int count = 0;
    struct FreeBlock *cur = m_head;
    while (cur != nullptr)
    {
        count++;
        cur = cur->next;
    }
    return count;
}

void FBL_InMemory::verify ()
{
    // todo, check the structure of the list
}

void FBL_InMemory::coalesce (struct FreeBlock *first)
{
    if (!first || !first->next
        || (byte *) first->next != ((byte *) first) + first->len * m_block_size)
    {
        return;
    }

    first->len += first->next->len;
    first->next = first->next->next;

    if (first->next)
    {
        first->next->prev = first;
    }
}

struct FBL_InMemory::FreeBlock *FBL_InMemory::insert (word block)
{
    struct FreeBlock *next = (struct FreeBlock *) (m_mem + block);
    next->len = 1;
    if (!m_head || block < ptr_to_mem_index (m_head))
    {
        next->next = m_head;
        next->prev = nullptr;
        m_head = next;
        if (m_head->next)
        {
            m_head->next->prev = m_head;
        }
        return m_head;
    }

    FreeBlock *cur = m_head;
    while (cur->next && ptr_to_mem_index (cur->next) < block)
    {
        cur = cur->next;
    }

    next->prev = cur;
    next->next = cur->next;

    if (cur->next)
    {
        cur->next->prev = next;
    }
    cur->next = next;
    return next;
}