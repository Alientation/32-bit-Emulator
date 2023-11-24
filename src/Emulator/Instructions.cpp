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

// ======================TRANSFER========================
// ===================LOAD=ACCUMULATOR===================
// AFFECTS FLAGS: N-----Z-
//
//  M -> A
//
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
    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A >> 15);
}


// ===================LOAD=X=REGISTER===================
// AFFECTS FLAGS: N-----Z-
//
//  M -> X
//
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
    setFlag(ZERO, X == 0);
    setFlag(NEGATIVE, X >> 15);
}


// ===================LOAD=Y=REGISTER===================
// AFFECTS FLAGS: N-----Z-
//
//  M -> Y
//
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
    setFlag(ZERO, Y == 0);
    setFlag(NEGATIVE, Y >> 15);
}


// ==================STORE=ACCUMULATOR==================
// AFFECTS FLAGS: --------
//
//  A -> M
//
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
// AFFECTS FLAGS: --------
//
//  X -> M
//
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
// AFFECTS FLAGS: --------
//
//  Y -> M
//
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
// AFFECTS FLAGS: N-----Z-
//
//  A -> X
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $AA       1         2           TAX
//
// CYCLES           DESCRIPTION
void AlienCPU::TAX_Instruction(Word address) {
    X = A;
    setFlag(ZERO, X == 0);
    setFlag(NEGATIVE, X >> 15);
}


// =========TRANSFER=ACCUMULATOR=TO=Y=REGISTER==========
// AFFECTS FLAGS: N-----Z-
//
//  A -> Y
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $A8       1         2           TAY
//
// CYCLES           DESCRIPTION
void AlienCPU::TAY_Instruction(Word address) {
    Y = A;
    setFlag(ZERO, Y == 0);
    setFlag(NEGATIVE, Y >> 15);
}


// ========TRANSFER=STACK=POINTER=TO=X=REGISTER=========
// AFFECTS FLAGS: N-----Z-
//
//  SP -> X
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $BA       1         2           TSX
//
// CYCLES           DESCRIPTION
void AlienCPU::TSX_Instruction(Word address) {
    X = SP;
    setFlag(ZERO, X == 0);
    setFlag(NEGATIVE, X >> 15);
}


// =========TRANSFER=X=REGISTER=TO=ACCUMULATOR==========
// AFFECTS FLAGS: N-----Z-
//
//  X -> A
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $8A       1         2           TXA
//
// CYCLES           DESCRIPTION
void AlienCPU::TXA_Instruction(Word address) {
    A = X;
    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A >> 15);
}


// ========TRANSFER=X=REGISTER=TO=STACK=POINTER=========
// AFFECTS FLAGS: --------
//
//  X -> SP
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $9A       1         2           TXS
//
// CYCLES           DESCRIPTION
void AlienCPU::TXS_Instruction(Word address) {
    SP = X;
}


// =========TRANSFER=Y=REGISTER=TO=ACCUMULATOR==========
// AFFECTS FLAGS: N-----Z-
//
//  Y -> A
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $98       1         2           TYA
//
// CYCLES           DESCRIPTION
void AlienCPU::TYA_Instruction(Word address) {
    A = Y;
    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A >> 15);
}


// ========================STACK=========================
// ===================PUSH=ACCUMULATOR===================
// AFFECTS FLAGS: --------
//
//  A -> (S)
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $48       1         4           PHA
//
// CYCLES           DESCRIPTION
void AlienCPU::PHA_Instruction(Word address) {
    pushTwoBytesToStack(A);
}


// =================PUSH=PROCESSOR=STATUS================
// AFFECTS FLAGS: --------
//
//  P -> (S)
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $08       1         3           PHP
//
// CYCLES           DESCRIPTION
void AlienCPU::PHP_Instruction(Word address) {
    pushByteToStack(P | 0b00110000); // Default and Break flag is always set when pushing to stack
}


// ===================POP=ACCUMULATOR====================
// AFFECTS FLAGS: N-----Z-
//
//  A <- (S)
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $68       1         4           PLA
//
// CYCLES           DESCRIPTION
void AlienCPU::PLA_Instruction(Word address) {
    A = popTwoBytesFromStack();
    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A >> 15);
}


// =================POP=PROCESSOR=STATUS=================
// AFFECTS FLAGS: NV--DIZC
//
//  P <- (S)
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $28       1         4           PLP
//
// CYCLES           DESCRIPTION
void AlienCPU::PLP_Instruction(Word address) {
    bool previousBFlag = getFlag(BREAK);
    P = popByteFromStack(); // ignore reading from B flag
    setFlag(BREAK, previousBFlag); // keep the original B flag
    setFlag(UNUSED, 1);
}


// ================DECREMENTS=&=INCREMENTS===============
// ===================DECREMENT=MEMORY===================
// TODO: DECIDE WHETHER TO MAKE THIS DECREMENT A VALUE THAT IS TWO BYTES LONG
// AFFECTS FLAGS: N-----Z-
//
//  M - 1 -> M
//
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
    Byte value = --motherboard[address]; cycles+=3;
    setFlag(ZERO, value == 0);
    setFlag(NEGATIVE, value >> 15);
}


// =================DECREMENT=X=REGISTER=================
// AFFECTS FLAGS: N-----Z-
//
//  X - 1 -> X
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $CA       1         2           DEX
//
// CYCLES           DESCRIPTION
void AlienCPU::DEX_Instruction(Word address) {
    X--;
    setFlag(ZERO, X == 0);
    setFlag(NEGATIVE, X >> 15);
}


// =================DECREMENT=Y=REGISTER=================
// AFFECTS FLAGS: N-----Z-
//
//  Y - 1 -> Y
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $88       1         2           DEY
//
// CYCLES           DESCRIPTION
void AlienCPU::DEY_Instruction(Word address) {
    Y--;
    setFlag(ZERO, Y == 0);
    setFlag(NEGATIVE, Y >> 15);
}


// ===================INCREMENT=MEMORY===================
// TODO: DECIDE WHETHER TO MAKE THIS DECREMENT A VALUE THAT IS TWO BYTES LONG
// AFFECTS FLAGS: N-----Z-
//
//  M + 1 -> M
//
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
    Byte value = ++motherboard[address]; cycles+=3;
    setFlag(ZERO, value == 0);
    setFlag(NEGATIVE, value >> 15);
}


// =================INCREMENT=X=REGISTER=================
// AFFECTS FLAGS: N-----Z-
//
//  X + 1 -> X
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $E8       1         2           INX
//
// CYCLES           DESCRIPTION
void AlienCPU::INX_Instruction(Word address) {
    X++;
    setFlag(ZERO, X == 0);
    setFlag(NEGATIVE, X >> 15);
}


// =================INCREMENT=Y=REGISTER=================
// AFFECTS FLAGS: N-----Z-
//
//  Y + 1 -> Y
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $C8       1         2           INY
//
// CYCLES           DESCRIPTION
void AlienCPU::INY_Instruction(Word address) {
    Y++;
    setFlag(ZERO, Y == 0);
    setFlag(NEGATIVE, Y >> 15);
}


// =================ARITHMETIC=OPERATIONS================
// =====================ADD=WITH=CARRY===================
// AFFECTS FLAGS: NV----ZC
//
//  A + M + C -> A, C
//
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
    u16 result = A + value + getFlag(CARRY);
    
    // update carry if overflow happens
    setFlag(CARRY, (A + value + getFlag(CARRY)) > 0xFFFF);
    
    // set overflow flag if adding two same signed numbers results in different sign
    setFlag(OVERFLOW, ((A ^ result) & NEGATIVE_MASK) && ((value ^ result) & NEGATIVE_MASK));

    A = result;

    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A >> 15);
}


// =====================SUBTRACT=WITH=BORROW=============
// AFFECTS FLAGS: NV----ZC
//
//  A - M - ~C -> A
//
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
    u16 value = ~readTwoBytes(address); // one's complement so we can add the numbers instead of dealing with subtraction
    u16 result = A + value + getFlag(CARRY);

    // update carry if overflow happens
    setFlag(CARRY, (A + value + getFlag(CARRY)) > 0xFFFF);
    
    // set overflow flag if adding two same signed numbers results in different sign
    setFlag(OVERFLOW, ((A ^ result) & NEGATIVE_MASK) && ((value ^ result) & NEGATIVE_MASK));

    A = result;

    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A >> 15);
}


// ==================LOGICAL=OPERATIONS==================
// =====================AND=WITH=ACCUMULATOR==============
// AFFECTS FLAGS: N-----Z-
//
//  A & M -> A
//
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
    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A >> 15);
}


// =====================EOR=WITH=ACCUMULATOR==============
// AFFECTS FLAGS: N-----Z-
//
//  A ^ M -> A
//
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
    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A >> 15);
}


// =====================ORA=WITH=ACCUMULATOR==============
// AFFECTS FLAGS: N-----Z-
//
//  A | M -> A
//
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
    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A >> 15);
}


// ====================SHIFT=&=ROTATE====================
// =============ARITHMETIC=SHIFT=ACCUMULATOR=============
// AFFECTS FLAGS: N-----ZC
//
//  C <- [A15...A0] <- 0
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  accumulator         $0A       1         2           ASL A
//
// CYCLES           DESCRIPTION
void AlienCPU::ASL_Accumulator_Instruction(Word address) {
    setFlag(CARRY, A >> 15); // sets carry flag to the value of the 16th bit (that is rotated off)
    A <<= 1;
    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A >> 15);
}


// ===================ARITHMETIC=SHIFT===================
// AFFECTS FLAGS: N-----ZC
//
//  C <- [M15...M0] <- 0
//
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
    setFlag(CARRY, motherboard[address] >> 7); // sets carry flag to the value of the 8th bit (that is rotated off)
    Byte value = (motherboard[address] <<= 1); cycles += 3;
    setFlag(ZERO, value == 0);
    setFlag(NEGATIVE, value >> 7);
}


// ==============LOGICAL=SHIFT=ACCUMULATOR===============
// AFFECTS FLAGS: N-----ZC
//
//  0 -> [A15...A0] -> C
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  accumulator         $4A       1         2           LSR A
//
// CYCLES           DESCRIPTION
void AlienCPU::LSR_Accumulator_Instruction(Word address) {
    setFlag(CARRY, A & 0x0001); // sets carry flag to the value of the 1st bit (that is rotated off)
    A >>= 1;
    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, 0); // always 0 since the last bit is always rotated in as 0
}


// ====================LOGICAL=SHIFT=====================
// AFFECTS FLAGS: N-----ZC
//
//  0 -> [M15...M0] -> C
//
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
    setFlag(CARRY, motherboard[address] & 0x0001); // sets carry flag to the value of the 1st bit (that is rotated off)
    Byte value = (motherboard[address] >>= 1); cycles += 3;
    setFlag(ZERO, value == 0);
    setFlag(NEGATIVE, 0); // always 0 since the last bit is always rotated in as 0
}


// =====================ROTATE=LEFT=ACCUMULATOR===========
// AFFECTS FLAGS: N-----ZC
//
//  C <- [A15...A0] <- C
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  accumulator         $2A       1         2           ROL A
//
// CYCLES           DESCRIPTION
void AlienCPU::ROL_Accumulator_Instruction(Word address) {
    Byte carryBit = getFlag(CARRY);
    setFlag(CARRY, A >> 15); // sets carry flag to the value of the 16th bit (that is rotated off)
    A <<= 1;
    A |= carryBit; // rotate in the previous carry bit
    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A >> 15);
}


// ======================ROTATE=LEFT======================
// TODO: DECIDE WHETHER TO MAKE THIS ROTATE A VALUE THAT IS TWO BYTES LONG
// AFFECTS FLAGS: N-----ZC
//
//  C <- [M15...M0] <- C
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  absolute            $2E       5         8           ROL $nnnnnnnn
//  absolute,X          $3E       5         9           ROL $nnnnnnnn,X
//  zero page           $26       3         6           ROL $nnnn
//  zero page,X         $36       3         7           ROL $nnnn,X
//
// CYCLES           DESCRIPTION
//  +1      read value from address
//  +2      write value to address, rotate value left
//  +3      write rotated value to address
void AlienCPU::ROL_Instruction(Word address) {
    Byte carryBit = getFlag(CARRY);
    setFlag(CARRY, motherboard[address] >> 7); // sets carry flag to the value of the 8th bit (that is rotated off)
    Byte value = (motherboard[address] <<= 1); cycles += 3;
    motherboard[address] |= carryBit; // rotate in the previous carry bit
    setFlag(ZERO, value == 0);
    setFlag(NEGATIVE, value >> 7);
}


// =====================ROTATE=RIGHT=ACCUMULATOR==========
// AFFECTS FLAGS: N-----ZC
//
//  C -> [A15...A0] -> C
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  accumulator         $6A       1         2           ROR A
//
// CYCLES           DESCRIPTION
void AlienCPU::ROR_Accumulator_Instruction(Word address) {
    Byte carryBit = getFlag(CARRY);
    setFlag(CARRY, A & 0x0001); // sets carry flag to the value of the 1st bit (that is rotated off)
    A >>= 1;
    A |= carryBit << 15; // rotate in the previous carry bit
    setFlag(ZERO, A == 0);
    setFlag(NEGATIVE, A >> 15);
}


// ======================ROTATE=RIGHT=====================
// TODO: DECIDE WHETHER TO MAKE THIS ROTATE A VALUE THAT IS TWO BYTES LONG
// AFFECTS FLAGS: N-----ZC
//
//  C -> [M15...M0] -> C
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  absolute            $6E       5         8           ROR $nnnnnnnn
//  absolute,X          $7E       5         9           ROR $nnnnnnnn,X
//  zero page           $66       3         6           ROR $nnnn
//  zero page,X         $76       3         7           ROR $nnnn,X
//
// CYCLES           DESCRIPTION
//  +1      read value from address
//  +2      write value to address, rotate value right
//  +3      write rotated value to address
void AlienCPU::ROR_Instruction(Word address) {
    Byte carryBit = getFlag(CARRY);
    setFlag(CARRY, motherboard[address] & 0x0001); // sets carry flag to the value of the 1st bit (that is rotated off)
    Byte value = (motherboard[address] >>= 1); cycles += 3;
    motherboard[address] |= carryBit << 7; // rotate in the previous carry bit
    setFlag(ZERO, value == 0);
    setFlag(NEGATIVE, carryBit);
}


// =========================FLAG==========================
// =====================CLEAR=CARRY=======================
// AFFECTS FLAGS: -------0
//
//  0 -> C
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $18       1         2           CLC
//
// CYCLES           DESCRIPTION
void AlienCPU::CLC_Instruction(Word address) {
    setFlag(CARRY, false);
}


// =====================CLEAR=DECIMAL=====================
// AFFECTS FLAGS: ----0---
//
//  0 -> D
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $D8       1         2           CLD
//
// CYCLES           DESCRIPTION
void AlienCPU::CLD_Instruction(Word address) {
    setFlag(DECIMAL, false);
}


// =====================CLEAR=INTERRUPT===================
// AFFECTS FLAGS: -----0--
//
//  0 -> I
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $58       1         2           CLI
//
// CYCLES           DESCRIPTION
void AlienCPU::CLI_Instruction(Word address) {
    setFlag(INTERRUPT, false);
}


// =====================CLEAR=OVERFLOW====================
// AFFECTS FLAGS: -0------
//
//  0 -> V
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $B8       1         2           CLV
//
// CYCLES           DESCRIPTION
void AlienCPU::CLV_Instruction(Word address) {
    setFlag(OVERFLOW, false);
}


// =====================SET=CARRY=========================
// AFFECTS FLAGS: -------1
//
//  1 -> C
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $38       1         2           SEC
//
// CYCLES           DESCRIPTION
void AlienCPU::SEC_Instruction(Word address) {
    setFlag(CARRY, true);
}


// =====================SET=DECIMAL=======================
// AFFECTS FLAGS: ----1--- TODO: IMPLEMENT DECIMAL MODE
//
//  1 -> D
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $F8       1         2           SED
//
// CYCLES           DESCRIPTION
void AlienCPU::SED_Instruction(Word address) {
    setFlag(DECIMAL, true);
}


// =====================SET=INTERRUPT=====================
// AFFECTS FLAGS: -----1--
//
//  1 -> I
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $78       1         2           SEI
//
// CYCLES           DESCRIPTION
void AlienCPU::SEI_Instruction(Word address) {
    setFlag(INTERRUPT, true);
}


// =====================COMPARISONS======================
// =====================COMPARE=ACCUMULATOR==============
// AFFECTS FLAGS: N-----ZC
//
//  A - M
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  immediate           $C9       3         2           CMP #$nnnn
//  absolute            $CD       5         4           CMP $nnnnnnnn
//  absolute,X          $DD       5         4           CMP $nnnnnnnn,X
//  absolute,Y          $D9       5         4           CMP $nnnnnnnn,Y
//  zero page           $C5       3         3           CMP $nnnn
//  zero page,X         $D5       3         4           CMP $nnnn,X
//  x,indirect          $C1       3         6           CMP ($nnnn,X)
//  indirect,Y          $D1       3         5           CMP ($nnnn),Y
//
// CYCLES           DESCRIPTION TODO: VERIFY THE CYCLE DESCRIPTIONS
//  +1      read low byte of value from address (compare with accumulator)
//  +2      read high byte of value from address + 1 (compare with accumulator)
void AlienCPU::CMP_Instruction(Word address) {
    u16 difference = A - readTwoBytes(address);
    setFlag(CARRY, difference >= 0);
    setFlag(ZERO, difference == 0);
    setFlag(NEGATIVE, difference >> 15);
}


// =====================COMPARE=X=REGISTER==============
// AFFECTS FLAGS: N-----ZC
//
//  X - M
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  immediate           $E0       3         2           CPX #$nnnn
//  absolute            $EC       5         4           CPX $nnnnnnnn
//  zero page           $E4       3         3           CPX $nnnn
//
// CYCLES           DESCRIPTION
//  +1      read low byte of value from address (compare with X register)
//  +2      read high byte of value from address + 1 (compare with X register)
void AlienCPU::CPX_Instruction(Word address) {
    u16 difference = X - readTwoBytes(address);
    setFlag(CARRY, difference >= 0);
    setFlag(ZERO, difference == 0);
    setFlag(NEGATIVE, difference >> 15);
}


// =====================COMPARE=Y=REGISTER==============
// AFFECTS FLAGS: N-----ZC
//
//  Y - M
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  immediate           $C0       3         2           CPY #$nnnn
//  absolute            $CC       5         4           CPY $nnnnnnnn
//  zero page           $C4       3         3           CPY $nnnn
//
// CYCLES           DESCRIPTION
//  +1      read low byte of value from address (compare with Y register)
//  +2      read high byte of value from address + 1 (compare with Y register)
void AlienCPU::CPY_Instruction(Word address) {
    u16 difference = Y - readTwoBytes(address);
    setFlag(CARRY, difference >= 0);
    setFlag(ZERO, difference == 0);
    setFlag(NEGATIVE, difference >> 15);
}


// ==================CONDITIONAL=BRANCH==================
// =================BRANCH=IF=CARRY=CLEAR================  
// ** THIS IS NOT A TRUE ACCURATE REPRESENTATION OF THE 6502 BCC INSTRUCTION
// AFFECTS FLAGS: --------
//
//  if C = 1 then PC = PC + offset
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  relative            $B0       3         4-5         BCS $nnnn
//
// CYCLES           DESCRIPTION
//  +1      read low byte of offset from address
//  +2      read high byte of offset from address + 1
//  +3      if C = 1 then add offset to PC
//  +4*     fix high two bytes of PC if branch is taken
void AlienCPU::BCC_Instruction(Word address) {
    u16 offset = readTwoBytes(address); cycles++;
    short signedOffset = offset;
    if (getFlag(CARRY)) {
        PC += signedOffset; cycles++;
    }
}


// =================BRANCH=IF=CARRY=SET==================
// ** THIS IS NOT A TRUE ACCURATE REPRESENTATION OF THE 6502 BCS INSTRUCTION
// AFFECTS FLAGS: --------
//
//  if C = 0 then PC = PC + offset
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  relative            $90       3         4-5         BCS $nnnn
//
// CYCLES           DESCRIPTION
//  +1      read low byte of offset from address
//  +2      read high byte of offset from address + 1
//  +3      if C = 0 then add offset to PC
//  +4*     fix high two bytes of PC if branch is taken
void AlienCPU::BCS_Instruction(Word address) {
    u16 offset = readTwoBytes(address); cycles++;
    short signedOffset = offset;
    if (!getFlag(CARRY)) {
        PC += signedOffset; cycles++;
    }
}


// ================BRANCH=IF=RESULT=ZERO=================
// ** THIS IS NOT A TRUE ACCURATE REPRESENTATION OF THE 6502 BEQ INSTRUCTION
// AFFECTS FLAGS: --------
//
//  if Z = 1 then PC = PC + offset
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  relative            $F0       3         4-5         BEQ $nnnn
//
// CYCLES           DESCRIPTION
//  +1      read low byte of offset from address
//  +2      read high byte of offset from address + 1
//  +3      if Z = 1 then add offset to PC
//  +4*     fix high two bytes of PC if branch is taken
void AlienCPU::BEQ_Instruction(Word address) {
    u16 offset = readTwoBytes(address); cycles++;
    short signedOffset = offset;
    if (getFlag(ZERO)) {
        PC += signedOffset; cycles++;
    }
}


// ================BRANCH=IF=RESULT=PLUS=================
// ** THIS IS NOT A TRUE ACCURATE REPRESENTATION OF THE 6502 BMI INSTRUCTION
// AFFECTS FLAGS: --------
//
//  if N = 0 then PC = PC + offset
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  relative            $10       3         4-5         BMI $nnnn
//
// CYCLES           DESCRIPTION
//  +1      read low byte of offset from address
//  +2      read high byte of offset from address + 1
//  +3      if N = 0 then add offset to PC
//  +4*     fix high two bytes of PC if branch is taken
void AlienCPU::BPL_Instruction(Word address) {
    u16 offset = readTwoBytes(address); cycles++;
    short signedOffset = offset;
    if (!getFlag(NEGATIVE)) {
        PC += signedOffset; cycles++;
    }
}


// ================BRANCH=IF=RESULT=MINUS================
// ** THIS IS NOT A TRUE ACCURATE REPRESENTATION OF THE 6502 BNE INSTRUCTION
// AFFECTS FLAGS: --------
//
//  if N = 1 then PC = PC + offset
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  relative            $30       3         4-5         BNE $nnnn
//
// CYCLES           DESCRIPTION
//  +1      read low byte of offset from address
//  +2      read high byte of offset from address + 1
//  +3      if N = 1 then add offset to PC
//  +4*     fix high two bytes of PC if branch is taken
void AlienCPU::BMI_Instruction(Word address) {
    u16 offset = readTwoBytes(address); cycles++;
    short signedOffset = offset;
    if (getFlag(NEGATIVE)) {
        PC += signedOffset; cycles++;
    }
}


// ================BRANCH=IF=RESULT=NOT=ZERO=============
// ** THIS IS NOT A TRUE ACCURATE REPRESENTATION OF THE 6502 BNE INSTRUCTION
// AFFECTS FLAGS: --------
//
//  if Z = 0 then PC = PC + offset
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  relative            $D0       3         4-5         BNE $nnnn
//
// CYCLES           DESCRIPTION
//  +1      read low byte of offset from address
//  +2      read high byte of offset from address + 1
//  +3      if Z = 0 then add offset to PC
//  +4*     fix high two bytes of PC if branch is taken
void AlienCPU::BNE_Instruction(Word address) {
    u16 offset = readTwoBytes(address); cycles++;
    short signedOffset = offset;
    if (!getFlag(ZERO)) {
        PC += signedOffset; cycles++;
    }
}


// ===============BRANCH=IF=OVERFLOW=CLEAR===============
// ** THIS IS NOT A TRUE ACCURATE REPRESENTATION OF THE 6502 BVC INSTRUCTION
// AFFECTS FLAGS: --------
//
//  if V = 0 then PC = PC + offset
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  relative            $50       3         4-5         BVC $nnnn
//
// CYCLES           DESCRIPTION
//  +1      read low byte of offset from address
//  +2      read high byte of offset from address + 1
//  +3      if V = 0 then add offset to PC
//  +4*     fix high two bytes of PC if branch is taken
void AlienCPU::BVC_Instruction(Word address) {
    u16 offset = readTwoBytes(address); cycles++;
    short signedOffset = offset;
    if (!getFlag(OVERFLOW)) {
        PC += signedOffset; cycles++;
    }
}


// ===============BRANCH=IF=OVERFLOW=SET=================
// ** THIS IS NOT A TRUE ACCURATE REPRESENTATION OF THE 6502 BVS INSTRUCTION
// AFFECTS FLAGS: --------
//
//  if V = 1 then PC = PC + offset
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  relative            $70       3         4-5         BVS $nnnn
//
// CYCLES           DESCRIPTION
//  +1      read low byte of offset from address
//  +2      read high byte of offset from address + 1
//  +3      if V = 1 then add offset to PC
//  +4*     fix high two bytes of PC if branch is taken
void AlienCPU::BVS_Instruction(Word address) {
    u16 offset = readTwoBytes(address); cycles++;
    short signedOffset = offset;
    if (getFlag(OVERFLOW)) {
        PC += signedOffset; cycles++;
    }
}


// ==================JUMPS=&=SUBROUTINES=================
// =====================JUMP=INDIRECT====================
// AFFECTS FLAGS: --------
//
//  M -> PC
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  absolute            $4C       5         5           JMP $nnnnnnnn
//  absolute indirect   $6C       5         9           JMP ($nnnnnnnn)
//
// CYCLES           DESCRIPTION
void AlienCPU::JMP_Instruction(Word address) {
    PC = address;
}


// ==================JUMP=TO=SUBROUTINE==================
// ** IN THE 6502, PC STORED ON STACK WOULD POINT TO THE LAST BYTE OF THE JUMP
//    INSTRUCTION, HOWEVER, HERE IT POINTS TO THE FIRST BYTE OF THE NEXT INSTRUCTION
// AFFECTS FLAGS: --------
//
//  PC -> (S)   M -> PC
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  absolute            $20       5         9           JSR $nnnnnnnn
//
// CYCLES           DESCRIPTION
//  +1      push high byte of PC to stack
//  +2      push mid high byte of PC to stack
//  +3      push mid low byte of PC to stack
//  +4      push low byte of PC to stack
void AlienCPU::JSR_Instruction(Word address) {
    pushWordToStack(PC);
    PC = address;
}


// =====================RETURN=FROM=SUBROUTINE===========
// ** IN THE 6502, PC STORED ON STACK WOULD POINT TO THE LAST BYTE OF THE JUMP
//    INSTRUCTION, HOWEVER, HERE IT POINTS TO THE FIRST BYTE OF THE NEXT INSTRUCTION
// AFFECTS FLAGS: --------
//
//  (S) -> PC
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $60       1         6           RTS
//
// CYCLES           DESCRIPTION
//  +1      pull low byte of PC from stack
//  +2      pull mid low byte of PC from stack
//  +3      pull mid high byte of PC from stack
//  +4      pull high byte of PC from stack
void AlienCPU::RTS_Instruction(Word address) {
    PC = popWordFromStack();
}


// ====================INTERRUPTS========================
// ===================BREAK=INTERRUPT====================
// TODO: figure out what the break mark (spacing) is for and whether to include it in
//       the bytes count
// AFFECTS FLAGS: -----I--
//
//  PC -> (S)   P -> (S)   I = 1   PC -> ($FFFE)
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $00       1         11           BRK
//
// CYCLES           DESCRIPTION
//  +1      push high byte of PC+1 to stack
//  +2      push mid high byte of PC+1 to stack
//  +3      push mid low byte of PC+1 to stack
//  +4      push low byte of PC+1 to stack
//  +5      push P to stack
void AlienCPU::BRK_Instruction(Word address) {
    PC++; // create a spacing for a break mark (identifying the reason for the break)
    pushWordToStack(PC);
    pushByteToStack(P);
    setFlag(INTERRUPT, true);
    PC = readWord(0xFFFE);
}


// =================RETURN=FROM=INTERRUPT==================
// AFFECTS FLAGS: NV--DIZC
//
//  (S) -> P   (S) -> PC
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  implied             $40       1         6           RTI
//
// CYCLES           DESCRIPTION
//  +1      pull P from stack
//  +2      pull low byte of PC from stack
//  +3      pull mid low byte of PC from stack
//  +4      pull mid high byte of PC from stack
//  +5      pull high byte of PC from stack
void AlienCPU::RTI_Instruction(Word address) {
    P = popByteFromStack();
    PC = popWordFromStack();
}


// =========================OTHER=========================
// =======================BIT=TEST========================
// AFFECTS FLAGS: NV----Z-
//
//  M15 -> N     M14 -> V     A & M -> Z
//
// ADDRESSING MODE     OPCODE    BYTES     CYCLES       ASSEMBLY
//  absolute            $2C       5         7           BIT $nnnnnnnn
//  zero page           $24       3         5           BIT $nnnn
void AlienCPU::BIT_Instruction(Word address) {
    u16 value = readTwoBytes(address);
    setFlag(NEGATIVE, value >> 15);
    setFlag(OVERFLOW, (value >> 14) & 0b01);
    setFlag(ZERO, (A & value) == 0);
}


// ====================NULL=INSTRUCTION===================
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
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::ANC_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::ANC2_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::ANE_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::ARR_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::DCP_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::ISC_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::LAS_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}

void AlienCPU::LAX_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::LXA_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


// ========================JAMS===========================
// Freezes the CPU indefinitely in T1 phase with $FFFF on the data bus, requires reset
// https://www.nesdev.org/wiki/Visual6502wiki/6502_Timing_States (for T- phases)
void AlienCPU::JAM_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::RLA_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::RRA_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::SAX_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::SBX_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::SHA_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::SHX_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::SHY_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::SLO_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::SRE_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::TAS_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}


void AlienCPU::USBC_Illegal_Instruction(Word address) {
    std::cout << std::endl << "UNIMPLEMENTED INSTRUCTION" << std::endl;
}