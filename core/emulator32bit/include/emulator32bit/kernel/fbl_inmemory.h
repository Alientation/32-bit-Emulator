#pragma once
#ifndef FBL_INMEMORY_H
#define FBL_INMEMORY_H

#include "emulator32bit/emulator32bit_util.h"

#include <string>

/**
 * @brief               Stores a free list in memory instead of dynamically allocating it.
 *
 * TODO: since we only allocate blocks in fixed sizes, this means we do not need
 * to coalesce, and thus we do not need to store nodes in order. This means
 * removing and returning blocks should be O(1) instead of returning blocks
 * being O(n)
 *
 */
class FBL_InMemory
{
    public:
        FBL_InMemory(byte *mem, word mem_start, word mem_size, word block_size);

        class Exception : public std::exception
        {
            protected:
                std::string message;

            public:
                Exception(const std::string& msg);

                const char* what() const noexcept override;
        };

        word get_free_block();
        void return_block(word block_addr);

        bool empty();
        int nfree();
        int nnodes();
        void verify();

    private:
        struct FreeBlock
        {
            word len;
            FreeBlock *next = nullptr;
            FreeBlock *prev = nullptr;
        };

        byte *mem;
        word mem_start;
        word mem_size;
        word block_size;

        struct FreeBlock *head = nullptr;

        struct FreeBlock* insert(word block);
        void coalesce(FreeBlock *first);

        inline word ptr_to_mem_index(void *ptr)
        {
            return (word) ((uintptr_t) ptr - (uintptr_t) mem);
        }
};




#endif  /* FBL_INMEMORY_H */