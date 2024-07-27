#include "util/types.h"
#include "emulator6502/RAM.h"
#include "emulator6502/ROM.h"

#ifndef MOTHERBOARD_H
#define MOTHERBOARD_H


class Motherboard;

class Motherboard {
    private:
        RAM ram;
        ROM rom;

    public:
        void reset();
        void writeByte(Word address, Byte byte);
        Byte readByte(Word address);

        // overload subscript operator to write to ram
        Byte& operator[](Word address);

        // overload subscript operator to read from ram
        const Byte& operator[](Word address) const;

};

#endif // MOTHERBOARD_H