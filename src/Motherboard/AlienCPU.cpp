#include "AlienCPU.h"
#include <iostream>
#include <iomanip>

const std::string AlienCPU::VERSION = "0.0.1";

AlienCPU::AlienCPU() {
    // null out instructions
    for (int i = 0; i < INSTRUCTION_COUNT; i++) {
        instructions[i] = _0000NullInstruction;
    }

    instructions[INS_LDA_IM] = _00A9_LoadAccumulator_Immediate;
}

void AlienCPU::Reset() {
    // reset all registers
    PC = PC_INIT; // program counter (instruction pointer)
    SP = SP_INIT; // stack pointer (start of the first element on the stack)
    BP = BP_INIT;
    SS = SS_INIT;

    A = A_INIT;
    X = X_INIT;
    Y = Y_INIT;

    // reset flags
    Z = FLAGS_Z_INIT;
    C = FLAGS_C_INIT;
    H = FLAGS_H_INIT;
    P = FLAGS_P_INIT;
    I = FLAGS_I_INIT;
    N = FLAGS_N_INIT;
    V = FLAGS_V_INIT;
    S = FLAGS_S_INIT;


    NextInterruptCheck = INTERRUPT_CHECK_INTERVAL;

    Cycles = 0;

    RAM.Initialize();
}

void AlienCPU::Start(u64 maxCycles) {
    
    // Reset the CPU, all registers, ram etc
    Reset();

    std::cout << "Starting Alien CPU v" << VERSION << std::endl;
    
    // Fetch, Decode, Execute Cycle loop
    for (;;) {
        // Halt execution because max cycles has been reached
        if (Cycles >= maxCycles) {
            std::cout << std::endl << "Max cycles reached" << std::endl;
            break;
        }

        std::cout << ".";

        // Reads in the next instruction of variable length (1-2 bytes for this cpu)
        u16 nextInstruction = 0x0000;
        Byte nextInstructionByte = 0;
        u8 instructionBytesLength = 0;
        
        // while the length of the instruction in bytes is less than the max instruction bytes length
        // stop when the current read instruction is valid
        while (instructionBytesLength < MAX_INSTRUCTION_BYTES_LENGTH && !ValidInstruction(nextInstruction)) {
            nextInstructionByte = FetchNextByte(); // get the next byte in memory

            nextInstruction = nextInstruction << 8; // shift the current instruction 8 bits to the left
            nextInstruction = nextInstruction | nextInstructionByte; // add the read instruction byte to the current instruction
            instructionBytesLength++; //increment byte count
        }

        // Executes the instruction even if it is invalid
        // ExecuteInstruction(nextInstruction);


        // Check for Interrupts
        if (NextInterruptCheck == 0) {

        }
    }
}

// Executes the instruction if it is valid, otherwise throws an exception
void AlienCPU::ExecuteInstruction(u16 instruction) {
    if (!ValidInstruction(instruction)) {
        std::stringstream stream;
        stream << "Error: Invalid instruction 0x" << std::hex << instruction << std::endl;
        
        throw std::invalid_argument(stream.str());
    }

    instructions[instruction](*this); // calls the function associated with the instruction
}

// Checks if the instruction is a valid instruction. Must be within max instructions and must not be a null instruction
bool AlienCPU::ValidInstruction(u16 instruction) {
    return instruction < INSTRUCTION_COUNT && &instructions[instruction] != &instructions[0];
}

// Gets the next byte in memory and increments PC and Cycles
Byte AlienCPU::FetchNextByte() {
    Byte Data = ReadByte(PC);
    PC++;
    Cycles++;
    return Data;
}

// Reads the byte at the specified address in memory if valid, otherwise throws an exception
Byte AlienCPU::ReadByte(Word address) {
    if (address >= RAM.MEMORY_SIZE) {
        // display the hex address of the out of bounds memory access
        std::stringstream stream;
        stream << "Error: Out of bounds memory access at address 0x" << std::hex << address << std::endl;
        
        throw std::invalid_argument(stream.str());
    }
    return RAM.Data[address];
}

// Null Instruction, throws error if called
void AlienCPU::_0000NullInstruction() {
    std::stringstream stream;
    stream << "Error: Null Instruction" << std::endl;
    
    throw std::invalid_argument(stream.str());
}

// Load Accumulator Immediate Instruction (LDA_IM) into register A
void AlienCPU::_00A9_LoadAccumulator_Immediate() {
    
}