#include "RAM.h"

void RAM::Initialize() {

    // Reset memory
    for (u32 i = 0; i < MEMORY_SIZE; i++) {
        Data[i] = 0;
    }
}