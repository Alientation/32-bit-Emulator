#include "emulator6502/Motherboard.h"
#include "emulator6502/RAM.h"

#include <iostream>
#include <iomanip>
#include <assert.h>
#include <sstream>


void Motherboard::reset() {

    // reset the memory
    ram.reset();
    rom.reset();

    // todo SSD and HDD
}

// Writes a byte to the appropriate mapped memory address
void Motherboard::writeByte(Word address, Byte byte) {
    (*this)[address] = byte;
}

// Reads a byte from the appropriate mapped memory address
Byte Motherboard::readByte(Word address) {
    return (*this)[address];
}

Byte& Motherboard::operator[](Word address) {
    Word searchAddress = address;
    if (searchAddress <= RAM::MEMORY_SIZE) {
        return ram[searchAddress];
    }
    searchAddress -= RAM::MEMORY_SIZE;

    if (searchAddress <= ROM::MEMORY_SIZE) {
        throw "Cannot write to ROM";
    }
    searchAddress -= ROM::MEMORY_SIZE;

    // throw out of bounds memory address error
    std::stringstream stream;
    stream << "Error: Out of bounds memory address " << stringifyHex(address) << std::endl;
    return ram[0]; // default return reference to first byte of ram
}

const Byte& Motherboard::operator[](Word address) const {
    Word searchAddress = address;
    if (searchAddress <= RAM::MEMORY_SIZE) {
        return ram[searchAddress];
    }
    searchAddress -= RAM::MEMORY_SIZE;

    if (searchAddress <= ROM::MEMORY_SIZE) {
        return rom[searchAddress];
    }
    searchAddress -= ROM::MEMORY_SIZE;

    // throw out of bounds memory address error
    std::stringstream stream;
    stream << "Error: Out of bounds memory address " << stringifyHex(address) << std::endl;
    return ram[0]; // default return reference to first byte of ram
}