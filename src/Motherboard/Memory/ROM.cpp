#include "ROM.h"

void ROM::initialize() {

    // reset memory
    for (u32 i = 0; i < MEMORY_SIZE; i++) {
        Data[i] = 0;
    }
}

Byte ROM::readByte(Word address) {
    return Data[address];
}