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
    Word searchAddress = address;
    if (searchAddress <= RAM::MEMORY_SIZE) {
        ram.WriteByte(searchAddress, byte);
        return;
    }
    searchAddress -= RAM::MEMORY_SIZE;

    if (searchAddress <= ROM::MEMORY_SIZE) {
        throw "Cannot write to ROM";
    }
    searchAddress -= ROM::MEMORY_SIZE;

    // throw out of bounds memory address error
    std::stringstream stream;
    stream << "Error: Out of bounds memory address " << stringifyHex(address) << std::endl;
    
    throw std::invalid_argument(stream.str());
}

// Reads a byte from the appropriate mapped memory address
Byte Motherboard::ReadByte(Word address) {
    Word searchAddress = address;
    if (searchAddress <= RAM::MEMORY_SIZE) {
        return ram.ReadByte(searchAddress);
    }
    searchAddress -= RAM::MEMORY_SIZE;

    if (searchAddress <= ROM::MEMORY_SIZE) {
        return rom.ReadByte(searchAddress);
    }
    searchAddress -= ROM::MEMORY_SIZE;

    // throw out of bounds memory address error
    std::stringstream stream;
    stream << "Error: Out of bounds memory address " << stringifyHex(address) << std::endl;
    
    throw std::invalid_argument(stream.str());
}