#include "Motherboard.h"

#include <../src/Motherboard/Memory/RAM.h>
#include <iostream>
#include <iomanip>
#include <assert.h>


void Motherboard::initialize() {

    // initialize the memory
    ram.initialize();
    rom.initialize();

    // todo SSD and HDD
}

// Writes a byte to the appropriate mapped memory address
void Motherboard::writeByte(Word address, Byte byte) {
    Word searchAddress = address;
    if (searchAddress <= RAM::MEMORY_SIZE) {
        ram.writeByte(searchAddress, byte);
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
Byte Motherboard::readByte(Word address) {
    Word searchAddress = address;
    if (searchAddress <= RAM::MEMORY_SIZE) {
        return ram.readByte(searchAddress);
    }
    searchAddress -= RAM::MEMORY_SIZE;

    if (searchAddress <= ROM::MEMORY_SIZE) {
        return rom.readByte(searchAddress);
    }
    searchAddress -= ROM::MEMORY_SIZE;

    // throw out of bounds memory address error
    std::stringstream stream;
    stream << "Error: Out of bounds memory address " << stringifyHex(address) << std::endl;
    
    throw std::invalid_argument(stream.str());
}

Byte& RAM::operator[](Word address) {
    Word searchAddress = address;
    if (searchAddress <= RAM::MEMORY_SIZE) {
        return data[searchAddress];
    }
    searchAddress -= RAM::MEMORY_SIZE;

    if (searchAddress <= ROM::MEMORY_SIZE) {
        throw "Cannot write to ROM";
    }
    searchAddress -= ROM::MEMORY_SIZE;

    // throw out of bounds memory address error
    std::stringstream stream;
    stream << "Error: Out of bounds memory address " << stringifyHex(address) << std::endl;
}