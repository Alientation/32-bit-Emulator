#include "emulator32bit/kernel/fbl_inmemory.h"

#include "util/loggerv2.h"

FBL_InMemory::FBL_InMemory(byte *mem, word mem_start, word mem_size, word block_size)
    : mem(mem), mem_start(mem_start), mem_size(mem_size), block_size(block_size)
{
    EXPECT_TRUE((mem_size - mem_start) % block_size == 0, "Block size must divide memory space.");
    EXPECT_TRUE(block_size >= sizeof(struct FreeBlock), "Block size is too small");

    head = (struct FreeBlock*) (mem + mem_start);
    head->len = (mem_size - mem_start) / block_size;
    head->prev = nullptr;
    head->next = nullptr;
}

FBL_InMemory::Exception::Exception(const std::string& msg) :
	message(msg)
{

}

const char* FBL_InMemory::Exception::what() const noexcept
{
	return message.c_str();
}

word FBL_InMemory::get_free_block()
{
    if (head == nullptr)
    {
        throw Exception("Free list is empty.");
    }

    struct FreeBlock *free = head;
    if (head->len > 1)
    {
        head = ((struct FreeBlock*) ((byte *) head + block_size));
        head->len = free->len - 1;
        head->next = free->next;
        head->prev = nullptr;
    }
    else
    {
        head = head->next;
        if (head)
        {
            head->prev = nullptr;
        }
    }

    return ptr_to_mem_index(free);
}

void FBL_InMemory::return_block(word block)
{
    EXPECT_TRUE((block - mem_start) % block_size == 0, "Block size must divide memory space.");

    struct FreeBlock *ret_block = insert(block);
    coalesce(ret_block);
    coalesce(ret_block->prev);
}

bool FBL_InMemory::empty()
{
    return head == nullptr;
}

int FBL_InMemory::nfree()
{
    int count = 0;
    struct FreeBlock *cur = head;
    while (cur != nullptr)
    {
        count += cur->len;
        cur = cur->next;
    }
    return count;
}

int FBL_InMemory::nnodes()
{
    int count = 0;
    struct FreeBlock *cur = head;
    while (cur != nullptr)
    {
        count++;
        cur = cur->next;
    }
    return count;
}

void FBL_InMemory::verify()
{
    // todo, check the structure of the list
}

void FBL_InMemory::coalesce(struct FreeBlock *first)
{
    if (!first || !first->next || 
        (byte *) first->next != ((byte *) first) + first->len * block_size)
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

struct FBL_InMemory::FreeBlock* FBL_InMemory::insert(word block)
{   
    struct FreeBlock *next = (struct FreeBlock*) (mem + block);
    next->len = 1;
    if (!head || block < ptr_to_mem_index(head))
    {
        next->next = head;
        next->prev = nullptr;
        head = next;
        if (head->next)
        {
            head->next->prev = head;
        }
        return head;
    }

    FreeBlock *cur = head;
    while (cur->next && ptr_to_mem_index(cur->next) < block)
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