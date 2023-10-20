#include <AlienCPUTest.h>

class RORTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


TEST_F(RORTest, RORTest_ACCUMULATOR_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ROR_ACC, 0x00001023);
    cpu.A = 0b0001011001010110;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0b0000101100101011) << "Accumulator should be shifted right";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(RORTest, RORTest_ACCUMULATOR_CARRYFLAG_PRESET) {
    LoadInstruction(cpu, AlienCPU::INS_ROR_ACC, 0x00001023);
    cpu.A = 0b0001011001010110;
    cpu.setFlag(CARRY, 1);

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0b1000101100101011) << "Accumulator should be shifted right with carry bit rotated in from the left";
    EXPECT_EQ(cpu.P & 0b10000011, 0b10000000) << "Negative flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(RORTest, RORTest_ACCUMULATOR_CARRYFLAG_POSTSET) {
    LoadInstruction(cpu, AlienCPU::INS_ROR_ACC, 0x00001023);
    cpu.A = 0b0001011001010111;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0b0000101100101011) << "Accumulator should be shifted right";
    EXPECT_EQ(cpu.P & 0b10000011, 0b00000001) << "Carry flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(RORTest, RORTest_ACCUMULATOR_CARRYFLAG_SET) {
    LoadInstruction(cpu, AlienCPU::INS_ROR_ACC, 0x00001023);
    cpu.A = 0b1001011001010111;
    cpu.setFlag(CARRY, 1);

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0b1100101100101011) << "Accumulator should be shifted right";
    EXPECT_EQ(cpu.P & 0b10000011, 0b10000001) << "Carry and negative flags should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(RORTest, RORTest_ACCUMULATOR_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ROR_ACC, 0x00001023);
    cpu.A = 0b0000000000000000;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0b0000000000000000) << "Accumulator should be shifted right";
    EXPECT_EQ(cpu.P & 0b10000011, 0b00000010) << "Zero flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}



TEST_F(RORTest, RORTest_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ROR_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00000123);
    cpu.writeByte(0x00000123, 0b00101110);

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard[0x00000123], 0b00010111) << "Accumulator should be shifted right";
    TestUnchangedState(cpu, A, X, Y, SP, P);
}

TEST_F(RORTest, RORTest_CARRYFLAG_PRESET) {
    LoadInstruction(cpu, AlienCPU::INS_ROR_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00000123);
    cpu.writeByte(0x00000123, 0b00101110);
    cpu.setFlag(CARRY, 1);

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard[0x00000123], 0b10010111) << "Accumulator should be shifted right with carry bit rotated in from the left";
    EXPECT_EQ(cpu.P & 0b10000011, 0b10000000) << "Negative flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(RORTest, RORTest_CARRYFLAG_POSTSET) {
    LoadInstruction(cpu, AlienCPU::INS_ROR_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00000123);
    cpu.writeByte(0x00000123, 0b00101111);

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard[0x00000123], 0b00010111) << "Accumulator should be shifted right";
    EXPECT_EQ(cpu.P & 0b10000011, 0b00000001) << "Carry flag should be set";
    TestUnchangedState(cpu, A, X, Y, SP);
}

TEST_F(RORTest, RORTest_CARRYFLAG_SET) {
    LoadInstruction(cpu, AlienCPU::INS_ROR_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00000123);
    cpu.writeByte(0x00000123, 0b10101111);
    cpu.setFlag(CARRY, 1);

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard[0x00000123], 0b11010111) << "Accumulator should be shifted right";
    EXPECT_EQ(cpu.P & 0b10000011, 0b10000001) << "Carry and negative flags should be set";
    TestUnchangedState(cpu, A, X, Y, SP);
}

TEST_F(RORTest, RORTest_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ROR_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00000123);
    cpu.writeByte(0x00000123, 0b00000000);

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard[0x00000123], 0b00000000) << "Accumulator should be shifted right";
    EXPECT_EQ(cpu.P & 0b10000011, 0b00000010) << "Zero flag should be set";
    TestUnchangedState(cpu, A, X, Y, SP);
}