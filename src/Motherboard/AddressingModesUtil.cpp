#include "AlienCPU.h"
// Helper functions for instructions that have multiple addressing modes


// =====================ADDRESSING=MODE=ACCUMULATOR=====================
//                          1 byte | 2 cycles
// 1: fetch opcode from PC, increment PC
// 2: useless read from PC (for the instruction to perform its job)
Word AlienCPU::ADDRESSING_ACCUMULATOR() {
    cycles++; // 2
    return 0;
}


// =======================ADDRESSING=MODE=IMPLIED=======================
//                          1 byte | 2 cycles
// 1: fetch opcode from PC, increment PC
// 2: useless read from PC (for the instruction to perform its job)
Word AlienCPU::ADDRESSING_IMPLIED() {
    cycles++; // 2
    return 0;
}


// ======================ADDRESSING=MODE=IMMEDIATE======================
//                          3 bytes | 3 cycles
// 1: fetch opcode from PC, increment PC
// 2: fetch low byte address from PC, increment PC
// 3: fetch high byte address from PC, increment PC
Word AlienCPU::ADDRESSING_IMMEDIATE() {
    PC+=2; // 2-3
    return PC-2;
}


// =======================ADDRESSING=MODE=RELATIVE======================
//                          ? bytes | ? cycles
Word AlienCPU::ADDRESSING_RELATIVE() {
    return 0; // TODO: implement
}


// =======================ADDRESSING=MODE=INDIRECT======================
//                          ? bytes | ? cycles
Word AlienCPU::ADDRESSING_INDIRECT() {
    return 0; // TODO: implement
}


// ======================ADDRESSING=MODE=ABSOLUTE=======================
//                          5 bytes | 5 cycles
// 1: fetch opcode from PC, increment PC
// 2: fetch low byte address from PC, increment PC
// 3: fetch mid low byte address from PC, increment PC
// 4: fetch mid high byte address from PC, increment PC
// 5: fetch high byte address from PC, increment PC
Word AlienCPU::ADDRESSING_ABSOLUTE() {
    return fetchNextWord(); // 2 - 5
}

// ================ADDRESSING=MODE=ABSOLUTE=XINDEXED=READ================
//                          5 bytes | 7-9 cycles
// 1: fetch opcode from PC, increment PC
// 2: fetch low byte address from PC, increment PC
// 3: fetch mid low byte address from PC, increment PC
// 4: fetch mid high byte address from PC, increment PC, add X register to lower 2 bytes of effective address
// 5: fetch high byte address from PC, increment PC
// 6: useless read, fix the high 2 bytes of the effective address
Word AlienCPU::ADDRESSING_ABSOLUTE_XINDEXED() {
    Word address = fetchNextWord() + X; cycles++; // 2-6
    return address;
}

// ================ADDRESSING=MODE=ABSOLUTE=YINDEXED=READ================
//                          5 bytes | 7-9 cycles
// 1: fetch opcode from PC, increment PC
// 2: fetch low byte address from PC, increment PC
// 3: fetch mid low byte address from PC, increment PC
// 4: fetch mid high byte address from PC, increment PC, add Y register to lower 2 bytes of effective address
// 5: fetch high byte address from PC, increment PC
// 6: useless read, fix the high 2 bytes of the effective address
Word AlienCPU::ADDRESSING_ABSOLUTE_YINDEXED() {
    Word address = fetchNextWord() + Y; cycles++; // 2-6
    return address;
}


// ================ADDRESSING=MODE=XINDEXED=INDIRECT=READ================
//                          3 bytes | 10 cycles
// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address byte from PC, increment PC
// 4: read useless data, add X index register to base zero page address (wraps around in zero page)
// 5: fetch low byte address from calculated effective zero page address
// 6: fetch mid low byte address from calculated effective zero page address + 1
// 7: fetch mid high byte address from calculated effective zero page address + 2
// 8: fetch high byte address from calculated effective zero page address + 3
Word AlienCPU::ADDRESSING_XINDEXED_INDIRECT() {
    u16 zeroPageAddressOfAddress = fetchNextTwoBytes(); // 2-3
    zeroPageAddressOfAddress += X; cycles++; // 4
    return readWord(zeroPageAddressOfAddress); // 5-8
}


// ================ADDRESSING=MODE=INDIRECT=YINDEXED=READ=================
//                          3 bytes | 9-11 cycles
// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address byte from PC, increment PC
// 4: fetch address low byte from zero page address
// 5: fetch address mid low byte from zero page address + 1
// 6: fetch address mid high byte from zero page address + 2, add Y index register to lower 2 bytes of effective address
// 7: fetch address high byte from zero page address + 3
// 8: useless read, fix high 2 bytes of effective address
Word AlienCPU::ADDRESSING_INDIRECT_YINDEXED() {
    // get address in the zero page that points to part of the address of the data
    u16 zeroPageAddressOfAddress = fetchNextTwoBytes(); // 2-3
    Word address = readWord(zeroPageAddressOfAddress) + Y; cycles++; // 4-8
    return address;
}


// =====================ADDRESSING=MODE=ZEROPAGE=READ=====================
//                          3 bytes | 5 cycles
// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address from PC, increment PC
Word AlienCPU::ADDRESSING_ZEROPAGE() {
    return fetchNextTwoBytes(); // 2-3
}


// ===================ADDRESSING=MODE=ZEROPAGE=XINDEXED===================
//                          3 bytes | 6 cycles
// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address byte from PC, increment PC
// 4: read useless data, add X register to base zero page address (wraps around in zero page)
Word AlienCPU::ADDRESSING_ZEROPAGE_XINDEXED() {
    u16 zeroPageAddress = fetchNextTwoBytes(); // 2-3
    zeroPageAddress += X; cycles++; // 4
    return zeroPageAddress;
}


// ===================ADDRESSING=MODE=ZEROPAGE=YINDEXED===================
//                          3 bytes | 6 cycles
// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address byte from PC, increment PC
// 4: read useless data, add Y register to base zero page address (wraps around in zero page)
Word AlienCPU::ADDRESSING_ZEROPAGE_YINDEXED() {
    u16 zeroPageAddress = fetchNextTwoBytes(); // 2-3
    zeroPageAddress += Y; cycles++; // 4
    return zeroPageAddress;
}