#include "Motherboard.h"

#include <../src/Motherboard/Memory/RAM.h>
#include <iostream>
#include <iomanip>


void Motherboard::Initialize() {

    // initialize the memory
    ram.Initialize();
    rom.Initialize();

    // todo SSD and HDD
}

// Writes a byte to the appropriate mapped memory address
void Motherboard::WriteByte(Word address, Byte byte) {
    if (address <= RAM::MEMORY_SIZE) {
        ram.WriteByte(address, byte);
    }

    if (address <= ROM::MEMORY_SIZE + RAM::MEMORY_SIZE) {
        throw "Cannot write to ROM";
    }

    // throw out of bounds memory address error
    std::stringstream stream;
    stream << "Error: Out of bounds memory address 0x" << std::hex << address << std::endl;
    
    throw std::invalid_argument(stream.str());
}

// Reads a byte from the appropriate mapped memory address
Byte Motherboard::ReadByte(Word address) {
    if (address <= RAM::MEMORY_SIZE) {
        return ram.ReadByte(address);
    }

    if (address <= ROM::MEMORY_SIZE + RAM::MEMORY_SIZE) {
        return rom.ReadByte(address);
    }

    // throw out of bounds memory address error
    std::stringstream stream;
    stream << "Error: Out of bounds memory address 0x" << std::hex << address << std::endl;
    
    throw std::invalid_argument(stream.str());
}