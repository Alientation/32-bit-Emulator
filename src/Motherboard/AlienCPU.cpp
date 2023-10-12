#include "AlienCPU.h"
#include <iostream>
#include <iomanip>

const std::string AlienCPU::VERSION = "0.0.1";

AlienCPU::AlienCPU() {
    AlienCPU::initInstructions();
    
    // Reset the CPU, all registers, memory etc
    reset();
}

// realistically, reset actually randomizes values for memory and registers
// should also technically perform the 7 cycle reset sequence, which would require
// fixing all the tests
void AlienCPU::reset() {
    // reset all registers
    PC = PC_INIT;
    SP = SP_INIT;

    A = A_INIT;
    X = X_INIT;
    Y = Y_INIT;

    // reset flags
    P = P_INIT;

    // reset cycle counters
    nextInterruptCheck = INTERRUPT_CHECK_INTERVAL;
    cycles = 0;

    // prepare motherboard
    motherboard.initialize();
}

void AlienCPU::start(u64 maxCycles) {
    if (debugMode) {
        std::cout << "Starting Alien CPU v" << VERSION << std::endl;
        std::cout << "Max cycles: " << maxCycles << std::endl;
    }

    // start sequence / boot process, read from RESET vector and jump to there
    PC = readWord(POWER_ON_RESET_VECTOR);

    // reset cycle counter
    cycles = 0;
    
    // Fetch, Decode, Execute Cycle loop
    for (;;) {
        // Halt execution because max cycles has been reached
        if (cycles >= maxCycles) {
            if (debugMode) {
                std::cout << std::endl << "Max cycles reached" << std::endl;
            }
            break;
        }

        // Reads in the next instruction
        Byte nextInstruction = fetchNextByte();

        // Executes the instruction even if it is invalid
        executeInstruction(nextInstruction);


        // Check for Interrupts
        if (nextInterruptCheck == 0) {

        }
    }

    if (debugMode) {
        std::cout << "Stopping Alien CPU v" << VERSION << std::endl;
        std::cout << "Cycles ran " << cycles << std::endl;
    }
}


// Executes the instruction if it is valid, otherwise throws an exception
void AlienCPU::executeInstruction(u16 instruction) {
    if (!isValidInstruction(instruction)) {
        std::stringstream stream;
        stream << std::endl << "Error: Invalid instruction " << stringifyHex(instruction) << std::endl;
        
        //throw std::invalid_argument(stream.str()); // to allow compiling
        std::cout << stream.str();
        return;
    }

    instructions[instruction](*this); // calls the function associated with the instruction
}

// Checks if the instruction is a valid instruction. 
// Must be within max instructions and must not be a null instruction
bool AlienCPU::isValidInstruction(u16 instruction) {
    return instruction < INSTRUCTION_COUNT && &instructions[instruction] != &instructions[0];
}


// Clear the specified flag bit from processor status register
void AlienCPU::clearFlag(Byte bit) {
    P &= ~((u8)1 << bit);
}

// Sets the specified flag bit from processor status register
void AlienCPU::setFlag(Byte bit, bool isSet) {
    // 11111111
    // 00000001
    // 11111110
    // 11111110 | 0

    P = (P & ~((u8)1 << bit)) | ((u8)isSet << bit);
}

// Gets the specified flag bit from processor status register
bool AlienCPU::getFlag(Byte bit) {
    return P & (1 << bit);
}


u16 AlienCPU::convertToLowEndianTwoBytes(u16 highEndianTwoBytes) {
    return (highEndianTwoBytes >> 8) | (highEndianTwoBytes << 8);
}

u16 AlienCPU::convertToHighEndianTwoBytes(u16 lowEndianTwoBytes) {
    return (lowEndianTwoBytes << 8) | (lowEndianTwoBytes >> 8);
}

Word AlienCPU::convertToLowEndianWord(Word highEndianWord) {
    return (highEndianWord >> 24) | ((highEndianWord >> 8) & 0xFF00) | 
            ((highEndianWord << 8) & 0xFF0000) | (highEndianWord << 24);
}

Word AlienCPU::convertToHighEndianWord(Word lowEndianWord) {
    return (lowEndianWord << 24) | ((lowEndianWord << 8) & 0xFF0000) | 
            ((lowEndianWord >> 8) & 0xFF00) | (lowEndianWord >> 24);
}


// Reads a byte at a high endian memory address (1 cycle)
Byte AlienCPU::readByte(Word highEndianAddress) {
    cycles++;
    return motherboard.readByte(highEndianAddress);
}

// Reads the next 2 low endian bytes at a high endian memory address 
// and converts to high endian (2 cycles)
u16 AlienCPU::readTwoBytes(Word highEndianAddress) {
    // reads in owest byte
    u16 highEndianData = readByte(highEndianAddress);
    highEndianData |= readByte(highEndianAddress + 1) << 8;
    // reads in highest byte

    return highEndianData;
}

// Reads the next 4 low endian bytes at a high endian memory address 
// and converts to high endian (4 cycles)
Word AlienCPU::readWord(Word highEndianAddress) {
    // reads in lowest byte
    Word highEndianData = readByte(highEndianAddress);
    highEndianData |= readByte(highEndianAddress + 1) << 8;
    highEndianData |= readByte(highEndianAddress + 2) << 16;
    highEndianData |= readByte(highEndianAddress + 3) << 24;
    // reads in highest byte

    return highEndianData;
}


// Reads the next byte in memory and increments PC (1 cycle)
Byte AlienCPU::fetchNextByte() {
    Byte data = motherboard.readByte(PC);
    PC++;
    cycles++;
    return data;
}

// Reads the next 4 low endian bytes in memory but converts to 
// high endian and increments PC (2 cycles)
u16 AlienCPU::fetchNextTwoBytes() {
    // reads in lowest byte
    u16 highEndianData = fetchNextByte();
    highEndianData |= fetchNextByte() << 8;
    // reads in highest byte

    return highEndianData;
}

// Reads the next 4 low endian bytes in memory but converts to 
// high endian and increments PC (4 cycles)
Word AlienCPU::fetchNextWord() {
    // reads in lowest byte
    Word highEndianData = fetchNextByte();
    highEndianData |= fetchNextByte() << 8;
    highEndianData |= fetchNextByte() << 16;
    highEndianData |= fetchNextByte() << 24;
    // reads in highest byte

    return highEndianData;
}

// Write byte to the specified high endian address in memory (1 cycle)
void AlienCPU::writeByte(Word highEndianAddress, Byte value) {
    // write byte 0 to memory
    motherboard.writeByte(highEndianAddress, value);
    cycles++;
}

// Writes 2 high endian bytes converted to low endian to the 
// specified high endian address in memory (2 cycles)
void AlienCPU::writeTwoBytes(Word highEndianAddress, u16 highEndianValue) {
    // lowest byte
    writeByte(highEndianAddress, highEndianValue & 0xFF);
    writeByte(highEndianAddress + 1, (highEndianValue >> 8) & 0xFF);
    // highest byte
}

// Writes 2 low endian bytes to the specified high endian address in memory (2 cycles)
void AlienCPU::writeTwoBytesAbsolute(Word highEndianAddress, u16 lowEndianValue) {
    writeByte(highEndianAddress, (lowEndianValue >> 8) & 0xFF);
    writeByte(highEndianAddress + 1, lowEndianValue & 0xFF);
}

// Writes 4 high endian bytes converted to low endian to the 
// specified high endian address in memory (4 cycles)
void AlienCPU::writeWord(Word highEndianAddress, Word value) {
    // lowest byte
    writeByte(highEndianAddress, value & 0xFF);
    writeByte(highEndianAddress + 1, (value >> 8) & 0xFF);
    writeByte(highEndianAddress + 2, (value >> 16) & 0xFF);
    writeByte(highEndianAddress + 3, (value >> 24) & 0xFF);
    // highest byte
}

// Writes 4 low endian bytes to the specified high endian address in memory (4 cycles)
void AlienCPU::writeWordAbsolute(Word highEndianAddress, Word lowEndianValue) {
    writeByte(highEndianAddress, (lowEndianValue >> 24) & 0xFF);
    writeByte(highEndianAddress + 1, (lowEndianValue >> 16) & 0xFF);
    writeByte(highEndianAddress + 2, (lowEndianValue >> 8) & 0xFF);
    writeByte(highEndianAddress + 3, lowEndianValue & 0xFF);
}


// =======================STACK============================
// Stack representation in memory
// [0x00000000]
// [0x00010000] End of stack memory
// [SP]         First free byte in stack memory
// [SP+1]       Last used byte in stack memory
// [0x0001FFFF] Start of stack memory
// [0x000FFFFF]
//

// Converts the stack pointer to a full 32 bit address in memory on the first page
Word AlienCPU::SPToAddress() {
    return 0x00010000 | SP;
}

// Pushes the Program Counter to stack memory (4 bytes)
void AlienCPU::pushPCToStack() {
    pushWordToStack(PC);
}

// Pops the Program Counter from stack memory (4 bytes)
void AlienCPU::popPCFromStack() {
    PC = popWordFromStack();
}

// Push 4 bytes to stack memory
void AlienCPU::pushWordToStack(Word value) {
    // push the high byte first so it comes after the low byte in memory
    // since stack is stored backwards in memory
    pushByteToStack((value & 0xFF000000) >> 24); // high byte
    pushByteToStack((value & 0x00FF0000) >> 16);
    pushByteToStack((value & 0x0000FF00) >> 8);
    pushByteToStack(value & 0x000000FF); // low byte
}

// Pops 4 bytes from stack memory
Word AlienCPU::popWordFromStack() {
    // pops the little endian value from the stack and converts to high endian
    // since stack is stored backwards, the first values read from stack are the low bytes
    Word value = popByteFromStack(); // low byte
    value |= popByteFromStack() << 8;
    value |= popByteFromStack() << 16;
    value |= popByteFromStack() << 24; // high byte
    return value;
}

// Push 2 bytes to stack memory
void AlienCPU::pushTwoBytesToStack(u16 value) {
    // push the high byte first so it comes after the low byte in memory
    // since stack is stored backwards in memory
    pushByteToStack((value & 0xFF00) >> 8); // high byte
    pushByteToStack(value & 0x00FF); // low byte
}

// Pops 2 bytes from stack memory
u16 AlienCPU::popTwoBytesFromStack() {
    // pops the little endian value from the stack and converts to high endian
    // since stack is stored backwards, the first values read from stack are the low bytes
    u16 value = popByteFromStack(); // low byte
    value |= popByteFromStack() << 8; // high byte
    return value;
}

// Push 1 byte to stack memory
void AlienCPU::pushByteToStack(Byte value) {
    // write the value in the first free byte represented by the Stack Pointer
    motherboard.writeByte(SPToAddress(), value);

    // move stack pointer so it points to the first free byte in stack memory
    SP--;
}

// Pops 1 byte from stack memory
Byte AlienCPU::popByteFromStack() {
    // move stack pointer so it points to the first free byte in stack memory
    SP++;

    // read the value from the byte at the now free byte represented by the Stack Pointer
    return motherboard.readByte(SPToAddress());
}



// ====================INSTRUCTIONS======================
// Every cycle must memory read or write (using the pins for address and return data)
// To add, it must be done after it is read (cannot be done in the same cycle it was read)
// It must also have a useless memory read (not write because we do not want to change program data)
// 
// Although the cycle counts and what is done at each cycle may not be truly accurate to the 6502,
// it is closely emulates the process of the 6502's instruction cycles
// The general pattern of instruction clock cycles in this processor compared to 6502 follows this rule
//          (6502's instruction clock cycles - 1) * 2 + 1 = this processor's instruction clock cycles
// This is because the first cycle is reading in the opcode (1 byte and therefore 1 cycle for both processors)
// But the rest are memory read's and write's (double the cycle count for this processor because this processor handles double the bytes)
// There are a few instances where there are *useless* memory reads to allow for the performing of addition
// Which results in an equation like this (6502's instruction clock cycles - 2) * 2 + 2 = this processor's instruction clock cycles
//
// The implementation of the instructions may not be entirely based accurately on the 6502's implementation or
// the cycle count described by the documentation. If and once the transition to cycle accurate cpu processing
// then the implementation shall be accurate to the described process
//
// https://www.nesdev.org/6502_cpu.txt
// TODO: figure out a way to have cycle stepping instead of completing the entire instruction in one pass
//       This will also allow basic pipelining (reading in next instruction at the last cycle step 
//       of this current instruction if this instruction does not write to memory in the current cycle)
//
// TODO figure out a way to have a single instruction for each instruction and its addressable modes
//      and have addressing modes be functions that return the correct value to use
//

// Sets ZERO flag if the modified register is 0 and NEGATIVE flag if the 
// last bit of the modified register is set
void AlienCPU::UPDATE_FLAGS(u16 modifiedRegister) {
    setFlag(Z_FLAG, modifiedRegister == 0);
    setFlag(N_FLAG, modifiedRegister >> 15);
}

// 1: fetch opcode from PC, increment PC
// 2: useless read from PC (for the instruction to perform its job)
void AlienCPU::ADDRESSING_MODE_ACCUMULATOR() {
    cycles++;
}

// 1: fetch opcode from PC, increment PC
// 2: useless read from PC (for the instruction to perform its job)
void AlienCPU::ADDRESSING_MODE_IMPLIED() {
    cycles++;
}


// 1: fetch opcode from PC, increment PC
// 2: fetch low byte address from PC, increment PC
// 3: fetch high byte address from PC, increment PC
u16 AlienCPU::ADDRESSING_MODE_IMMEDIATE_READ_TWOBYTES() {
    return fetchNextTwoBytes();
}


// 1: fetch opcode from PC, increment PC
// 2: fetch low byte address from PC, increment PC
// 3: fetch mid low byte address from PC, increment PC
// 4: fetch mid high byte address from PC, increment PC
// 5: fetch high byte address from PC, increment PC
// 6: read into register's low byte from effective address
// 7: read into register's high byte from effective address + 1
u16 AlienCPU::ADDRESSING_MODE_ABSOLUTE_READ_TWOBYTES() {
    Word address = fetchNextWord();
    return readTwoBytes(address);
}

// 1: fetch opcode from PC, increment PC
// 2: fetch low byte address from PC, increment PC
// 3: fetch mid low byte address from PC, increment PC
// 4: fetch mid high byte address from PC, increment PC
// 5: fetch high byte address from PC, increment PC
// 6: write register's low byte to effective address
// 7: write register's high byte to effective address + 1
void AlienCPU::ADDRESSING_MODE_ABSOLUTE_WRITE_TWOBYTES(u16 registerValue) {
    Word address = fetchNextWord();
    writeTwoBytes(address, registerValue);
}


// 1: fetch opcode from PC, increment PC
// 2: fetch low byte address from PC, increment PC
// 3: fetch mid low byte address from PC, increment PC
// 4: fetch mid high byte address from PC, increment PC, add index register to lower 2 bytes of effective address
// 5: fetch high byte address from PC, increment PC
// 6: read low byte from effective address, fix the high 2 bytes of the effective address
// 7: read high byte from effective address + 1
// 8+: read low byte from effective address if high byte changed
// 9+: read low byte from effective address + 1 if high byte changed
u16 AlienCPU::ADDRESSING_MODE_ABSOLUTE_INDEXED_READ_TWOBYTES(u16 indexRegister) {
    Word address = fetchNextWord();
    
    // check for page crossing, solely for accurate cycle counting
    if ((address | 0x0000FFFF) < (address + indexRegister)) { // fill last 2 bytes with max value
        cycles+=2;
    }

    return readTwoBytes(address + indexRegister);
}

// 1: fetch opcode from PC, increment PC
// 2: fetch low byte address from PC, increment PC
// 3: fetch mid low byte address from PC, increment PC
// 4: fetch mid high byte address from PC, increment PC, add index register to lower 2 bytes of effective address
// 5: fetch high byte address from PC, increment PC
// 6: useless read from effective address, fix the high 2 bytes of the effective address
// 7: write register's low byte to effective address
// 8: write register's high byte to effective address + 1
void AlienCPU::ADDRESSING_MODE_ABSOLUTE_INDEXED_WRITE_TWOBYTES(u16 indexRegister, u16 registerValue) {
    Word address = fetchNextWord() + indexRegister;
    cycles++;
    writeTwoBytes(address, registerValue);
}


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
u16 AlienCPU::ADDRESSING_MODE_XINDEXED_INDIRECT_READ_TWOBYTES() {
    u16 zeroPageAddressOfAddress = fetchNextTwoBytes() + X;
    Word address = readWord(zeroPageAddressOfAddress);
    cycles++;
    return readTwoBytes(address);
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
void AlienCPU::ADDRESSING_MODE_XINDEXED_INDIRECT_WRITE_TWOBYTES(u16 registerValue) {
    u16 zeroPageAddressOfAddress = fetchNextTwoBytes() + X;
    Word address = readWord(zeroPageAddressOfAddress);
    cycles++;
    writeTwoBytes(address, registerValue);
}


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
u16 AlienCPU::ADDRESSING_MODE_INDIRECT_YINDEXED_READ_TWOBYTES() {
    // get address in the zero page that points to part of the address of the data
    u16 zeroPageAddressOfAddress = fetchNextTwoBytes();
    Word address = readWord(zeroPageAddressOfAddress);

    // check for page crossing, solely for accurate cycle counting
    if ((address | 0x0000FFFF) < (address + Y)) {
        cycles+=2;
    }

    return readTwoBytes(address + Y);
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
void AlienCPU::ADDRESSING_MODE_INDIRECT_YINDEXED_WRITE_TWOBYTES(u16 registerValue) {
    // get address in the zero page that points to part of the address of the data
    u16 zeroPageAddressOfAddress = fetchNextTwoBytes();
    Word address = readWord(zeroPageAddressOfAddress);
    cycles++;
    writeTwoBytes(address + Y, registerValue);
}


// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address from PC, increment PC
// 4: read low byte from effective zero page address
// 5: read high byte from effective zero page address + 1
u16 AlienCPU::ADDRESSING_MODE_ZERO_PAGE_READ_TWOBYTES() {
    u16 zeroPageAddress = fetchNextTwoBytes();
    return readTwoBytes(zeroPageAddress);
}

// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address from PC, increment PC
// 4: write register's low byte to effective zero page address
// 5: write register's high byte to effective zero page address + 1
void AlienCPU::ADDRESSING_MODE_ZERO_PAGE_WRITE_TWOBYTES(u16 registerValue) {
    u16 zeroPageAddress = fetchNextTwoBytes();
    writeTwoBytes(zeroPageAddress, registerValue);
}


// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address byte from PC, increment PC
// 4: read useless data, add index register to base zero page address (wraps around in zero page)
// 5: read low byte from calculated effective zero page address
// 6: read high byte from calculated effective zero page address + 1
u16 AlienCPU::ADDRESSING_MODE_ZERO_PAGE_INDEXED_READ_TWOBYTES(u16 indexRegister) {
    u16 zeroPageAddress = fetchNextTwoBytes() + indexRegister;
    cycles++;
    return readTwoBytes(zeroPageAddress);
}

// 1: fetch opcode from PC, increment PC
// 2: fetch low byte zero page address from PC, increment PC
// 3: fetch mid low zero page address byte from PC, increment PC
// 4: read useless data, add index register to base zero page address (wraps around in zero page)
// 5: write register's low byte from calculated effective zero page address
// 6: write register's high byte from calculated effective zero page address + 1
void AlienCPU::ADDRESSING_MODE_ZERO_PAGE_INDEXED_WRITE_TWOBYTES(u16 indexRegister, u16 registerValue) {
    u16 zeroPageAddress = fetchNextTwoBytes() + indexRegister;
    cycles++;
    writeTwoBytes(zeroPageAddress, registerValue);
}


// ======================TRANSFER========================
// ===================LOAD=ACCUMULATOR===================
// LOAD ACCUMULATOR IMMEDIATE ($A9 | 3 bytes | 3 cycles)
// 1-3: Immediate addressing mode load value
void AlienCPU::_A9_LDA_Immediate_Instruction() {
    A = ADDRESSING_MODE_IMMEDIATE_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// LOAD ACCUMULATOR ABSOLUTE ($AD | 5 bytes | 7 cycles)
// 1-7: Absolute addressing mode load value
void AlienCPU::_AD_LDA_Absolute_Instruction() {
    A = ADDRESSING_MODE_ABSOLUTE_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// LOAD ACCUMULATOR ABSOLUTE X-INDEXED ($BD | 5 bytes | 7-9 cycles)
// 1-7/9: Absolute indexed addressing mode load value
void AlienCPU::_BD_LDA_Absolute_XIndexed_Instruction() {
    A = ADDRESSING_MODE_ABSOLUTE_INDEXED_READ_TWOBYTES(X);
    UPDATE_FLAGS(A);
}

// LOAD ACCUMULATOR ABSOLUTE Y-INDEXED ($B9 | 5 bytes | 7-9 cycles)
// 1-7/9: Absolute indexed addressing mode load value
void AlienCPU::_B9_LDA_Absolute_YIndexed_Instruction() {
    A = ADDRESSING_MODE_ABSOLUTE_INDEXED_READ_TWOBYTES(Y);
    UPDATE_FLAGS(A);
}

// LOAD ACCUMULATOR X-INDEXED INDIRECT ($A1 | 3 bytes | 10 cycles)
// 1-10: X indexed indirect addressing mode load value
void AlienCPU::_A1_LDA_XIndexed_Indirect_Instruction() {
    A = ADDRESSING_MODE_XINDEXED_INDIRECT_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// LOAD ACCUMULATOR INDIRECT Y-INDEXED ($B1 | 3 bytes | 9-11 cycles)
// 1-9/11: Indirect Y indexed addressing mode load value
void AlienCPU::_B1_LDA_Indirect_YIndexed_Instruction() {
    A = ADDRESSING_MODE_INDIRECT_YINDEXED_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// LOAD ACCUMULATOR ZEROPAGE ($A5 | 3 bytes | 5 cycles)
// 1-5: Zero page addressing mode load value
void AlienCPU::_A5_LDA_ZeroPage_Instruction() {
    A = ADDRESSING_MODE_ZERO_PAGE_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// LOAD ACCUMULATOR ZEROPAGE X-INDEXED ($B5 | 3 bytes | 6 cycles)
// 1-6: Zero page indexed addressing mode load value
void AlienCPU::_B5_LDA_ZeroPage_XIndexed_Instruction() {
    A = ADDRESSING_MODE_ZERO_PAGE_INDEXED_READ_TWOBYTES(X);
    UPDATE_FLAGS(A);
}


// ===================LOAD=X=REGISTER===================
// LOAD X IMMEDIATE ($A9 | 3 bytes | 3 cycles)
// 1-3: Immediate addressing mode load value
void AlienCPU::_A2_LDX_Immediate_Instruction() {
    X = ADDRESSING_MODE_IMMEDIATE_READ_TWOBYTES();
    UPDATE_FLAGS(X);
}

// LOAD X ABSOLUTE ($AD | 5 bytes | 7 cycles)
// 1-7: Absolute addressing mode load value
void AlienCPU::_AE_LDX_Absolute_Instruction() {
    X = ADDRESSING_MODE_ABSOLUTE_READ_TWOBYTES();
    UPDATE_FLAGS(X);
}

// LOAD X ABSOLUTE Y-INDEXED ($BD | 5 bytes | 7-9 cycles)
// 1-7/9: Absolute indexed addressing mode load value
void AlienCPU::_BE_LDX_Absolute_YIndexed_Instruction() {
    X = ADDRESSING_MODE_ABSOLUTE_INDEXED_READ_TWOBYTES(Y);
    UPDATE_FLAGS(X);
}

// LOAD X ZEROPAGE ($A5 | 3 bytes | 5 cycles)
// 1-5: Zero page addressing mode load value
void AlienCPU::_A6_LDX_ZeroPage_Instruction() {
    X = ADDRESSING_MODE_ZERO_PAGE_READ_TWOBYTES();
    UPDATE_FLAGS(X);
}

// LOAD X ZEROPAGE Y-INDEXED ($B5 | 3 bytes | 6 cycles)
// 1-6: Zero page indexed addressing mode load value
void AlienCPU::_B6_LDX_ZeroPage_YIndexed_Instruction() {
    X = ADDRESSING_MODE_ZERO_PAGE_INDEXED_READ_TWOBYTES(Y);
    UPDATE_FLAGS(X);
}


// ===================LOAD=Y=REGISTER===================
// LOAD Y IMMEDIATE ($A9 | 3 bytes | 3 cycles)
// 1-3: Immediate addressing mode load value
void AlienCPU::_A0_LDY_Immediate_Instruction() {
    Y = ADDRESSING_MODE_IMMEDIATE_READ_TWOBYTES();
    UPDATE_FLAGS(Y);
}

// LOAD Y ABSOLUTE ($AD | 5 bytes | 7 cycles)
// 1-7: Absolute addressing mode load value
void AlienCPU::_AC_LDY_Absolute_Instruction() {
    Y = ADDRESSING_MODE_ABSOLUTE_READ_TWOBYTES();
    UPDATE_FLAGS(Y);
}

// LOAD Y ABSOLUTE X-INDEXED ($BD | 5 bytes | 7-9 cycles)
// 1-7/9: Absolute indexed addressing mode load value
void AlienCPU::_BC_LDY_Absolute_XIndexed_Instruction() {
    Y = ADDRESSING_MODE_ABSOLUTE_INDEXED_READ_TWOBYTES(X);
    UPDATE_FLAGS(Y);
}

// LOAD Y ZEROPAGE ($A5 | 3 bytes | 5 cycles)
// 1-5: Zero page addressing mode load value
void AlienCPU::_A4_LDY_ZeroPage_Instruction() {
    Y = ADDRESSING_MODE_ZERO_PAGE_READ_TWOBYTES();
    UPDATE_FLAGS(Y);
}

// LOAD Y ZEROPAGE X-INDEXED ($B5 | 3 bytes | 6 cycles)
// 1-6: Zero page indexed addressing mode load value
void AlienCPU::_B4_LDY_ZeroPage_XIndexed_Instruction() {
    Y = ADDRESSING_MODE_ZERO_PAGE_INDEXED_READ_TWOBYTES(X);
    UPDATE_FLAGS(Y);
}


// ==================STORE=ACCUMULATOR==================
// STORE ACCUMULATOR ABSOLUTE ($8D | 5 bytes | 7 cycles)
// 1-7: Absolute addressing mode store value
void AlienCPU::_8D_STA_Absolute_Instruction() {
    ADDRESSING_MODE_ABSOLUTE_WRITE_TWOBYTES(A);
}

// STORE ACCUMULATOR ABSOLUTE X-INDEXED ($9D | 5 bytes | 7 cycles)
// 1-8: Absolute indexed addressing mode store value
void AlienCPU::_9D_STA_Absolute_XIndexed_Instruction() {
    ADDRESSING_MODE_ABSOLUTE_INDEXED_WRITE_TWOBYTES(X, A);
}

// STORE ACCUMULATOR ABSOLUTE Y-INDEXED ($99 | 5 bytes | 7 cycles)
// 1-8: Absolute indexed addressing mode store value
void AlienCPU::_99_STA_Absolute_YIndexed_Instruction() {
    ADDRESSING_MODE_ABSOLUTE_INDEXED_WRITE_TWOBYTES(Y, A);
}

// STORE ACCUMULATOR X-INDEXED INDIRECT ($81 | 3 bytes | 10 cycles)
// 1-10: X indexed indirect addressing mode store value
void AlienCPU::_81_STA_XIndexed_Indirect_Instruction() {
    ADDRESSING_MODE_XINDEXED_INDIRECT_WRITE_TWOBYTES(A);
}

// STORE ACCUMULATOR INDIRECT Y-INDEXED ($91 | 3 bytes | 10 cycles)
// 1-10: Indirect Y indexed addressing mode store value
void AlienCPU::_91_STA_Indirect_YIndexed_Instruction() {
    ADDRESSING_MODE_INDIRECT_YINDEXED_WRITE_TWOBYTES(A);
}

// STORE ACCUMULATOR ZEROPAGE ($85 | 3 bytes | 5 cycles)
// 1-5: Zero page addressing mode store value
void AlienCPU::_85_STA_ZeroPage_Instruction() {
    ADDRESSING_MODE_ZERO_PAGE_WRITE_TWOBYTES(A);
}

// STORE ACCUMULATOR ZEROPAGE X-INDEXED ($95 | 3 bytes | 6 cycles)
// 1-6: Zero page indexed addressing mode store value
void AlienCPU::_95_STA_ZeroPage_XIndexed_Instruction() {
    ADDRESSING_MODE_ZERO_PAGE_INDEXED_WRITE_TWOBYTES(X, A);
}


// ===================STORE=X=REGISTER==================
// STORE X REGISTER ABSOLUTE ($8E | 5 bytes | 7 cycles)
// 1-7: Absolute addressing mode store value
void AlienCPU::_8E_STX_Absolute_Instruction() {
    ADDRESSING_MODE_ABSOLUTE_WRITE_TWOBYTES(X);
}

// STORE X REGISTER ZEROPAGE ($86 | 3 bytes | 5 cycles)
// 1-5: Zero page addressing mode store value
void AlienCPU::_86_STX_ZeroPage_Instruction() {
    ADDRESSING_MODE_ZERO_PAGE_WRITE_TWOBYTES(X);
}

// STORE X REGISTER ZEROPAGE Y-INDEXED ($96 | 3 bytes | 6 cycles)
// 1-6: Zero page indexed addressing mode store value
void AlienCPU::_96_STX_ZeroPage_YIndexed_Instruction() {
    ADDRESSING_MODE_ZERO_PAGE_INDEXED_WRITE_TWOBYTES(Y, X);
}


// ===================STORE=Y=REGISTER==================
// STORE Y REGISTER ABSOLUTE ($8C | 5 bytes | 7 cycles)
// 1-7: Absolute addressing mode store value
void AlienCPU::_8C_STY_Absolute_Instruction() {
    ADDRESSING_MODE_ABSOLUTE_WRITE_TWOBYTES(Y);
}

// STORE Y REGISTER ZEROPAGE ($84 | 3 bytes | 3 cycles)
// 1-3: Zero page addressing mode store value
void AlienCPU::_84_STY_ZeroPage_Instruction() {
    ADDRESSING_MODE_ZERO_PAGE_WRITE_TWOBYTES(Y);
}

// STORE Y REGISTER ZEROPAGE X-INDEXED ($94 | 3 bytes | 4 cycles)
// 1-4: Zero page indexed addressing mode store value
void AlienCPU::_94_STY_ZeroPage_XIndexed_Instruction() {
    ADDRESSING_MODE_ZERO_PAGE_INDEXED_WRITE_TWOBYTES(X, Y);
}


// =========TRANSFER=ACCUMULATOR=TO=X=REGISTER==========
// 1-2: Implied addressing mode
void AlienCPU::_AA_TAX_Implied_Instruction() {
    ADDRESSING_MODE_IMPLIED();
    X = A;
}


// =========TRANSFER=ACCUMULATOR=TO=Y=REGISTER==========
// 1-2: Implied addressing mode
void AlienCPU::_A8_TAY_Implied_Instruction() {
    ADDRESSING_MODE_IMPLIED();
    Y = A;
}


// ========TRANSFER=STACK=POINTER=TO=X=REGISTER=========
// 1-2: Implied addressing mode
void AlienCPU::_BA_TSX_Implied_Instruction() {
    ADDRESSING_MODE_IMPLIED();
    X = SP;
}


// =========TRANSFER=X=REGISTER=TO=ACCUMULATOR==========
// 1-2: Implied addressing mode
void AlienCPU::_8A_TXA_Implied_Instruction() {
    ADDRESSING_MODE_IMPLIED();
    A = X;
}


// ========TRANSFER=X=REGISTER=TO=STACK=POINTER=========
// 1-2: Implied addressing mode
void AlienCPU::_9A_TXS_Implied_Instruction() {
    ADDRESSING_MODE_IMPLIED();
    SP = X;
}


// =========TRANSFER=Y=REGISTER=TO=ACCUMULATOR==========
// 1-2: Implied addressing mode
void AlienCPU::_98_TYA_Implied_Instruction() {
    ADDRESSING_MODE_IMPLIED();
    A = Y;
}


// ========================STACK=========================
// ===================PUSH=ACCUMULATOR===================
// 1-2: Implied addressing mode (useless read so the high byte of accumulator can be written to stack)
// 3: useless read so the low byte of accumulator can be written to stack
void AlienCPU::_48_PHA_Implied_Instruction() {
    ADDRESSING_MODE_IMPLIED();
    pushTwoBytesToStack(A);
    cycles++;
}


// =================PUSH=PROCESSOR=STATUS================
// 1-2: Implied addressing mode (useless read so the processor status can be written to stack)
void AlienCPU::_08_PHP_Implied_Instruction() {
    ADDRESSING_MODE_IMPLIED();
    pushByteToStack(P);
}


// ===================POP=ACCUMULATOR====================
// 1-2: Implied addressing mode (read low byte of accumulator from stack)
// 3: read high byte of accumulator from stack
void AlienCPU::_68_PLA_Implied_Instruction() {
    ADDRESSING_MODE_IMPLIED();
    A = popTwoBytesFromStack();
    cycles++;
}


// =================POP=PROCESSOR=STATUS=================
// 1-2: Implied addressing mode (read processor status from stack)
void AlienCPU::_28_PLP_Implied_Instruction() {
    ADDRESSING_MODE_IMPLIED();
    P = popByteFromStack();
}


// ================DECREMENTS=&=INCREMENTS===============
// ===================DECREMENT=MEMORY===================
void AlienCPU::_CE_DEC_Absolute_Instruction() {

}

void AlienCPU::_DE_DEC_Absolute_XIndexed_Instruction() {

}

void AlienCPU::_C6_DEC_ZeroPage_Instruction() {

}

void AlienCPU::_D6_DEC_ZeroPage_XIndexed_Instruction() {

}


// =================DECREMENT=X=REGISTER=================
void AlienCPU::_CA_DEX_Implied_Instruction() {

}


// =================DECREMENT=Y=REGISTER=================
void AlienCPU::_88_DEY_Implied_Instruction() {

}


// ===================INCREMENT=MEMORY===================
void AlienCPU::_EE_INC_Absolute_Instruction() {

}

void AlienCPU::_FE_INC_Absolute_XIndexed_Instruction() {

}

void AlienCPU::_E6_INC_ZeroPage_Instruction() {

}

void AlienCPU::_F6_INC_ZeroPage_XIndexed_Instruction() {

}


// =================INCREMENT=X=REGISTER=================
void AlienCPU::_E8_INX_Implied_Instruction() {

}


// =================INCREMENT=Y=REGISTER=================
void AlienCPU::_C8_INY_Implied_Instruction() {

}


// =================ARITHMETIC=OPERATIONS================
// =====================ADD=WITH=CARRY===================
void AlienCPU::_69_ADC_Immediate_Instruction() {

}

void AlienCPU::_6D_ADC_Absolute_Instruction() {

}

void AlienCPU::_7D_ADC_Absolute_XIndexed_Instruction() {

}

void AlienCPU::_79_ADC_Absolute_YIndexed_Instruction() {

}

void AlienCPU::_61_ADC_XIndexed_Indirect_Instruction() {

}

void AlienCPU::_71_ADC_Indirect_YIndexed_Instruction() {

}

void AlienCPU::_65_ADC_ZeroPage_Instruction() {

}

void AlienCPU::_75_ADC_ZeroPage_XIndexed_Instruction() {

}


// =====================SUBTRACT=WITH=BORROW=============
void AlienCPU::_E9_SBC_Immediate_Instruction() {

}

void AlienCPU::_ED_SBC_Absolute_Instruction() {

}

void AlienCPU::_FD_SBC_Absolute_XIndexed_Instruction() {

}

void AlienCPU::_F9_SBC_Absolute_YIndexed_Instruction() {

}

void AlienCPU::_E1_SBC_XIndexed_Indirect_Instruction() {

}

void AlienCPU::_F1_SBC_Indirect_YIndexed_Instruction() {

}

void AlienCPU::_E5_SBC_ZeroPage_Instruction() {

}

void AlienCPU::_F5_SBC_ZeroPage_XIndexed_Instruction() {

}


// ==================LOGICAL=OPERATIONS==================
// =====================AND=WITH=ACCUMULATOR==============
void AlienCPU::_29_AND_Immediate_Instruction() {

}

void AlienCPU::_2D_AND_Absolute_Instruction() {

}

void AlienCPU::_3D_AND_Absolute_XIndexed_Instruction() {

}

void AlienCPU::_39_AND_Absolute_YIndexed_Instruction() {

}

void AlienCPU::_21_AND_XIndexed_Indirect_Instruction() {

}

void AlienCPU::_31_AND_Indirect_YIndexed_Instruction() {

}

void AlienCPU::_25_AND_ZeroPage_Instruction() {

}

void AlienCPU::_35_AND_ZeroPage_XIndexed_Instruction() {

}


// =====================EOR=WITH=ACCUMULATOR==============
void AlienCPU::_49_EOR_Immediate_Instruction() {

}

void AlienCPU::_4D_EOR_Absolute_Instruction() {

}

void AlienCPU::_5D_EOR_Absolute_XIndexed_Instruction() {

}

void AlienCPU::_59_EOR_Absolute_YIndexed_Instruction() {

}

void AlienCPU::_41_EOR_XIndexed_Indirect_Instruction() {

}

void AlienCPU::_51_EOR_Indirect_YIndexed_Instruction() {

}

void AlienCPU::_45_EOR_ZeroPage_Instruction() {

}

void AlienCPU::_55_EOR_ZeroPage_XIndexed_Instruction() {

}


// =====================ORA=WITH=ACCUMULATOR==============
void AlienCPU::_09_ORA_Immediate_Instruction() {

}

void AlienCPU::_0D_ORA_Absolute_Instruction() {

}

void AlienCPU::_19_ORA_Absolute_YIndexed_Instruction() {

}

void AlienCPU::_1D_ORA_Absolute_XIndexed_Instruction() {

}

void AlienCPU::_01_ORA_XIndexed_Indirect_Instruction() {

}

void AlienCPU::_11_ORA_Indirect_YIndexed_Instruction() {

}

void AlienCPU::_05_ORA_ZeroPage_Instruction() {

}

void AlienCPU::_15_ORA_ZeroPage_XIndexed_Instruction() {

}


// ====================SHIFT=&=ROTATE====================
// =====================ARITHMETIC=SHIFT==================
void AlienCPU::_0A_ASL_Accumulator_Instruction() {

}

void AlienCPU::_0E_ASL_Absolute_Instruction() {

}

void AlienCPU::_1E_ASL_Absolute_XIndexed_Instruction() {

}

void AlienCPU::_06_ASL_ZeroPage_Instruction() {

}

void AlienCPU::_16_ASL_ZeroPage_XIndexed_Instruction() {

}


// =====================LOGICAL=SHIFT=====================
void AlienCPU::_4A_LSR_Accumulator_Instruction() {

}

void AlienCPU::_4E_LSR_Absolute_Instruction() {

}

void AlienCPU::_5E_LSR_Absolute_XIndexed_Instruction() {

}

void AlienCPU::_46_LSR_ZeroPage_Instruction() {

}

void AlienCPU::_56_LSR_ZeroPage_XIndexed_Instruction() {

}


// =====================ROTATE=LEFT=======================
void AlienCPU::_2A_ROL_Accumulator_Instruction() {

}

void AlienCPU::_2E_ROL_Absolute_Instruction() {

}

void AlienCPU::_3E_ROL_Absolute_XIndexed_Instruction() {

}

void AlienCPU::_26_ROL_ZeroPage_Instruction() {

}

void AlienCPU::_36_ROL_ZeroPage_XIndexed_Instruction() {

}


// =====================ROTATE=RIGHT======================
void AlienCPU::_6A_ROR_Accumulator_Instruction() {

}

void AlienCPU::_6E_ROR_Absolute_Instruction() {

}

void AlienCPU::_7E_ROR_Absolute_XIndexed_Instruction() {

}

void AlienCPU::_66_ROR_ZeroPage_Instruction() {

}

void AlienCPU::_76_ROR_ZeroPage_XIndexed_Instruction() {

}


// =========================FLAG==========================
void AlienCPU::_18_CLC_Implied_Instruction() {

}

void AlienCPU::_D8_CLD_Implied_Instruction() {

}

void AlienCPU::_58_CLI_Implied_Instruction() {

}

void AlienCPU::_B8_CLV_Implied_Instruction() {

}

void AlienCPU::_38_SEC_Implied_Instruction() {

}

void AlienCPU::_F8_SED_Implied_Instruction() {

}

void AlienCPU::_78_SEI_Implied_Instruction() {

}


// =====================COMPARISONS======================
// =====================COMPARE=ACCUMULATOR==============
void AlienCPU::_C9_CMP_Immediate_Instruction() {

}

void AlienCPU::_CD_CMP_Absolute_Instruction() {

}

void AlienCPU::_DD_CMP_Absolute_XIndexed_Instruction() {

}

void AlienCPU::_D9_CMP_Absolute_YIndexed_Instruction() {

}

void AlienCPU::_C1_CMP_XIndexed_Indirect_Instruction() {

}

void AlienCPU::_D1_CMP_Indirect_YIndexed_Instruction() {

}

void AlienCPU::_C5_CMP_ZeroPage_Instruction() {

}

void AlienCPU::_D5_CMP_ZeroPage_XIndexed_Instruction() {

}


// =====================COMPARE=X=REGISTER==============
void AlienCPU::_E0_CPX_Immediate_Instruction() {

}

void AlienCPU::_EC_CPX_Absolute_Instruction() {

}

void AlienCPU::_E4_CPX_ZeroPage_Instruction() {

}


// =====================COMPARE=Y=REGISTER==============
void AlienCPU::_C0_CPY_Immediate_Instruction() {

}

void AlienCPU::_CC_CPY_Absolute_Instruction() {

}

void AlienCPU::_C4_CPY_ZeroPage_Instruction() {

}


// ==================CONDITIONAL=BRANCH==================
void AlienCPU::_90_BCC_Relative_Instruction() {

}

void AlienCPU::_B0_BCS_Relative_Instruction() {

}

void AlienCPU::_F0_BEQ_Relative_Instruction() {

}

void AlienCPU::_10_BPL_Relative_Instruction() {

}

void AlienCPU::_30_BMI_Relative_Instruction() {

}

void AlienCPU::_D0_BNE_Relative_Instruction() {

}

void AlienCPU::_50_BVC_Relative_Instruction() {

}

void AlienCPU::_70_BVS_Relative_Instruction() {

}


// ==================JUMPS=&=SUBROUTINES=================
void AlienCPU::_4C_JMP_Absolute_Instruction() {

}

void AlienCPU::_6C_JMP_Indirect_Instruction() {

}

void AlienCPU::_20_JSR_Absolute_Instruction() {

}

void AlienCPU::_60_RTS_Implied_Instruction() {

}


// ====================INTERRUPTS========================
void AlienCPU::_02_BRK_Implied_Instruction() {

}

void AlienCPU::_40_RTI_Implied_Instruction() {

}


// =========================OTHER=========================
// =======================BIT=TEST========================
void AlienCPU::_24_BIT_ZeroPage_Instruction() {

}

void AlienCPU::_2C_BIT_Absolute_Instruction() {

}

// ====================NULL=INSTRUCTION===================
// Null Instruction, throws error if called
void AlienCPU::_00_NULL_Illegal_Instruction() {
    std::stringstream stream;
    stream << std::endl << "Error: NULL Instruction" << std::endl;

    throw std::invalid_argument(stream.str());
}


// =====================NO=OPERATION======================
void AlienCPU::_04_NOP_ZeroPage_Illegal_Instruction() {

}

void AlienCPU::_0C_NOP_Absolute_Illegal_Instruction() {

}

void AlienCPU::_14_NOP_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_1A_NOP_Implied_Illegal_Instruction() {

}

void AlienCPU::_1C_NOP_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_34_NOP_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_3A_NOP_Implied_Illegal_Instruction() {

}

void AlienCPU::_3C_NOP_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_44_NOP_ZeroPage_Illegal_Instruction() {

}

void AlienCPU::_54_NOP_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_5A_NOP_Implied_Illegal_Instruction() {

}

void AlienCPU::_5C_NOP_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_64_NOP_ZeroPage_Illegal_Instruction() {

}

void AlienCPU::_74_NOP_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_7A_NOP_Implied_Illegal_Instruction() {

}

void AlienCPU::_7C_NOP_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_80_NOP_Immediate_Illegal_Instruction() {

}

void AlienCPU::_82_NOP_Immediate_Illegal_Instruction() {

}

void AlienCPU::_89_NOP_Immediate_Illegal_Instruction() {

}

void AlienCPU::_C2_NOP_Immediate_Illegal_Instruction() {

}

void AlienCPU::_D4_NOP_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_DA_NOP_Implied_Illegal_Instruction() {

}

void AlienCPU::_DC_NOP_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_E2_NOP_Immediate_Illegal_Instruction() {

}

void AlienCPU::_EA_NOP_Implied_Instruction() {

}

void AlienCPU::_F4_NOP_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_FA_NOP_Implied_Illegal_Instruction() {

}

void AlienCPU::_FC_NOP_Absolute_XIndexed_Illegal_Instruction() {

}


// ========================ILLEGAL========================
void AlienCPU::_4B_ALR_Immediate_Illegal_Instruction() {

}

void AlienCPU::_0B_ANC_Immediate_Illegal_Instruction() {

}

void AlienCPU::_2B_ANC_Immediate_Illegal_Instruction() { // ANC 2

}

void AlienCPU::_8B_ANE_Immediate_Illegal_Instruction() {

}

void AlienCPU::_6B_ARR_Immediate_Illegal_Instruction() {

}

void AlienCPU::_C3_DCP_XIndexed_Indirect_Illegal_Instruction() {

}

void AlienCPU::_C7_DCP_ZeroPage_Illegal_Instruction() {

}

void AlienCPU::_CF_DCP_Absolute_Illegal_Instruction() {

}

void AlienCPU::_D3_DCP_Indirect_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_D7_DCP_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_DB_DCP_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_DF_DCP_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_E3_ISC_XIndexed_Indirect_Illegal_Instruction() {

}

void AlienCPU::_E7_ISC_ZeroPage_Illegal_Instruction() {

}

void AlienCPU::_EF_ISC_Absolute_Illegal_Instruction() {

}

void AlienCPU::_F3_ISC_Indirect_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_F7_ISC_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_FB_ISC_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_FF_ISC_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_BB_LAS_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_A3_LAX_XIndexed_Indirect_Illegal_Instruction() {

}

void AlienCPU::_A7_LAX_ZeroPage_Illegal_Instruction() {

}

void AlienCPU::_AF_LAX_Absolute_Illegal_Instruction() {

}

void AlienCPU::_B3_LAX_Indirect_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_B7_LAX_ZeroPage_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_BF_LAX_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_AB_LXA_Immediate_Illegal_Instruction() {

}


// ========================JAMS===========================
// Freezes the CPU indefinitely in T1 phase with $FFFF on the data bus, requires reset
// https://www.nesdev.org/wiki/Visual6502wiki/6502_Timing_States (for T- phases)
void AlienCPU::_12_JAM_Illegal_Instruction() {

}

void AlienCPU::_22_JAM_Illegal_Instruction() {

}

void AlienCPU::_32_JAM_Illegal_Instruction() {

}

void AlienCPU::_42_JAM_Illegal_Instruction() {

}

void AlienCPU::_52_JAM_Illegal_Instruction() {

}

void AlienCPU::_62_JAM_Illegal_Instruction() {

}

void AlienCPU::_72_JAM_Illegal_Instruction() {

}

void AlienCPU::_92_JAM_Illegal_Instruction() {

}

void AlienCPU::_B2_JAM_Illegal_Instruction() {

}

void AlienCPU::_D2_JAM_Illegal_Instruction() {

}

void AlienCPU::_F2_JAM_Illegal_Instruction() {

}

void AlienCPU::_23_RLA_XIndexed_Indirect_Illegal_Instruction() {

}

void AlienCPU::_27_RLA_ZeroPage_Illegal_Instruction() {

}

void AlienCPU::_2F_RLA_Absolute_Illegal_Instruction() {

}

void AlienCPU::_33_RLA_Indirect_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_37_RLA_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_3B_RLA_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_3F_RLA_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_63_RRA_XIndexed_Indirect_Illegal_Instruction() {

}

void AlienCPU::_67_RRA_ZeroPage_Illegal_Instruction() {

}

void AlienCPU::_6F_RRA_Absolute_Illegal_Instruction() {

}

void AlienCPU::_73_RRA_Indirect_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_77_RRA_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_7B_RRA_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_7F_RRA_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_83_SAX_XIndexed_Indirect_Illegal_Instruction() {

}

void AlienCPU::_87_SAX_ZeroPage_Illegal_Instruction() {

}

void AlienCPU::_8F_SAX_Absolute_Illegal_Instruction() {

}

void AlienCPU::_97_SAX_ZeroPage_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_CB_SBX_Immediate_Illegal_Instruction() {

}

void AlienCPU::_93_SHA_Indirect_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_9F_SHA_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_9E_SHX_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_9C_SHY_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_03_SLO_XIndexed_Indirect_Illegal_Instruction() {

}

void AlienCPU::_07_SLO_ZeroPage_Illegal_Instruction() {

}

void AlienCPU::_0F_SLO_Absolute_Illegal_Instruction() {

}

void AlienCPU::_13_SLO_Indirect_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_17_SLO_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_1B_SLO_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_1F_SLO_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_43_SRE_XIndexed_Indirect_Illegal_Instruction() {

}

void AlienCPU::_47_SRE_ZeroPage_Illegal_Instruction() {

}

void AlienCPU::_4F_SRE_Absolute_Illegal_Instruction() {

}

void AlienCPU::_53_SRE_Indirect_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_57_SRE_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU::_5B_SRE_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_5F_SRE_Absolute_XIndexed_Illegal_Instruction() {

}


void AlienCPU::_9B_TAS_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU::_EB_USBC_Immediate_Illegal_Instruction() {

}





void AlienCPU::initInstructions() {
    // null out instructions to catch errors
    for (int i = 0; i < INSTRUCTION_COUNT; i++) {
        instructions[i] = _00_NULL_Illegal_Instruction;
    }

    instructions[INS_NULL] = _00_NULL_Illegal_Instruction;
    instructions[INS_ORA_X_IND] = _01_ORA_XIndexed_Indirect_Instruction;
    instructions[INS_BRK_IMPL] = _02_BRK_Implied_Instruction;
    instructions[INS_SLO_X_IND] = _03_SLO_XIndexed_Indirect_Illegal_Instruction;

    instructions[INS_NOP_ZP] = _04_NOP_ZeroPage_Illegal_Instruction;
    instructions[INS_ORA_ZP] = _05_ORA_ZeroPage_Instruction;
    instructions[INS_ASL_ZP] = _06_ASL_ZeroPage_Instruction;
    instructions[INS_SLO_ZP] = _07_SLO_ZeroPage_Illegal_Instruction;
    instructions[INS_PHP_IMPL] = _08_PHP_Implied_Instruction;
    instructions[INS_ORA_IMM] = _09_ORA_Immediate_Instruction;
    instructions[INS_ASL_ACC] = _0A_ASL_Accumulator_Instruction;
    instructions[INS_ANC_IMM] = _0B_ANC_Immediate_Illegal_Instruction;
    instructions[INS_NOP_ABS] = _0C_NOP_Absolute_Illegal_Instruction;
    instructions[INS_ORA_ABS] = _0D_ORA_Absolute_Instruction;
    instructions[INS_ASL_ABS] = _0E_ASL_Absolute_Instruction;
    instructions[INS_SLO_ABS] = _0F_SLO_Absolute_Illegal_Instruction;
    instructions[INS_BPL_REL] = _10_BPL_Relative_Instruction;
    instructions[INS_ORA_IND_Y] = _11_ORA_Indirect_YIndexed_Instruction;
    instructions[INS_JAM_1] = _12_JAM_Illegal_Instruction;
    instructions[INS_SLO_IND_Y] = _13_SLO_Indirect_YIndexed_Illegal_Instruction;
    instructions[INS_NOP_ZP_X] = _14_NOP_ZeroPage_XIndexed_Illegal_Instruction;
    instructions[INS_ORA_ZP_X] = _15_ORA_ZeroPage_XIndexed_Instruction;
    instructions[INS_ASL_ZP_X] = _16_ASL_ZeroPage_XIndexed_Instruction;
    instructions[INS_SLO_ZP_X] = _17_SLO_ZeroPage_XIndexed_Illegal_Instruction;
    instructions[INS_CLC_IMPL] = _18_CLC_Implied_Instruction;
    instructions[INS_ORA_ABS_Y] = _19_ORA_Absolute_YIndexed_Instruction;
    instructions[INS_NOP_IMPL] = _1A_NOP_Implied_Illegal_Instruction;
    instructions[INS_SLO_ABS_Y] = _1B_SLO_Absolute_YIndexed_Illegal_Instruction;
    instructions[INS_NOP_ABS_X] = _1C_NOP_Absolute_XIndexed_Illegal_Instruction;
    instructions[INS_ORA_ABS_X] = _1D_ORA_Absolute_XIndexed_Instruction;
    instructions[INS_ASL_ABS_X] = _1E_ASL_Absolute_XIndexed_Instruction;
    instructions[INS_SLO_ABS_X] = _1F_SLO_Absolute_XIndexed_Illegal_Instruction;
    instructions[INS_JSR_ABS] = _20_JSR_Absolute_Instruction;
    instructions[INS_AND_X_IND] = _21_AND_XIndexed_Indirect_Instruction;
    instructions[INS_JAM_2] = _22_JAM_Illegal_Instruction;
    instructions[INS_RLA_X_IND] = _23_RLA_XIndexed_Indirect_Illegal_Instruction;
    instructions[INS_BIT_ZP] = _24_BIT_ZeroPage_Instruction;
    instructions[INS_AND_ZP] = _25_AND_ZeroPage_Instruction;
    instructions[INS_ROL_ZP] = _26_ROL_ZeroPage_Instruction;
    instructions[INS_RLA_ZP] = _27_RLA_ZeroPage_Illegal_Instruction;
    instructions[INS_PLP_IMPL] = _28_PLP_Implied_Instruction;
    instructions[INS_AND_IMM] = _29_AND_Immediate_Instruction;
    instructions[INS_ROL_ACC] = _2A_ROL_Accumulator_Instruction;
    instructions[INS_ANC_IMM_2] = _2B_ANC_Immediate_Illegal_Instruction;
    instructions[INS_BIT_ABS] = _2C_BIT_Absolute_Instruction;
    instructions[INS_AND_ABS] = _2D_AND_Absolute_Instruction;
    instructions[INS_ROL_ABS] = _2E_ROL_Absolute_Instruction;
    instructions[INS_RLA_ABS] = _2F_RLA_Absolute_Illegal_Instruction;
    instructions[INS_BMI_REL] = _30_BMI_Relative_Instruction;
    instructions[INS_AND_IND_Y] = _31_AND_Indirect_YIndexed_Instruction;
    instructions[INS_JAM_3] = _32_JAM_Illegal_Instruction;
    instructions[INS_RLA_IND_Y] = _33_RLA_Indirect_YIndexed_Illegal_Instruction;
    instructions[INS_NOP_ZP_X_2] = _34_NOP_ZeroPage_XIndexed_Illegal_Instruction;
    instructions[INS_AND_ZP_X] = _35_AND_ZeroPage_XIndexed_Instruction;
    instructions[INS_ROL_ZP_X] = _36_ROL_ZeroPage_XIndexed_Instruction;
    instructions[INS_RLA_ZP_X] = _37_RLA_ZeroPage_XIndexed_Illegal_Instruction;
    instructions[INS_SEC_IMPL] = _38_SEC_Implied_Instruction;
    instructions[INS_AND_ABS_Y] = _39_AND_Absolute_YIndexed_Instruction;
    instructions[INS_NOP_IMPL_2] = _3A_NOP_Implied_Illegal_Instruction;
    instructions[INS_RLA_ABS_Y] = _3B_RLA_Absolute_YIndexed_Illegal_Instruction;
    instructions[INS_NOP_ABS_X_2] = _3C_NOP_Absolute_XIndexed_Illegal_Instruction;
    instructions[INS_AND_ABS_X] = _3D_AND_Absolute_XIndexed_Instruction;
    instructions[INS_ROL_ABS_X] = _3E_ROL_Absolute_XIndexed_Instruction;
    instructions[INS_RLA_ABS_X] = _3F_RLA_Absolute_XIndexed_Illegal_Instruction;
    instructions[INS_RTI_IMPL] = _40_RTI_Implied_Instruction;
    instructions[INS_EOR_X_IND] = _41_EOR_XIndexed_Indirect_Instruction;
    instructions[INS_JAM_4] = _42_JAM_Illegal_Instruction;
    instructions[INS_SRE_X_IND] = _43_SRE_XIndexed_Indirect_Illegal_Instruction;
    instructions[INS_NOP_ZP_2] = _44_NOP_ZeroPage_Illegal_Instruction;
    instructions[INS_EOR_ZP] = _45_EOR_ZeroPage_Instruction;
    instructions[INS_LSR_ZP] = _46_LSR_ZeroPage_Instruction;
    instructions[INS_SRE_ZP] = _47_SRE_ZeroPage_Illegal_Instruction;
    instructions[INS_PHA_IMPL] = _48_PHA_Implied_Instruction;
    instructions[INS_EOR_IMM] = _49_EOR_Immediate_Instruction;
    instructions[INS_LSR_ACC] = _4A_LSR_Accumulator_Instruction;
    instructions[INS_ALR_IMM] = _4B_ALR_Immediate_Illegal_Instruction;
    instructions[INS_JMP_ABS] = _4C_JMP_Absolute_Instruction;
    instructions[INS_EOR_ABS] = _4D_EOR_Absolute_Instruction;
    instructions[INS_LSR_ABS] = _4E_LSR_Absolute_Instruction;
    instructions[INS_SRE_ABS] = _4F_SRE_Absolute_Illegal_Instruction;
    instructions[INS_BVC_REL] = _50_BVC_Relative_Instruction;
    instructions[INS_EOR_IND_Y] = _51_EOR_Indirect_YIndexed_Instruction;
    instructions[INS_JAM_5] = _52_JAM_Illegal_Instruction;
    instructions[INS_SRE_IND_Y] = _53_SRE_Indirect_YIndexed_Illegal_Instruction;
    instructions[INS_NOP_ZP_X_3] = _54_NOP_ZeroPage_XIndexed_Illegal_Instruction;
    instructions[INS_EOR_ZP_X] = _55_EOR_ZeroPage_XIndexed_Instruction;
    instructions[INS_LSR_ZP_X] = _56_LSR_ZeroPage_XIndexed_Instruction;
    instructions[INS_SRE_ZP_X] = _57_SRE_ZeroPage_XIndexed_Illegal_Instruction;                 
    instructions[INS_CLI_IMPL] = _58_CLI_Implied_Instruction;
    instructions[INS_EOR_ABS_Y] = _59_EOR_Absolute_YIndexed_Instruction;
    instructions[INS_NOP_IMPL_3] = _5A_NOP_Implied_Illegal_Instruction;
    instructions[INS_SRE_ABS_Y] = _5B_SRE_Absolute_YIndexed_Illegal_Instruction;
    instructions[INS_NOP_ABS_X_3] = _5C_NOP_Absolute_XIndexed_Illegal_Instruction;
    instructions[INS_EOR_ABS_X] = _5D_EOR_Absolute_XIndexed_Instruction;
    instructions[INS_LSR_ABS_X] = _5E_LSR_Absolute_XIndexed_Instruction;
    instructions[INS_SRE_ABS_X] = _5F_SRE_Absolute_XIndexed_Illegal_Instruction;
    instructions[INS_RTS_IMPL] = _60_RTS_Implied_Instruction;
    instructions[INS_ADC_X_IND] = _61_ADC_XIndexed_Indirect_Instruction;
    instructions[INS_JAM_6] = _62_JAM_Illegal_Instruction;
    instructions[INS_RRA_X_IND] = _63_RRA_XIndexed_Indirect_Illegal_Instruction;
    instructions[INS_NOP_ZP_4] = _64_NOP_ZeroPage_Illegal_Instruction;
    instructions[INS_ADC_ZP] = _65_ADC_ZeroPage_Instruction;
    instructions[INS_ROR_ZP] = _66_ROR_ZeroPage_Instruction;
    instructions[INS_RRA_ZP] = _67_RRA_ZeroPage_Illegal_Instruction;
    instructions[INS_PLA_IMPL] = _68_PLA_Implied_Instruction;
    instructions[INS_ADC_IMM] = _69_ADC_Immediate_Instruction;
    instructions[INS_ROR_ACC] = _6A_ROR_Accumulator_Instruction;
    instructions[INS_ARR_IMM] = _6B_ARR_Immediate_Illegal_Instruction;
    instructions[INS_JMP_IND] = _6C_JMP_Indirect_Instruction;
    instructions[INS_ADC_ABS] = _6D_ADC_Absolute_Instruction;
    instructions[INS_ROR_ABS] = _6E_ROR_Absolute_Instruction;
    instructions[INS_RRA_ABS] = _6F_RRA_Absolute_Illegal_Instruction;
    instructions[INS_BVS_REL] = _70_BVS_Relative_Instruction;
    instructions[INS_ADC_IND_Y] = _71_ADC_Indirect_YIndexed_Instruction;
    instructions[INS_JAM_7] = _72_JAM_Illegal_Instruction;
    instructions[INS_RRA_IND_Y] = _73_RRA_Indirect_YIndexed_Illegal_Instruction;
    instructions[INS_NOP_ZP_X_4] = _74_NOP_ZeroPage_XIndexed_Illegal_Instruction;
    instructions[INS_ADC_ZP_X] = _75_ADC_ZeroPage_XIndexed_Instruction;
    instructions[INS_ROR_ZP_X] = _76_ROR_ZeroPage_XIndexed_Instruction;
    instructions[INS_RRA_ZP_X] = _77_RRA_ZeroPage_XIndexed_Illegal_Instruction;
    instructions[INS_SEI_IMPL] = _78_SEI_Implied_Instruction;
    instructions[INS_ADC_ABS_Y] = _79_ADC_Absolute_YIndexed_Instruction;
    instructions[INS_NOP_IMPL_4] = _7A_NOP_Implied_Illegal_Instruction;
    instructions[INS_RRA_ABS_Y] = _7B_RRA_Absolute_YIndexed_Illegal_Instruction;
    instructions[INS_NOP_ABS_X_4] = _7C_NOP_Absolute_XIndexed_Illegal_Instruction;
    instructions[INS_ADC_ABS_X] = _7D_ADC_Absolute_XIndexed_Instruction;
    instructions[INS_ROR_ABS_X] = _7E_ROR_Absolute_XIndexed_Instruction;
    instructions[INS_RRA_ABS_X] = _7F_RRA_Absolute_XIndexed_Illegal_Instruction;
    instructions[INS_NOP_IMM] = _80_NOP_Immediate_Illegal_Instruction;
    instructions[INS_STA_X_IND] = _81_STA_XIndexed_Indirect_Instruction;
    instructions[INS_NOP_IMM_2] = _82_NOP_Immediate_Illegal_Instruction;
    instructions[INS_SAX_X_IND] = _83_SAX_XIndexed_Indirect_Illegal_Instruction;
    instructions[INS_STY_ZP] = _84_STY_ZeroPage_Instruction;
    instructions[INS_STA_ZP] = _85_STA_ZeroPage_Instruction;
    instructions[INS_STX_ZP] = _86_STX_ZeroPage_Instruction;
    instructions[INS_SAX_ZP] = _87_SAX_ZeroPage_Illegal_Instruction;
    instructions[INS_DEY_IMPL] = _88_DEY_Implied_Instruction;
    instructions[INS_NOP_IMM_3] = _89_NOP_Immediate_Illegal_Instruction;
    instructions[INS_TXA_IMPL] = _8A_TXA_Implied_Instruction;
    instructions[INS_ANE_IMM] = _8B_ANE_Immediate_Illegal_Instruction;
    instructions[INS_STY_ABS] = _8C_STY_Absolute_Instruction;
    instructions[INS_STA_ABS] = _8D_STA_Absolute_Instruction;
    instructions[INS_STX_ABS] = _8E_STX_Absolute_Instruction;
    instructions[INS_SAX_ABS] = _8F_SAX_Absolute_Illegal_Instruction;
    instructions[INS_BCC_REL] = _90_BCC_Relative_Instruction;
    instructions[INS_STA_IND_Y] = _91_STA_Indirect_YIndexed_Instruction;
    instructions[INS_JAM_8] = _92_JAM_Illegal_Instruction;
    instructions[INS_SHA_IND_Y] = _93_SHA_Indirect_YIndexed_Illegal_Instruction;
    instructions[INS_STY_ZP_X] = _94_STY_ZeroPage_XIndexed_Instruction;
    instructions[INS_STA_ZP_X] = _95_STA_ZeroPage_XIndexed_Instruction;
    instructions[INS_STX_ZP_Y] = _96_STX_ZeroPage_YIndexed_Instruction;
    instructions[INS_SAX_ZP_Y] = _97_SAX_ZeroPage_YIndexed_Illegal_Instruction;
    instructions[INS_TYA_IMPL] = _98_TYA_Implied_Instruction;
    instructions[INS_STA_ABS_Y] = _99_STA_Absolute_YIndexed_Instruction;
    instructions[INS_TXS_IMPL] = _9A_TXS_Implied_Instruction;
    instructions[INS_TAS_ABS_Y] = _9B_TAS_Absolute_YIndexed_Illegal_Instruction;
    instructions[INS_SHY_ABS_X] = _9C_SHY_Absolute_XIndexed_Illegal_Instruction;
    instructions[INS_STA_ABS_X] = _9D_STA_Absolute_XIndexed_Instruction;
    instructions[INS_SHX_ABS_Y] = _9E_SHX_Absolute_YIndexed_Illegal_Instruction;
    instructions[INS_SHA_ABS_Y] = _9F_SHA_Absolute_YIndexed_Illegal_Instruction;
    instructions[INS_LDY_IMM] = _A0_LDY_Immediate_Instruction;
    instructions[INS_LDA_X_IND] = _A1_LDA_XIndexed_Indirect_Instruction;
    instructions[INS_LDX_IMM] = _A2_LDX_Immediate_Instruction;
    instructions[INS_LAX_X_IND] = _A3_LAX_XIndexed_Indirect_Illegal_Instruction;
    instructions[INS_LDY_ZP] = _A4_LDY_ZeroPage_Instruction;
    instructions[INS_LDA_ZP] = _A5_LDA_ZeroPage_Instruction;
    instructions[INS_LDX_ZP] = _A6_LDX_ZeroPage_Instruction;
    instructions[INS_LAX_ZP] = _A7_LAX_ZeroPage_Illegal_Instruction;
    instructions[INS_TAY_IMPL] = _A8_TAY_Implied_Instruction;
    instructions[INS_LDA_IMM] = _A9_LDA_Immediate_Instruction;
    instructions[INS_TAX_IMPL] = _AA_TAX_Implied_Instruction;
    instructions[INS_LXA_IMM] = _AB_LXA_Immediate_Illegal_Instruction;
    instructions[INS_LDY_ABS] = _AC_LDY_Absolute_Instruction;
    instructions[INS_LDA_ABS] = _AD_LDA_Absolute_Instruction;
    instructions[INS_LDX_ABS] = _AE_LDX_Absolute_Instruction;
    instructions[INS_LAX_ABS] = _AF_LAX_Absolute_Illegal_Instruction;
    instructions[INS_BCS_REL] = _B0_BCS_Relative_Instruction;
    instructions[INS_LDA_IND_Y] = _B1_LDA_Indirect_YIndexed_Instruction;
    instructions[INS_JAM_9] = _B2_JAM_Illegal_Instruction;
    instructions[INS_LAX_IND_Y] = _B3_LAX_Indirect_YIndexed_Illegal_Instruction;
    instructions[INS_LDY_ZP_X] = _B4_LDY_ZeroPage_XIndexed_Instruction;
    instructions[INS_LDA_ZP_X] = _B5_LDA_ZeroPage_XIndexed_Instruction;
    instructions[INS_LDX_ZP_Y] = _B6_LDX_ZeroPage_YIndexed_Instruction;
    instructions[INS_LAX_ZP_Y] = _B7_LAX_ZeroPage_YIndexed_Illegal_Instruction;
    instructions[INS_CLV_IMPL] = _B8_CLV_Implied_Instruction;
    instructions[INS_LDA_ABS_Y] = _B9_LDA_Absolute_YIndexed_Instruction;
    instructions[INS_TSX_IMPL] = _BA_TSX_Implied_Instruction;
    instructions[INS_LAS_ABS_Y] = _BB_LAS_Absolute_YIndexed_Illegal_Instruction;
    instructions[INS_LDY_ABS_X] = _BC_LDY_Absolute_XIndexed_Instruction;
    instructions[INS_LDA_ABS_X] = _BD_LDA_Absolute_XIndexed_Instruction;
    instructions[INS_LDX_ABS_Y] = _BE_LDX_Absolute_YIndexed_Instruction;
    instructions[INS_LAX_ABS_Y] = _BF_LAX_Absolute_YIndexed_Illegal_Instruction;
    instructions[INS_CPY_IMM] = _C0_CPY_Immediate_Instruction;
    instructions[INS_CMP_X_IND] = _C1_CMP_XIndexed_Indirect_Instruction;
    instructions[INS_NOP_IMM_4] = _C2_NOP_Immediate_Illegal_Instruction;
    instructions[INS_DCP_X_IND] = _C3_DCP_XIndexed_Indirect_Illegal_Instruction;
    instructions[INS_CPY_ZP] = _C4_CPY_ZeroPage_Instruction;
    instructions[INS_CMP_ZP] = _C5_CMP_ZeroPage_Instruction;
    instructions[INS_DEC_ZP] = _C6_DEC_ZeroPage_Instruction;
    instructions[INS_DCP_ZP] = _C7_DCP_ZeroPage_Illegal_Instruction;
    instructions[INS_INY_IMPL] = _C8_INY_Implied_Instruction;
    instructions[INS_CMP_IMM] = _C9_CMP_Immediate_Instruction;
    instructions[INS_DEX_IMPL] = _CA_DEX_Implied_Instruction;
    instructions[INS_SBX_IMM] = _CB_SBX_Immediate_Illegal_Instruction;
    instructions[INS_CPY_ABS] = _CC_CPY_Absolute_Instruction;
    instructions[INS_CMP_ABS] = _CD_CMP_Absolute_Instruction;
    instructions[INS_DEC_ABS] = _CE_DEC_Absolute_Instruction;
    instructions[INS_DCP_ABS] = _CF_DCP_Absolute_Illegal_Instruction;
    instructions[INS_BNE_REL] = _D0_BNE_Relative_Instruction;
    instructions[INS_CMP_IND_Y] = _D1_CMP_Indirect_YIndexed_Instruction;
    instructions[INS_JAM_10] = _D2_JAM_Illegal_Instruction;
    instructions[INS_DCP_IND_Y] = _D3_DCP_Indirect_YIndexed_Illegal_Instruction;
    instructions[INS_NOP_ZP_X_5] = _D4_NOP_ZeroPage_XIndexed_Illegal_Instruction;
    instructions[INS_CMP_ZP_X] = _D5_CMP_ZeroPage_XIndexed_Instruction;
    instructions[INS_DEC_ZP_X] = _D6_DEC_ZeroPage_XIndexed_Instruction;
    instructions[INS_DCP_ZP_X] = _D7_DCP_ZeroPage_XIndexed_Illegal_Instruction;
    instructions[INS_CLD_IMPL] = _D8_CLD_Implied_Instruction;
    instructions[INS_CMP_ABS_Y] = _D9_CMP_Absolute_YIndexed_Instruction;
    instructions[INS_NOP_IMPL_5] = _DA_NOP_Implied_Illegal_Instruction;
    instructions[INS_DCP_ABS_Y] = _DB_DCP_Absolute_YIndexed_Illegal_Instruction;
    instructions[INS_NOP_ABS_X_5] = _DC_NOP_Absolute_XIndexed_Illegal_Instruction;
    instructions[INS_CMP_ABS_X] = _DD_CMP_Absolute_XIndexed_Instruction;
    instructions[INS_DEC_ABS_X] = _DE_DEC_Absolute_XIndexed_Instruction;
    instructions[INS_DCP_ABS_X] = _DF_DCP_Absolute_XIndexed_Illegal_Instruction;
    instructions[INS_CPX_IMM] = _E0_CPX_Immediate_Instruction;
    instructions[INS_SBC_X_IND] = _E1_SBC_XIndexed_Indirect_Instruction;
    instructions[INS_NOP_IMM_5] = _E2_NOP_Immediate_Illegal_Instruction;
    instructions[INS_ISC_X_IND] = _E3_ISC_XIndexed_Indirect_Illegal_Instruction;
    instructions[INS_CPX_ZP] = _E4_CPX_ZeroPage_Instruction;
    instructions[INS_SBC_ZP] = _E5_SBC_ZeroPage_Instruction;
    instructions[INS_INC_ZP] = _E6_INC_ZeroPage_Instruction;
    instructions[INS_ISC_ZP] = _E7_ISC_ZeroPage_Illegal_Instruction;
    instructions[INS_INX_IMPL] = _E8_INX_Implied_Instruction;
    instructions[INS_SBC_IMM] = _E9_SBC_Immediate_Instruction;
    instructions[INS_NOP_IMPL_6] = _EA_NOP_Implied_Instruction;
    instructions[INS_USBC_IMM] = _EB_USBC_Immediate_Illegal_Instruction;
    instructions[INS_CPX_ABS] = _EC_CPX_Absolute_Instruction;
    instructions[INS_SBC_ABS] = _ED_SBC_Absolute_Instruction;
    instructions[INS_INC_ABS] = _EE_INC_Absolute_Instruction;
    instructions[INS_ISC_ABS] = _EF_ISC_Absolute_Illegal_Instruction;
    instructions[INS_BEQ_REL] = _F0_BEQ_Relative_Instruction;
    instructions[INS_SBC_IND_Y] = _F1_SBC_Indirect_YIndexed_Instruction;
    instructions[INS_JAM_11] = _F2_JAM_Illegal_Instruction;
    instructions[INS_ISC_IND_Y] = _F3_ISC_Indirect_YIndexed_Illegal_Instruction;
    instructions[INS_NOP_ZP_X_6] = _F4_NOP_ZeroPage_XIndexed_Illegal_Instruction;
    instructions[INS_SBC_ZP_X] = _F5_SBC_ZeroPage_XIndexed_Instruction;
    instructions[INS_INC_ZP_X] = _F6_INC_ZeroPage_XIndexed_Instruction;
    instructions[INS_ISC_ZP_X] = _F7_ISC_ZeroPage_XIndexed_Illegal_Instruction;
    instructions[INS_SED_IMPL] = _F8_SED_Implied_Instruction;
    instructions[INS_SBC_ABS_Y] = _F9_SBC_Absolute_YIndexed_Instruction;
    instructions[INS_NOP_IMPL_7] = _FA_NOP_Implied_Illegal_Instruction;
    instructions[INS_ISC_ABS_Y] = _FB_ISC_Absolute_YIndexed_Illegal_Instruction;
    instructions[INS_NOP_ABS_X_6] = _FC_NOP_Absolute_XIndexed_Illegal_Instruction;
    instructions[INS_SBC_ABS_X] = _FD_SBC_Absolute_XIndexed_Instruction;
    instructions[INS_INC_ABS_X] = _FE_INC_Absolute_XIndexed_Instruction;
    instructions[INS_ISC_ABS_X] = _FF_ISC_Absolute_XIndexed_Illegal_Instruction;
}