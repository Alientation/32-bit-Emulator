#include "AlienCPU.h"
// Helper functions for instructions that have multiple addressing modes


// =====================ADDRESSING=MODE=ACCUMULATOR=====================
//                          (1 byte, 2 cycles) TODO: decide whether to describe this information in addressing modes documentation
// 1: fetch opcode from PC, increment PC
// 2: useless read from PC (for the instruction to perform its job)
void AlienCPU::ADDRESSING_ACCUMULATOR() {
    cycles++; // 2
}


// =====================ADDRESSING=MODE=IMPLIED=====================
// 1: fetch opcode from PC, increment PC
// 2: useless read from PC (for the instruction to perform its job)
// TODO: this might require 3 cycles because registers are generally 2 bytes not
//       1 byte like the 6502. This depends on how the internal works, ie how
//       the cpu is able to write to registers
void AlienCPU::ADDRESSING_IMPLIED() {
    cycles++; // 2
}


// =====================ADDRESSING=MODE=IMMEDIATE=====================
// 1: fetch opcode from PC, increment PC
// 2: fetch low byte address from PC, increment PC
// 3: fetch high byte address from PC, increment PC
u16 AlienCPU::ADDRESSING_IMMEDIATE_READ_TWOBYTES() {
    return fetchNextTwoBytes(); // 2-3
}


// =====================ADDRESSING=MODE=ABSOLUTE=====================
// 1: fetch opcode from PC, increment PC
// 2: fetch low byte address from PC, increment PC
// 3: fetch mid low byte address from PC, increment PC
// 4: fetch mid high byte address from PC, increment PC
// 5: fetch high byte address from PC, increment PC
// 6: read into register's low byte from effective address
// 7: read into register's high byte from effective address + 1
u16 AlienCPU::ADDRESSING_ABSOLUTE_READ_TWOBYTES() {
    Word address = fetchNextWord(); // 2-5
    return readTwoBytes(address); // 6-7
}

// 1: fetch opcode from PC, increment PC
// 2: fetch low byte address from PC, increment PC
// 3: fetch mid low byte address from PC, increment PC
// 4: fetch mid high byte address from PC, increment PC
// 5: fetch high byte address from PC, increment PC
// 6: read byte from effective address
// 7: useless write the value back to effective address, perform operation on value
// 8: write modified value back to effective address (implicit in this emulator)
Byte* AlienCPU::ADDRESSING_ABSOLUTE_READ_MODIFY_WRITE_BYTE() {
    Word address = fetchNextWord(); // 2-5
    Byte* valuePointer = &motherboard[address]; cycles+=3; // 6-8
    return valuePointer;
}

// 1: fetch opcode from PC, increment PC
// 2: fetch low byte address from PC, increment PC
// 3: fetch mid low byte address from PC, increment PC
// 4: fetch mid high byte address from PC, increment PC
// 5: fetch high byte address from PC, increment PC
// 6: write register's low byte to effective address
// 7: write register's high byte to effective address + 1
void AlienCPU::ADDRESSING_ABSOLUTE_WRITE_TWOBYTES(u16 registerValue) {
    Word address = fetchNextWord(); // 2-5
    writeTwoBytes(address, registerValue); // 6-7
}


// =====================ADDRESSING=MODE=ABSOLUTE=INDEXED=====================
// 1: fetch opcode from PC, increment PC
// 2: fetch low byte address from PC, increment PC
// 3: fetch mid low byte address from PC, increment PC
// 4: fetch mid high byte address from PC, increment PC, add index register to lower 2 bytes of effective address
// 5: fetch high byte address from PC, increment PC
// 6: read low byte from effective address, fix the high 2 bytes of the effective address
// 7: read high byte from effective address + 1
// 8+: read low byte from effective address if high byte changed
// 9+: read low byte from effective address + 1 if high byte changed
u16 AlienCPU::ADDRESSING_ABSOLUTE_INDEXED_READ_TWOBYTES(u16 indexRegister) {
    Word address = fetchNextWord(); // 2-5
    
    // check for page crossing
    if ((address | 0x0000FFFF) < (address + indexRegister)) { // fill last 2 bytes with max value
        cycles+=2; // 6-7
    }

    return readTwoBytes(address + indexRegister); // 6-7 or 8-9
}

// 1: fetch opcode from PC, increment PC
// 2: fetch low byte address from PC, increment PC
// 3: fetch mid low byte address from PC, increment PC
// 4: fetch mid high byte address from PC, increment PC, add index register to lower 2 bytes of effective address
// 5: fetch high byte address from PC, increment PC
// 6: useless read from effective address, fix the high 2 bytes of the effective address
// 7: read the value from effective address
// 8: useless write the value back to effective address, perform operation on value
// 9: write the modified value back to effective address
Byte* AlienCPU::ADDRESSING_ABSOLUTE_INDEXED_READ_MODIFY_WRITE_BYTE(u16 indexRegister) {
    Word address = fetchNextWord(); // 2-5
    cycles++; // 6 (fix high bytes)
    Byte* valuePointer = &motherboard[address + indexRegister]; cycles+=3; // 7-9
    return valuePointer;
}

// 1: fetch opcode from PC, increment PC
// 2: fetch low byte address from PC, increment PC
// 3: fetch mid low byte address from PC, increment PC
// 4: fetch mid high byte address from PC, increment PC, add index register to lower 2 bytes of effective address
// 5: fetch high byte address from PC, increment PC
// 6: useless read from effective address, fix the high 2 bytes of the effective address
// 7: write register's low byte to effective address
// 8: write register's high byte to effective address + 1
void AlienCPU::ADDRESSING_ABSOLUTE_INDEXED_WRITE_TWOBYTES(u16 indexRegister, u16 registerValue) {
    Word address = fetchNextWord(); // 2-5
    address += indexRegister; cycles++; // 6
    writeTwoBytes(address, registerValue); // 7-8
}


// =====================ADDRESSING=MODE=XINDEXED=INDIRECT=====================s
// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address byte from PC, increment PC
// 4: read useless data, add X index register to base zero page address (wraps around in zero page)
// 5: fetch low byte address from calculated effective zero page address
// 6: fetch mid low byte address from calculated effective zero page address + 1
// 7: fetch mid high byte address from calculated effective zero page address + 2
// 8: fetch high byte address from calculated effective zero page address + 3
// 9: read low byte from calculated effective address
// 10: read high byte from calculated effective address + 1
u16 AlienCPU::ADDRESSING_XINDEXED_INDIRECT_READ_TWOBYTES() {
    u16 zeroPageAddressOfAddress = fetchNextTwoBytes(); // 2-3
    zeroPageAddressOfAddress += X; cycles++; // 4
    Word address = readWord(zeroPageAddressOfAddress); // 5-8
    return readTwoBytes(address); // 9-10
}

// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address byte from PC, increment PC
// 4: read useless data, add X index register to base zero page address (wraps around in zero page)
// 5: fetch low byte address from calculated effective zero page address
// 6: fetch mid low byte address from calculated effective zero page address + 1
// 7: fetch mid high byte address from calculated effective zero page address + 2
// 8: fetch high byte address from calculated effective zero page address + 3
// 9: write register's low byte to calculated effective address
// 10: write register's high byte to calculated effective address + 1
void AlienCPU::ADDRESSING_XINDEXED_INDIRECT_WRITE_TWOBYTES(u16 registerValue) {
    u16 zeroPageAddressOfAddress = fetchNextTwoBytes(); // 2-3
    zeroPageAddressOfAddress += X; cycles++; // 4
    Word address = readWord(zeroPageAddressOfAddress); // 5-8
    writeTwoBytes(address, registerValue); // 9-10
}


// =====================ADDRESSING=MODE=INDIRECT=YINDEXED=====================
// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address byte from PC, increment PC
// 4: fetch address low byte from zero page address
// 5: fetch address mid low byte from zero page address + 1
// 6: fetch address mid high byte from zero page address + 2, add Y index register to lower 2 bytes of effective address
// 7: fetch address high byte from zero page address + 3
// 8: read low byte from calculated effective address, fix high 2 bytes of effective address
// 9: read high byte from calculated effective address + 1
// 10+: read low byte from calculated effective address if high 2 bytes changed
// 11+: read high byte from calculated effective address + 1 if high 2 bytes changed
u16 AlienCPU::ADDRESSING_INDIRECT_YINDEXED_READ_TWOBYTES() {
    // get address in the zero page that points to part of the address of the data
    u16 zeroPageAddressOfAddress = fetchNextTwoBytes(); // 2-3
    Word address = readWord(zeroPageAddressOfAddress); // 4-7

    // check for page crossing
    if ((address | 0x0000FFFF) < (address + Y)) {
        cycles+=2; // 8-9
    }

    return readTwoBytes(address + Y); // 8-9 or 10-11
}

// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address byte from PC, increment PC
// 4: fetch address low byte from zero page address
// 5: fetch address mid low byte from zero page address + 1
// 6: fetch address mid high byte from zero page address + 2, add Y index register to lower 2 bytes of effective address
// 7: fetch address high byte from zero page address + 3
// 8: useless read from calculated effective address, fix high 2 bytes of effective address
// 9: write register's low byte to calculated effective address
// 10: write register's high byte to calculated effective address + 1
void AlienCPU::ADDRESSING_INDIRECT_YINDEXED_WRITE_TWOBYTES(u16 registerValue) {
    // get address in the zero page that points to part of the address of the data
    u16 zeroPageAddressOfAddress = fetchNextTwoBytes(); // 2-3
    Word address = readWord(zeroPageAddressOfAddress); // 4-7
    cycles++; // 8 (fix high bytes)
    writeTwoBytes(address + Y, registerValue); // 9-10
}


// ===================ADDRESSING=MODE=ZEROPAGE===================
// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address from PC, increment PC
// 4: read low byte from effective zero page address
// 5: read high byte from effective zero page address + 1
u16 AlienCPU::ADDRESSING_ZEROPAGE_READ_TWOBYTES() {
    u16 zeroPageAddress = fetchNextTwoBytes(); // 2-3
    return readTwoBytes(zeroPageAddress); // 4-5
}

// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address from PC, increment PC
// 4: read byte from effective zero page address
// 5: useless write the value back to effective zero page address, perform operation on value
// 6: write the modified value back to effective zero page address
Byte* AlienCPU::ADDRESSING_ZEROPAGE_READ_MODIFY_WRITE_BYTE() {
    u16 zeroPageAddress = fetchNextTwoBytes(); // 2-3
    Byte* valuePointer = &motherboard[zeroPageAddress]; cycles+=3; // 4-6
    return valuePointer;
}

// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address from PC, increment PC
// 4: write register's low byte to effective zero page address
// 5: write register's high byte to effective zero page address + 1
void AlienCPU::ADDRESSING_ZEROPAGE_WRITE_TWOBYTES(u16 registerValue) {
    u16 zeroPageAddress = fetchNextTwoBytes(); // 2-3
    writeTwoBytes(zeroPageAddress, registerValue); // 4-5
}


// ===================ADDRESSING=MODE=ZEROPAGE=INDEXED===================
// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address byte from PC, increment PC
// 4: read useless data, add index register to base zero page address (wraps around in zero page)
// 5: read low byte from calculated effective zero page address
// 6: read high byte from calculated effective zero page address + 1
u16 AlienCPU::ADDRESSING_ZEROPAGE_INDEXED_READ_TWOBYTES(u16 indexRegister) {
    u16 zeroPageAddress = fetchNextTwoBytes(); // 2-3
    zeroPageAddress += indexRegister; cycles++; // 4
    return readTwoBytes(zeroPageAddress); // 5-6
}

// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address byte from PC, increment PC
// 4: read useless data, add index register to base zero page address (wraps around in zero page)
// 5: read byte from calculated effective zero page address
// 6: useless write the value back to calculated effective zero page address, perform operation on value
// 7: write the modified value back to calculated effective zero page address
Byte* AlienCPU::ADDRESSING_ZEROPAGE_INDEXED_READ_MODIFY_WRITE_BYTE(u16 indexRegister) {
    u16 zeroPageAddress = fetchNextTwoBytes(); // 2-3
    zeroPageAddress += indexRegister; cycles++; // 4
    Byte* valuePointer = &motherboard[zeroPageAddress]; cycles+=3; // 5-7
    return valuePointer;
}

// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address byte from PC, increment PC
// 4: read useless data, add index register to base zero page address (wraps around in zero page)
// 5: write register's low byte from calculated effective zero page address
// 6: write register's high byte from calculated effective zero page address + 1
void AlienCPU::ADDRESSING_ZEROPAGE_INDEXED_WRITE_TWOBYTES(u16 indexRegister, u16 registerValue) {
    u16 zeroPageAddress = fetchNextTwoBytes(); // 2-3
    zeroPageAddress += indexRegister; cycles++; // 4
    writeTwoBytes(zeroPageAddress, registerValue); // 5-6
}