#include "AlienCPU.h"
#include <iostream>
#include <iomanip>

const std::string AlienCPU::VERSION = "0.0.1";

AlienCPU::AlienCPU() {
    AlienCPU::InitInstructions();
    
}

// realistically, reset actually randomizes values for memory and registers
void AlienCPU::Reset() {
    // reset all registers
    pc = PC_INIT;
    sp = SP_INIT;

    a = A_INIT;
    x = X_INIT;
    y = Y_INIT;

    // reset flags
    p = P_INIT;

    // reset cycle counters
    nextInterruptCheck = INTERRUPT_CHECK_INTERVAL;
    cycles = 0;

    // prepare motherboard
    motherboard.Initialize();
}

void AlienCPU::Start(u64 maxCycles) {
    // Reset the CPU, all registers, ram etc
    Reset();

    std::cout << "Starting Alien CPU v" << VERSION << std::endl;

    // start sequence / boot process
    // read from PC (which should be pointed to the RESET vector)
    // go to the address specified by the RESET vector
    // and continue on from there
    
    // Fetch, Decode, Execute Cycle loop
    for (;;) {
        // Halt execution because max cycles has been reached
        if (cycles >= maxCycles) {
            std::cout << std::endl << "Max cycles reached" << std::endl;
            break;
        }

        std::cout << ".";

        // Reads in the next instruction (1 byte)
        u16 nextInstruction = FetchNextByte();

        // Executes the instruction even if it is invalid
        // ExecuteInstruction(nextInstruction);


        // Check for Interrupts
        if (nextInterruptCheck == 0) {

        }
    }

    std::cout << "Stopping Alien CPU v" << VERSION << std::endl;
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

// Gets the next byte in memory and increments PC and cycles
Byte AlienCPU::FetchNextByte() {
    Byte data = motherboard.ReadByte(pc); // gets byte at the program pointer (PC)
    pc++;
    cycles++;
    return data;
}

u16 AlienCPU::FetchNextTwoBytes() {
    // lowest byte
    u16 data = FetchNextByte();
    data |= FetchNextByte() << 8;
    // highest byte
}

// Gets the next 4 bytes in memory
Word AlienCPU::FetchNextWord() {
    // lowest byte
    Word data = FetchNextByte();
    data |= FetchNextByte() << 8;
    data |= FetchNextByte() << 16;
    data |= FetchNextByte() << 24;
    // highest byte

    return data;
}

// Write the byte to the specified address in memory if valid, otherwise throws an exception
void AlienCPU::WriteByte(Word address, Byte value) {
    // write byte 0 to memory
    motherboard.WriteByte(address, value);
}

void AlienCPU::WriteTwoBytes(Word address, u16 value) {
    // lowest byte
    WriteByte(address, value & 0xFF);
    WriteByte(address + 1, (value >> 8) & 0xFF);
    // highest byte
}

// Write the next 4 bytes to the specified address in memory if valid, otherwise throws an exception
void AlienCPU::WriteWord(Word address, Word value) {
    // lowest byte
    WriteByte(address, value & 0xFF);
    WriteByte(address + 1, (value >> 8) & 0xFF);
    WriteByte(address + 2, (value >> 16) & 0xFF);
    WriteByte(address + 3, (value >> 24) & 0xFF);
    // highest byte
}


// =======================STACK============================
//
//

//
void AlienCPU::SPtoAddress(Byte page) {

}

//
void AlienCPU::PushWordToStack(Word value) {

}

//
Word AlienCPU::PopWordFromStack() {
    return NULL_ADDRESS;
}

//
void AlienCPU::PushByteToStack(Byte value) {

}

//
Byte AlienCPU::PopByteFromStack() {
    return NULL_ADDRESS >> 24;
}



// ====================INSTRUCTIONS======================
//
//

// ======================TRANSFER========================
// ===================LOAD=ACCUMULATOR===================
void AlienCPU::_A1_LDA_XIndexed_Indirect_Instruction() {

}
void AlienCPU::_A5_LDA_ZeroPage_Instruction() {

}

// Load Accumulator Immediate 
// Loads the next 2 bytes into Accumulator
void AlienCPU::_A9_LDA_Immediate_Instruction() {
    Byte value = FetchNextByte() | (FetchNextByte() << 8);
}

void AlienCPU::_AD_LDA_Absolute_Instruction() {

}

void AlienCPU::_B1_LDA_Indirect_YIndexed_Instruction() {

}

void AlienCPU::_B5_LDA_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU::_B9_LDA_Absolute_YIndexed_Instruction() {

}

void AlienCPU::_BD_LDA_Absolute_XIndexed_Instruction() {

}

// ===================LOAD=X=REGISTER===================
void AlienCPU::_A2_LDX_Immediate_Instruction() {

}

void AlienCPU::_A6_LDX_ZeroPage_Instruction() {

}

void AlienCPU::_AE_LDX_Absolute_Instruction() {

}

void AlienCPU::_B6_LDX_ZeroPage_YIndexed_Instruction() {

}

void AlienCPU::_BE_LDX_Absolute_YIndexed_Instruction() {

}

// ===================LOAD=Y=REGISTER===================
void AlienCPU::_A0_LDY_Immediate_Instruction() {

}

void AlienCPU::_A4_LDY_ZeroPage_Instruction() {

}

void AlienCPU::_AC_LDY_Absolute_Instruction() {

}

void AlienCPU::_B4_LDY_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU::_BC_LDY_Absolute_XIndexed_Instruction() {

}

// ===================LOAD=ACCUMULATOR==================
void AlienCPU::_81_STA_XIndexed_Indirect_Instruction() {

}

void AlienCPU::_85_STA_ZeroPage_Instruction() {

}

void AlienCPU::_8D_STA_Absolute_Instruction() {

}

void AlienCPU::_91_STA_Indirect_YIndexed_Instruction() {

}

void AlienCPU::_95_STA_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU::_99_STA_Absolute_YIndexed_Instruction() {

}

void AlienCPU::_9D_STA_Absolute_XIndexed_Instruction() {

}

// ===================STORE=X=REGISTER==================
void AlienCPU::_86_STX_ZeroPage_Instruction() {

}

void AlienCPU::_8E_STX_Absolute_Instruction() {

}

void AlienCPU::_96_STX_ZeroPage_YIndexed_Instruction() {

}

// ===================STORE=Y=REGISTER==================
void AlienCPU::_84_STY_ZeroPage_Instruction() {

}

void AlienCPU::_8C_STY_Absolute_Instruction() {

}

void AlienCPU::_94_STY_ZeroPage_XIndexed_Instruction() {

}

// =========TRANSFER=ACCUMULATOR=TO=X=REGISTER==========
void AlienCPU::_AA_TAX_Implied_Instruction() {

}

// =========TRANSFER=ACCUMULATOR=TO=Y=REGISTER==========
void AlienCPU::_A8_TAY_Implied_Instruction() {

}

// ========TRANSFER=STACK=POINTER=TO=X=REGISTER=========
void AlienCPU::_BA_TSX_Implied_Instruction() {

}

// =========TRANSFER=X=REGISTER=TO=ACCUMULATOR==========
void AlienCPU::_8A_TXA_Implied_Instruction() {

}

// ========TRANSFER=X=REGISTER=TO=STACK=POINTER=========
void AlienCPU::_9A_TXS_Implied_Instruction() {

}

// =========TRANSFER=Y=REGISTER=TO=ACCUMULATOR==========
void AlienCPU::_98_TYA_Implied_Instruction() {

}


// ========================STACK=========================
// ===================PUSH=ACCUMULATOR===================
void AlienCPU::_48_PHA_Implied_Instruction() {

}

// =================PUSH=PROCESSOR=STATUS================
void AlienCPU::_08_PHP_Implied_Instruction() {

}

// ===================POP=ACCUMULATOR====================
void AlienCPU::_68_PLA_Implied_Instruction() {

}

// =================POP=PROCESSOR=STATUS=================
void AlienCPU::_28_PLP_Implied_Instruction() {

}


// ================DECREMENTS=&=INCREMENTS===============
// ===================DECREMENT=MEMORY===================
void AlienCPU::_C6_DEC_ZeroPage_Instruction() {

}

void AlienCPU::_CE_DEC_Absolute_Instruction() {

}

void AlienCPU::_D6_DEC_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU::_DE_DEC_Absolute_XIndexed_Instruction() {

}

// =================DECREMENT=X=REGISTER=================
void AlienCPU::_CA_DEX_Implied_Instruction() {

}

// =================DECREMENT=Y=REGISTER=================
void AlienCPU::_88_DEY_Implied_Instruction() {

}

// ===================INCREMENT=MEMORY===================
void AlienCPU::_E6_INC_ZeroPage_Instruction() {

}

void AlienCPU::_EE_INC_Absolute_Instruction() {

}

void AlienCPU::_F6_INC_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU::_FE_INC_Absolute_XIndexed_Instruction() {

}

// =================INCREMENT=X=REGISTER=================
void AlienCPU::_E8_INX_Implied_Instruction() {

}

// =================INCREMENT=Y=REGISTER=================
void AlienCPU::_C8_INY_Implied_Instruction() {

}


// =================ARITHMETIC=OPERATIONS================
// =====================ADD=WITH=CARRY===================
void AlienCPU::_61_ADC_XIndexed_Indirect_Instruction() {

}

void AlienCPU::_65_ADC_ZeroPage_Instruction() {

}

void AlienCPU::_69_ADC_Immediate_Instruction() {

}

void AlienCPU::_6D_ADC_Absolute_Instruction() {

}

void AlienCPU::_71_ADC_Indirect_YIndexed_Instruction() {

}

void AlienCPU::_75_ADC_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU::_79_ADC_Absolute_YIndexed_Instruction() {

}

void AlienCPU::_7D_ADC_Absolute_XIndexed_Instruction() {

}

// =====================SUBTRACT=WITH=BORROW=============
void AlienCPU::_E1_SBC_XIndexed_Indirect_Instruction() {

}

void AlienCPU::_E5_SBC_ZeroPage_Instruction() {

}

void AlienCPU::_E9_SBC_Immediate_Instruction() {

}

void AlienCPU::_ED_SBC_Absolute_Instruction() {

}

void AlienCPU::_F1_SBC_Indirect_YIndexed_Instruction() {

}

void AlienCPU::_F5_SBC_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU::_F9_SBC_Absolute_YIndexed_Instruction() {

}

void AlienCPU::_FD_SBC_Absolute_XIndexed_Instruction() {

}


// ==================LOGICAL=OPERATIONS==================
// =====================AND=WITH=ACCUMULATOR==============
void AlienCPU::_21_AND_XIndexed_Indirect_Instruction() {

}

void AlienCPU::_25_AND_ZeroPage_Instruction() {

}

void AlienCPU::_29_AND_Immediate_Instruction() {

}

void AlienCPU::_2D_AND_Absolute_Instruction() {

}

void AlienCPU::_31_AND_Indirect_YIndexed_Instruction() {

}

void AlienCPU::_35_AND_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU::_39_AND_Absolute_YIndexed_Instruction() {

}

void AlienCPU::_3D_AND_Absolute_XIndexed_Instruction() {

}

// =====================EOR=WITH=ACCUMULATOR==============
void AlienCPU::_41_EOR_XIndexed_Indirect_Instruction() {

}

void AlienCPU::_45_EOR_ZeroPage_Instruction() {

}

void AlienCPU::_49_EOR_Immediate_Instruction() {

}

void AlienCPU::_4D_EOR_Absolute_Instruction() {

}

void AlienCPU::_51_EOR_Indirect_YIndexed_Instruction() {

}

void AlienCPU::_55_EOR_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU::_59_EOR_Absolute_YIndexed_Instruction() {

}

void AlienCPU::_5D_EOR_Absolute_XIndexed_Instruction() {

}

// =====================ORA=WITH=ACCUMULATOR==============
void AlienCPU::_01_ORA_XIndexed_Indirect_Instruction() {

}

void AlienCPU::_05_ORA_ZeroPage_Instruction() {

}

void AlienCPU::_09_ORA_Immediate_Instruction() {

}

void AlienCPU::_0D_ORA_Absolute_Instruction() {

}

void AlienCPU::_11_ORA_Indirect_YIndexed_Instruction() {

}

void AlienCPU::_15_ORA_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU::_19_ORA_Absolute_YIndexed_Instruction() {

}

void AlienCPU::_1D_ORA_Absolute_XIndexed_Instruction() {

}


// ====================SHIFT=&=ROTATE====================
// =====================ARITHMETIC=SHIFT==================
void AlienCPU::_06_ASL_ZeroPage_Instruction() {

}

void AlienCPU::_0A_ASL_Accumulator_Instruction() {

}

void AlienCPU::_0E_ASL_Absolute_Instruction() {

}

void AlienCPU::_16_ASL_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU::_1E_ASL_Absolute_XIndexed_Instruction() {

}

// =====================LOGICAL=SHIFT=====================
void AlienCPU::_46_LSR_ZeroPage_Instruction() {

}

void AlienCPU::_4A_LSR_Accumulator_Instruction() {

}

void AlienCPU::_4E_LSR_Absolute_Instruction() {

}

void AlienCPU::_56_LSR_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU::_5E_LSR_Absolute_XIndexed_Instruction() {

}

// =====================ROTATE=LEFT=======================
void AlienCPU::_26_ROL_ZeroPage_Instruction() {

}

void AlienCPU::_2A_ROL_Accumulator_Instruction() {

}

void AlienCPU::_2E_ROL_Absolute_Instruction() {

}

void AlienCPU::_36_ROL_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU::_3E_ROL_Absolute_XIndexed_Instruction() {

}

// =====================ROTATE=RIGHT======================
void AlienCPU::_66_ROR_ZeroPage_Instruction() {

}

void AlienCPU::_6A_ROR_Accumulator_Instruction() {

}

void AlienCPU::_6E_ROR_Absolute_Instruction() {

}

void AlienCPU::_76_ROR_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU::_7E_ROR_Absolute_XIndexed_Instruction() {

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
void AlienCPU::_C1_CMP_XIndexed_Indirect_Instruction() {

}

void AlienCPU::_C5_CMP_ZeroPage_Instruction() {

}

void AlienCPU::_C9_CMP_Immediate_Instruction() {

}

void AlienCPU::_CD_CMP_Absolute_Instruction() {

}

void AlienCPU::_D1_CMP_Indirect_YIndexed_Instruction() {

}

void AlienCPU::_D5_CMP_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU::_D9_CMP_Absolute_YIndexed_Instruction() {

}

void AlienCPU::_DD_CMP_Absolute_XIndexed_Instruction() {

}

// =====================COMPARE=X=REGISTER==============
void AlienCPU::_E0_CPX_Immediate_Instruction() {

}

void AlienCPU::_E4_CPX_ZeroPage_Instruction() {

}

void AlienCPU::_EC_CPX_Absolute_Instruction() {

}

// =====================COMPARE=Y=REGISTER==============
void AlienCPU::_C0_CPY_Immediate_Instruction() {

}

void AlienCPU::_C4_CPY_ZeroPage_Instruction() {

}

void AlienCPU::_CC_CPY_Absolute_Instruction() {

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
    stream << "Error: NULL Instruction" << std::endl;

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





void AlienCPU::InitInstructions() {
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
    instructions[INS_LDA_IM] = _A9_LDA_Immediate_Instruction;
    instructions[INS_TAX_IMPL] = _AA_TAX_Implied_Instruction;
    instructions[INS_LXA_IM] = _AB_LXA_Immediate_Illegal_Instruction;
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