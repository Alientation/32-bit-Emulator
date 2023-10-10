#ifndef ROM_H
#define ROM_H

#include <Types.h>

// Read Only Memory
// Hardcoded program by computer manufacturer and cannot be changed or
// overwritten unless "flashing" the chip
class ROM;

class ROM {
    public:
        static constexpr u32 MEMORY_SIZE = 0x00010000; // ~65536 bytes

        Byte Data[MEMORY_SIZE];
    
    public:
        void initialize();
        Byte readByte(Word address);
};

#endif // ROM_H