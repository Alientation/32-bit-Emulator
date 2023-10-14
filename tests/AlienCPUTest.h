#ifndef ALIENCPUTEST_H
#define ALIENCPUTEST_H

#include <gtest/gtest.h>

// CHEATY HACK to be able to access private members of another class (for testing purposes)
#define private public
#include <../src/Motherboard/AlienCPU.h>
#undef private

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

static void TestInstruction(AlienCPU& cpu, u64 expectedCycles, Word expectedPC) {
    cpu.start(expectedCycles);

    // check that the PC is at the expected address
    EXPECT_EQ(cpu.PC, expectedPC);

    // check that the number of cycles is as expected
    EXPECT_EQ(cpu.cycles, expectedCycles);
}

static void TestUnchangedState(AlienCPU& cpu, CPUElement elements...) { 
    for (auto&& element : {elements}) {
        switch (element) {
            case A:
                EXPECT_EQ(cpu.A, cpu.A_INIT);
                break;
            case X:
                EXPECT_EQ(cpu.X, cpu.X_INIT);
                break;
            case Y:
                EXPECT_EQ(cpu.Y, cpu.Y_INIT);
                break;
            case SP:
                EXPECT_EQ(cpu.SP, cpu.SP_INIT);
                break;
            case P:
                EXPECT_EQ(cpu.P, cpu.P_INIT);
                break;
        }
    }
}


#endif // ALIENCPUTEST_H
