#include "AlienCPU6502.h"
#include <iostream>
#include <iomanip>

const std::string AlienCPU6502::VERSION = "0.0.1";

AlienCPU6502::AlienCPU6502() {
    // null out instructions to catch errors
    for (int i = 0; i < INSTRUCTION_COUNT; i++) {
        instructions[i] = _00_NULL_Illegal_Instruction;
    }

    instructions[INS_LDA_IM] = _A9_LDA_Immediate_Instruction;
}

// realistically, reset actually randomizes values for memory and registers
void AlienCPU6502::Reset() {
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

    // prepare ram
    ram.Initialize();
}

void AlienCPU6502::Start(u64 maxCycles) {
    // Reset the CPU, all registers, ram etc
    Reset();

    std::cout << "Starting Alien CPU v" << VERSION << std::endl;
    
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
void AlienCPU6502::ExecuteInstruction(u16 instruction) {
    if (!ValidInstruction(instruction)) {
        std::stringstream stream;
        stream << "Error: Invalid instruction 0x" << std::hex << instruction << std::endl;
        
        throw std::invalid_argument(stream.str());
    }

    instructions[instruction](*this); // calls the function associated with the instruction
}

// Checks if the instruction is a valid instruction. Must be within max instructions and must not be a null instruction
bool AlienCPU6502::ValidInstruction(u16 instruction) {
    return instruction < INSTRUCTION_COUNT && &instructions[instruction] != &instructions[0];
}

// Gets the next byte in memory and increments PC and cycles
Byte AlienCPU6502::FetchNextByte() {
    Byte data = ReadByte(pc); // gets byte at the program pointer (PC)
    pc++;
    cycles++;
    return data;
}

// Gets the next 4 bytes in memory
Word AlienCPU6502::FetchNextWord() {
    Word data = FetchNextByte(); // byte 1

    data = (data << 8) | FetchNextByte(); // byte 2
    data = (data << 8) | FetchNextByte(); // byte 3
    data = (data << 8) | FetchNextByte(); // byte 4

    return data;
}

// Reads the byte at the specified address in memory if valid, otherwise throws an exception
Byte AlienCPU6502::ReadByte(Word address) {
    if (address >= ram.MEMORY_SIZE) {
        // display the hex address of the out of bounds memory access
        std::stringstream stream;
        stream << "Error: Out of bounds memory access at address 0x" << std::hex << address <<
             ". Requires 1 byte" << std::endl;
        
        throw std::invalid_argument(stream.str());
    }
    return ram.Data[address];
}

// Reads the next 4 bytes at the specified address in memory if valid, otherwise throws an exception
Word AlienCPU6502::ReadWord(Word address) {
    if (address >= ram.MEMORY_SIZE - 4) {
        // display the hex address of the out of bounds memory access
        std::stringstream stream;
        stream << "Error: Out of bounds memory access at address 0x" << std::hex << address << 
            ". Requires 4 bytes." << std::endl;
        
        throw std::invalid_argument(stream.str());
    }

    Word data = ReadByte(address); // byte 1

    data = (data << 8) | ReadByte(address + 1); // byte 2
    data = (data << 8) | ReadByte(address + 2); // byte 3
    data = (data << 8) | ReadByte(address + 3); // byte 4

    return data;
}

// Write the byte to the specified address in memory if valid, otherwise throws an exception
void AlienCPU6502::WriteByte(Word address, Byte value) {
    if (address >= ram.MEMORY_SIZE) {
        // display the hex address of the out of bounds memory access
        std::stringstream stream;
        stream << "Error: Out of bounds memory access at address 0x" << std::hex << address <<
             ". Requires 1 byte" << std::endl;
        
        throw std::invalid_argument(stream.str());
    }

    // write byte 1 to ram
    ram.Data[address] = value;
}

// Write the next 4 bytes to the specified address in memory if valid, otherwise throws an exception
void AlienCPU6502::WriteWord(Word address, Word value) {
    if (address >= ram.MEMORY_SIZE - 4) {
        // display the hex address of the out of bounds memory access
        std::stringstream stream;
        stream << "Error: Out of bounds memory access at address 0x" << std::hex << address << 
            ". Requires 4 bytes." << std::endl;
        
        throw std::invalid_argument(stream.str());
    }

    // write bytes 1 to 4 to ram
    WriteByte(address, (value >> 24) & 0xFF);
    WriteByte(address + 1, (value >> 16) & 0xFF);
    WriteByte(address + 2, (value >> 8) & 0xFF);
    WriteByte(address + 3, value & 0xFF);
}


// =======================STACK============================
//
//

//
void AlienCPU6502::SPtoAddress(Byte page) {

}

//
void AlienCPU6502::PushWordToStack(Word value) {

}

//
Word AlienCPU6502::PopWordFromStack() {
    return NULL_ADDRESS;
}

//
void AlienCPU6502::PushByteToStack(Byte value) {

}

//
Byte AlienCPU6502::PopByteFromStack() {
    return NULL_ADDRESS >> 24;
}



// ====================INSTRUCTIONS======================
//
//

// ======================TRANSFER========================
// ===================LOAD=ACCUMULATOR===================
void AlienCPU6502::_A1_LDA_XIndexed_Indirect_Instruction() {

}
void AlienCPU6502::_A5_LDA_ZeroPage_Instruction() {

}

// Load Accumulator Immediate 
// Loads the next 2 bytes into Accumulator
void AlienCPU6502::_A9_LDA_Immediate_Instruction() {

}

void AlienCPU6502::_AD_LDA_Absolute_Instruction() {

}

void AlienCPU6502::_B1_LDA_Indirect_YIndexed_Instruction() {

}

void AlienCPU6502::_B5_LDA_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU6502::_B9_LDA_Absolute_YIndexed_Instruction() {

}

void AlienCPU6502::_BD_LDA_Absolute_XIndexed_Instruction() {

}

// ===================LOAD=X=REGISTER===================
void AlienCPU6502::_A2_LDX_Immediate_Instruction() {

}

void AlienCPU6502::_A6_LDX_ZeroPage_Instruction() {

}

void AlienCPU6502::_AE_LDX_Absolute_Instruction() {

}

void AlienCPU6502::_B6_LDX_ZeroPage_YIndexed_Instruction() {

}

void AlienCPU6502::_BE_LDX_Absolute_YIndexed_Instruction() {

}

// ===================LOAD=Y=REGISTER===================
void AlienCPU6502::_A0_LDY_Immediate_Instruction() {

}

void AlienCPU6502::_A4_LDY_ZeroPage_Instruction() {

}

void AlienCPU6502::_AC_LDY_Absolute_Instruction() {

}

void AlienCPU6502::_B4_LDY_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU6502::_BC_LDY_Absolute_XIndexed_Instruction() {

}

// ===================LOAD=ACCUMULATOR==================
void AlienCPU6502::_81_STA_XIndexed_Indirect_Instruction() {

}

void AlienCPU6502::_85_STA_ZeroPage_Instruction() {

}

void AlienCPU6502::_8D_STA_Absolute_Instruction() {

}

void AlienCPU6502::_91_STA_Indirect_YIndexed_Instruction() {

}

void AlienCPU6502::_95_STA_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU6502::_99_STA_Absolute_YIndexed_Instruction() {

}

void AlienCPU6502::_9D_STA_Absolute_XIndexed_Instruction() {

}

// ===================STORE=X=REGISTER==================
void AlienCPU6502::_86_STX_ZeroPage_Instruction() {

}

void AlienCPU6502::_8E_STX_Absolute_Instruction() {

}

void AlienCPU6502::_96_STX_ZeroPage_YIndexed_Instruction() {

}

// ===================STORE=Y=REGISTER==================
void AlienCPU6502::_84_STY_ZeroPage_Instruction() {

}

void AlienCPU6502::_8C_STY_Absolute_Instruction() {

}

void AlienCPU6502::_94_STY_ZeroPage_XIndexed_Instruction() {

}

// =========TRANSFER=ACCUMULATOR=TO=X=REGISTER==========
void AlienCPU6502::_AA_TAX_Implied_Instruction() {

}

// =========TRANSFER=ACCUMULATOR=TO=Y=REGISTER==========
void AlienCPU6502::_A8_TAY_Implied_Instruction() {

}

// ========TRANSFER=STACK=POINTER=TO=X=REGISTER=========
void AlienCPU6502::_BA_TSX_Implied_Instruction() {

}

// =========TRANSFER=X=REGISTER=TO=ACCUMULATOR==========
void AlienCPU6502::_8A_TXA_Implied_Instruction() {

}

// ========TRANSFER=X=REGISTER=TO=STACK=POINTER=========
void AlienCPU6502::_9A_TXS_Implied_Instruction() {

}

// =========TRANSFER=Y=REGISTER=TO=ACCUMULATOR==========
void AlienCPU6502::_98_TYA_Implied_Instruction() {

}


// ========================STACK=========================
// ===================PUSH=ACCUMULATOR===================
void AlienCPU6502_48_PHA_Implied_Instruction() {

}

// =================PUSH=PROCESSOR=STATUS================
void AlienCPU6502_08_PHP_Implied_Instruction() {

}

// ===================POP=ACCUMULATOR====================
void AlienCPU6502_68_PLA_Implied_Instruction() {

}

// =================POP=PROCESSOR=STATUS=================
void AlienCPU6502_28_PLP_Implied_Instruction() {

}


// ================DECREMENTS=&=INCREMENTS===============
// ===================DECREMENT=MEMORY===================
void AlienCPU6502_C6_DEC_ZeroPage_Instruction() {

}

void AlienCPU6502_CE_DEC_Absolute_Instruction() {

}

void AlienCPU6502_D6_DEC_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU6502_DE_DEC_Absolute_XIndexed_Instruction() {

}

// =================DECREMENT=X=REGISTER=================
void AlienCPU6502_CA_DEX_Implied_Instruction() {

}

// =================DECREMENT=Y=REGISTER=================
void AlienCPU6502_88_DEY_Implied_Instruction() {

}

// ===================INCREMENT=MEMORY===================
void AlienCPU6502_E6_INC_ZeroPage_Instruction() {

}

void AlienCPU6502_EE_INC_Absolute_Instruction() {

}

void AlienCPU6502_F6_INC_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU6502_FE_INC_Absolute_XIndexed_Instruction() {

}

// =================INCREMENT=X=REGISTER=================
void AlienCPU6502_E8_INX_Implied_Instruction() {

}

// =================INCREMENT=Y=REGISTER=================
void AlienCPU6502_C8_INY_Implied_Instruction() {

}


// =================ARITHMETIC=OPERATIONS================
// =====================ADD=WITH=CARRY===================
void AlienCPU6502::_61_ADC_XIndexed_Indirect_Instruction() {

}

void AlienCPU6502::_65_ADC_ZeroPage_Instruction() {

}

void AlienCPU6502::_69_ADC_Immediate_Instruction() {

}

void AlienCPU6502::_6D_ADC_Absolute_Instruction() {

}

void AlienCPU6502::_71_ADC_Indirect_YIndexed_Instruction() {

}

void AlienCPU6502::_75_ADC_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU6502::_79_ADC_Absolute_YIndexed_Instruction() {

}

void AlienCPU6502::_7D_ADC_Absolute_XIndexed_Instruction() {

}

// =====================SUBTRACT=WITH=BORROW=============
void AlienCPU6502::_E1_SBC_XIndexed_Indirect_Instruction() {

}

void AlienCPU6502::_E5_SBC_ZeroPage_Instruction() {

}

void AlienCPU6502::_E9_SBC_Immediate_Instruction() {

}

void AlienCPU6502::_ED_SBC_Absolute_Instruction() {

}

void AlienCPU6502::_F1_SBC_Indirect_YIndexed_Instruction() {

}

void AlienCPU6502::_F5_SBC_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU6502::_F9_SBC_Absolute_YIndexed_Instruction() {

}

void AlienCPU6502::_FD_SBC_Absolute_XIndexed_Instruction() {

}


// ==================LOGICAL=OPERATIONS==================
// =====================AND=WITH=ACCUMULATOR==============
void AlienCPU6502::_21_AND_XIndexed_Indirect_Instruction() {

}

void AlienCPU6502::_25_AND_ZeroPage_Instruction() {

}

void AlienCPU6502::_29_AND_Immediate_Instruction() {

}

void AlienCPU6502::_2D_AND_Absolute_Instruction() {

}

void AlienCPU6502::_31_AND_Indirect_YIndexed_Instruction() {

}

void AlienCPU6502::_35_AND_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU6502::_39_AND_Absolute_YIndexed_Instruction() {

}

void AlienCPU6502::_3D_AND_Absolute_XIndexed_Instruction() {

}

// =====================EOR=WITH=ACCUMULATOR==============
void AlienCPU6502::_41_EOR_XIndexed_Indirect_Instruction() {

}

void AlienCPU6502::_45_EOR_ZeroPage_Instruction() {

}

void AlienCPU6502::_49_EOR_Immediate_Instruction() {

}

void AlienCPU6502::_4D_EOR_Absolute_Instruction() {

}

void AlienCPU6502::_51_EOR_Indirect_YIndexed_Instruction() {

}

void AlienCPU6502::_55_EOR_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU6502::_59_EOR_Absolute_YIndexed_Instruction() {

}

void AlienCPU6502::_5D_EOR_Absolute_XIndexed_Instruction() {

}

// =====================ORA=WITH=ACCUMULATOR==============
void AlienCPU6502::_01_ORA_XIndexed_Indirect_Instruction() {

}

void AlienCPU6502::_05_ORA_ZeroPage_Instruction() {

}

void AlienCPU6502::_09_ORA_Immediate_Instruction() {

}

void AlienCPU6502::_0D_ORA_Absolute_Instruction() {

}

void AlienCPU6502::_11_ORA_Indirect_YIndexed_Instruction() {

}

void AlienCPU6502::_15_ORA_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU6502::_19_ORA_Absolute_YIndexed_Instruction() {

}

void AlienCPU6502::_1D_ORA_Absolute_XIndexed_Instruction() {

}


// ====================SHIFT=&=ROTATE====================
// =====================ARITHMETIC=SHIFT==================
void AlienCPU6502::_06_ASL_ZeroPage_Instruction() {

}

void AlienCPU6502::_0A_ASL_Accumulator_Instruction() {

}

void AlienCPU6502::_0E_ASL_Absolute_Instruction() {

}

void AlienCPU6502::_16_ASL_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU6502::_1E_ASL_Absolute_XIndexed_Instruction() {

}

// =====================LOGICAL=SHIFT=====================
void AlienCPU6502::_46_LSR_ZeroPage_Instruction() {

}

void AlienCPU6502::_4A_LSR_Accumulator_Instruction() {

}

void AlienCPU6502::_4E_LSR_Absolute_Instruction() {

}

void AlienCPU6502::_56_LSR_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU6502::_5E_LSR_Absolute_XIndexed_Instruction() {

}

// =====================ROTATE=LEFT=======================
void AlienCPU6502::_26_ROL_ZeroPage_Instruction() {

}

void AlienCPU6502::_2A_ROL_Accumulator_Instruction() {

}

void AlienCPU6502::_2E_ROL_Absolute_Instruction() {

}

void AlienCPU6502::_36_ROL_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU6502::_3E_ROL_Absolute_XIndexed_Instruction() {

}

// =====================ROTATE=RIGHT======================
void AlienCPU6502::_66_ROR_ZeroPage_Instruction() {

}

void AlienCPU6502::_6A_ROR_Accumulator_Instruction() {

}

void AlienCPU6502::_6E_ROR_Absolute_Instruction() {

}

void AlienCPU6502::_76_ROR_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU6502::_7E_ROR_Absolute_XIndexed_Instruction() {

}


// =========================FLAG==========================
void AlienCPU6502::_18_CLC_Implied_Instruction() {

}

void AlienCPU6502::_D8_CLD_Implied_Instruction() {

}

void AlienCPU6502::_58_CLI_Implied_Instruction() {

}

void AlienCPU6502::_B8_CLV_Implied_Instruction() {

}

void AlienCPU6502::_38_SEC_Implied_Instruction() {

}

void AlienCPU6502::_F8_SED_Implied_Instruction() {

}

void AlienCPU6502::_78_SEI_Implied_Instruction() {

}


// =====================COMPARISONS======================
// =====================COMPARE=ACCUMULATOR==============
void AlienCPU6502::_C1_CMP_XIndexed_Indirect_Instruction() {

}

void AlienCPU6502::_C5_CMP_ZeroPage_Instruction() {

}

void AlienCPU6502::_C9_CMP_Immediate_Instruction() {

}

void AlienCPU6502::_CD_CMP_Absolute_Instruction() {

}

void AlienCPU6502::_D1_CMP_Indirect_YIndexed_Instruction() {

}

void AlienCPU6502::_D5_CMP_ZeroPage_XIndexed_Instruction() {

}

void AlienCPU6502::_D9_CMP_Absolute_YIndexed_Instruction() {

}

void AlienCPU6502::_DD_CMP_Absolute_XIndexed_Instruction() {

}

// =====================COMPARE=X=REGISTER==============
void AlienCPU6502::_E0_CPX_Immediate_Instruction() {

}

void AlienCPU6502::_E4_CPX_ZeroPage_Instruction() {

}

void AlienCPU6502::_EC_CPX_Absolute_Instruction() {

}

// =====================COMPARE=Y=REGISTER==============
void AlienCPU6502::_C0_CPY_Immediate_Instruction() {

}

void AlienCPU6502::_C4_CPY_ZeroPage_Instruction() {

}

void AlienCPU6502::_CC_CPY_Absolute_Instruction() {

}


// ==================CONDITIONAL=BRANCH==================
void AlienCPU6502::_90_BCC_Relative_Instruction() {

}

void AlienCPU6502::_B0_BCS_Relative_Instruction() {

}

void AlienCPU6502::_F0_BEQ_Relative_Instruction() {

}

void AlienCPU6502::_10_BPL_Relative_Instruction() {

}

void AlienCPU6502::_30_BMI_Relative_Instruction() {

}

void AlienCPU6502::_D0_BNE_Relative_Instruction() {

}

void AlienCPU6502::_50_BVC_Relative_Instruction() {

}

void AlienCPU6502::_70_BVS_Relative_Instruction() {

}


// ==================JUMPS=&=SUBROUTINES=================
void AlienCPU6502::_4C_JMP_Absolute_Instruction() {

}

void AlienCPU6502::_6C_JMP_Indirect_Instruction() {

}

void AlienCPU6502::_20_JSR_Absolute_Instruction() {

}

void AlienCPU6502::_60_RTS_Implied_Instruction() {

}


// ====================INTERRUPTS========================
void AlienCPU6502::_02_BRK_Implied_Instruction() {

}

void AlienCPU6502::_40_RTI_Implied_Instruction() {

}


// =========================OTHER=========================
// =======================BIT=TEST========================
void AlienCPU6502::_24_BIT_ZeroPage_Instruction() {

}

void AlienCPU6502::_2C_BIT_Absolute_Instruction() {

}

// ====================NULL=INSTRUCTION===================
// Null Instruction, throws error if called
void AlienCPU6502::_00_NULL_Illegal_Instruction() {
    std::stringstream stream;
    stream << "Error: NULL Instruction" << std::endl;

    throw std::invalid_argument(stream.str());
}


// =====================NO=OPERATION======================
void AlienCPU6502::_04_NOP_ZeroPage_Illegal_Instruction() {

}

void AlienCPU6502::_0C_NOP_Absolute_Illegal_Instruction() {

}

void AlienCPU6502::_14_NOP_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_1A_NOP_Implied_Illegal_Instruction() {

}

void AlienCPU6502::_1C_NOP_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_34_NOP_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_3A_NOP_Implied_Illegal_Instruction() {

}

void AlienCPU6502::_3C_NOP_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_44_NOP_ZeroPage_Illegal_Instruction() {

}

void AlienCPU6502::_54_NOP_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_5A_NOP_Implied_Illegal_Instruction() {

}

void AlienCPU6502::_5C_NOP_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_64_NOP_ZeroPage_Illegal_Instruction() {

}

void AlienCPU6502::_74_NOP_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_7A_NOP_Implied_Illegal_Instruction() {

}

void AlienCPU6502::_7C_NOP_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_80_NOP_Immediate_Illegal_Instruction() {

}

void AlienCPU6502::_82_NOP_Immediate_Illegal_Instruction() {

}

void AlienCPU6502::_89_NOP_Immediate_Illegal_Instruction() {

}

void AlienCPU6502::_C2_NOP_Immediate_Illegal_Instruction() {

}

void AlienCPU6502::_D4_NOP_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_DA_NOP_Implied_Illegal_Instruction() {

}

void AlienCPU6502::_DC_NOP_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_E2_NOP_Immediate_Illegal_Instruction() {

}

void AlienCPU6502::_EA_NOP_Implied_Instruction() {

}

void AlienCPU6502::_F4_NOP_ZeroPage_XIndexed_Illegal_Instruction() {

}


// ========================ILLEGAL========================
void AlienCPU6502::_4B_ALR_Immediate_Illegal_Instruction() {

}

void AlienCPU6502::_0B_ANC_Immediate_Illegal_Instruction() {

}

void AlienCPU6502::_2B_ANC_Immediate_Illegal_Instruction() { // ANC 2

}

void AlienCPU6502::_8B_ANE_Immediate_Illegal_Instruction() {

}

void AlienCPU6502::_6B_ARR_Immediate_Illegal_Instruction() {

}

void AlienCPU6502::_C3_DCP_XIndexed_Indirect_Illegal_Instruction() {

}

void AlienCPU6502::_C7_DCP_ZeroPage_Illegal_Instruction() {

}

void AlienCPU6502::_CF_DCP_Absolute_Illegal_Instruction() {

}

void AlienCPU6502::_D3_DCP_Indirect_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_D7_DCP_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_DB_DCP_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_DF_DCP_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_E7_ISC_ZeroPage_Illegal_Instruction() {

}

void AlienCPU6502::_EF_ISC_Absolute_Illegal_Instruction() {

}

void AlienCPU6502::_F3_ISC_Indirect_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_F7_ISC_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_FB_ISC_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_FF_ISC_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_BB_LAS_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_A3_LAX_XIndexed_Indirect_Illegal_Instruction() {

}

void AlienCPU6502::_A7_LAX_ZeroPage_Illegal_Instruction() {

}

void AlienCPU6502::_AF_LAX_Absolute_Illegal_Instruction() {

}

void AlienCPU6502::_B3_LAX_Indirect_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_B7_LAX_ZeroPage_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_BF_LAX_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_AB_LXA_Immediate_Illegal_Instruction() {

}


// ========================JAMS===========================
// Freezes the CPU indefinitely in T1 phase with $FFFF on the data bus, requires reset
// https://www.nesdev.org/wiki/Visual6502wiki/6502_Timing_States (for T- phases)
void AlienCPU6502::_12_JAM_Illegal_Instruction() {

}

void AlienCPU6502::_22_JAM_Illegal_Instruction() {

}

void AlienCPU6502::_32_JAM_Illegal_Instruction() {

}

void AlienCPU6502::_42_JAM_Illegal_Instruction() {

}

void AlienCPU6502::_52_JAM_Illegal_Instruction() {

}

void AlienCPU6502::_62_JAM_Illegal_Instruction() {

}

void AlienCPU6502::_72_JAM_Illegal_Instruction() {

}

void AlienCPU6502::_92_JAM_Illegal_Instruction() {

}

void AlienCPU6502::_B2_JAM_Illegal_Instruction() {

}

void AlienCPU6502::_D2_JAM_Illegal_Instruction() {

}

void AlienCPU6502::_F2_JAM_Illegal_Instruction() {

}

void AlienCPU6502::_23_RLA_XIndexed_Indirect_Illegal_Instruction() {

}

void AlienCPU6502::_27_RLA_ZeroPage_Illegal_Instruction() {

}

void AlienCPU6502::_2F_RLA_Absolute_Illegal_Instruction() {

}

void AlienCPU6502::_33_RLA_Indirect_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_37_RLA_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_3B_RLA_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_3F_RLA_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_63_RRA_XIndexed_Indirect_Illegal_Instruction() {

}

void AlienCPU6502::_67_RRA_ZeroPage_Illegal_Instruction() {

}

void AlienCPU6502::_6F_RRA_Absolute_Illegal_Instruction() {

}

void AlienCPU6502::_73_RRA_Indirect_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_77_RRA_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_7B_RRA_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_7F_RRA_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_83_SAX_XIndexed_Indirect_Illegal_Instruction() {

}

void AlienCPU6502::_87_SAX_ZeroPage_Illegal_Instruction() {

}

void AlienCPU6502::_8F_SAX_Absolute_Illegal_Instruction() {

}

void AlienCPU6502::_97_SAX_ZeroPage_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_CB_SBX_Immediate_Illegal_Instruction() {

}

void AlienCPU6502::_93_SHA_Indirect_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_9F_SHA_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_9E_SHX_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_9C_SHY_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_03_SLO_XIndexed_Indirect_Illegal_Instruction() {

}

void AlienCPU6502::_07_SLO_ZeroPage_Illegal_Instruction() {

}

void AlienCPU6502::_0F_SLO_Absolute_Illegal_Instruction() {

}

void AlienCPU6502::_13_SLO_Indirect_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_17_SLO_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_1B_SLO_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_1F_SLO_Absolute_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_43_SRE_XIndexed_Indirect_Illegal_Instruction() {

}

void AlienCPU6502::_47_SRE_ZeroPage_Illegal_Instruction() {

}

void AlienCPU6502::_4F_SRE_Absolute_Illegal_Instruction() {

}

void AlienCPU6502::_53_SRE_Indirect_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_57_SRE_ZeroPage_XIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_5B_SRE_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_5F_SRE_Absolute_XIndexed_Illegal_Instruction() {

}


void AlienCPU6502::_9B_TAS_Absolute_YIndexed_Illegal_Instruction() {

}

void AlienCPU6502::_EB_USBC_Immediate_Illegal_Instruction() {

}
