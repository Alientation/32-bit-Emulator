#pragma once
#ifndef MEMORY_H
#define MEMORY_H

#include "emulator32bit/emulator32bit_util.h"
#include "util/file.h"

#include <string>

class BaseMemory
{
    public:
        BaseMemory(word npages, word start_page);
        virtual ~BaseMemory();

        virtual inline byte read_byte(word address) = 0;
        virtual inline hword read_hword(word address) = 0;
        virtual inline word read_word(word address) = 0;
        virtual inline void write_byte(word address, byte value) = 0;
        virtual inline void write_hword(word address, hword value) = 0;
        virtual inline void write_word(word address, word value) = 0;

        inline word get_mem_pages()
        {
            return npages;
        }

        inline word get_lo_page()
        {
            return start_page;
        }

        inline word get_hi_page()
        {
            return start_page + npages - 1;
        }

        inline bool in_bounds(word address)
        {
            return address >= (start_page << kNumPageOffsetBits) && address < ((get_hi_page()+1) << kNumPageOffsetBits);
        }

    protected:
        word npages;
        word start_page;
        word start_addr;
};

class Memory : public BaseMemory
{
    public:
        Memory(word npages, word start_page);
        Memory(Memory& other);
        virtual ~Memory();

        // FOR SEG FAULTS, WE CAN USE SIGNAL HANDLERS TO CATCH AND HANDLE THEM IN THE KERNEL, WITHOUT
        // HAVING AN EXPENSIVE CONDITIONAL CHECK EVERYTIME MEMORY IS ACCESSED
        inline byte read_byte(word address) override
        {
            return data[address - (start_page << kNumPageOffsetBits)];
        }

        inline hword read_hword(word address)
        {
            address -= start_addr;
            return ((hword*)(data + (address & 1)))[address >> 1];
        }

        inline word read_word(word address)
        {
            address -= start_addr;
            return ((word*)(data + (address & 0b11)))[address >> 2];
        }

        virtual inline word read_word_aligned(word address)
        {
            return ((word*) data)[(address - start_addr) >> 2];
        }

        inline void write_byte(word address, byte value)
        {
            data[address - (start_page << kNumPageOffsetBits)] = value;
        }

        inline void write_hword(word address, hword value)
        {
            address -= start_addr;
            ((hword*)(data + (address & 1)))[address >> 1] = value;
        }

        inline void write_word(word address, word value)
        {
            address -= start_addr;
            ((word*)(data + (address & 0b11)))[address >> 2] = value;
        }


        void reset();

        byte* data;
};

class RAM : public Memory
{
    public:
        RAM(word npages, word start_pages);
};

class ROM : public Memory
{
    public:
        ROM(const byte* data, word npages, word start_page);
        ROM(File file, word npages, word start_page);
        ~ROM() override;

        class ROM_Exception : public std::exception
        {
            private:
                std::string message;

            public:
                ROM_Exception(std::string msg);
                const char* what() const noexcept override;
        };

        /* todo, prevent writes, have special way to flash memory */


    private:
        bool save_file = false;
        File file;
};

#endif /* MEMORY_H */