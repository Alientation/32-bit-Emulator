#include "RAM.h"

void RAM::initialize() {

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