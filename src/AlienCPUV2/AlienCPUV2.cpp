#include "AlienCPUV2.h"
#include "Types.h"


AlienCPUV2::AlienCPUV2(bool debugMode) {
    this->debugMode = debugMode;
    reset();
}


void AlienCPUV2::Step(u64 cycles) {

}


void AlienCPUV2::reset() {
    cycles = 0;

    IR = 0x00;
    A = 0xAAAA;
    X = 0x0000;
    Y = 0x0000;
    SP = 0x0000;
}


// Clear the specified flag bit from processor status register
void AlienCPUV2::clearFlag(StatusFlag flag) {
    P &= ~(1 << flag);
}

// Sets the specified flag bit from processor status register
void AlienCPUV2::setFlag(StatusFlag flag, bool isSet) {
    P = (P & ~((u8)isSet << flag)) | ((u8)isSet << flag);
}

// Gets the specified flag bit from processor status register
bool AlienCPUV2::getFlag(StatusFlag flag) {
    return P & (1 << flag);
}