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

    // reset counters
    nextInterruptCheck = INTERRUPT_CHECK_INTERVAL;
    cycles = 0;
    instructionsExecuted = 0;

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

    // reset counters
    cycles = 0;
    instructionsExecuted = 0;
    
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
        // TODO: create better error logger that stores information about the cpu
        std::stringstream stream;
        stream << std::endl << "Error: Invalid instruction " << stringifyHex(instruction) 
                << std::endl << "PC=[" + stringifyHex(PC) << "]" << std::endl;
        
        //throw std::invalid_argument(stream.str()); // to allow compiling
        std::cout << stream.str();
        return;
    }

    instructionsExecuted++;

    Word address = (this->*instructions[instruction].addressingMode)(); // calls the addressing mode function
    (this->*instructions[instruction].instruction)(address); // calls the function associated with the instruction
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


// TODO: apparently the 6502 when reading from PC first increments PC then
// reads it??? I don't think we should do this for this pc
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
    writeByte(SPToAddress(), value);

    // move stack pointer so it points to the first free byte in stack memory
    SP--;
}

// Pops 1 byte from stack memory
Byte AlienCPU::popByteFromStack() {
    // move stack pointer so it points to the first free byte in stack memory
    SP++;

    // read the value from the byte at the now free byte represented by the Stack Pointer
    return readByte(SPToAddress());
}