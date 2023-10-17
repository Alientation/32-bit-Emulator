#include "AlienCPU.h"


// ====================INSTRUCTIONS======================
// Every cycle must memory read or write (using the pins for address and return data)
// To add, it must be done after it is read (cannot be done in the same cycle it was read)
// It must also have a useless memory read (not write because we do not want to change program data)
// 
// Although the cycle counts and what is done at each cycle may not be truly accurate to the 6502,
// it closely emulates the process of the 6502's instruction cycles. There are some exceptions in
// where the 6502 had variable cycle counts depending on pagecrossing, this cpu simply uses an extra
// cycle to allow for pagecrossing even if it is not necessary.
//
// https://www.nesdev.org/6502_cpu.txt

// Sets ZERO flag if the modified register is 0 and NEGATIVE flag if the 
// last bit of the modified register is set
void AlienCPU::UPDATE_FLAGS(u16 modifiedValue) {
    setFlag(Z_FLAG, modifiedValue == 0);
    setFlag(N_FLAG, modifiedValue >> 15);
}

// ======================TRANSFER======================== TODO: document which addressing modes are suppoorted and the bytes and cycles taken
// ===================LOAD=ACCUMULATOR===================
// AFFECTS FLAGS: Z, N
// +2 cycles
void AlienCPU::LDA_Instruction(Word address) {
    A = readTwoBytes(address);
    UPDATE_FLAGS(A);
}


// ===================LOAD=X=REGISTER===================
// AFFECTS FLAGS: Z, N
// +2 cycles
void AlienCPU::LDX_Instruction(Word address) {
    X = readTwoBytes(address);
    UPDATE_FLAGS(X);
}


// ===================LOAD=Y=REGISTER===================
// AFFECTS FLAGS: Z, N
// +2 cycles
void AlienCPU::LDY_Instruction(Word address) {
    Y = readTwoBytes(address);
    UPDATE_FLAGS(Y);
}


// ==================STORE=ACCUMULATOR==================
// AFFECTS FLAGS: NONE
// +2 cycles
void AlienCPU::STA_Instruction(Word address) {
    writeTwoBytes(address, A);
}


// ===================STORE=X=REGISTER==================
// AFFECTS FLAGS: NONE
// +2 cycles
void AlienCPU::STX_Instruction(Word address) {
    writeTwoBytes(address, X);
}


// ===================STORE=Y=REGISTER==================
// AFFECTS FLAGS: NONE
// +2 cycles
void AlienCPU::STY_Instruction(Word address) {
    writeTwoBytes(address, Y);
}


// =========TRANSFER=ACCUMULATOR=TO=X=REGISTER==========
// AFFECTS FLAGS: Z, N
void AlienCPU::TAX_Instruction(Word address) {
    X = A;
    UPDATE_FLAGS(X);
}


// =========TRANSFER=ACCUMULATOR=TO=Y=REGISTER==========
// AFFECTS FLAGS: Z, N
void AlienCPU::TAY_Instruction(Word address) {
    Y = A;
    UPDATE_FLAGS(Y);
}


// ========TRANSFER=STACK=POINTER=TO=X=REGISTER=========
// AFFECTS FLAGS: Z, N
void AlienCPU::TSX_Instruction(Word address) {
    X = SP;
    UPDATE_FLAGS(X);
}


// =========TRANSFER=X=REGISTER=TO=ACCUMULATOR==========
// AFFECTS FLAGS: Z, N
void AlienCPU::TXA_Instruction(Word address) {
    A = X;
    UPDATE_FLAGS(A);
}


// ========TRANSFER=X=REGISTER=TO=STACK=POINTER=========
// AFFECTS FLAGS: Z, N
void AlienCPU::TXS_Instruction(Word address) {
    SP = X;
    UPDATE_FLAGS(SP);
}


// =========TRANSFER=Y=REGISTER=TO=ACCUMULATOR==========
// AFFECTS FLAGS: Z, N
void AlienCPU::TYA_Instruction(Word address) {
    A = Y;
    UPDATE_FLAGS(A);
}


// ========================STACK=========================
// ===================PUSH=ACCUMULATOR===================
// AFFECTS FLAGS: NONE
// +1 cycles
void AlienCPU::PHA_Instruction(Word address) {
    pushTwoBytesToStack(A);
    cycles++;
}


// =================PUSH=PROCESSOR=STATUS================
// AFFECTS FLAGS: Z, N, C, V, D, I
void AlienCPU::PHP_Instruction(Word address) {
    pushByteToStack(P);
}


// ===================POP=ACCUMULATOR====================
// TODO: correct tests to check for Z and N flags
// AFFECTS FLAGS: Z, N
// +1 cycles
void AlienCPU::PLA_Instruction(Word address) {
    A = popTwoBytesFromStack();
    UPDATE_FLAGS(A);
    cycles++;
}


// =================POP=PROCESSOR=STATUS=================
// AFFECTS FLAGS: Z, N, C, V, D, I
void AlienCPU::PLP_Instruction(Word address) {
    P = popByteFromStack();
}


// ================DECREMENTS=&=INCREMENTS===============
// ===================DECREMENT=MEMORY===================
// AFFECTS FLAGS: Z
// +3 cycles
void AlienCPU::DEC_Instruction(Word address) {
    motherboard[address]--; cycles+=3;
    UPDATE_FLAGS(motherboard[address]);
}


// =================DECREMENT=X=REGISTER=================
// AFFECTS FLAGS: Z, N
void AlienCPU::DEX_Instruction(Word address) {
    X--;
    UPDATE_FLAGS(X);
}


// =================DECREMENT=Y=REGISTER=================
// AFFECTS FLAGS: Z, N
void AlienCPU::DEY_Instruction(Word address) {
    Y--;
    UPDATE_FLAGS(Y);
}


// ===================INCREMENT=MEMORY===================
// AFFECTS FLAGS: Z
// +3 cycles
void AlienCPU::INC_Instruction(Word address) {
    motherboard[address]++; cycles+=3;
    UPDATE_FLAGS(motherboard[address]);
}


// =================INCREMENT=X=REGISTER=================
// AFFECTS FLAGS: Z, N
void AlienCPU::INX_Instruction(Word address) {
    X++;
    UPDATE_FLAGS(X);
}


// =================INCREMENT=Y=REGISTER=================
// AFFECTS FLAGS: Z, N
void AlienCPU::INY_Instruction(Word address) {
    Y++;
    UPDATE_FLAGS(Y);
}


// =================ARITHMETIC=OPERATIONS================
// =====================ADD=WITH=CARRY===================
// AFFECTS FLAGS: Z, V, N, C
// + 2 cycles
void AlienCPU::ADC_Instruction(Word address) {
    u16 value = readTwoBytes(address);
    u16 result = A + value + getFlag(C_FLAG);
    
    // update carry if overflow happens
    setFlag(C_FLAG, result < A);
    
    // set overflow flag if adding two same signed numbers results in different sign
    clearFlag(V_FLAG);
    if ((A & NEGATIVE_MASK) == (value & NEGATIVE_MASK)) {
        setFlag(V_FLAG, (result & NEGATIVE_MASK) != (A & NEGATIVE_MASK));
    }

    UPDATE_FLAGS(result);
    A = result;
}


// =====================SUBTRACT=WITH=BORROW=============
// TODO: correct tests to check for Z, N, and V flags
// AFFECTS FLAGS: V, N, Z, C
// + 2 cycles
void AlienCPU::SBC_Instruction(Word address) {
    u16 value = readTwoBytes(address);
    u16 result = A - value - !getFlag(C_FLAG);

    // set carry (represented negation of borrow) if no underflow happens
    setFlag(C_FLAG, result <= A);

    // set overflow flag if subtracting two same signed numbers results in different sign
    clearFlag(V_FLAG);
    if ((A & NEGATIVE_MASK) != (value & NEGATIVE_MASK)) {
        setFlag(V_FLAG, (result & NEGATIVE_MASK) != (A & NEGATIVE_MASK));
    }

    UPDATE_FLAGS(result);
    A = result;
}


// ==================LOGICAL=OPERATIONS==================
// =====================AND=WITH=ACCUMULATOR==============
// AFFECTS FLAGS: Z, N
// +2 cycles
void AlienCPU::AND_Instruction(Word address) {
    A &= readTwoBytes(address);
    UPDATE_FLAGS(A);
}


// =====================EOR=WITH=ACCUMULATOR==============
// AFFECTS FLAGS: Z, N
// +2 cycles
void AlienCPU::EOR_Instruction(Word address) {
    A ^= readTwoBytes(address);
    UPDATE_FLAGS(A);
}


// =====================ORA=WITH=ACCUMULATOR==============
// AFFECTS FLAGS: Z, N
// +2 cycles
void AlienCPU::ORA_Instruction(Word address) {
    A |= readTwoBytes(address);
    UPDATE_FLAGS(A);
}


// ====================SHIFT=&=ROTATE====================
// =============ARITHMETIC=SHIFT=ACCUMULATOR=============
void AlienCPU::ASL_Accumulator_Instruction(Word address) {
    setFlag(C_FLAG, A >> 15); // sets carry flag to the value of the 16th bit (that is rotated off)
    A <<= 1;
    UPDATE_FLAGS(A);
}


// ===================ARITHMETIC=SHIFT===================
// +3 cycles
void AlienCPU::ASL_Instruction(Word address) {
    setFlag(C_FLAG, motherboard[address] >> 7); // sets carry flag to the value of the 8th bit (that is rotated off)
    motherboard[address] <<= 1; cycles += 3;
    UPDATE_FLAGS(motherboard[address]);
}


// ==============LOGICAL=SHIFT=ACCUMULATOR===============
void AlienCPU::LSR_Accumulator_Instruction(Word address) {
    setFlag(C_FLAG, A & 0x0001); // sets carry flag to the value of the 1st bit (that is rotated off)
    A >>= 1; // right shift, sign does not carry since A is unsigned short
    UPDATE_FLAGS(A);
}


// ====================LOGICAL=SHIFT=====================
// +3 cycles
void AlienCPU::LSR_Instruction(Word address) {
    setFlag(C_FLAG, motherboard[address] & 0x0001); // sets carry flag to the value of the 1st bit (that is rotated off)
    motherboard[address] >>= 1; cycles += 3;
    UPDATE_FLAGS(motherboard[address]);
}


// =====================ROTATE=LEFT=======================
void AlienCPU::ROL_Accumulator_Instruction(Word address) {

}


void AlienCPU::ROL_Instruction(Word address) {

}


// =====================ROTATE=RIGHT======================
void AlienCPU::ROR_Accumulator_Instruction(Word address) {

}


void AlienCPU::ROR_Instruction(Word address) {

}


// =========================FLAG==========================
void AlienCPU::CLC_Instruction(Word address) {

}


void AlienCPU::CLD_Instruction(Word address) {

}


void AlienCPU::CLI_Instruction(Word address) {

}


void AlienCPU::CLV_Instruction(Word address) {

}


void AlienCPU::SEC_Instruction(Word address) {

}


void AlienCPU::SED_Instruction(Word address) {

}


void AlienCPU::SEI_Instruction(Word address) {

}


// =====================COMPARISONS======================
// =====================COMPARE=ACCUMULATOR==============
void AlienCPU::CMP_Instruction(Word address) {

}


// =====================COMPARE=X=REGISTER==============
void AlienCPU::CPX_Instruction(Word address) {

}


// =====================COMPARE=Y=REGISTER==============
void AlienCPU::CPY_Instruction(Word address) {

}


// ==================CONDITIONAL=BRANCH==================
void AlienCPU::BCC_Instruction(Word address) {

}


void AlienCPU::BCS_Instruction(Word address) {

}


void AlienCPU::BEQ_Instruction(Word address) {

}


void AlienCPU::BPL_Instruction(Word address) {

}


void AlienCPU::BMI_Instruction(Word address) {

}


void AlienCPU::BNE_Instruction(Word address) {

}


void AlienCPU::BVC_Instruction(Word address) {

}


void AlienCPU::BVS_Instruction(Word address) {

}


// ==================JUMPS=&=SUBROUTINES=================
void AlienCPU::JMP_Instruction(Word address) {

}


void AlienCPU::JSR_Instruction(Word address) {

}


void AlienCPU::RTS_Instruction(Word address) {

}


// ====================INTERRUPTS========================
void AlienCPU::BRK_Instruction(Word address) {

}


void AlienCPU::RTI_Instruction(Word address) {

}


// =========================OTHER=========================
// =======================BIT=TEST========================
void AlienCPU::BIT_Instruction(Word address) {

}


// ====================NULL=INSTRUCTION===================
// Null Instruction, throws error if called
void AlienCPU::NULL_Illegal_Instruction(Word address) {
    std::stringstream stream;
    stream << std::endl << "Error: NULL Instruction" << std::endl;

    throw std::invalid_argument(stream.str());
}


// =====================NO=OPERATION======================
void AlienCPU::NOP_Illegal_Instruction(Word address) {
    std::cout << std::endl << "NO OPERATION" << std::endl;
}


// ========================ILLEGAL========================
void AlienCPU::ALR_Illegal_Instruction(Word address) {

}


void AlienCPU::ANC_Illegal_Instruction(Word address) {

}


void AlienCPU::ANC2_Illegal_Instruction(Word address) {

}


void AlienCPU::ANE_Illegal_Instruction(Word address) {

}


void AlienCPU::ARR_Illegal_Instruction(Word address) {

}


void AlienCPU::DCP_Illegal_Instruction(Word address) {

}


void AlienCPU::ISC_Illegal_Instruction(Word address) {

}


void AlienCPU::LAS_Illegal_Instruction(Word address) {


}

void AlienCPU::LAX_Illegal_Instruction(Word address) {

}


void AlienCPU::LXA_Illegal_Instruction(Word address) {

}


// ========================JAMS===========================
// Freezes the CPU indefinitely in T1 phase with $FFFF on the data bus, requires reset
// https://www.nesdev.org/wiki/Visual6502wiki/6502_Timing_States (for T- phases)
void AlienCPU::JAM_Illegal_Instruction(Word address) {

}


void AlienCPU::RLA_Illegal_Instruction(Word address) {

}


void AlienCPU::RRA_Illegal_Instruction(Word address) {

}


void AlienCPU::SAX_Illegal_Instruction(Word address) {

}


void AlienCPU::SBX_Illegal_Instruction(Word address) {

}


void AlienCPU::SHA_Illegal_Instruction(Word address) {

}


void AlienCPU::SHX_Illegal_Instruction(Word address) {

}


void AlienCPU::SHY_Illegal_Instruction(Word address) {

}


void AlienCPU::SLO_Illegal_Instruction(Word address) {

}


void AlienCPU::SRE_Illegal_Instruction(Word address) {

}


void AlienCPU::TAS_Illegal_Instruction(Word address) {

}


void AlienCPU::USBC_Illegal_Instruction(Word address) {

}