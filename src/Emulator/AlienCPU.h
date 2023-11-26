#ifndef ALIENCPU_H
#define ALIENCPU_H

#include <../src/util/Types.h>
#include <../src/Emulator/Motherboard.h>
#include <string>
#include <functional>

class AlienCPU; //forward declaration (!!)

enum StatusFlag {
    CARRY, ZERO, INTERRUPT, DECIMAL, BREAK, UNUSED, OVERFLOW, NEGATIVE
};

enum AddressingMode {
    NO_ADDRESSING_MODE,
    ACCUMULATOR, IMPLIED, IMMEDIATE, RELATIVE, INDIRECT, 
    ABSOLUTE, ABSOLUTE_XINDEXED, ABSOLUTE_YINDEXED, 
    XINDEXED_INDIRECT, INDIRECT_YINDEXED, ZEROPAGE, ZEROPAGE_XINDEXED, ZEROPAGE_YINDEXED
};

// CPU Emulator (modeled off of 6502)
// https://en.wikipedia.org/wiki/Central_processing_unit
// http://www.6502.org/users/obelisk/index.html

// 32 bit CPU (expanded version of 6502)
//  - Everything stored in memory is little endian, but this emulator uses high endian representations for easier convienience
//  - 4 Gb of addressable memory (via 32 bit address bus : 0x00000000 - 0xFFFFFFFF)
//      - 1 Mb of RAM (0x00000000 - 0x000FFFFF)
//          - 0x00000000 to 0x0000FFFF : Zero page (65536 bytes)
//          - 0x00010000 to 0x0001FFFF : Stack memory (65536 bytes)
//          - 0x00020000 to 0x000FFFEF : General purpose memory (917488 bytes)
//          - 0x000FFFF0 to 0x000FFFFF : Special reserved memory (16 bytes) // TODO move this further back since this should be mapped to ROM and not RAM
//              * 0x000FFFF0 to 0x000FFFF3 : Interrupt handler
//              * 0x000FFFF4 to 0x000FFFF7 : Power on reset location
//              * 0x000FFFF8 to 0x000FFFFB : BRK / Interrupt Request (IRQ) handler
//              * 0x000FFFFC to 0x000FFFFF : Reserved
//      - 64 Kb of ROM (0x00100000 - 0x0010FFFF)
//          - BIOS STORED HERE
//      - Memory mappings to other hardware devices (Monitor,Speaker,Keyboard,Mouse,GPU,HDD,SSD,etc)
//
// 6502 
//  - 8 bit CPU
//  - 64 Kb of memory (addressable via 16 bit address bus : 0x0000 - 0xFFFF)
//      - 0x0000 to 0x00FF : Zero page, first 256 bytes
//          - some special addressing modes allow shorter instructions to default access to zero page memory
//      - 0x0100 to 0x01FF : reserved as stack memory
//      - 0x0200 to 0xFFF9 : general memory
//      - 0xFFFA to 0xFFFF : special reserved memory
//          * 0xFFFA/B : interrupt handler
//          * 0xFFFC/D : power on reset location
//          * 0xFFFE/F : BRK / Interrupt Request (IRQ) handler  
//  - little endian (lowest bytes stored first in memory)
//      - for example in big endian mode 0x12345678 would be stored as 12 34 56 78 (from byte 1 to byte 4)
//        however in little endian, that same number would be stored as 78 56 34 12 (from byte 4 to byte 1)
class AlienCPU {
    public:
        static const std::string VERSION;

        // TODO decide whether this is needed
        // Number of cycles between each Interrupt check
        static constexpr Word INTERRUPT_CHECK_INTERVAL = 16;


        // ============================INSTRUCTIONS============================
        // Total Number of instructions supported by the processor
        static constexpr u16 INSTRUCTION_COUNT = 256;


        // Null address (high endian)
        static constexpr Word NULL_ADDRESS = 0x00000000;

        // Jump address to handle hardware interrupts (high endian)
        static constexpr Word INTERRUPT_HANDLER_VECTOR = 0x000FFFF0;

        // Jump address to handle CPU reset (high endian)
        static constexpr Word POWER_ON_RESET_VECTOR = 0x000FFFF4;

        // Jump address to handle software interrupts (high endian)
        static constexpr Word BRK_HANDLER_VECTOR = 0x000FFFF8;


        // Program Stack
        // SP_INIT - STACK_SIZE = End of stack (high endian)
        // 65536 Bytes of STACK MEMORY
        static constexpr Word STACK_SIZE = 0x00010000;


        // should never exceed 0x000FFFFF (high endian)
        static constexpr Word PC_INIT = POWER_ON_RESET_VECTOR;
        
        static constexpr u16 // (high endian)
            // stored as an offset from 0x00010000 so start of stack is at 0x0001FFFF
            SP_INIT = 0xFFFF,

            A_INIT = 0x0000, 
            X_INIT = 0x0000,
            Y_INIT = 0x0000;

        static constexpr Byte P_INIT = 0b00100000;

        static constexpr u16
            NEGATIVE_MASK = 0b1000000000000000;

    private:
        // Instruction Set
        // using InstructionFunction = std::function<void(AlienCPU&)>;

        typedef void (AlienCPU::*InstructionFunction)(Word);
        typedef Word (AlienCPU::*AddressingModeFunction)();

        struct Instruction {
            InstructionFunction instruction;
            AddressingModeFunction addressingMode;
        };
        Instruction instructions[INSTRUCTION_COUNT];

        bool debugMode = false;

        // System Memory
        // 0x00100000 total memory (0x00000000 - 0x000FFFFF)
        // 
        // 0x00010000 Pages memory (0x000N0000 - 0x000NFFFF)
        //
        // 0x00000000 - 0x000000FF : Reserved for boot process
        // 0x00000100 - 0x0000FFFF : Zero Page memory
        // 0x00010000 - 0x0001FFFF : Stack memory
        // 0x00020000 - 0x000FFFFF : General purpose memory 
        Motherboard motherboard;

        // Number of cycles till the next Interrupt should be processed
        Word nextInterruptCheck;

        // cpu state variables for debugging
        u64 cycles;
        u64 instructionsExecuted;

        bool isRunning;


        // ======================PROGRAM=COUNTER=REGISTER=======================
        //  - high endian memory address of the next instruction byte to be executed,
        //    stored as [high byte, mid high byte, mid low byte, low byte]
        //  - can be modified by the execution of a jump, subroutine (function) call, or branches (if/else)
        //    or by returning from a subroutine or interrupt
        Word PC; 

        // =======================STACK=POINTER=REGISTER========================
        //  - high endian memory address of the first empty byte of the call stack (located in first page)
        //    stored as [high byte, low byte]
        //  - represents an offset from the start of the stack page, therefore to reference the stack
        //    memory, the stack pointer must be converted to the correct address
        //  - every stack element is 4 bytes big so to add elements to the stack, 
        //    the stack pointer is decremented by 4 (TODO figure out stack element sizes)
        //      - [SP]   = empty stack element
        //      - [SP+1] = byte 0
        //      - [SP+2] = byte 1      (little endian)
        //      - [SP+3] = byte 2
        //      - [SP+4] = byte 3
        // https://en.wikipedia.org/wiki/Stack_register
        // https://en.wikipedia.org/wiki/Call_stack
        u16 SP;

        // ======================GENERAL=PURPOSE=REGISTERS======================
        // https://codebase64.org/doku.php?id=base:6502_registers

        // ========================ACCUMULATOR=REGISTER=========================
        // high endian main register for arithmetic and logic operations (direct connection to ALU)
        // stored as [high byte, low byte]
        u16 A;

        // high endian index register X
        // stored as [high byte, low byte]
        u16 X;

        // high endian index register Y
        // stored as [high byte, low byte]
        u16 Y;


        // ======================PROCESSOR=STATUS=REGISTER======================
        // https://codebase64.org/doku.php?id=base:6502_registers
        // https://www.nesdev.org/wiki/Status_flags
        // stores flags
        //
        // bit 7 -> NV1BBDIZC <- bit 0
        // - N (Negative)
        //      Set after any arithmetic operations (when A, X, or Y registers are loaded with a value)
        //      Stores the topmost bit of the register being loaded, generally used for signed integers
        //      Behaves differently in Decimal instructions
        //
        // - V (Overflow)
        //      Set after addition or subtraction operations if signed overflow occurs
        //      Reset after any other operation
        //
        // - 1 (Unused)
        //      Always 1
        //
        // - B (Break) https://en.wikipedia.org/wiki/Interrupts_in_65xx_processors
        //      Distinguishes software (BRK) interrupts from hardware (IRQ or NMI) interrupts
        //      Always set except when the P register is pushed to the stack when processing a 
        //      hardware interrupt
        //      (0) hardware interrupt 
        //          RESET: Reset signal, resets the CPU
        //          NMI: Non-maskable interrupt, must be immediately handled
        //          IRQ: Interrupt request, can be enabled/disabled
        //      (1) software interrupt (BRK instruction)
        //
        // - D (Decimal)
        //      Used to select the Binary Coded Decimal mode for addition and subtraction
        //      Defaults to 0 (binary mode)
        //
        // - I (Interrupt Disable)
        //      If set, the CPU will ignore all IRQ interrupts and prevent jumping to the IRQ handler vector
        //      Set after CPU processes an interrupt request 
        //
        // - Z (Zero)
        //      Set if the result of the last operation was zero
        //      Reset if the result of the last operation was non-zero
        //
        // - C (Carry)
        //      Set if the last operation resulted in a carry or borrow
        //      Reset if the last operation did not result in a carry or borrow
        Byte P;

        
    public:
        AlienCPU();

        void start();
        void startCycles(u64 maxCycles);
        void startInstructions(u64 instructions);
        void reset(bool resetMotherboard = false);
        void stop();

        std::stringstream cpustate();
        std::stringstream memdump();
    
    private:
        void processReset();

        void initInstructions();

        void clearFlag(StatusFlag flag);
        void setFlag(StatusFlag flag, bool isSet);
        bool getFlag(StatusFlag flag);

        u16 convertToLowEndianTwoBytes(u16 highEndianValue);
        Word convertToLowEndianWord(Word highEndianValue);
        u16 convertToHighEndianTwoBytes(u16 lowEndianValue);
        Word convertToHighEndianWord(Word lowEndianValue);

        Byte readByte(Word highEndianAddress);
        u16 readTwoBytes(Word highEndianAddress);
        Word readWord(Word highEndianAddress);

        Byte fetchNextByte();
        u16 fetchNextTwoBytes();
        Word fetchNextWord();

        void writeByte(Word highEndianAddress, Byte value);
        void writeTwoBytes(Word highEndianAddress, u16 highEndianValue);
        void writeWord(Word highEndianAddress, Word highEndianValue);

        Word SPToAddress();
        void pushWordToStack(Word value);
        Word popWordFromStack();
        void pushTwoBytesToStack(u16 value);
        u16 popTwoBytesFromStack();
        void pushByteToStack(Byte value);
        Byte popByteFromStack();

        void executeInstruction(u16 instruction);
        bool isValidInstruction(u16 instruction);

        // =========================INSTRUCTIONS=========================
        // | Instruction opcodes are 1 byte, $00 to $FF (256 possible instructions)
        // |
        // |    *   add 1 to cycles if page boundary is crossed
        // |    **  add 1 to cycles if branch occurs on same page
        // |        add 2 to cycles if branch occurs to different page
        // |
        // |    +   modified
        // |    -   not modified
        // |    0   cleared
        // |    1   set
        // |    M6  memory bit 6
        // |    M7  memory bit 7
        // |
        // NON MEMORY ADDRESSING MODES
        Word ADDRESSING_ACCUMULATOR();
        Word ADDRESSING_IMPLIED();

        // GET ADDRESS IN MEMORY
        Word ADDRESSING_IMMEDIATE();
        Word ADDRESSING_RELATIVE();
        Word ADDRESSING_INDIRECT();
        Word ADDRESSING_ABSOLUTE();
        Word ADDRESSING_ABSOLUTE_XINDEXED();
        Word ADDRESSING_ABSOLUTE_YINDEXED();
        Word ADDRESSING_XINDEXED_INDIRECT();
        Word ADDRESSING_INDIRECT_YINDEXED();
        Word ADDRESSING_ZEROPAGE();
        Word ADDRESSING_ZEROPAGE_XINDEXED();
        Word ADDRESSING_ZEROPAGE_YINDEXED();

        // ==========================TRANSFER============================
        // | LDA    :   Load Accumulator
        // | LDX    :   Load X register
        // | LDY    :   Load Y register
        // | STA    :   Store Accumulator
        // | STX    :   Store X register
        // | STY    :   Store Y register
        // | TAX    :   Transfer Accumulator to X register
        // | TAY    :   Transfer Accumulator to Y register
        // | TSX    :   Transfer Stack pointer to X register
        // | TXA    :   Transfer X register to Accumulator
        // | TXS    :   Transfer X register to Stack pointer
        // | TYA    :   Transfer Y register to Accumulator
        void LDA_Instruction(Word address);
        void LDX_Instruction(Word address);
        void LDY_Instruction(Word address);
        void STA_Instruction(Word address);
        void STX_Instruction(Word address);
        void STY_Instruction(Word address);
        void TAX_Instruction(Word address);
        void TAY_Instruction(Word address);
        void TSX_Instruction(Word address);
        void TXA_Instruction(Word address);
        void TXS_Instruction(Word address);
        void TYA_Instruction(Word address);
        
        
        // ============================STACK=============================
        // | PHA    :   Push Accumulator onto stack
        // | PHP    :   Push Processor Status register onto stack (sets break flag)
        // | PLA    :   Pull Accumulator from stack
        // | PLP    :   Pull Processor Status register from stack
        void PHA_Instruction(Word address);
        void PHP_Instruction(Word address);
        void PLA_Instruction(Word address);
        void PLP_Instruction(Word address);


        // ====================DECREMENTS=&=INCREMENTS===================
        // | DEC    :   Decrement values stored in memory
        // | DEX    :   Decrement X register
        // | DEY    :   Decrement Y register
        // | INC    :   Increment values stored in memory
        // | INX    :   Increment X register
        // | INY    :   Increment Y register
        void DEC_Instruction(Word address);
        void DEX_Instruction(Word address);
        void DEY_Instruction(Word address);
        void INC_Instruction(Word address);
        void INX_Instruction(Word address);
        void INY_Instruction(Word address);
        

        // =====================ARITHMETIC=OPERATIONS=====================
        // | https://www.masswerk.at/6502/6502_instruction_set.html#arithmetic
        // | ADC    :   Add with carry (prepare by CLC)
        // | SBC    :   Subtract with carry (prepare by SEC)
        void ADC_Instruction(Word address);
        void SBC_Instruction(Word address);
        
        // ======================LOGICAL=OPERATIONS=======================
        // | AND    :   And (with Accumulator)
        // | EOR    :   Exclusive or (with Accumulator)
        // | ORA    :   Or (with Accumulator)
        void AND_Instruction(Word address);
        void EOR_Instruction(Word address);
        void ORA_Instruction(Word address);


        // ========================SHIFT=&=ROTATE=========================
        // | ASL    :   Arithmetic shift left (shifts in a zero bit on the right)
        // | LSR    :   Logical shift right (shifts in a zero bit from the left)
        // | ROL    :   Rotate left (shifts in a carry bit on the right)
        // | ROR    :   Rotate right (shifts in a zero bit on the left)
        void ASL_Instruction(Word address); void ASL_Accumulator_Instruction(Word address);
        void LSR_Instruction(Word address); void LSR_Accumulator_Instruction(Word address);
        void ROL_Instruction(Word address); void ROL_Accumulator_Instruction(Word address);
        void ROR_Instruction(Word address); void ROR_Accumulator_Instruction(Word address);


        // =============================FLAG==============================
        // | CLC    :   Clear carry flag
        // | CLD    :   Clear decimal flag (Binary Coded Decimal arithmetics disabled)
        // | CLI    :   Clear interrupt disable flag
        // | CLV    :   Clear overflow flag
        // | SEC    :   Set carry flag
        // | SED    :   Set decimal flag (Binary Coded Decimal arithmetics enabled)
        // | SEI    :   Set interrupt disable flag
        void CLC_Instruction(Word address);
        void CLD_Instruction(Word address);
        void CLI_Instruction(Word address);
        void CLV_Instruction(Word address);
        void SEC_Instruction(Word address);
        void SED_Instruction(Word address);
        void SEI_Instruction(Word address);


        // =========================COMPARISONS===========================
        // | CMP    :   Compare (with Accumulator)
        // | CPX    :   Compare with X register
        // | CPY    :   Compare with Y register
        void CMP_Instruction(Word address);
        void CPX_Instruction(Word address);
        void CPY_Instruction(Word address);
        

        // ======================CONDITIONAL=BRANCH=======================
        // | BCC    :   Branch if carry clear
        // | BCS    :   Branch if carry set
        // | BEQ    :   Branch on equal (zero set)
        // | BMI    :   Branch on minus (negative set)
        // | BNE    :   Branch on not equal (zero clear)
        // | BPL    :   Branch on plus (negative clear)
        // | BVC    :   Branch on overflow clear
        // | BVS    :   Branch on overflow set
        void BCC_Instruction(Word address);
        void BCS_Instruction(Word address);
        void BEQ_Instruction(Word address);
        void BPL_Instruction(Word address);
        void BMI_Instruction(Word address);
        void BNE_Instruction(Word address);
        void BVC_Instruction(Word address);
        void BVS_Instruction(Word address);


        // =======================JUMPS=&=SUBROUTINES======================
        // | JMP    :   Jump
        // | JSR    :   Jump to subroutine
        // | RTS    :   Return from subroutine
        void JMP_Instruction(Word address);
        void JSR_Instruction(Word address);
        void RTS_Instruction(Word address);


        // =========================INTERRUPTS============================
        // | BRK    :   Break / software interrupt
        // | RTI    :   Return from interrupt
        void BRK_Instruction(Word address);
        void RTI_Instruction(Word address);

        // =============================OTHER=============================
        // | BIT    :   Bit test (Accumulator & Memory)
        // | NULL   :   No operation
        // | NOP    :   No operation
        // in the 6502, $00 was the BRK implied instruction, but to be sure to capture errors easily
        // (ie when incorrect memory is accessed) for this CPU, $00 will be a null instruction
        void BIT_Instruction(Word address);
        void NULL_Illegal_Instruction(Word address);
        void NOP_Illegal_Instruction(Word address);

        // =============================ILLEGAL=============================
        // | ALR    :   
        // | ANC    : 
        // | ANE    :   
        // | ARR    :
        // | DCP    :   
        // | ISC    :   
        // | LAS    :   
        // | LAX    :   
        // | LXA    :
        // | JAM    :   
        // | RLA    :
        // | RRA    :
        // | SAX    :   
        // | SBX    :   
        // | SHA    :
        // | SHX    :
        // | SHY    :
        // | SLO    :
        // | SRE    :
        // | TAS    :
        // | USBC   :
        void ALR_Illegal_Instruction(Word address);
        void ANC_Illegal_Instruction(Word address);
        void ANC2_Illegal_Instruction(Word address);

        void ANE_Illegal_Instruction(Word address);
        void ARR_Illegal_Instruction(Word address);
        void DCP_Illegal_Instruction(Word address);
        void ISC_Illegal_Instruction(Word address);
        void LAS_Illegal_Instruction(Word address);

        void LAX_Illegal_Instruction(Word address);
        void LXA_Illegal_Instruction(Word address);

        void JAM_Illegal_Instruction(Word address);
        void RLA_Illegal_Instruction(Word address);
        void RRA_Illegal_Instruction(Word address);

        void SAX_Illegal_Instruction(Word address);
        void SBX_Illegal_Instruction(Word address);
        void SHA_Illegal_Instruction(Word address);
        void SHX_Illegal_Instruction(Word address);
        void SHY_Illegal_Instruction(Word address);

        void SLO_Illegal_Instruction(Word address);
        void SRE_Illegal_Instruction(Word address);

        void TAS_Illegal_Instruction(Word address);
        void USBC_Illegal_Instruction(Word address);

        

        // ============================ADDRESS=MODES============================
        // | https://www.masswerk.at/6502/6502_instruction_set.html#modes
        // | SYMBOL :         NAME        :    OPERAND   :      DESCRIPTION
        // | A      : Accumulator         : A            :   operand is stored in the Accumulator register
        // | abs    : absolute            : $LLxxxxHH    :   operand is address stored in the next four bytes $HHxxxxLL *
        // | abs,X  : absolute,X          : $LLxxxxHH,X  :   operand is address stored in the next four bytes incremented by X register with carry $HHxxxxLL + X **
        // | abs,Y  : absolute,Y          : $LLxxxxHH,Y  :   operand is address stored in the next four bytes incremented by Y register with carry $HHxxxxLL + Y **
        // | #      : immediate           : #$BBBB       :   operand is the value of the next two bytes
        // | impl   : implied             :              :   no operand, implied by instruction
        // | ind    : indirect            : $($LLxxxxHH) :   operand is address stored in the memory address represented by the next four bytes $($HHxxxxLL)
        // | X, ind : X-indexed, indirect : $($LLxx,X)   :   operand is zeropage address stored in the next two bytes incremented by X register without carry, ie wraps around in zeropage $($0000xxLL + X)
        // | ind, Y : indirect, Y-indexed : $($LLxx),Y   :   operand is zeropage address stored in the next two bytes incremented by Y register with carry $($0000xxLL) + Y
        // | rel    : relative            : $BBBB        :   branch target is a signed 16 bit offset (next two bytes) from the current PC value ***
        // | zpg    : zeropage            : $LLxx        :   operand is address stored in the next two bytes $0000xxLL
        // | zpg,X  : zeropage,X          : $LLxx,X      :   operand is address stored in the next two bytes incremented by X register with carry $0000xxLL + X **
        // | zpg,Y  : zeropage,Y          : $LLxx,Y      :   operand is address stored in the next two bytes incremented by Y register with carry $0000xxLL + Y **
        // |
        // |
        // |    %   Binary
        // |    N   Decimal
        // |    $N  Hexadecimal     Hexadecimal without literal value operator represents a memory address
        // |    #   Literal value
        // |    
        // |
        // |    *   32-bit address words are little endian, low bytes first, follwowed by high bytes
        // |        ie $HHxxxxLL is stored as $LL $xx $xx $HH
        // |
        // |    **  The available 32 bit address space consists of pages of 65536 bytes each
        // |        The high bytes (stored as the last 2 bytes) represent the page index. An increment with carry
        // |        will affect the high bytes (crossing page boundaries), adding an extra
        // |        cycle to execution. (unrelated to the state of carry bit in the P register)
        // |
        // |    *** Branch offsets are signed 16 bit values, -32768 to 32767, negative offsets in two's complement
        // |        Page transitions may occur and add an extra cycle to execution
        // |
        
        
        public:
            // Instruction opcodes (1 byte)
            // https://en.wikipedia.org/wiki/X86_instruction_listings#Added_as_instruction_set_extensions
            // https://www.masswerk.at/6502/6502_instruction_set.html
            static constexpr u8
                INS_NULL = 0x00, // Null Instruction (ILLEGAL)
                INS_BRK_IMPL = 0x00, // Break/Interrupt, implied addressing
                INS_ORA_X_IND = 0x01, // OR Accumulator, indexed by X register, indirect addressing
                INS_JAM_0 = 0x02, // Illegal opcode (ILLEGAL)
                INS_SLO_X_IND = 0x03, // Shift left one bit, OR Accumulator, indexed by X register, indirect addressing (ILLEGAL)
                INS_NOP_ZP = 0x04, // No operation, zero page addressing (ILLEGAL)
                INS_ORA_ZP = 0x05, // OR Accumulator, zero page addressing
                INS_ASL_ZP = 0x06, // Arithmetic shift left, zero page addressing
                INS_SLO_ZP = 0x07, // Shift left one bit, OR Accumulator, zero page addressing (ILLEGAL)
                INS_PHP_IMPL = 0x08, // Push Processor Status register, implied addressing
                INS_ORA_IMM = 0x09, // OR Accumulator, immediate addressing
                INS_ASL_ACC = 0x0A, // Arithmetic shift left, accumulator addressing
                INS_ANC_IMM = 0x0B, // AND Accumulator, set C, immediate addressing (ILLEGAL)
                INS_NOP_ABS = 0x0C, // No operation, absolute addressing (ILLEGAL)
                INS_ORA_ABS = 0x0D, // OR Accumulator, absolute addressing
                INS_ASL_ABS = 0x0E, // Arithmetic shift left, absolute addressing
                INS_SLO_ABS = 0x0F, // Shift left one bit, OR Accumulator, absolute addressing (ILLEGAL)
                INS_BPL_REL = 0x10, // Branch on plus (negative clear), relative addressing
                INS_ORA_IND_Y = 0x11, // OR Accumulator, indirect indexed by Y register addressing
                INS_JAM_1 = 0x12, // Illegal opcode (ILLEGAL)
                INS_SLO_IND_Y = 0x13, // Shift left one bit, OR Accumulator, indirect indexed by Y register addressing (ILLEGAL)
                INS_NOP_ZP_X = 0x14, // No operation, zero page indexed by X register addressing (ILLEGAL)
                INS_ORA_ZP_X = 0x15, // OR Accumulator, zero page indexed by X register addressing
                INS_ASL_ZP_X = 0x16, // Arithmetic shift left, zero page indexed by X register addressing
                INS_SLO_ZP_X = 0x17, // Shift left one bit, OR Accumulator, zero page indexed by X register addressing (ILLEGAL)
                INS_CLC_IMPL = 0x18, // Clear carry flag, implied addressing
                INS_ORA_ABS_Y = 0x19, // OR Accumulator, absolute indexed by Y register addressing
                INS_NOP_IMPL = 0x1A, // No operation, implied addressing (ILLEGAL)
                INS_SLO_ABS_Y = 0x1B, // Shift left one bit, OR Accumulator, absolute indexed by Y register addressing (ILLEGAL)
                INS_NOP_ABS_X = 0x1C, // No operation, absolute indexed by X register addressing (ILLEGAL)
                INS_ORA_ABS_X = 0x1D, // OR Accumulator, absolute indexed by X register addressing
                INS_ASL_ABS_X = 0x1E, // Arithmetic shift left, absolute indexed by X register addressing
                INS_SLO_ABS_X = 0x1F, // Shift left one bit, OR Accumulator, absolute indexed by X register addressing (ILLEGAL)
                INS_JSR_ABS = 0x20, // Jump to subroutine, absolute addressing
                INS_AND_X_IND = 0x21, // AND Accumulator, indexed by X register, indirect addressing
                INS_JAM_2 = 0x22, // Illegal opcode (ILLEGAL)
                INS_RLA_X_IND = 0x23, // Rotate left one bit, AND Accumulator, indexed by X register, indirect addressing (ILLEGAL)
                INS_BIT_ZP = 0x24, // Bit test, zero page addressing
                INS_AND_ZP = 0x25, // AND Accumulator, zero page addressing
                INS_ROL_ZP = 0x26, // Rotate left one bit, zero page addressing
                INS_RLA_ZP = 0x27, // Rotate left one bit, AND Accumulator, zero page addressing (ILLEGAL)
                INS_PLP_IMPL = 0x28, // Pull Processor Status register, implied addressing
                INS_AND_IMM = 0x29, // AND Accumulator, immediate addressing
                INS_ROL_ACC = 0x2A, // Rotate left one bit, accumulator addressing
                INS_ANC_IMM_2 = 0x2B, // AND Accumulator, set C, immediate addressing (ILLEGAL)
                INS_BIT_ABS = 0x2C, // Bit test, absolute addressing
                INS_AND_ABS = 0x2D, // AND Accumulator, absolute addressing
                INS_ROL_ABS = 0x2E, // Rotate left one bit, absolute addressing
                INS_RLA_ABS = 0x2F, // Rotate left one bit, AND Accumulator, absolute addressing (ILLEGAL)
                INS_BMI_REL = 0x30, // Branch on minus (negative set), relative addressing
                INS_AND_IND_Y = 0x31, // AND Accumulator, indirect indexed by Y register addressing
                INS_JAM_3 = 0x32, // Illegal opcode (ILLEGAL)
                INS_RLA_IND_Y = 0x33, // Rotate left one bit, AND Accumulator, indirect indexed by Y register addressing (ILLEGAL)
                INS_NOP_ZP_X_2 = 0x34, // No operation, zero page indexed by X register addressing (ILLEGAL)
                INS_AND_ZP_X = 0x35, // AND Accumulator, zero page indexed by X register addressing
                INS_ROL_ZP_X = 0x36, // Rotate left one bit, zero page indexed by X register addressing
                INS_RLA_ZP_X = 0x37, // Rotate left one bit, AND Accumulator, zero page indexed by X register addressing (ILLEGAL)
                INS_SEC_IMPL = 0x38, // Set carry flag, implied addressing
                INS_AND_ABS_Y = 0x39, // AND Accumulator, absolute indexed by Y register addressing
                INS_NOP_IMPL_2 = 0x3A, // No operation, implied addressing (ILLEGAL)
                INS_RLA_ABS_Y = 0x3B, // Rotate left one bit, AND Accumulator, absolute indexed by Y register addressing (ILLEGAL)
                INS_NOP_ABS_X_2 = 0x3C, // No operation, absolute indexed by X register addressing (ILLEGAL)
                INS_AND_ABS_X = 0x3D, // AND Accumulator, absolute indexed by X register addressing
                INS_ROL_ABS_X = 0x3E, // Rotate left one bit, absolute indexed by X register addressing
                INS_RLA_ABS_X = 0x3F, // Rotate left one bit, AND Accumulator, absolute indexed by X register addressing (ILLEGAL)
                INS_RTI_IMPL = 0x40, // Return from interrupt, implied addressing
                INS_EOR_X_IND = 0x41, // Exclusive or, indexed by X register, indirect addressing
                INS_JAM_4 = 0x42, // Illegal opcode (ILLEGAL)
                INS_SRE_X_IND = 0x43, // Shift right one bit, exclusive or, indexed by X register, indirect addressing (ILLEGAL)
                INS_NOP_ZP_2 = 0x44, // No operation, zero page addressing (ILLEGAL)
                INS_EOR_ZP = 0x45, // Exclusive or, zero page addressing
                INS_LSR_ZP = 0x46, // Logical shift right, zero page addressing
                INS_SRE_ZP = 0x47, // Shift right one bit, exclusive or, zero page addressing (ILLEGAL)
                INS_PHA_IMPL = 0x48, // Push Accumulator, implied addressing
                INS_EOR_IMM = 0x49, // Exclusive or, immediate addressing
                INS_LSR_ACC = 0x4A, // Logical shift right, accumulator addressing
                INS_ALR_IMM = 0x4B, // AND Accumulator, logical shift right, immediate addressing (ILLEGAL)
                INS_JMP_ABS = 0x4C, // Jump, absolute addressing
                INS_EOR_ABS = 0x4D, // Exclusive or, absolute addressing
                INS_LSR_ABS = 0x4E, // Logical shift right, absolute addressing
                INS_SRE_ABS = 0x4F, // Shift right one bit, exclusive or, absolute addressing (ILLEGAL)
                INS_BVC_REL = 0x50, // Branch on overflow clear, relative addressing
                INS_EOR_IND_Y = 0x51, // Exclusive or, indirect indexed by Y register addressing
                INS_JAM_5 = 0x52, // Illegal opcode (ILLEGAL)
                INS_SRE_IND_Y = 0x53, // Shift right one bit, exclusive or, indirect indexed by Y register addressing (ILLEGAL)
                INS_NOP_ZP_X_3 = 0x54, // No operation, zero page indexed by X register addressing (ILLEGAL)
                INS_EOR_ZP_X = 0x55, // Exclusive or, zero page indexed by X register addressing
                INS_LSR_ZP_X = 0x56, // Logical shift right, zero page indexed by X register addressing
                INS_SRE_ZP_X = 0x57, // Shift right one bit, exclusive or, zero page indexed by X register addressing (ILLEGAL)
                INS_CLI_IMPL = 0x58, // Clear interrupt disable flag, implied addressing
                INS_EOR_ABS_Y = 0x59, // Exclusive or, absolute indexed by Y register addressing
                INS_NOP_IMPL_3 = 0x5A, // No operation, implied addressing (ILLEGAL)
                INS_SRE_ABS_Y = 0x5B, // Shift right one bit, exclusive or, absolute indexed by Y register addressing (ILLEGAL)
                INS_NOP_ABS_X_3 = 0x5C, // No operation, absolute indexed by X register addressing (ILLEGAL)
                INS_EOR_ABS_X = 0x5D, // Exclusive or, absolute indexed by X register addressing
                INS_LSR_ABS_X = 0x5E, // Logical shift right, absolute indexed by X register addressing
                INS_SRE_ABS_X = 0x5F, // Shift right one bit, exclusive or, absolute indexed by X register addressing (ILLEGAL)
                INS_RTS_IMPL = 0x60, // Return from subroutine, implied addressing
                INS_ADC_X_IND = 0x61, // Add with carry, indexed by X register, indirect addressing
                INS_JAM_6 = 0x62, // Illegal opcode (ILLEGAL)
                INS_RRA_X_IND = 0x63, // Rotate right one bit, add with carry, indexed by X register, indirect addressing (ILLEGAL)
                INS_NOP_ZP_4 = 0x64, // No operation, zero page addressing (ILLEGAL)
                INS_ADC_ZP = 0x65, // Add with carry, zero page addressing
                INS_ROR_ZP = 0x66, // Rotate right one bit, zero page addressing
                INS_RRA_ZP = 0x67, // Rotate right one bit, add with carry, zero page addressing (ILLEGAL)
                INS_PLA_IMPL = 0x68, // Pull Accumulator, implied addressing
                INS_ADC_IMM = 0x69, // Add with carry, immediate addressing
                INS_ROR_ACC = 0x6A, // Rotate right one bit, accumulator addressing
                INS_ARR_IMM = 0x6B, // AND Accumulator, rotate right one bit, immediate addressing (ILLEGAL)
                INS_JMP_IND = 0x6C, // Jump, indirect addressing
                INS_ADC_ABS = 0x6D, // Add with carry, absolute addressing
                INS_ROR_ABS = 0x6E, // Rotate right one bit, absolute addressing
                INS_RRA_ABS = 0x6F, // Rotate right one bit, add with carry, absolute addressing (ILLEGAL)
                INS_BVS_REL = 0x70, // Branch on overflow set, relative addressing
                INS_ADC_IND_Y = 0x71, // Add with carry, indirect indexed by Y register addressing
                INS_JAM_7 = 0x72, // Illegal opcode (ILLEGAL)
                INS_RRA_IND_Y = 0x73, // Rotate right one bit, add with carry, indirect indexed by Y register addressing (ILLEGAL)
                INS_NOP_ZP_X_4 = 0x74, // No operation, zero page indexed by X register addressing (ILLEGAL)
                INS_ADC_ZP_X = 0x75, // Add with carry, zero page indexed by X register addressing
                INS_ROR_ZP_X = 0x76, // Rotate right one bit, zero page indexed by X register addressing
                INS_RRA_ZP_X = 0x77, // Rotate right one bit, add with carry, zero page indexed by X register addressing (ILLEGAL)
                INS_SEI_IMPL = 0x78, // Set interrupt disable flag, implied addressing
                INS_ADC_ABS_Y = 0x79, // Add with carry, absolute indexed by Y register addressing
                INS_NOP_IMPL_4 = 0x7A, // No operation, implied addressing (ILLEGAL)
                INS_RRA_ABS_Y = 0x7B, // Rotate right one bit, add with carry, absolute indexed by Y register addressing (ILLEGAL)
                INS_NOP_ABS_X_4 = 0x7C, // No operation, absolute indexed by X register addressing (ILLEGAL)
                INS_ADC_ABS_X = 0x7D, // Add with carry, absolute indexed by X register addressing
                INS_ROR_ABS_X = 0x7E, // Rotate right one bit, absolute indexed by X register addressing
                INS_RRA_ABS_X = 0x7F, // Rotate right one bit, add with carry, absolute indexed by X register addressing (ILLEGAL)
                INS_NOP_IMM = 0x80, // No operation, immediate addressing (ILLEGAL)
                INS_STA_X_IND = 0x81, // Store Accumulator, indexed by X register, indirect addressing
                INS_NOP_IMM_2 = 0x82, // No operation, immediate addressing (ILLEGAL)
                INS_SAX_X_IND = 0x83, // AND X register with accumulator, store result in memory, indexed by X register, indirect addressing (ILLEGAL)
                INS_STY_ZP = 0x84, // Store Y register, zero page addressing
                INS_STA_ZP = 0x85, // Store Accumulator, zero page addressing
                INS_STX_ZP = 0x86, // Store X register, zero page addressing
                INS_SAX_ZP = 0x87, // AND X register with accumulator, store result in memory, zero page addressing (ILLEGAL)
                INS_DEY_IMPL = 0x88, // Decrement Y register, implied addressing
                INS_NOP_IMM_3 = 0x89, // No operation, immediate addressing (ILLEGAL)
                INS_TXA_IMPL = 0x8A, // Transfer X register to Accumulator, implied addressing
                INS_ANE_IMM = 0x8B, // Transfer X register to Accumulator, AND immediate, implied addressing (ILLEGAL)
                INS_STY_ABS = 0x8C, // Store Y register, absolute addressing
                INS_STA_ABS = 0x8D, // Store Accumulator, absolute addressing
                INS_STX_ABS = 0x8E, // Store X register, absolute addressing
                INS_SAX_ABS = 0x8F, // AND X register with accumulator, store result in memory, absolute addressing (ILLEGAL)
                INS_BCC_REL = 0x90, // Branch on carry clear, relative addressing
                INS_STA_IND_Y = 0x91, // Store Accumulator, indirect indexed by Y register addressing
                INS_JAM_8 = 0x92, // Illegal opcode (ILLEGAL)
                INS_SHA_IND_Y = 0x93, // AND X register with accumulator, AND high byte of memory, store result in memory, indirect indexed by Y register addressing (ILLEGAL)
                INS_STY_ZP_X = 0x94, // Store Y register, zero page indexed by X register addressing
                INS_STA_ZP_X = 0x95, // Store Accumulator, zero page indexed by X register addressing
                INS_STX_ZP_Y = 0x96, // Store X register, zero page indexed by Y register addressing
                INS_SAX_ZP_Y = 0x97, // AND X register with accumulator, store result in memory, zero page indexed by Y register addressing (ILLEGAL)
                INS_TYA_IMPL = 0x98, // Transfer Y register to Accumulator, implied addressing
                INS_STA_ABS_Y = 0x99, // Store Accumulator, absolute indexed by Y register addressing
                INS_TXS_IMPL = 0x9A, // Transfer X register to Stack pointer, implied addressing
                INS_TAS_ABS_Y = 0x9B, // Transfer Accumulator to Stack pointer, AND X register, store result in memory, absolute indexed by Y register addressing (ILLEGAL)
                INS_SHY_ABS_X = 0x9C, // AND Y register with high byte of memory, store result in memory, absolute indexed by X register addressing (ILLEGAL)
                INS_STA_ABS_X = 0x9D, // Store Accumulator, absolute indexed by X register addressing
                INS_SHX_ABS_Y = 0x9E, // AND X register with high byte of memory, store result in memory, absolute indexed by Y register addressing (ILLEGAL)
                INS_SHA_ABS_Y = 0x9F, // AND X register with accumulator, AND high byte of memory, store result in memory, absolute indexed by Y register addressing (ILLEGAL)
                INS_LDY_IMM = 0xA0, // Load Y register, immediate addressing
                INS_LDA_X_IND = 0xA1, // Load Accumulator, indexed by X register, indirect addressing
                INS_LDX_IMM = 0xA2, // Load X register, immediate addressing
                INS_LAX_X_IND = 0xA3, // Load Accumulator and X register, indexed by X register, indirect addressing (ILLEGAL)
                INS_LDY_ZP = 0xA4, // Load Y register, zero page addressing
                INS_LDA_ZP = 0xA5, // Load Accumulator, zero page addressing
                INS_LDX_ZP = 0xA6, // Load X register, zero page addressing
                INS_LAX_ZP = 0xA7, // Load Accumulator and X register, zero page addressing (ILLEGAL)
                INS_TAY_IMPL = 0xA8, // Transfer Accumulator to Y register, implied addressing
                INS_LDA_IMM = 0xA9, // Load Accumulator, immediate addressing
                INS_TAX_IMPL = 0xAA, // Transfer Accumulator to X register, implied addressing
                INS_LXA_IMM = 0xAB, // Load Accumulator and X register, immediate addressing (ILLEGAL)
                INS_LDY_ABS = 0xAC, // Load Y register, absolute addressing
                INS_LDA_ABS = 0xAD, // Load Accumulator, absolute addressing
                INS_LDX_ABS = 0xAE, // Load X register, absolute addressing
                INS_LAX_ABS = 0xAF, // Load Accumulator and X register, absolute addressing (ILLEGAL)
                INS_BCS_REL = 0xB0, // Branch on carry set, relative addressing
                INS_LDA_IND_Y = 0xB1, // Load Accumulator, indirect indexed by Y register addressing
                INS_JAM_9 = 0xB2, // Illegal opcode (ILLEGAL)
                INS_LAX_IND_Y = 0xB3, // Load Accumulator and X register, indirect indexed by Y register addressing (ILLEGAL)
                INS_LDY_ZP_X = 0xB4, // Load Y register, zero page indexed by X register addressing
                INS_LDA_ZP_X = 0xB5, // Load Accumulator, zero page indexed by X register addressing
                INS_LDX_ZP_Y = 0xB6, // Load X register, zero page indexed by Y register addressing
                INS_LAX_ZP_Y = 0xB7, // Load Accumulator and X register, zero page indexed by Y register addressing (ILLEGAL)
                INS_CLV_IMPL = 0xB8, // Clear overflow flag, implied addressing
                INS_LDA_ABS_Y = 0xB9, // Load Accumulator, absolute indexed by Y register addressing
                INS_TSX_IMPL = 0xBA, // Transfer Stack pointer to X register, implied addressing
                INS_LAS_ABS_Y = 0xBB, // Load Accumulator and Stack pointer, AND high byte of memory, store result in memory, absolute indexed by Y register addressing (ILLEGAL)
                INS_LDY_ABS_X = 0xBC, // Load Y register, absolute indexed by X register addressing
                INS_LDA_ABS_X = 0xBD, // Load Accumulator, absolute indexed by X register addressing
                INS_LDX_ABS_Y = 0xBE, // Load X register, absolute indexed by Y register addressing
                INS_LAX_ABS_Y = 0xBF, // Load Accumulator and X register, absolute indexed by Y register addressing (ILLEGAL)
                INS_CPY_IMM = 0xC0, // Compare Y register, immediate addressing
                INS_CMP_X_IND = 0xC1, // Compare Accumulator, indexed by X register, indirect addressing
                INS_NOP_IMM_4 = 0xC2, // No operation, immediate addressing (ILLEGAL)
                INS_DCP_X_IND = 0xC3, // Decrement memory, compare Accumulator, indexed by X register, indirect addressing (ILLEGAL)
                INS_CPY_ZP = 0xC4, // Compare Y register, zero page addressing
                INS_CMP_ZP = 0xC5, // Compare Accumulator, zero page addressing
                INS_DEC_ZP = 0xC6, // Decrement memory, zero page addressing
                INS_DCP_ZP = 0xC7, // Decrement memory, compare Accumulator, zero page addressing (ILLEGAL)
                INS_INY_IMPL = 0xC8, // Increment Y register, implied addressing
                INS_CMP_IMM = 0xC9, // Compare Accumulator, immediate addressing
                INS_DEX_IMPL = 0xCA, // Decrement X register, implied addressing
                INS_SBX_IMM = 0xCB, // AND X register with accumulator, subtract immediate, store result in X register, implied addressing (ILLEGAL)
                INS_CPY_ABS = 0xCC, // Compare Y register, absolute addressing
                INS_CMP_ABS = 0xCD, // Compare Accumulator, absolute addressing
                INS_DEC_ABS = 0xCE, // Decrement memory, absolute addressing
                INS_DCP_ABS = 0xCF, // Decrement memory, compare Accumulator, absolute addressing (ILLEGAL)
                INS_BNE_REL = 0xD0, // Branch on not equal (zero clear), relative addressing
                INS_CMP_IND_Y = 0xD1, // Compare Accumulator, indirect indexed by Y register addressing
                INS_JAM_10 = 0xD2, // Illegal opcode (ILLEGAL)
                INS_DCP_IND_Y = 0xD3, // Decrement memory, compare Accumulator, indirect indexed by Y register addressing (ILLEGAL)
                INS_NOP_ZP_X_5 = 0xD4, // No operation, zero page indexed by X register addressing (ILLEGAL)
                INS_CMP_ZP_X = 0xD5, // Compare Accumulator, zero page indexed by X register addressing
                INS_DEC_ZP_X = 0xD6, // Decrement memory, zero page indexed by X register addressing
                INS_DCP_ZP_X = 0xD7, // Decrement memory, compare Accumulator, zero page indexed by X register addressing (ILLEGAL)
                INS_CLD_IMPL = 0xD8, // Clear decimal flag, implied addressing
                INS_CMP_ABS_Y = 0xD9, // Compare Accumulator, absolute indexed by Y register addressing
                INS_NOP_IMPL_5 = 0xDA, // No operation, implied addressing (ILLEGAL)
                INS_DCP_ABS_Y = 0xDB, // Decrement memory, compare Accumulator, absolute indexed by Y register addressing (ILLEGAL)
                INS_NOP_ABS_X_5 = 0xDC, // No operation, absolute indexed by X register addressing (ILLEGAL)
                INS_CMP_ABS_X = 0xDD, // Compare Accumulator, absolute indexed by X register addressing
                INS_DEC_ABS_X = 0xDE, // Decrement memory, absolute indexed by X register addressing
                INS_DCP_ABS_X = 0xDF, // Decrement memory, compare Accumulator, absolute indexed by X register addressing (ILLEGAL)
                INS_CPX_IMM = 0xE0, // Compare X register, immediate addressing
                INS_SBC_X_IND = 0xE1, // Subtract with carry, indexed by X register, indirect addressing
                INS_NOP_IMM_5 = 0xE2, // No operation, immediate addressing (ILLEGAL)
                INS_ISC_X_IND = 0xE3, // Increment memory, subtract with carry, indexed by X register, indirect addressing (ILLEGAL)
                INS_CPX_ZP = 0xE4, // Compare X register, zero page addressing
                INS_SBC_ZP = 0xE5, // Subtract with carry, zero page addressing
                INS_INC_ZP = 0xE6, // Increment memory, zero page addressing
                INS_ISC_ZP = 0xE7, // Increment memory, subtract with carry, zero page addressing (ILLEGAL)
                INS_INX_IMPL = 0xE8, // Increment X register, implied addressing
                INS_SBC_IMM = 0xE9, // Subtract with carry, immediate addressing
                INS_NOP_IMPL_6 = 0xEA, // No operation, implied addressing (ILLEGAL)
                INS_USBC_IMM = 0xEB, // Subtract with carry, immediate addressing (ILLEGAL)
                INS_CPX_ABS = 0xEC, // Compare X register, absolute addressing
                INS_SBC_ABS = 0xED, // Subtract with carry, absolute addressing
                INS_INC_ABS = 0xEE, // Increment memory, absolute addressing
                INS_ISC_ABS = 0xEF, // Increment memory, subtract with carry, absolute addressing (ILLEGAL)
                INS_BEQ_REL = 0xF0, // Branch on equal (zero set), relative addressing
                INS_SBC_IND_Y = 0xF1, // Subtract with carry, indirect indexed by Y register addressing
                INS_JAM_11 = 0xF2, // Illegal opcode (ILLEGAL)
                INS_ISC_IND_Y = 0xF3, // Increment memory, subtract with carry, indirect indexed by Y register addressing (ILLEGAL)
                INS_NOP_ZP_X_6 = 0xF4, // No operation, zero page indexed by X register addressing (ILLEGAL)
                INS_SBC_ZP_X = 0xF5, // Subtract with carry, zero page indexed by X register addressing
                INS_INC_ZP_X = 0xF6, // Increment memory, zero page indexed by X register addressing
                INS_ISC_ZP_X = 0xF7, // Increment memory, subtract with carry, zero page indexed by X register addressing (ILLEGAL)
                INS_SED_IMPL = 0xF8, // Set decimal flag, implied addressing
                INS_SBC_ABS_Y = 0xF9, // Subtract with carry, absolute indexed by Y register addressing
                INS_NOP_IMPL_7 = 0xFA, // No operation, implied addressing (ILLEGAL)
                INS_ISC_ABS_Y = 0xFB, // Increment memory, subtract with carry, absolute indexed by Y register addressing (ILLEGAL)
                INS_NOP_ABS_X_6 = 0xFC, // No operation, absolute indexed by X register addressing (ILLEGAL)
                INS_SBC_ABS_X = 0xFD, // Subtract with carry, absolute indexed by X register addressing
                INS_INC_ABS_X = 0xFE, // Increment memory, absolute indexed by X register addressing
                INS_ISC_ABS_X = 0xFF; // Increment memory, subtract with carry, absolute indexed by X register addressing (ILLEGAL)
};

#endif // ALIENCPU_H