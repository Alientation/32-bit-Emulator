#include "Motherboard.h"

#include <../src/Motherboard/Memory/RAM.h>
#include <iostream>
#include <iomanip>

void Motherboard::Initialize() {

    ram.Initialize();
    rom.Initialize();
}

void Motherboard::WriteByte(Word address, Byte byte) {
    if (address <= RAM::MEMORY_SIZE) {
        ram.WriteByte(address, byte);
    }

    if (address <= ROM::MEMORY_SIZE + RAM::MEMORY_SIZE) {
        throw "Cannot write to ROM";
    }

    // display the hexadecimal of the out of bounds memory address
    std::stringstream stream;
    stream << "Error: Out of bounds memory address 0x" << std::hex << address << std::endl;
    
    throw std::invalid_argument(stream.str());
}

Byte Motherboard::ReadByte(Word address) {
    if (address <= RAM::MEMORY_SIZE) {
        return ram.ReadByte(address);
    }

    if (address <= ROM::MEMORY_SIZE + RAM::MEMORY_SIZE) {
        return rom.ReadByte(address);
    }

    // display the hexadecimal of the out of bounds memory address
    std::stringstream stream;
    stream << "Error: Out of bounds memory address 0x" << std::hex << address << std::endl;
    
    throw std::invalid_argument(stream.str());
}