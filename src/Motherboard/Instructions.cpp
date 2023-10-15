#include "AlienCPU.h"


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
// There are a few instances where there are *useless* memory reads/writes to allow for the performing of addition
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
// TODO: figure out a way to have a single instruction for each instruction and its addressable modes
//      and have addressing modes be functions that return the correct value to use
//

// Sets ZERO flag if the modified register is 0 and NEGATIVE flag if the 
// last bit of the modified register is set
void AlienCPU::UPDATE_FLAGS(u16 modifiedValue) {
    setFlag(Z_FLAG, modifiedValue == 0);
    setFlag(N_FLAG, modifiedValue >> 15);
}

// ======================TRANSFER========================
// ===================LOAD=ACCUMULATOR===================
// AFFECTS FLAGS: Z, N
// LOAD ACCUMULATOR IMMEDIATE ($A9 | 3 bytes | 3 cycles)
// 1-3: Immediate addressing mode load value
void AlienCPU::_A9_LDA_Immediate_Instruction() {
    A = ADDRESSING_IMMEDIATE_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// LOAD ACCUMULATOR ABSOLUTE ($AD | 5 bytes | 7 cycles)
// 1-7: Absolute addressing mode load value
void AlienCPU::_AD_LDA_Absolute_Instruction() {
    A = ADDRESSING_ABSOLUTE_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// LOAD ACCUMULATOR ABSOLUTE X-INDEXED ($BD | 5 bytes | 7-9 cycles)
// 1-7/9: Absolute indexed addressing mode load value
void AlienCPU::_BD_LDA_Absolute_XIndexed_Instruction() {
    A = ADDRESSING_ABSOLUTE_INDEXED_READ_TWOBYTES(X);
    UPDATE_FLAGS(A);
}

// LOAD ACCUMULATOR ABSOLUTE Y-INDEXED ($B9 | 5 bytes | 7-9 cycles)
// 1-7/9: Absolute indexed addressing mode load value
void AlienCPU::_B9_LDA_Absolute_YIndexed_Instruction() {
    A = ADDRESSING_ABSOLUTE_INDEXED_READ_TWOBYTES(Y);
    UPDATE_FLAGS(A);
}

// LOAD ACCUMULATOR X-INDEXED INDIRECT ($A1 | 3 bytes | 10 cycles)
// 1-10: X indexed indirect addressing mode load value
void AlienCPU::_A1_LDA_XIndexed_Indirect_Instruction() {
    A = ADDRESSING_XINDEXED_INDIRECT_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// LOAD ACCUMULATOR INDIRECT Y-INDEXED ($B1 | 3 bytes | 9-11 cycles)
// 1-9/11: Indirect Y indexed addressing mode load value
void AlienCPU::_B1_LDA_Indirect_YIndexed_Instruction() {
    A = ADDRESSING_INDIRECT_YINDEXED_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// LOAD ACCUMULATOR ZEROPAGE ($A5 | 3 bytes | 5 cycles)
// 1-5: Zero page addressing mode load value
void AlienCPU::_A5_LDA_ZeroPage_Instruction() {
    A = ADDRESSING_ZEROPAGE_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// LOAD ACCUMULATOR ZEROPAGE X-INDEXED ($B5 | 3 bytes | 6 cycles)
// 1-6: Zero page indexed addressing mode load value
void AlienCPU::_B5_LDA_ZeroPage_XIndexed_Instruction() {
    A = ADDRESSING_ZEROPAGE_INDEXED_READ_TWOBYTES(X);
    UPDATE_FLAGS(A);
}


// ===================LOAD=X=REGISTER===================
// AFFECTS FLAGS: Z, N
// LOAD X IMMEDIATE ($A9 | 3 bytes | 3 cycles)
// 1-3: Immediate addressing mode load value
void AlienCPU::_A2_LDX_Immediate_Instruction() {
    X = ADDRESSING_IMMEDIATE_READ_TWOBYTES();
    UPDATE_FLAGS(X);
}

// LOAD X ABSOLUTE ($AD | 5 bytes | 7 cycles)
// 1-7: Absolute addressing mode load value
void AlienCPU::_AE_LDX_Absolute_Instruction() {
    X = ADDRESSING_ABSOLUTE_READ_TWOBYTES();
    UPDATE_FLAGS(X);
}

// LOAD X ABSOLUTE Y-INDEXED ($BD | 5 bytes | 7-9 cycles)
// 1-7/9: Absolute indexed addressing mode load value
void AlienCPU::_BE_LDX_Absolute_YIndexed_Instruction() {
    X = ADDRESSING_ABSOLUTE_INDEXED_READ_TWOBYTES(Y);
    UPDATE_FLAGS(X);
}

// LOAD X ZEROPAGE ($A5 | 3 bytes | 5 cycles)
// 1-5: Zero page addressing mode load value
void AlienCPU::_A6_LDX_ZeroPage_Instruction() {
    X = ADDRESSING_ZEROPAGE_READ_TWOBYTES();
    UPDATE_FLAGS(X);
}

// LOAD X ZEROPAGE Y-INDEXED ($B5 | 3 bytes | 6 cycles)
// 1-6: Zero page indexed addressing mode load value
void AlienCPU::_B6_LDX_ZeroPage_YIndexed_Instruction() {
    X = ADDRESSING_ZEROPAGE_INDEXED_READ_TWOBYTES(Y);
    UPDATE_FLAGS(X);
}


// ===================LOAD=Y=REGISTER===================
// AFFECTS FLAGS: Z, N
// LOAD Y IMMEDIATE ($A9 | 3 bytes | 3 cycles)
// 1-3: Immediate addressing mode load value
void AlienCPU::_A0_LDY_Immediate_Instruction() {
    Y = ADDRESSING_IMMEDIATE_READ_TWOBYTES();
    UPDATE_FLAGS(Y);
}

// LOAD Y ABSOLUTE ($AD | 5 bytes | 7 cycles)
// 1-7: Absolute addressing mode load value
void AlienCPU::_AC_LDY_Absolute_Instruction() {
    Y = ADDRESSING_ABSOLUTE_READ_TWOBYTES();
    UPDATE_FLAGS(Y);
}

// LOAD Y ABSOLUTE X-INDEXED ($BD | 5 bytes | 7-9 cycles)
// 1-7/9: Absolute indexed addressing mode load value
void AlienCPU::_BC_LDY_Absolute_XIndexed_Instruction() {
    Y = ADDRESSING_ABSOLUTE_INDEXED_READ_TWOBYTES(X);
    UPDATE_FLAGS(Y);
}

// LOAD Y ZEROPAGE ($A5 | 3 bytes | 5 cycles)
// 1-5: Zero page addressing mode load value
void AlienCPU::_A4_LDY_ZeroPage_Instruction() {
    Y = ADDRESSING_ZEROPAGE_READ_TWOBYTES();
    UPDATE_FLAGS(Y);
}

// LOAD Y ZEROPAGE X-INDEXED ($B5 | 3 bytes | 6 cycles)
// 1-6: Zero page indexed addressing mode load value
void AlienCPU::_B4_LDY_ZeroPage_XIndexed_Instruction() {
    Y = ADDRESSING_ZEROPAGE_INDEXED_READ_TWOBYTES(X);
    UPDATE_FLAGS(Y);
}


// ==================STORE=ACCUMULATOR==================
// AFFECTS FLAGS: NONE
// STORE ACCUMULATOR ABSOLUTE ($8D | 5 bytes | 7 cycles)
// 1-7: Absolute addressing mode store value
void AlienCPU::_8D_STA_Absolute_Instruction() {
    ADDRESSING_ABSOLUTE_WRITE_TWOBYTES(A);
}

// STORE ACCUMULATOR ABSOLUTE X-INDEXED ($9D | 5 bytes | 7 cycles)
// 1-8: Absolute indexed addressing mode store value
void AlienCPU::_9D_STA_Absolute_XIndexed_Instruction() {
    ADDRESSING_ABSOLUTE_INDEXED_WRITE_TWOBYTES(X, A);
}

// STORE ACCUMULATOR ABSOLUTE Y-INDEXED ($99 | 5 bytes | 7 cycles)
// 1-8: Absolute indexed addressing mode store value
void AlienCPU::_99_STA_Absolute_YIndexed_Instruction() {
    ADDRESSING_ABSOLUTE_INDEXED_WRITE_TWOBYTES(Y, A);
}

// STORE ACCUMULATOR X-INDEXED INDIRECT ($81 | 3 bytes | 10 cycles)
// 1-10: X indexed indirect addressing mode store value
void AlienCPU::_81_STA_XIndexed_Indirect_Instruction() {
    ADDRESSING_XINDEXED_INDIRECT_WRITE_TWOBYTES(A);
}

// STORE ACCUMULATOR INDIRECT Y-INDEXED ($91 | 3 bytes | 10 cycles)
// 1-10: Indirect Y indexed addressing mode store value
void AlienCPU::_91_STA_Indirect_YIndexed_Instruction() {
    ADDRESSING_INDIRECT_YINDEXED_WRITE_TWOBYTES(A);
}

// STORE ACCUMULATOR ZEROPAGE ($85 | 3 bytes | 5 cycles)
// 1-5: Zero page addressing mode store value
void AlienCPU::_85_STA_ZeroPage_Instruction() {
    ADDRESSING_ZEROPAGE_WRITE_TWOBYTES(A);
}

// STORE ACCUMULATOR ZEROPAGE X-INDEXED ($95 | 3 bytes | 6 cycles)
// 1-6: Zero page indexed addressing mode store value
void AlienCPU::_95_STA_ZeroPage_XIndexed_Instruction() {
    ADDRESSING_ZEROPAGE_INDEXED_WRITE_TWOBYTES(X, A);
}


// ===================STORE=X=REGISTER==================
// AFFECTS FLAGS: NONE
// STORE X REGISTER ABSOLUTE ($8E | 5 bytes | 7 cycles)
// 1-7: Absolute addressing mode store value
void AlienCPU::_8E_STX_Absolute_Instruction() {
    ADDRESSING_ABSOLUTE_WRITE_TWOBYTES(X);
}

// STORE X REGISTER ZEROPAGE ($86 | 3 bytes | 5 cycles)
// 1-5: Zero page addressing mode store value
void AlienCPU::_86_STX_ZeroPage_Instruction() {
    ADDRESSING_ZEROPAGE_WRITE_TWOBYTES(X);
}

// STORE X REGISTER ZEROPAGE Y-INDEXED ($96 | 3 bytes | 6 cycles)
// 1-6: Zero page indexed addressing mode store value
void AlienCPU::_96_STX_ZeroPage_YIndexed_Instruction() {
    ADDRESSING_ZEROPAGE_INDEXED_WRITE_TWOBYTES(Y, X);
}


// ===================STORE=Y=REGISTER==================
// AFFECTS FLAGS: NONE
// STORE Y REGISTER ABSOLUTE ($8C | 5 bytes | 7 cycles)
// 1-7: Absolute addressing mode store value
void AlienCPU::_8C_STY_Absolute_Instruction() {
    ADDRESSING_ABSOLUTE_WRITE_TWOBYTES(Y);
}

// STORE Y REGISTER ZEROPAGE ($84 | 3 bytes | 3 cycles)
// 1-3: Zero page addressing mode store value
void AlienCPU::_84_STY_ZeroPage_Instruction() {
    ADDRESSING_ZEROPAGE_WRITE_TWOBYTES(Y);
}

// STORE Y REGISTER ZEROPAGE X-INDEXED ($94 | 3 bytes | 4 cycles)
// 1-4: Zero page indexed addressing mode store value
void AlienCPU::_94_STY_ZeroPage_XIndexed_Instruction() {
    ADDRESSING_ZEROPAGE_INDEXED_WRITE_TWOBYTES(X, Y);
}


// =========TRANSFER=ACCUMULATOR=TO=X=REGISTER==========
// AFFECTS FLAGS: Z, N
// TRANSFER ACCUMULATOR TO X REGISTER ($AA | 1 byte | 2 cycles)
// 1-2: Implied addressing mode
void AlienCPU::_AA_TAX_Implied_Instruction() {
    ADDRESSING_IMPLIED();
    X = A;
    UPDATE_FLAGS(X);
}


// =========TRANSFER=ACCUMULATOR=TO=Y=REGISTER==========
// AFFECTS FLAGS: Z, N
// TRANSFER ACCUMULATOR TO Y REGISTER ($A8 | 1 byte | 2 cycles)
// 1-2: Implied addressing mode
void AlienCPU::_A8_TAY_Implied_Instruction() {
    ADDRESSING_IMPLIED();
    Y = A;
    UPDATE_FLAGS(Y);
}


// ========TRANSFER=STACK=POINTER=TO=X=REGISTER=========
// AFFECTS FLAGS: Z, N
// TRANSFER STACK POINTER TO X REGISTER ($BA | 1 byte | 2 cycles)
// 1-2: Implied addressing mode
void AlienCPU::_BA_TSX_Implied_Instruction() {
    ADDRESSING_IMPLIED();
    X = SP;
    UPDATE_FLAGS(X);
}


// =========TRANSFER=X=REGISTER=TO=ACCUMULATOR==========
// AFFECTS FLAGS: Z, N
// TRANSFER X REGISTER TO ACCUMULATOR ($8A | 1 byte | 2 cycles)
// 1-2: Implied addressing mode
void AlienCPU::_8A_TXA_Implied_Instruction() {
    ADDRESSING_IMPLIED();
    A = X;
    UPDATE_FLAGS(A);
}


// ========TRANSFER=X=REGISTER=TO=STACK=POINTER=========
// AFFECTS FLAGS: Z, N
// TRANSFER X REGISTER TO STACK POINTER ($9A | 1 byte | 2 cycles)
// 1-2: Implied addressing mode
void AlienCPU::_9A_TXS_Implied_Instruction() {
    ADDRESSING_IMPLIED();
    SP = X;
    UPDATE_FLAGS(SP);
}


// =========TRANSFER=Y=REGISTER=TO=ACCUMULATOR==========
// AFFECTS FLAGS: Z, N
// TRANSFER Y REGISTER TO ACCUMULATOR ($98 | 1 byte | 2 cycles)
// 1-2: Implied addressing mode
void AlienCPU::_98_TYA_Implied_Instruction() {
    ADDRESSING_IMPLIED();
    A = Y;
    UPDATE_FLAGS(A);
}


// ========================STACK=========================
// ===================PUSH=ACCUMULATOR===================
// AFFECTS FLAGS: NONE
// PUSH ACCUMULATOR ($48 | 1 byte | 3 cycles)
// 1-2: Implied addressing mode (useless read so the high byte of accumulator can be written to stack)
// 3: useless read so the low byte of accumulator can be written to stack
void AlienCPU::_48_PHA_Implied_Instruction() {
    ADDRESSING_IMPLIED();
    pushTwoBytesToStack(A);
    cycles++;
}


// =================PUSH=PROCESSOR=STATUS================
// AFFECTS FLAGS: Z, N, C, V, D, I
// PUSH PROCESSOR STATUS ($08 | 1 byte | 2 cycles)
// 1-2: Implied addressing mode (useless read so the processor status can be written to stack)
void AlienCPU::_08_PHP_Implied_Instruction() {
    ADDRESSING_IMPLIED();
    pushByteToStack(P);
}


// ===================POP=ACCUMULATOR====================
// TODO: correct tests to check for Z and N flags
// AFFECTS FLAGS: Z, N
// POP ACCUMULATOR ($68 | 1 byte | 3 cycles)
// 1-2: Implied addressing mode (read low byte of accumulator from stack)
// 3: read high byte of accumulator from stack
void AlienCPU::_68_PLA_Implied_Instruction() {
    ADDRESSING_IMPLIED();
    A = popTwoBytesFromStack();
    UPDATE_FLAGS(A);
    cycles++;
}


// =================POP=PROCESSOR=STATUS=================
// AFFECTS FLAGS: Z, N, C, V, D, I
// POP PROCESSOR STATUS ($28 | 1 byte | 2 cycles)
// 1-2: Implied addressing mode (read processor status from stack)
void AlienCPU::_28_PLP_Implied_Instruction() {
    ADDRESSING_IMPLIED();
    P = popByteFromStack();
}


// ================DECREMENTS=&=INCREMENTS===============
// ===================DECREMENT=MEMORY===================
// AFFECTS FLAGS: Z
// DEC MEMORY ABSOLUTE ($CE | 5 bytes | 8 cycles)
// 1-8: Absolute addressing mode (read, decrement, and write value back to memory)
void AlienCPU::_CE_DEC_Absolute_Instruction() {
    Byte* valuePointer = ADDRESSING_ABSOLUTE_READ_MODIFY_WRITE_BYTE();
    (*valuePointer)--;
    UPDATE_FLAGS(*valuePointer);
}

// DEC MEMORY ABSOLUTE X-INDEXED ($DE | 5 bytes | 9 cycles)
// 1-9: Absolute indexed addressing mode (read, decrement, and write value back to memory)
void AlienCPU::_DE_DEC_Absolute_XIndexed_Instruction() {
    Byte* valuePointer = ADDRESSING_ABSOLUTE_INDEXED_READ_MODIFY_WRITE_BYTE(X);
    (*valuePointer)--;
    UPDATE_FLAGS(*valuePointer);
}

// DEC MEMORY ZEROPAGE ($C6 | 3 bytes | 6 cycles)
// 1-6: Zero page addressing mode (read, decrement, and write value back to memory)
void AlienCPU::_C6_DEC_ZeroPage_Instruction() {
    Byte* valuePointer = ADDRESSING_ZEROPAGE_READ_MODIFY_WRITE_BYTE();
    (*valuePointer)--;
    UPDATE_FLAGS(*valuePointer);
}

// DEC MEMORY ZEROPAGE X-INDEXED ($D6 | 3 bytes | 7 cycles)
// 1-7: Zero page indexed addressing mode (read, decrement, and write value back to memory)
void AlienCPU::_D6_DEC_ZeroPage_XIndexed_Instruction() {
    Byte* valuePointer = ADDRESSING_ZEROPAGE_INDEXED_READ_MODIFY_WRITE_BYTE(X);
    (*valuePointer)--;
    UPDATE_FLAGS(*valuePointer);
}


// =================DECREMENT=X=REGISTER=================
// AFFECTS FLAGS: Z, N
// DEC X REGISTER ($CA | 1 byte | 2 cycles)
// 1-2: Implied addressing mode
void AlienCPU::_CA_DEX_Implied_Instruction() {
    ADDRESSING_IMPLIED();
    X--;
    UPDATE_FLAGS(X);
}


// =================DECREMENT=Y=REGISTER=================
// AFFECTS FLAGS: Z, N
// DEC Y REGISTER ($88 | 1 byte | 2 cycles)
// 1-2: Implied addressing mode
void AlienCPU::_88_DEY_Implied_Instruction() {
    ADDRESSING_IMPLIED();
    Y--;
    UPDATE_FLAGS(Y);
}


// ===================INCREMENT=MEMORY===================
// AFFECTS FLAGS: Z
// INC MEMORY ABSOLUTE ($EE | 5 bytes | 8 cycles)
// 1-8: Absolute addressing mode (read, increment, and write value back to memory)
void AlienCPU::_EE_INC_Absolute_Instruction() {
    Byte* valuePointer = ADDRESSING_ABSOLUTE_READ_MODIFY_WRITE_BYTE();
    (*valuePointer)++;
    UPDATE_FLAGS(*valuePointer);
}

// INC MEMORY ABSOLUTE X-INDEXED ($FE | 5 bytes | 9 cycles)
// 1-9: Absolute indexed addressing mode (read, increment, and write value back to memory)
void AlienCPU::_FE_INC_Absolute_XIndexed_Instruction() {
    Byte* valuePointer = ADDRESSING_ABSOLUTE_INDEXED_READ_MODIFY_WRITE_BYTE(X);
    (*valuePointer)++;
    UPDATE_FLAGS(*valuePointer);
}

// INC MEMORY ZEROPAGE ($E6 | 3 bytes | 6 cycles)
// 1-6: Zero page addressing mode (read, increment, and write value back to memory)
void AlienCPU::_E6_INC_ZeroPage_Instruction() {
    Byte* valuePointer = ADDRESSING_ZEROPAGE_READ_MODIFY_WRITE_BYTE();
    (*valuePointer)++;
    UPDATE_FLAGS(*valuePointer);
}

// INC MEMORY ZEROPAGE X-INDEXED ($F6 | 3 bytes | 7 cycles)
// 1-7: Zero page indexed addressing mode (read, increment, and write value back to memory)
void AlienCPU::_F6_INC_ZeroPage_XIndexed_Instruction() {
    Byte* valuePointer = ADDRESSING_ZEROPAGE_INDEXED_READ_MODIFY_WRITE_BYTE(X);
    (*valuePointer)++;
    UPDATE_FLAGS(*valuePointer);
}


// =================INCREMENT=X=REGISTER=================
// AFFECTS FLAGS: Z, N
// INC X REGISTER ($E8 | 1 byte | 2 cycles)
// 1-2: Implied addressing mode
void AlienCPU::_E8_INX_Implied_Instruction() {
    ADDRESSING_IMPLIED();
    X++;
    UPDATE_FLAGS(X);
}


// =================INCREMENT=Y=REGISTER=================
// AFFECTS FLAGS: Z, N
// INC Y REGISTER ($C8 | 1 byte | 2 cycles)
// 1-2: Implied addressing mode
void AlienCPU::_C8_INY_Implied_Instruction() {
    ADDRESSING_IMPLIED();
    Y++;
    UPDATE_FLAGS(Y);
}


// =================ARITHMETIC=OPERATIONS================
// =====================ADD=WITH=CARRY===================
// TODO: correct tests to check for Z and N flags
// AFFECTS FLAGS: Z, N, C
// ADD WITH CARRY IMMEDIATE ($69 | 3 bytes | 3 cycles)
// 1-3: Immediate addressing mode
void AlienCPU::_69_ADC_Immediate_Instruction() {
    u16 value = ADDRESSING_IMMEDIATE_READ_TWOBYTES();
    u16 result = A + value + getFlag(C_FLAG);

    // update carry if overflow happens
    setFlag(C_FLAG, result < A);
    UPDATE_FLAGS(result);
    A = result;
}

// ADD WITH CARRY ABSOLUTE ($6D | 5 bytes | 7 cycles)
// 1-7: Absolute addressing mode
void AlienCPU::_6D_ADC_Absolute_Instruction() {
    u16 value = ADDRESSING_ABSOLUTE_READ_TWOBYTES();
    u16 result = A + value + getFlag(C_FLAG);

    // update carry if overflow happens
    setFlag(C_FLAG, result < A);
    UPDATE_FLAGS(result);
    A = result;
}

// ADD WITH CARRY ABSOLUTE X-INDEXED ($7D | 5 bytes | 7-9 cycles)
// 1-7/9: Absolute indexed addressing mode
void AlienCPU::_7D_ADC_Absolute_XIndexed_Instruction() {
    u16 value = ADDRESSING_ABSOLUTE_INDEXED_READ_TWOBYTES(X);
    u16 result = A + value + getFlag(C_FLAG);

    // update carry if overflow happens
    setFlag(C_FLAG, result < A);
    UPDATE_FLAGS(result);
    A = result;
}

// ADD WITH CARRY ABSOLUTE Y-INDEXED ($79 | 5 bytes | 7-9 cycles)
// 1-7/9: Absolute indexed addressing mode
void AlienCPU::_79_ADC_Absolute_YIndexed_Instruction() {
    u16 value = ADDRESSING_ABSOLUTE_INDEXED_READ_TWOBYTES(Y);
    u16 result = A + value + getFlag(C_FLAG);

    // update carry if overflow happens
    setFlag(C_FLAG, result < A);
    UPDATE_FLAGS(result);
    A = result;
}

// ADD WITH CARRY X-INDEXED INDIRECT ($61 | 3 bytes | 10 cycles)
// 1-10: X indexed indirect addressing mode
void AlienCPU::_61_ADC_XIndexed_Indirect_Instruction() {
    u16 value = ADDRESSING_XINDEXED_INDIRECT_READ_TWOBYTES();
    u16 result = A + value + getFlag(C_FLAG);

    // update carry if overflow happens
    setFlag(C_FLAG, result < A);
    UPDATE_FLAGS(result);
    A = result;
}

// ADD WITH CARRY INDIRECT Y-INDEXED ($71 | 3 bytes | 9-11 cycles)
// 1-9/11: Indirect Y indexed addressing mode
void AlienCPU::_71_ADC_Indirect_YIndexed_Instruction() {
    u16 value = ADDRESSING_INDIRECT_YINDEXED_READ_TWOBYTES();
    u16 result = A + value + getFlag(C_FLAG);

    // update carry if overflow happens
    setFlag(C_FLAG, result < A);
    UPDATE_FLAGS(result);
    A = result;
}

// ADD WITH CARRY ZERO PAGE ($65 | 3 bytes | 5 cycles)
// 1-5: Zero page addressing mode
void AlienCPU::_65_ADC_ZeroPage_Instruction() {
    u16 value = ADDRESSING_ZEROPAGE_READ_TWOBYTES();
    u16 result = A + value + getFlag(C_FLAG);

    // update carry if overflow happens
    setFlag(C_FLAG, result < A);
    UPDATE_FLAGS(result);
    A = result;
}

// ADD WITH CARRY ZERO PAGE X-INDEXED ($75 | 3 bytes | 6 cycles)
// 1-6: Zero page indexed addressing mode
void AlienCPU::_75_ADC_ZeroPage_XIndexed_Instruction() {
    u16 value = ADDRESSING_ZEROPAGE_INDEXED_READ_TWOBYTES(X);
    u16 result = A + value + getFlag(C_FLAG);

    // update carry if overflow happens
    setFlag(C_FLAG, result < A);
    UPDATE_FLAGS(result);
    A = result;
}


// =====================SUBTRACT=WITH=BORROW=============
// TODO: correct tests to check for Z and N flags
// AFFECTS FLAGS: Z, N, C
// SUBTRACT WITH BORROW IMMEDIATE ($E9 | 3 bytes | 3 cycles)
// 1-3: Immediate addressing mode
void AlienCPU::_E9_SBC_Immediate_Instruction() {
    u16 value = ADDRESSING_IMMEDIATE_READ_TWOBYTES();
    u16 result = A - value - getFlag(C_FLAG);

    // update carry (represented as borrow) if underflow happens
    setFlag(C_FLAG, result > A);
    UPDATE_FLAGS(result);
    A = result;
}

// SUBTRACT WITH BORROW ABSOLUTE ($ED | 5 bytes | 7 cycles)
// 1-7: Absolute addressing mode
void AlienCPU::_ED_SBC_Absolute_Instruction() {
    u16 value = ADDRESSING_ABSOLUTE_READ_TWOBYTES();
    u16 result = A - value - getFlag(C_FLAG);

    // update carry (represented as borrow) if underflow happens
    setFlag(C_FLAG, result > A);
    UPDATE_FLAGS(result);
    A = result;
}

// SUBTRACT WITH BORROW ABSOLUTE X-INDEXED ($FD | 5 bytes | 7-9 cycles)
// 1-7/9: Absolute indexed addressing mode
void AlienCPU::_FD_SBC_Absolute_XIndexed_Instruction() {
    u16 value = ADDRESSING_ABSOLUTE_INDEXED_READ_TWOBYTES(X);
    u16 result = A - value - getFlag(C_FLAG);

    // update carry (represented as borrow) if underflow happens
    setFlag(C_FLAG, result > A);
    UPDATE_FLAGS(result);
    A = result;
}

// SUBTRACT WITH BORROW ABSOLUTE Y-INDEXED ($F9 | 5 bytes | 7-9 cycles)
// 1-7/9: Absolute indexed addressing mode
void AlienCPU::_F9_SBC_Absolute_YIndexed_Instruction() {
    u16 value = ADDRESSING_ABSOLUTE_INDEXED_READ_TWOBYTES(Y);
    u16 result = A - value - getFlag(C_FLAG);

    // update carry (represented as borrow) if underflow happens
    setFlag(C_FLAG, result > A);
    UPDATE_FLAGS(result);
    A = result;
}

// SUBTRACT WITH BORROW X-INDEXED INDIRECT ($E1 | 3 bytes | 10 cycles)
// 1-10: X indexed indirect addressing mode
void AlienCPU::_E1_SBC_XIndexed_Indirect_Instruction() {
    u16 value = ADDRESSING_XINDEXED_INDIRECT_READ_TWOBYTES();
    u16 result = A - value - getFlag(C_FLAG);

    // update carry (represented as borrow) if underflow happens
    setFlag(C_FLAG, result > A);
    UPDATE_FLAGS(result);
    A = result;
}

// SUBTRACT WITH BORROW INDIRECT Y-INDEXED ($F1 | 3 bytes | 9-11 cycles)
// 1-9/11: Indirect Y indexed addressing mode
void AlienCPU::_F1_SBC_Indirect_YIndexed_Instruction() {
    u16 value = ADDRESSING_INDIRECT_YINDEXED_READ_TWOBYTES();
    u16 result = A - value - getFlag(C_FLAG);

    // update carry (represented as borrow) if underflow happens
    setFlag(C_FLAG, result > A);
    UPDATE_FLAGS(result);
    A = result;
}

// SUBTRACT WITH BORROW ZERO PAGE ($E5 | 3 bytes | 5 cycles)
// 1-5: Zero page addressing mode
void AlienCPU::_E5_SBC_ZeroPage_Instruction() {
    u16 value = ADDRESSING_ZEROPAGE_READ_TWOBYTES();
    u16 result = A - value - getFlag(C_FLAG);

    // update carry (represented as borrow) if underflow happens
    setFlag(C_FLAG, result > A);
    UPDATE_FLAGS(result);
    A = result;
}

// SUBTRACT WITH BORROW ZERO PAGE X-INDEXED ($F5 | 3 bytes | 6 cycles)
// 1-6: Zero page indexed addressing mode
void AlienCPU::_F5_SBC_ZeroPage_XIndexed_Instruction() {
    u16 value = ADDRESSING_ZEROPAGE_INDEXED_READ_TWOBYTES(X);
    u16 result = A - value - getFlag(C_FLAG);

    // update carry (represented as borrow) if underflow happens
    setFlag(C_FLAG, result > A);
    UPDATE_FLAGS(result);
    A = result;
}


// ==================LOGICAL=OPERATIONS==================
// =====================AND=WITH=ACCUMULATOR==============
// AFFECTS FLAGS: Z, N
// AND WITH ACCUMULATOR IMMEDIATE ($29 | 3 bytes | 3 cycles)
// 1-3: Immediate addressing mode
void AlienCPU::_29_AND_Immediate_Instruction() {
    A &= ADDRESSING_IMMEDIATE_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// AND WITH ACCUMULATOR ABSOLUTE ($2D | 5 bytes | 7 cycles)
// 1-7: Absolute addressing mode
void AlienCPU::_2D_AND_Absolute_Instruction() {
    A &= ADDRESSING_ABSOLUTE_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// AND WITH ACCUMULATOR ABSOLUTE X-INDEXED ($3D | 5 bytes | 7-9 cycles)
// 1-7/9: Absolute indexed addressing mode
void AlienCPU::_3D_AND_Absolute_XIndexed_Instruction() {
    A &= ADDRESSING_ABSOLUTE_INDEXED_READ_TWOBYTES(X);
    UPDATE_FLAGS(A);
}

// AND WITH ACCUMULATOR ABSOLUTE Y-INDEXED ($39 | 5 bytes | 7-9 cycles)
// 1-7/9: Absolute indexed addressing mode
void AlienCPU::_39_AND_Absolute_YIndexed_Instruction() {
    A &= ADDRESSING_ABSOLUTE_INDEXED_READ_TWOBYTES(Y);
    UPDATE_FLAGS(A);
}

// AND WITH ACCUMULATOR X-INDEXED INDIRECT ($21 | 3 bytes | 10 cycles)
// 1-10: X indexed indirect addressing mode
void AlienCPU::_21_AND_XIndexed_Indirect_Instruction() {
    A &= ADDRESSING_XINDEXED_INDIRECT_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// AND WITH ACCUMULATOR INDIRECT Y-INDEXED ($31 | 3 bytes | 9-11 cycles)
// 1-9/11: Indirect Y indexed addressing mode
void AlienCPU::_31_AND_Indirect_YIndexed_Instruction() {
    A &= ADDRESSING_INDIRECT_YINDEXED_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// AND WITH ACCUMULATOR ZERO PAGE ($25 | 3 bytes | 5 cycles)
// 1-5: Zero page addressing mode
void AlienCPU::_25_AND_ZeroPage_Instruction() {
    A &= ADDRESSING_ZEROPAGE_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// AND WITH ACCUMULATOR ZERO PAGE X-INDEXED ($35 | 3 bytes | 6 cycles)
// 1-6: Zero page indexed addressing mode
void AlienCPU::_35_AND_ZeroPage_XIndexed_Instruction() {
    A &= ADDRESSING_ZEROPAGE_INDEXED_READ_TWOBYTES(X);
    UPDATE_FLAGS(A);
}


// =====================EOR=WITH=ACCUMULATOR==============
// AFFECTS FLAGS: Z, N
// EXCLUSIVE OR WITH ACCUMULATOR IMMEDIATE ($49 | 3 bytes | 3 cycles)
// 1-3: Immediate addressing mode
void AlienCPU::_49_EOR_Immediate_Instruction() {
    A ^= ADDRESSING_IMMEDIATE_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// EXCLUSIVE OR WITH ACCUMULATOR ABSOLUTE ($4D | 5 bytes | 7 cycles)
// 1-7: Absolute addressing mode
void AlienCPU::_4D_EOR_Absolute_Instruction() {
    A ^= ADDRESSING_ABSOLUTE_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// EXCLUSIVE OR WITH ACCUMULATOR ABSOLUTE X-INDEXED ($5D | 5 bytes | 7-9 cycles)
// 1-7/9: Absolute indexed addressing mode
void AlienCPU::_5D_EOR_Absolute_XIndexed_Instruction() {
    A ^= ADDRESSING_ABSOLUTE_INDEXED_READ_TWOBYTES(X);
    UPDATE_FLAGS(A);
}

// EXCLUSIVE OR WITH ACCUMULATOR ABSOLUTE Y-INDEXED ($59 | 5 bytes | 7-9 cycles)
// 1-7/9: Absolute indexed addressing mode
void AlienCPU::_59_EOR_Absolute_YIndexed_Instruction() {
    A ^= ADDRESSING_ABSOLUTE_INDEXED_READ_TWOBYTES(Y);
    UPDATE_FLAGS(A);
}

// EXCLUSIVE OR WITH ACCUMULATOR X-INDEXED INDIRECT ($41 | 3 bytes | 10 cycles)
// 1-10: X indexed indirect addressing mode
void AlienCPU::_41_EOR_XIndexed_Indirect_Instruction() {
    A ^= ADDRESSING_XINDEXED_INDIRECT_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// EXCLUSIVE OR WITH ACCUMULATOR INDIRECT Y-INDEXED ($51 | 3 bytes | 9-11 cycles)
// 1-9/11: Indirect Y indexed addressing mode
void AlienCPU::_51_EOR_Indirect_YIndexed_Instruction() {
    A ^= ADDRESSING_INDIRECT_YINDEXED_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// EXCLUSIVE OR WITH ACCUMULATOR ZERO PAGE ($45 | 3 bytes | 5 cycles)
// 1-5: Zero page addressing mode
void AlienCPU::_45_EOR_ZeroPage_Instruction() {
    A ^= ADDRESSING_ZEROPAGE_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// EXCLUSIVE OR WITH ACCUMULATOR ZERO PAGE X-INDEXED ($55 | 3 bytes | 6 cycles)
// 1-6: Zero page indexed addressing mode
void AlienCPU::_55_EOR_ZeroPage_XIndexed_Instruction() {
    A ^= ADDRESSING_ZEROPAGE_INDEXED_READ_TWOBYTES(X);
    UPDATE_FLAGS(A);
}


// =====================ORA=WITH=ACCUMULATOR==============
// AFFECTS FLAGS: Z, N
// INCLUSIVE OR WITH ACCUMULATOR IMMEDIATE ($09 | 3 bytes | 3 cycles)
// 1-3: Immediate addressing mode
void AlienCPU::_09_ORA_Immediate_Instruction() {
    A |= ADDRESSING_IMMEDIATE_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// INCLUSIVE OR WITH ACCUMULATOR ABSOLUTE ($0D | 5 bytes | 7 cycles)
// 1-7: Absolute addressing mode
void AlienCPU::_0D_ORA_Absolute_Instruction() {
    A |= ADDRESSING_ABSOLUTE_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// INCLUSIVE OR WITH ACCUMULATOR ABSOLUTE X-INDEXED ($1D | 5 bytes | 7-9 cycles)
// 1-7/9: Absolute indexed addressing mode
void AlienCPU::_19_ORA_Absolute_YIndexed_Instruction() {
    A |= ADDRESSING_ABSOLUTE_INDEXED_READ_TWOBYTES(Y);
    UPDATE_FLAGS(A);
}

// INCLUSIVE OR WITH ACCUMULATOR ABSOLUTE Y-INDEXED ($19 | 5 bytes | 7-9 cycles)
// 1-7/9: Absolute indexed addressing mode
void AlienCPU::_1D_ORA_Absolute_XIndexed_Instruction() {
    A |= ADDRESSING_ABSOLUTE_INDEXED_READ_TWOBYTES(X);
    UPDATE_FLAGS(A);
}

// INCLUSIVE OR WITH ACCUMULATOR X-INDEXED INDIRECT ($01 | 3 bytes | 10 cycles)
// 1-10: X indexed indirect addressing mode
void AlienCPU::_01_ORA_XIndexed_Indirect_Instruction() {
    A |= ADDRESSING_XINDEXED_INDIRECT_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// INCLUSIVE OR WITH ACCUMULATOR INDIRECT Y-INDEXED ($11 | 3 bytes | 9-11 cycles)
// 1-9/11: Indirect Y indexed addressing mode
void AlienCPU::_11_ORA_Indirect_YIndexed_Instruction() {
    A |= ADDRESSING_INDIRECT_YINDEXED_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// INCLUSIVE OR WITH ACCUMULATOR ZERO PAGE ($05 | 3 bytes | 5 cycles)
// 1-5: Zero page addressing mode
void AlienCPU::_05_ORA_ZeroPage_Instruction() {
    A |= ADDRESSING_ZEROPAGE_READ_TWOBYTES();
    UPDATE_FLAGS(A);
}

// INCLUSIVE OR WITH ACCUMULATOR ZERO PAGE X-INDEXED ($15 | 3 bytes | 6 cycles)
// 1-6: Zero page indexed addressing mode
void AlienCPU::_15_ORA_ZeroPage_XIndexed_Instruction() {
    A |= ADDRESSING_ZEROPAGE_INDEXED_READ_TWOBYTES(X);
    UPDATE_FLAGS(A);
}


// ====================SHIFT=&=ROTATE====================
// =====================ARITHMETIC=SHIFT==================
// ARITHMETIC SHIFT LEFT ACCUMULATOR ($0A | 1 byte | 2 cycles)
// 1-2: Accumulator addressing mode
void AlienCPU::_0A_ASL_Accumulator_Instruction() {
    ADDRESSING_ACCUMULATOR();
    setFlag(C_FLAG, A >> 15); // sets carry flag to the value of the 16th bit (that is rotated off)
    A <<= 1;
    UPDATE_FLAGS(A);
}

// ARITHMETIC SHIFT LEFT ABSOLUTE ($0E | 5 bytes | 8 cycles)
// 1-8: Absolute addressing mode
void AlienCPU::_0E_ASL_Absolute_Instruction() {
    Byte* valuePointer = ADDRESSING_ABSOLUTE_READ_MODIFY_WRITE_BYTE();
    setFlag(C_FLAG, (*valuePointer) >> 7); // sets carry flag to the value of the 8th bit (that is rotated off)
    (*valuePointer) <<= 1;
    UPDATE_FLAGS(*valuePointer);
}

// ARITHMETIC SHIFT LEFT ABSOLUTE X-INDEXED ($1E | 5 bytes | 9 cycles)
// 1-9: Absolute indexed addressing mode
void AlienCPU::_1E_ASL_Absolute_XIndexed_Instruction() {
    Byte* valuePointer = ADDRESSING_ABSOLUTE_INDEXED_READ_MODIFY_WRITE_BYTE(X);
    setFlag(C_FLAG, (*valuePointer) >> 7); // sets carry flag to the value of the 8th bit (that is rotated off)
    (*valuePointer) <<= 1;
    UPDATE_FLAGS(*valuePointer);
}

// ARITHMETIC SHIFT LEFT ZERO PAGE ($06 | 3 bytes | 6 cycles)
// 1-6: Zero page addressing mode
void AlienCPU::_06_ASL_ZeroPage_Instruction() {
    Byte* valuePointer = ADDRESSING_ZEROPAGE_READ_MODIFY_WRITE_BYTE();
    setFlag(C_FLAG, (*valuePointer) >> 7); // sets carry flag to the value of the 8th bit (that is rotated off)
    (*valuePointer) <<= 1;
    UPDATE_FLAGS(*valuePointer);
}

// ARITHMETIC SHIFT LEFT ZERO PAGE X-INDEXED ($16 | 3 bytes | 7 cycles)
// 1-7: Zero page indexed addressing mode
void AlienCPU::_16_ASL_ZeroPage_XIndexed_Instruction() {
    Byte* valuePointer = ADDRESSING_ZEROPAGE_INDEXED_READ_MODIFY_WRITE_BYTE(X);
    setFlag(C_FLAG, (*valuePointer) >> 7); // sets carry flag to the value of the 8th bit (that is rotated off)
    (*valuePointer) <<= 1;
    UPDATE_FLAGS(*valuePointer);
}


// =====================LOGICAL=SHIFT=====================
// LOGICAL SHIFT RIGHT ACCUMULATOR ($4A | 1 byte | 2 cycles)
// 1-2: Accumulator addressing mode
void AlienCPU::_4A_LSR_Accumulator_Instruction() {
    ADDRESSING_ACCUMULATOR();
    setFlag(C_FLAG, A & 0x0001); // sets carry flag to the value of the 1st bit (that is rotated off)
    A >>= 1; // right shift, sign does not carry since A is unsigned short
    UPDATE_FLAGS(A);
}

// LOGICAL SHIFT RIGHT ABSOLUTE ($4E | 5 bytes | 8 cycles)
// 1-8: Absolute addressing mode
void AlienCPU::_4E_LSR_Absolute_Instruction() {
    Byte* valuePointer = ADDRESSING_ABSOLUTE_READ_MODIFY_WRITE_BYTE();
    setFlag(C_FLAG, (*valuePointer) & 0x0001); // sets carry flag to the value of the 1st bit (that is rotated off)
    (*valuePointer) >>= 1;
    UPDATE_FLAGS(*valuePointer);
}

// LOGICAL SHIFT RIGHT ABSOLUTE XINDEXED ($5E | 5 bytes | 9 cycles)
// 1-9: Absolute indexed addressing mode
void AlienCPU::_5E_LSR_Absolute_XIndexed_Instruction() {
    Byte* valuePointer = ADDRESSING_ABSOLUTE_INDEXED_READ_MODIFY_WRITE_BYTE(X);
    setFlag(C_FLAG, (*valuePointer) & 0x0001); // sets carry flag to the value of the 1st bit (that is rotated off)
    (*valuePointer) >>= 1;
    UPDATE_FLAGS(*valuePointer);
}

// LOGICAL SHIFT RIGHT ZERO PAGE ($46 | 3 bytes | 6 cycles)
// 1-6: Zero page addressing mode
void AlienCPU::_46_LSR_ZeroPage_Instruction() {
    Byte* valuePointer = ADDRESSING_ZEROPAGE_READ_MODIFY_WRITE_BYTE();
    setFlag(C_FLAG, (*valuePointer) & 0x0001); // sets carry flag to the value of the 1st bit (that is rotated off)
    (*valuePointer) >>= 1;
    UPDATE_FLAGS(*valuePointer);
}

// LOGICAL SHIFT RIGHT ZERO PAGE XINDEXED ($56 | 3 bytes | 7 cycles)
// 1-6: Zero page indexed addressing mode
void AlienCPU::_56_LSR_ZeroPage_XIndexed_Instruction() {
    Byte* valuePointer = ADDRESSING_ZEROPAGE_INDEXED_READ_MODIFY_WRITE_BYTE(X);
    setFlag(C_FLAG, (*valuePointer) & 0x0001); // sets carry flag to the value of the 1st bit (that is rotated off)
    (*valuePointer) >>= 1;
    UPDATE_FLAGS(*valuePointer);
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