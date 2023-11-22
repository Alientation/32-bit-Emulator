#include "ROM.h"

#include <assert.h>

ROM::ROM() { }

ROM::~ROM() {
    delete[] data;
}

void ROM::reset() {

    // DO NOT RESET ROM MEMORY
    // SHOULD SAVE ROM MEMORY TO FILE
    // // reset memory
    // for (u32 i = 0; i < MEMORY_SIZE; i++) {
    //     data[i] = 0;
    // }
}

Byte ROM::readByte(Word address) {
    return data[address];
}

Byte& ROM::operator[](Word address) {
    assert(address < MEMORY_SIZE && "ROM::operator[]: address out of bounds");
    return data[address];
}

const Byte& ROM::operator[](Word address) const {
    assert(address < MEMORY_SIZE && "ROM::operator[]: address out of bounds");
    return data[address];
}