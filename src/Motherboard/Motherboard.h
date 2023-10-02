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

    public:
        RAM ram;
        ROM rom;

        void Initialize();
        void WriteByte(Word address, Byte byte);
        Byte ReadByte(Word address);
        
};

#endif // MOTHERBOARD_H