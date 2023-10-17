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

// Sets ZERO flag if the modified value is 0 and NEGATIVE flag if the 
// last bit of the modified value is set
void AlienCPU::UPDATE_FLAGS(u16 modifiedValue) {
    setFlag(Z_FLAG, modifiedValue == 0);
    setFlag(N_FLAG, modifiedValue >> 15);
}

// ======================TRANSFER========================
// ===================LOAD=ACCUMULATOR===================
// AFFECTS FLAGS: Z, N
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  immediate           $A9       3         3           LDA #$nnnn
//  absolute            $AD       5         7           LDA $nnnnnnnn
//  absolute,X          $BD       5         8           LDA $nnnnnnnn,X
//  absolute,Y          $B9       5         8           LDA $nnnnnnnn,Y
//  zero page           $A5       3         5           LDA $nnnn
//  zero page,X         $B5       3         6           LDA $nnnn,X
//  x,indirect          $A1       3         10          LDA ($nnnn,X)
//  indirect,Y          $B1       3         10          LDA ($nnnn),Y
// 
// CYCLES           DESCRIPTION         
//  +1      read low byte from address
//  +2      read high byte from address + 1
void AlienCPU::LDA_Instruction(Word address) {
    A = readTwoBytes(address);
    UPDATE_FLAGS(A);
}


// ===================LOAD=X=REGISTER===================
// AFFECTS FLAGS: Z, N
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  immediate           $A2       3         3           LDA #$nnnn
//  absolute            $AE       5         7           LDA $nnnnnnnn
//  absolute,Y          $BE       5         8           LDA $nnnnnnnn,Y
//  zero page           $A6       3         5           LDA $nnnn
//  zero page,Y         $B6       3         6           LDA $nnnn,Y
//
// CYCLES           DESCRIPTION         
//  +1      read low byte from address
//  +2      read high byte from address + 1
void AlienCPU::LDX_Instruction(Word address) {
    X = readTwoBytes(address);
    UPDATE_FLAGS(X);
}


// ===================LOAD=Y=REGISTER===================
// AFFECTS FLAGS: Z, N
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  immediate           $A0       3         3           LDA #$nnnn
//  absolute            $AC       5         7           LDA $nnnnnnnn
//  absolute,X          $BC       5         8           LDA $nnnnnnnn,X
//  zero page           $A4       3         5           LDA $nnnn
//  zero page,X         $B4       3         6           LDA $nnnn,X
//
// CYCLES           DESCRIPTION         
//  +1      read low byte from address
//  +2      read high byte from address
void AlienCPU::LDY_Instruction(Word address) {
    Y = readTwoBytes(address);
    UPDATE_FLAGS(Y);
}


// ==================STORE=ACCUMULATOR==================
// AFFECTS FLAGS: NONE
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  absolute            $8D       5         7           STA $nnnnnnnn
//  absolute,X          $9D       5         8           STA $nnnnnnnn,X
//  absolute,Y          $99       5         8           STA $nnnnnnnn,Y
//  zero page           $85       3         5           STA $nnnn
//  zero page,X         $95       3         6           STA $nnnn,X
//  x,indirect          $81       3         10          STA ($nnnn,X)
//  indirect,Y          $91       3         10          STA ($nnnn),Y
//
// CYCLES           DESCRIPTION         
//  +1      write low byte of A to address
//  +2      write high byte of A to address + 1
void AlienCPU::STA_Instruction(Word address) {
    writeTwoBytes(address, A);
}


// ===================STORE=X=REGISTER==================
// AFFECTS FLAGS: NONE
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  absolute            $8E       5         7           STA $nnnnnnnn
//  zero page           $86       3         5           STA $nnnn
//  zero page,Y         $96       3         6           STA $nnnn,Y
//
// CYCLES           DESCRIPTION         
//  +1      write low byte of X to address
//  +2      write high byte of X to address + 1
void AlienCPU::STX_Instruction(Word address) {
    writeTwoBytes(address, X);
}


// ===================STORE=Y=REGISTER==================
// AFFECTS FLAGS: NONE
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  absolute            $8C       5         7           STA $nnnnnnnn
//  zero page           $84       3         5           STA $nnnn
//  zero page,X         $94       3         6           STA $nnnn,X
//
// CYCLES           DESCRIPTION         
//  +1      write low byte of Y to address
//  +2      write high byte of Y to address + 1
void AlienCPU::STY_Instruction(Word address) {
    writeTwoBytes(address, Y);
}


// =========TRANSFER=ACCUMULATOR=TO=X=REGISTER==========
// AFFECTS FLAGS: Z, N
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $AA       1         2           TAX
//
// CYCLES           DESCRIPTION
void AlienCPU::TAX_Instruction(Word address) {
    X = A;
    UPDATE_FLAGS(X);
}


// =========TRANSFER=ACCUMULATOR=TO=Y=REGISTER==========
// AFFECTS FLAGS: Z, N
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $A8       1         2           TAY
//
// CYCLES           DESCRIPTION
void AlienCPU::TAY_Instruction(Word address) {
    Y = A;
    UPDATE_FLAGS(Y);
}


// ========TRANSFER=STACK=POINTER=TO=X=REGISTER=========
// AFFECTS FLAGS: Z, N
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $BA       1         2           TSX
//
// CYCLES           DESCRIPTION
void AlienCPU::TSX_Instruction(Word address) {
    X = SP;
    UPDATE_FLAGS(X);
}


// =========TRANSFER=X=REGISTER=TO=ACCUMULATOR==========
// AFFECTS FLAGS: Z, N
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $8A       1         2           TXA
//
// CYCLES           DESCRIPTION
void AlienCPU::TXA_Instruction(Word address) {
    A = X;
    UPDATE_FLAGS(A);
}


// ========TRANSFER=X=REGISTER=TO=STACK=POINTER=========
// AFFECTS FLAGS: NONE
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $9A       1         2           TXS
//
// CYCLES           DESCRIPTION
void AlienCPU::TXS_Instruction(Word address) {
    SP = X;
}


// =========TRANSFER=Y=REGISTER=TO=ACCUMULATOR==========
// AFFECTS FLAGS: Z, N
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $98       1         2           TYA
//
// CYCLES           DESCRIPTION
void AlienCPU::TYA_Instruction(Word address) {
    A = Y;
    UPDATE_FLAGS(A);
}


// ========================STACK=========================
// ===================PUSH=ACCUMULATOR===================
// AFFECTS FLAGS: NONE
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $48       1         4           PHA
//
// CYCLES           DESCRIPTION
void AlienCPU::PHA_Instruction(Word address) {
    pushTwoBytesToStack(A);
}


// =================PUSH=PROCESSOR=STATUS================
// AFFECTS FLAGS: Z, N, C, V, D, I
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $08       1         3           PHP
//
// CYCLES           DESCRIPTION
void AlienCPU::PHP_Instruction(Word address) {
    pushByteToStack(P);
}


// ===================POP=ACCUMULATOR====================
// TODO: correct tests to check for Z and N flags
// AFFECTS FLAGS: Z, N
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $68       1         4           PLA
//
// CYCLES           DESCRIPTION
void AlienCPU::PLA_Instruction(Word address) {
    A = popTwoBytesFromStack();
    UPDATE_FLAGS(A);
}


// =================POP=PROCESSOR=STATUS=================
// AFFECTS FLAGS: Z, N, C, V, D, I
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $28       1         4           PLP
//
// CYCLES           DESCRIPTION
void AlienCPU::PLP_Instruction(Word address) {
    P = popByteFromStack();
}


// ================DECREMENTS=&=INCREMENTS===============
// ===================DECREMENT=MEMORY===================
// TODO: DECIDE WHETHER TO MAKE THIS DECREMENT A VALUE THAT IS TWO BYTES LONG
// AFFECTS FLAGS: Z
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  absolute            $CE       5         8           DEC $nnnnnnnn
//  absolute,X          $DE       5         9           DEC $nnnnnnnn,X
//  zero page           $C6       3         6           DEC $nnnn
//  zero page,X         $D6       3         7           DEC $nnnn,X
// 
// CYCLES           DESCRIPTION         
//  +1   read value from address
//  +2   write value to address, decrement value
//  +3   write decremented value to address
void AlienCPU::DEC_Instruction(Word address) {
    motherboard[address]--; cycles+=3;
    UPDATE_FLAGS(motherboard[address]);
}


// =================DECREMENT=X=REGISTER=================
// AFFECTS FLAGS: Z, N
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $CA       1         2           DEX
//
// CYCLES           DESCRIPTION
void AlienCPU::DEX_Instruction(Word address) {
    X--;
    UPDATE_FLAGS(X);
}


// =================DECREMENT=Y=REGISTER=================
// AFFECTS FLAGS: Z, N
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $88       1         2           DEY
//
// CYCLES           DESCRIPTION
void AlienCPU::DEY_Instruction(Word address) {
    Y--;
    UPDATE_FLAGS(Y);
}


// ===================INCREMENT=MEMORY===================
// TODO: DECIDE WHETHER TO MAKE THIS DECREMENT A VALUE THAT IS TWO BYTES LONG
// AFFECTS FLAGS: Z
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  absolute            $EE       5         8           INC $nnnnnnnn
//  absolute,X          $FE       5         9           INC $nnnnnnnn,X
//  zero page           $E6       3         6           INC $nnnn
//  zero page,X         $F6       3         7           INC $nnnn,X
//
// CYCLES           DESCRIPTION
//  +1   read value from address
//  +2   write value to address, increment value
//  +3   write incremented value to address
void AlienCPU::INC_Instruction(Word address) {
    motherboard[address]++; cycles+=3;
    UPDATE_FLAGS(motherboard[address]);
}


// =================INCREMENT=X=REGISTER=================
// AFFECTS FLAGS: Z, N
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $E8       1         2           INX
//
// CYCLES           DESCRIPTION
void AlienCPU::INX_Instruction(Word address) {
    X++;
    UPDATE_FLAGS(X);
}


// =================INCREMENT=Y=REGISTER=================
// AFFECTS FLAGS: Z, N
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $C8       1         2           INY
//
// CYCLES           DESCRIPTION
void AlienCPU::INY_Instruction(Word address) {
    Y++;
    UPDATE_FLAGS(Y);
}


// =================ARITHMETIC=OPERATIONS================
// =====================ADD=WITH=CARRY===================
// TODO: correct tests to check for Z, N, and V flags
// AFFECTS FLAGS: N, V, Z, C
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  immediate           $69       3         3           ADC #$nnnn
//  absolute            $6D       5         7           ADC $nnnnnnnn
//  absolute,X          $7D       5         8           ADC $nnnnnnnn,X
//  absolute,Y          $79       5         8           ADC $nnnnnnnn,Y
//  zero page           $65       3         5           ADC $nnnn
//  zero page,X         $75       3         6           ADC $nnnn,X
//  x,indirect          $61       3         10          ADC ($nnnn,X)
//  indirect,Y          $71       3         10          ADC ($nnnn),Y
//
// CYCLES           DESCRIPTION
//  +1      read low byte of value from address (add to accumulator)
//  +2      read high byte of value from address + 1 (add to accumulator)
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
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  immediate           $E9       3         3           SBC #$nnnn
//  absolute            $ED       5         7           SBC $nnnnnnnn
//  absolute,X          $FD       5         8           SBC $nnnnnnnn,X
//  absolute,Y          $F9       5         8           SBC $nnnnnnnn,Y
//  zero page           $E5       3         5           SBC $nnnn
//  zero page,X         $F5       3         6           SBC $nnnn,X
//  x,indirect          $E1       3         10          SBC ($nnnn,X)
//  indirect,Y          $F1       3         10          SBC ($nnnn),Y
//
// CYCLES           DESCRIPTION
//  +1      read low byte of value from address (subtract from accumulator)
//  +2      read high byte of value from address + 1 (subtract from accumulator)
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
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  immediate           $29       3         3           AND #$nnnn
//  absolute            $2D       5         7           AND $nnnnnnnn
//  absolute,X          $3D       5         8           AND $nnnnnnnn,X
//  absolute,Y          $39       5         8           AND $nnnnnnnn,Y
//  zero page           $25       3         5           AND $nnnn
//  zero page,X         $35       3         6           AND $nnnn,X
//  x,indirect          $21       3         10          AND ($nnnn,X)
//  indirect,Y          $31       3         10          AND ($nnnn),Y
//
// CYCLES           DESCRIPTION
//  +1      read low byte of value from address (AND with accumulator)
//  +2      read high byte of value from address + 1 (AND with accumulator)
void AlienCPU::AND_Instruction(Word address) {
    A &= readTwoBytes(address);
    UPDATE_FLAGS(A);
}


// =====================EOR=WITH=ACCUMULATOR==============
// AFFECTS FLAGS: Z, N
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  immediate           $49       3         3           EOR #$nnnn
//  absolute            $4D       5         7           EOR $nnnnnnnn
//  absolute,X          $5D       5         8           EOR $nnnnnnnn,X
//  absolute,Y          $59       5         8           EOR $nnnnnnnn,Y
//  zero page           $45       3         5           EOR $nnnn
//  zero page,X         $55       3         6           EOR $nnnn,X
//  x,indirect          $41       3         10          EOR ($nnnn,X)
//  indirect,Y          $51       3         10          EOR ($nnnn),Y
//
// CYCLES           DESCRIPTION
//  +1      read low byte of value from address (EOR with accumulator)
//  +2      read high byte of value from address + 1 (EOR with accumulator)
void AlienCPU::EOR_Instruction(Word address) {
    A ^= readTwoBytes(address);
    UPDATE_FLAGS(A);
}


// =====================ORA=WITH=ACCUMULATOR==============
// AFFECTS FLAGS: Z, N
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  immediate           $09       3         3           ORA #$nnnn
//  absolute            $0D       5         7           ORA $nnnnnnnn
//  absolute,X          $1D       5         8           ORA $nnnnnnnn,X
//  absolute,Y          $19       5         8           ORA $nnnnnnnn,Y
//  zero page           $05       3         5           ORA $nnnn
//  zero page,X         $15       3         6           ORA $nnnn,X
//  x,indirect          $01       3         10          ORA ($nnnn,X)
//  indirect,Y          $11       3         10          ORA ($nnnn),Y
//
// CYCLES           DESCRIPTION
//  +1      read low byte of value from address (OR with accumulator)
//  +2      read high byte of value from address + 1 (OR with accumulator)
void AlienCPU::ORA_Instruction(Word address) {
    A |= readTwoBytes(address);
    UPDATE_FLAGS(A);
}


// ====================SHIFT=&=ROTATE====================
// =============ARITHMETIC=SHIFT=ACCUMULATOR=============
// AFFECTS FLAGS: N, Z, C
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  accumulator         $0A       1         2           ASL
//
// CYCLES           DESCRIPTION
void AlienCPU::ASL_Accumulator_Instruction(Word address) {
    setFlag(C_FLAG, A >> 15); // sets carry flag to the value of the 16th bit (that is rotated off)
    A <<= 1;
    UPDATE_FLAGS(A);
}


// ===================ARITHMETIC=SHIFT===================
// AFFECTS FLAGS: N, Z, C
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  absolute            $0E       5         8           ASL $nnnnnnnn
//  absolute,X          $1E       5         9           ASL $nnnnnnnn,X
//  zero page           $06       3         6           ASL $nnnn
//  zero page,X         $16       3         7           ASL $nnnn,X
//
// CYCLES           DESCRIPTION
//  +1      read value from address
//  +2      write value to address, shift value left
//  +3      write shifted value to address
void AlienCPU::ASL_Instruction(Word address) {
    setFlag(C_FLAG, motherboard[address] >> 7); // sets carry flag to the value of the 8th bit (that is rotated off)
    motherboard[address] <<= 1; cycles += 3;
    UPDATE_FLAGS(motherboard[address]);
}


// ==============LOGICAL=SHIFT=ACCUMULATOR===============
// AFFECTS FLAGS: N, Z, C
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  accumulator         $4A       1         2           LSR
//
// CYCLES           DESCRIPTION
void AlienCPU::LSR_Accumulator_Instruction(Word address) {
    setFlag(C_FLAG, A & 0x0001); // sets carry flag to the value of the 1st bit (that is rotated off)
    A >>= 1; // right shift, sign does not carry since A is unsigned short
    UPDATE_FLAGS(A);
}


// ====================LOGICAL=SHIFT=====================
// AFFECTS FLAGS: N, Z, C
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  absolute            $4E       5         8           LSR $nnnnnnnn
//  absolute,X          $5E       5         9           LSR $nnnnnnnn,X
//  zero page           $46       3         6           LSR $nnnn
//  zero page,X         $56       3         7           LSR $nnnn,X
//
// CYCLES           DESCRIPTION
//  +1      read value from address
//  +2      write value to address, shift value right
//  +3      write shifted value to address
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