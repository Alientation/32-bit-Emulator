#ifndef MOTHERBOARD_H
#define MOTHERBOARD_H

#include <Types.h>
#include <../src/Motherboard/Memory/RAM.h>
#include <../src/Motherboard/Memory/ROM.h>
#include <../src/Motherboard/BIOS.h>
#include <../src/Storage/HDD.h>
#include <../src/Storage/SSD.h>

class Motherboard;

class Motherboard {
    private:
        RAM ram;
        ROM rom;

    public:
        void initialize();
        void writeByte(Word address, Byte byte);
        Byte readByte(Word address);

        // overload subscript operator to read and write to ram
        Byte& operator[](Word address);
        
};

#endif // MOTHERBOARD_H