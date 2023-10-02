#include "ROM.h"

void ROM::Initialize() {

    // Reset memory
    for (u32 i = 0; i < MEMORY_SIZE; i++) {
        Data[i] = 0;
    }
}

Byte ROM::ReadByte(Word address) {
    return Data[address];
}