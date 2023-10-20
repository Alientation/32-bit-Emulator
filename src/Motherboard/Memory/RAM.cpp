#include "RAM.h"

#include <assert.h>

void RAM::reset() {

    // reset memory
    for (u32 i = 0; i < MEMORY_SIZE; i++) {
        data[i] = 0;
    }
}

void RAM::writeByte(Word address, Byte byte) {
    data[address] = byte;
}

Byte RAM::readByte(Word address) {
    return data[address];
}

Byte& RAM::operator[](Word address) {
    assert(address < MEMORY_SIZE && "RAM::operator[]: address out of bounds");
    return data[address];
}

const Byte& RAM::operator[](Word address) const {
    assert(address < MEMORY_SIZE && "RAM::operator[]: address out of bounds");
    return data[address];
}