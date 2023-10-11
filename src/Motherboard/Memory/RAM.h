#ifndef RAM_H
#define RAM_H

#include <Types.h>

// Random Access Memory
class RAM;

// Max Memory is 0x00100000 (because allocating on the stack)
//
// 0x00000000 - 0x000000FF : Reserved for boot process
// 0x00000100 - 0x000100FF : Stack memory
// 0x00010100 - 0x000FFFFF : General purpose memory 
//
// SOLVED figure out whether we should store addresses to a word or byte
// ie whether we should group memory into cells of 4 bytes and have addresses
// point to the specific cell in memory
//      - Allow byte addressing, word addressing is far too limiting and wastes memory
//
// SOLVED figure out if we want to segment memory or have paging
//      - for 6502, we will have paging, but for x86, we will have segmentation
//
// TODO also figure out if there are any significant performance benefits regarding
// reading/writing to memory allocated on the stack vs the heap, because since we are
// allocating on the stack, we don't have much memory to use, but given the use case of
// this application, 1 MB of memory should be enough, it just might cause some
// out of bounds memory errors
class RAM {
    public:
        static constexpr u32 MEMORY_SIZE = 0x00100000; // ~1 MB

    private:
        Byte data[MEMORY_SIZE];
    
    public:
        void initialize();
        void writeByte(Word address, Byte byte);
        Byte readByte(Word address);

        // overload subscript operator to read and write to ram
        Byte& operator[](Word address);
        const Byte& operator[](Word address) const;
};


#endif // RAM_H