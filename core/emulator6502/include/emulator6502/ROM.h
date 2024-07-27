#ifndef ROM_H
#define ROM_H

#include "util/types.h"

// Read Only Memory
// Hardcoded program by computer manufacturer and cannot be changed or
// overwritten unless "flashing" the chip
class ROM;

class ROM {
    public:
        static constexpr u32 MEMORY_SIZE = 0x00010000; // ~65536 bytes

    private:
        Byte* data = new Byte[MEMORY_SIZE];

    public:
        ROM();
        ~ROM();

        void reset();
        Byte readByte(Word address);

        // overload subscript operator to read and write to rom
        Byte& operator[](Word address);
        const Byte& operator[](Word address) const;
};

#endif // ROM_H