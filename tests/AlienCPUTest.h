#ifndef ALIENCPUTEST_H
#define ALIENCPUTEST_H

#include <gtest/gtest.h>

// CHEATY HACK to be able to access private members of another class (for testing purposes)
#define private public
#include <../src/Motherboard/AlienCPU.h>
#undef private

// registers in cpu that can be tested
enum CPUElement {
    A,
    X,
    Y,
    SP,
    P
};

// Helper to load an instruction into memory at a given address
static void LoadInstruction(AlienCPU& cpu, u8 instruction, Word address) {
    // setting reset vector to begin processing instructions at address
    cpu.writeWord(AlienCPU::POWER_ON_RESET_VECTOR, address);
    cpu.writeByte(address, instruction);
}

// Helper to start the cpu for a certain number of cycles and check the state of the cpu
static void TestInstruction(AlienCPU& cpu, u64 expectedCycles, Word expectedPC) {
    cpu.startCycles(expectedCycles); 

    EXPECT_EQ(cpu.PC, expectedPC) << "PC is not at the expected address";
    EXPECT_EQ(cpu.cycles, expectedCycles) << "Number of cycles is not as expected";
}

// Helper to check that the state of the specific elements of cpu is unchanged
static void TestUnchangedState(AlienCPU& cpu, CPUElement element) { 
    switch (element) {
        case A:
            EXPECT_EQ(cpu.A, cpu.A_INIT) << "A is not unchanged";
            break;
        case X:
            EXPECT_EQ(cpu.X, cpu.X_INIT) << "X is not unchanged";
            break;
        case Y:
            EXPECT_EQ(cpu.Y, cpu.Y_INIT) << "Y is not unchanged";
            break;
        case SP:
            EXPECT_EQ(cpu.SP, cpu.SP_INIT) << "SP is not unchanged";
            break;
        case P:
            EXPECT_EQ(cpu.P, cpu.P_INIT) << "P is not unchanged";
            break;
    }
}

// unwrap the variable arguments TODO: this does not work at ALL
static void TestUnchangedState(AlienCPU& cpu, CPUElement element, CPUElement elements...) {
    TestUnchangedState(cpu, element);
    TestUnchangedState(cpu, elements);
}


#endif // ALIENCPUTEST_H
