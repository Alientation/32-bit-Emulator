#include <AlienCPUTest.h>

class ROLTest : public testing::Test {
    public: 
        AlienCPU cpu;

    virtual void SetUp() {
        cpu.reset();
    }

    virtual void TearDown() {

    }
};


TEST_F(ROLTest, ROLTest_ACCUMULATOR_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ROL_ACC, 0x00001023);
    cpu.A = 0b0001011001010110;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0b0010110010101100) << "Accumulator should be shifted left";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(ROLTest, ROLTest_ACCUMULATOR_CARRYFLAG_PRESET) {
    LoadInstruction(cpu, AlienCPU::INS_ROL_ACC, 0x00001023);
    cpu.A = 0b0001011001010110;
    cpu.setFlag(cpu.C_FLAG, 1);

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0b0010110010101101) << "Accumulator should be shifted left with carry bit rotated in from the right";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(ROLTest, ROLTest_ACCUMULATOR_CARRYFLAG_POSTSET) {
    LoadInstruction(cpu, AlienCPU::INS_ROL_ACC, 0x00001023);
    cpu.A = 0b1001011001010110;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0b0010110010101100) << "Accumulator should be shifted left";
    EXPECT_EQ(cpu.P & 0b10000011, 0b00000001) << "Carry flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(ROLTest, ROLTest_ACCUMULATOR_CARRYFLAG_SET) {
    LoadInstruction(cpu, AlienCPU::INS_ROL_ACC, 0x00001023);
    cpu.A = 0b1001011001010110;
    cpu.setFlag(cpu.C_FLAG, 1);

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0b0010110010101101) << "Accumulator should be shifted left";
    EXPECT_EQ(cpu.P & 0b10000011, 0b00000001) << "Carry flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(ROLTest, ROLTest_ACCUMULATOR_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ROL_ACC, 0x00001023);
    cpu.A = 0b0101011001010110;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0b1010110010101100) << "Accumulator should be shifted left";
    EXPECT_EQ(cpu.P & 0b10000011, 0b10000000) << "Negative flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}

TEST_F(ROLTest, ROLTest_ACCUMULATOR_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ROL_ACC, 0x00001023);
    cpu.A = 0b0000000000000000;

    TestInstruction(cpu, 2, 0x00001024);

    EXPECT_EQ(cpu.A, 0b0000000000000000) << "Accumulator should be shifted left";
    EXPECT_EQ(cpu.P & 0b10000011, 0b00000010) << "Zero flag should be set";
    TestUnchangedState(cpu, X, Y, SP);
}



TEST_F(ROLTest, ROLTest_NORMAL) {
    LoadInstruction(cpu, AlienCPU::INS_ROL_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00000123);
    cpu.writeByte(0x00000123, 0b00101110);

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard[0x00000123], 0b01011100) << "Accumulator should be shifted left";
    TestUnchangedState(cpu, A, X, Y, SP, P);
}

TEST_F(ROLTest, ROLTest_CARRYFLAG_PRESET) {
    LoadInstruction(cpu, AlienCPU::INS_ROL_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00000123);
    cpu.writeByte(0x00000123, 0b00101110);
    cpu.setFlag(cpu.C_FLAG, 1);

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard[0x00000123], 0b01011101) << "Accumulator should be shifted left with carry bit rotated in from the right";
    TestUnchangedState(cpu, X, Y, SP, P);
}

TEST_F(ROLTest, ROLTest_CARRYFLAG_POSTSET) {
    LoadInstruction(cpu, AlienCPU::INS_ROL_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00000123);
    cpu.writeByte(0x00000123, 0b10101110);

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard[0x00000123], 0b01011100) << "Accumulator should be shifted left";
    EXPECT_EQ(cpu.P & 0b10000011, 0b00000001) << "Carry flag should be set";
    TestUnchangedState(cpu, A, X, Y, SP);
}

TEST_F(ROLTest, ROLTest_CARRYFLAG_SET) {
    LoadInstruction(cpu, AlienCPU::INS_ROL_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00000123);
    cpu.writeByte(0x00000123, 0b10101110);
    cpu.setFlag(cpu.C_FLAG, 1);

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard[0x00000123], 0b01011101) << "Accumulator should be shifted left";
    EXPECT_EQ(cpu.P & 0b10000011, 0b00000001) << "Carry flag should be set";
    TestUnchangedState(cpu, A, X, Y, SP);
}

TEST_F(ROLTest, ROLTest_NEGATIVEFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ROL_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00000123);
    cpu.writeByte(0x00000123, 0b01101110);

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard[0x00000123], 0b11011100) << "Accumulator should be shifted left";
    EXPECT_EQ(cpu.P & 0b10000011, 0b10000000) << "Negative flag should be set";
    TestUnchangedState(cpu, A, X, Y, SP);
}

TEST_F(ROLTest, ROLTest_ZEROFLAG) {
    LoadInstruction(cpu, AlienCPU::INS_ROL_ABS, 0x00001023);
    cpu.writeWord(0x00001024, 0x00000123);
    cpu.writeByte(0x00000123, 0b00000000);

    TestInstruction(cpu, 8, 0x00001028);

    EXPECT_EQ(cpu.motherboard[0x00000123], 0b00000000) << "Accumulator should be shifted left";
    EXPECT_EQ(cpu.P & 0b10000011, 0b00000010) << "Zero flag should be set";
    TestUnchangedState(cpu, A, X, Y, SP);
}