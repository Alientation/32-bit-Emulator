#include "RAM.h"

void RAM::Initialize() {

    // Reset memory
    for (u32 i = 0; i < MEMORY_SIZE; i++) {
        data[i] = 0;
    }
}

void RAM::WriteByte(Word address, Byte byte) {
    data[address] = byte;
}

Byte RAM::ReadByte(Word address) {
    return data[address];
}