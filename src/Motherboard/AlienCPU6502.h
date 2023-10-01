#ifndef ALIENCPU6502_H
#define ALIENCPU6502_H

#include <Types.h>
#include <../src/Motherboard/Memory/RAM.h>
#include <string>
#include <functional>

class AlienCPU6502; //forward declaration (!!)

// CPU Emulator (modeled off of 6502)
// https://en.wikipedia.org/wiki/Central_processing_unit
// http://www.6502.org/users/obelisk/index.html

// 32 bit CPU (expanded version of 6502)
//  - 1 Mb of memory (addressable via 32 bit address bus : 0x00000000 - 0x000FFFFF)
//      - 0x00000000 to 0x0000FFFF : Zero page (65536 bytes)
//      - 0x00010000 to 0x0001FFFF : Stack memory (65536 bytes)
//      - 0x00020000 to 0x000FFFEF : General purpose memory (917488 bytes)
//      - 0x000FFFF0 to 0x000FFFFF : Special reserved memory (16 bytes)
//          * 0x000FFFF0 to 0x000FFFF3 : Interrupt handler
//          * 0x000FFFF4 to 0x000FFFF7 : Power on reset location
//          * 0x000FFFF8 to 0x000FFFFB : BRK / Interrupt Request (IRQ) handler
//          * 0x000FFFFC to 0x000FFFFF : Reserved
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
class AlienCPU6502 {
    public:
        static const std::string VERSION;

        // Number of cycles between each Interrupt check
        static const Word INTERRUPT_CHECK_INTERVAL = 0x10;


        // ================INSTRUCTIONS================
        // Total Number of instructions supported by the processor
        static const u16 INSTRUCTION_COUNT = 0x0100; // 256 instructions
    
        // Instruction opcodes (1 byte)
        // https://en.wikipedia.org/wiki/X86_instruction_listings#Added_as_instruction_set_extensions
        // https://www.masswerk.at/6502/6502_instruction_set.html
        static constexpr u8
            INS_NULL = 0x00, // Null Instruction (ILLEGAL)
            INS_ORA_X_IND = 0x01, // OR Accumulator, indexed by X register, indirect addressing
            INS_BRK_IMPL = 0x02, // Break/Interrupt, implied addressing
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


            INS_LDA_IM = 0xA9; // Load Accumulator, immediate addressing



        // Null address
        static const Word NULL_ADDRESS = 0x00000000;


        // Program Stack
        // SP_INIT - STACK_SIZE = End of stack
        static const Word STACK_SIZE = 0x00010000; // 65536 Bytes of STACK MEMORY


        // should never exceed 0x000FFFFF
        static const Word PC_INIT = 0x00000000;
        
        static constexpr u16
            SP_INIT = 0xFFFF, // stored as an offset from 0x00000100

            A_INIT = 0x0000, 
            X_INIT = 0x0000,
            Y_INIT = 0x0000;



        static const Byte P_INIT = 0b00100000;

    //private:
        
        // Instruction Set
        using Instruction = std::function<void(AlienCPU6502&)>;
        Instruction instructions[INSTRUCTION_COUNT];

        // System Memory
        // 0x00100000 total memory (0x00000000 - 0x000FFFFF)
        //
        // 0x00000000 - 0x000000FF : Reserved for boot process
        // 0x00000100 - 0x000100FF : Stack memory
        // 0x00010100 - 0x000FFFFF : General purpose memory 
        RAM ram;

        // Number of cycles till the next Interrupt should be processed
        Word nextInterruptCheck;

        // cycle counter
        u64 cycles;


        // ==============Program Counter Register===============
        //  - memory address of the next instruction byte to be executed
        //  - PCL and PCH are the low and high bytes of the PC register
        //  - can be modified by the execution of a jump, subroutine (function) call, or branches (if/else)
        //    or by returning from a subroutine or interrupt
        Word pc;

        // ===============Stack Pointer Register================
        //  - points to the first byte of the top element of the call stack
        //  - stored as an offset from the start of the stack page (0x0100 in 6502)
        //  - every stack element is 4 bytes big so to add elements to the stack, 
        //    the stack pointer is decremented by 4 (TODO figure out stack element sizes)
        //      - [SP+0] = byte 3
        //      - [SP+1] = byte 2      (little endian)
        //      - [SP+2] = byte 1
        //      - [SP+3] = byte 0
        // https://en.wikipedia.org/wiki/Stack_register
        // https://en.wikipedia.org/wiki/Call_stack
        u16 sp;

        // ==============General Purpose Registers==============
        // https://codebase64.org/doku.php?id=base:6502_registers
        u16 a; // accumulator, main register for arithmetic and logic operations (Direct connection to ALU)
        u16 x; // index register X, addressing data with indices (like arrays)
        u16 y; // index register Y, addressing data with indices (like arrays)


        // ==============Processor Status register==============
        // https://codebase64.org/doku.php?id=base:6502_registers
        // https://www.nesdev.org/wiki/Status_flags
        // stores flags
        //
        // bit 7 -> NV1BBDIZC <- bit 0
        // - N (Negative)
        //   - Set after any arithmetic operations (when A, X, or Y registers are loaded with a value)
        //     Stores the topmost bit of the register being loaded, generally used for signed integers
        //     Behaves differently in Decimal instructions
        //
        // - V (Overflow)
        //   - Set after addition or subtraction operations if signed overflow occurs
        //   - Reset after any other operation
        // - 1 (Unused)
        //   - Always 1
        //
        // - B (Break) https://en.wikipedia.org/wiki/Interrupts_in_65xx_processors
        //   - Distinguishes software (BRK) interrupts from hardware (IRQ or NMI) interrupts
        //   - (0) hardware interrupt 
        //      - RESET: Reset signal, resets the CPU
        //      - NMI: Non-maskable interrupt, must be immediately handled
        //      - IRQ: Interrupt request, can be enabled/disabled
        //   - (1) software interrupt (BRK instruction)
        //   - Always set except when the P register is pushed to the stack when processing a 
        //     hardware interrupt
        //
        // - D (Decimal)
        //   - Used to select the Binary Coded Decimal mode for addition and subtraction
        //   - Defaults to 0 (binary mode)
        //
        // - I (Interrupt Disable)
        //   - If set, the CPU will ignore all IRQ interrupts and prevent jumping to the IRQ handler vector
        //   - Set after CPU processes an interrupt request 
        //
        // - Z (Zero)
        //   - Set if the result of the last operation was zero
        //   - Reset if the result of the last operation was non-zero
        //
        // - C (Carry)
        //  - Set if the last operation resulted in a carry or borrow
        //  - Reset if the last operation did not result in a carry or borrow
        Byte p; // processor status, flags register

        
    public:
        AlienCPU6502();
        void Start(u64 maxCycles = 0);
    
    private: 
        void Reset();

        Byte FetchNextByte();
        Word FetchNextWord();

        Byte ReadByte(Word address);
        Word ReadWord(Word address);

        void WriteByte(Word address, Byte value);
        void WriteWord(Word address, Word value);

        void SPtoAddress(Byte page = 0);
        void PushWordToStack(Word value);
        Word PopWordFromStack();
        void PushByteToStack(Byte value);
        Byte PopByteFromStack();

        void ExecuteInstruction(u16 instruction);
        bool ValidInstruction(u16 instruction);

        // =====================INSTRUCTIONS=====================
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

        // ======================TRANSFER========================
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
        void _A1_LDA_XIndexed_Indirect_Instruction();
        void _A5_LDA_ZeroPage_Instruction();
        void _A9_LDA_Immediate_Instruction();
        void _AD_LDA_Absolute_Instruction();
        void _B1_LDA_Indirect_YIndexed_Instruction();
        void _B5_LDA_ZeroPage_XIndexed_Instruction();
        void _B9_LDA_Absolute_YIndexed_Instruction();
        void _BD_LDA_Absolute_XIndexed_Instruction();

        void _A2_LDX_Immediate_Instruction();
        void _A6_LDX_ZeroPage_Instruction();
        void _AE_LDX_Absolute_Instruction();
        void _B6_LDX_ZeroPage_YIndexed_Instruction();
        void _BE_LDX_Absolute_YIndexed_Instruction();

        void _A0_LDY_Immediate_Instruction();
        void _A4_LDY_ZeroPage_Instruction();
        void _AC_LDY_Absolute_Instruction();
        void _B4_LDY_ZeroPage_XIndexed_Instruction();
        void _BC_LDY_Absolute_XIndexed_Instruction();

        void _81_STA_XIndexed_Indirect_Instruction();
        void _85_STA_ZeroPage_Instruction();
        void _8D_STA_Absolute_Instruction();
        void _91_STA_Indirect_YIndexed_Instruction();
        void _95_STA_ZeroPage_XIndexed_Instruction();
        void _99_STA_Absolute_YIndexed_Instruction();
        void _9D_STA_Absolute_XIndexed_Instruction();

        void _86_STX_ZeroPage_Instruction();
        void _8E_STX_Absolute_Instruction();
        void _96_STX_ZeroPage_YIndexed_Instruction();

        void _84_STY_ZeroPage_Instruction();
        void _8C_STY_Absolute_Instruction();
        void _94_STY_ZeroPage_XIndexed_Instruction();

        void _AA_TAX_Implied_Instruction();

        void _A8_TAY_Implied_Instruction();

        void _BA_TSX_Implied_Instruction();

        void _8A_TXA_Implied_Instruction();

        void _9A_TXS_Implied_Instruction();

        void _98_TYA_Implied_Instruction();
        
        
        // ========================STACK=========================
        // | PHA    :   Push Accumulator onto stack
        // | PHP    :   Push Processor Status register onto stack (sets break flag)
        // | PLA    :   Pull Accumulator from stack
        // | PLP    :   Pull Processor Status register from stack
        void _48_PHA_Implied_Instruction();

        void _08_PHP_Implied_Instruction();

        void _68_PLA_Implied_Instruction();

        void _28_PLP_Implied_Instruction();


        // ================DECREMENTS=&=INCREMENTS===============
        // | DEC    :   Decrement values stored in memory
        // | DEX    :   Decrement X register
        // | DEY    :   Decrement Y register
        // | INC    :   Increment values stored in memory
        // | INX    :   Increment X register
        // | INY    :   Increment Y register
        void _C6_DEC_ZeroPage_Instruction();
        void _CE_DEC_Absolute_Instruction();
        void _D6_DEC_ZeroPage_XIndexed_Instruction();
        void _DE_DEC_Absolute_XIndexed_Instruction();

        void _CA_DEX_Implied_Instruction();

        void _88_DEY_Implied_Instruction();

        void _E6_INC_ZeroPage_Instruction();
        void _EE_INC_Absolute_Instruction();
        void _F6_INC_ZeroPage_XIndexed_Instruction();
        void _FE_INC_Absolute_XIndexed_Instruction();

        void _E8_INX_Implied_Instruction();

        void _C8_INY_Implied_Instruction();
        

        // =================ARITHMETIC=OPERATIONS================
        // | https://www.masswerk.at/6502/6502_instruction_set.html#arithmetic
        // | ADC    :   Add with carry (prepare by CLC)
        // | SBC    :   Subtract with carry (prepare by SEC)
        void _61_ADC_XIndexed_Indirect_Instruction();
        void _65_ADC_ZeroPage_Instruction();
        void _69_ADC_Immediate_Instruction();
        void _6D_ADC_Absolute_Instruction();
        void _71_ADC_Indirect_YIndexed_Instruction();
        void _75_ADC_ZeroPage_XIndexed_Instruction();
        void _79_ADC_Absolute_YIndexed_Instruction();
        void _7D_ADC_Absolute_XIndexed_Instruction();

        void _E1_SBC_XIndexed_Indirect_Instruction();
        void _E5_SBC_ZeroPage_Instruction();
        void _E9_SBC_Immediate_Instruction();
        void _ED_SBC_Absolute_Instruction();
        void _F1_SBC_Indirect_YIndexed_Instruction();
        void _F5_SBC_ZeroPage_XIndexed_Instruction();
        void _F9_SBC_Absolute_YIndexed_Instruction();
        void _FD_SBC_Absolute_XIndexed_Instruction();
        
        // ==================LOGICAL=OPERATIONS==================
        // | AND    :   And (with Accumulator)
        // | EOR    :   Exclusive or (with Accumulator)
        // | ORA    :   Or (with Accumulator)
        void _21_AND_XIndexed_Indirect_Instruction();
        void _25_AND_ZeroPage_Instruction();
        void _29_AND_Immediate_Instruction();
        void _2D_AND_Absolute_Instruction();
        void _31_AND_Indirect_YIndexed_Instruction();
        void _35_AND_ZeroPage_XIndexed_Instruction();
        void _39_AND_Absolute_YIndexed_Instruction();
        void _3D_AND_Absolute_XIndexed_Instruction();

        void _41_EOR_XIndexed_Indirect_Instruction();
        void _45_EOR_ZeroPage_Instruction();
        void _49_EOR_Immediate_Instruction();
        void _4D_EOR_Absolute_Instruction();
        void _51_EOR_Indirect_YIndexed_Instruction();
        void _55_EOR_ZeroPage_XIndexed_Instruction();
        void _59_EOR_Absolute_YIndexed_Instruction();
        void _5D_EOR_Absolute_XIndexed_Instruction();

        void _01_ORA_XIndexed_Indirect_Instruction();
        void _05_ORA_ZeroPage_Instruction();
        void _09_ORA_Immediate_Instruction();
        void _0D_ORA_Absolute_Instruction();
        void _11_ORA_Indirect_YIndexed_Instruction();
        void _15_ORA_ZeroPage_XIndexed_Instruction();
        void _19_ORA_Absolute_YIndexed_Instruction();
        void _1D_ORA_Absolute_XIndexed_Instruction();


        // ====================SHIFT=&=ROTATE====================
        // | ASL    :   Arithmetic shift left (shifts in a zero bit on the right)
        // | LSR    :   Logical shift right (shifts in a zero bit from the left)
        // | ROL    :   Rotate left (shifts in a carry bit on the right)
        // | ROR    :   Rotate right (shifts in a zero bit on the left)
        void _06_ASL_ZeroPage_Instruction();
        void _0A_ASL_Accumulator_Instruction();
        void _0E_ASL_Absolute_Instruction();
        void _16_ASL_ZeroPage_XIndexed_Instruction();
        void _1E_ASL_Absolute_XIndexed_Instruction();

        void _46_LSR_ZeroPage_Instruction();
        void _4A_LSR_Accumulator_Instruction();
        void _4E_LSR_Absolute_Instruction();
        void _56_LSR_ZeroPage_XIndexed_Instruction();
        void _5E_LSR_Absolute_XIndexed_Instruction();

        void _26_ROL_ZeroPage_Instruction();
        void _2A_ROL_Accumulator_Instruction();
        void _2E_ROL_Absolute_Instruction();
        void _36_ROL_ZeroPage_XIndexed_Instruction();
        void _3E_ROL_Absolute_XIndexed_Instruction();

        void _66_ROR_ZeroPage_Instruction();
        void _6A_ROR_Accumulator_Instruction();
        void _6E_ROR_Absolute_Instruction();
        void _76_ROR_ZeroPage_XIndexed_Instruction();
        void _7E_ROR_Absolute_XIndexed_Instruction();



        // =========================FLAG==========================
        // | CLC    :   Clear carry flag
        // | CLD    :   Clear decimal flag (Binary Coded Decimal arithmetics disabled)
        // | CLI    :   Clear interrupt disable flag
        // | CLV    :   Clear overflow flag
        // | SEC    :   Set carry flag
        // | SED    :   Set decimal flag (Binary Coded Decimal arithmetics enabled)
        // | SEI    :   Set interrupt disable flag
        void _18_CLC_Implied_Instruction();

        void _D8_CLD_Implied_Instruction();

        void _58_CLI_Implied_Instruction();

        void _B8_CLV_Implied_Instruction();

        void _38_SEC_Implied_Instruction();

        void _F8_SED_Implied_Instruction();

        void _78_SEI_Implied_Instruction();


        // =====================COMPARISONS======================
        // | CMP    :   Compare (with Accumulator)
        // | CPX    :   Compare with X register
        // | CPY    :   Compare with Y register
        void _C1_CMP_XIndexed_Indirect_Instruction();
        void _C5_CMP_ZeroPage_Instruction();
        void _C9_CMP_Immediate_Instruction();
        void _CD_CMP_Absolute_Instruction();
        void _D1_CMP_Indirect_YIndexed_Instruction();
        void _D5_CMP_ZeroPage_XIndexed_Instruction();
        void _D9_CMP_Absolute_YIndexed_Instruction();
        void _DD_CMP_Absolute_XIndexed_Instruction();

        void _E0_CPX_Immediate_Instruction();
        void _E4_CPX_ZeroPage_Instruction();
        void _EC_CPX_Absolute_Instruction();

        void _C0_CPY_Immediate_Instruction();
        void _C4_CPY_ZeroPage_Instruction();
        void _CC_CPY_Absolute_Instruction();
        

        // ==================CONDITIONAL=BRANCH==================
        // | BCC    :   Branch if carry clear
        // | BCS    :   Branch if carry set
        // | BEQ    :   Branch on equal (zero set)
        // | BMI    :   Branch on minus (negative set)
        // | BNE    :   Branch on not equal (zero clear)
        // | BPL    :   Branch on plus (negative clear)
        // | BVC    :   Branch on overflow clear
        // | BVS    :   Branch on overflow set
        void _90_BCC_Relative_Instruction();

        void _B0_BCS_Relative_Instruction();

        void _F0_BEQ_Relative_Instruction();

        void _10_BPL_Relative_Instruction();

        void _30_BMI_Relative_Instruction();

        void _D0_BNE_Relative_Instruction();

        void _50_BVC_Relative_Instruction();

        void _70_BVS_Relative_Instruction();


        // ==================JUMPS=&=SUBROUTINES=================
        // | JMP    :   Jump
        // | JSR    :   Jump to subroutine
        // | RTS    :   Return from subroutine
        void _4C_JMP_Absolute_Instruction();
        void _6C_JMP_Indirect_Instruction();

        void _20_JSR_Absolute_Instruction();

        void _60_RTS_Implied_Instruction();


        // ====================INTERRUPTS========================
        // | BRK    :   Break / software interrupt
        // | RTI    :   Return from interrupt
        void _02_BRK_Implied_Instruction(); // moved from $00 to $02, replacing the illegal opcode JAM
        
        void _40_RTI_Implied_Instruction();

        // =========================OTHER=========================
        // | BIT    :   Bit test (Accumulator & Memory)
        // | NULL   :   No operation
        // | NOP    :   No operation
        // in the 6502, $00 was the BRK implied instruction, but to be sure to capture errors easily
        // (ie when incorrect memory is accessed) for this CPU, $00 will be a null instruction
        void _24_BIT_ZeroPage_Instruction();
        void _2C_BIT_Absolute_Instruction();

        void _00_NULL_Illegal_Instruction();

        void _04_NOP_ZeroPage_Illegal_Instruction();
        void _0C_NOP_Absolute_Illegal_Instruction();
        void _14_NOP_ZeroPage_XIndexed_Illegal_Instruction();
        void _1A_NOP_Implied_Illegal_Instruction();
        void _1C_NOP_Absolute_XIndexed_Illegal_Instruction();
        void _34_NOP_ZeroPage_XIndexed_Illegal_Instruction();
        void _3A_NOP_Implied_Illegal_Instruction();
        void _3C_NOP_Absolute_XIndexed_Illegal_Instruction();
        void _44_NOP_ZeroPage_Illegal_Instruction();
        void _54_NOP_ZeroPage_XIndexed_Illegal_Instruction();
        void _5A_NOP_Implied_Illegal_Instruction();
        void _5C_NOP_Absolute_XIndexed_Illegal_Instruction();
        void _64_NOP_ZeroPage_Illegal_Instruction();
        void _74_NOP_ZeroPage_XIndexed_Illegal_Instruction();
        void _7A_NOP_Implied_Illegal_Instruction();
        void _7C_NOP_Absolute_XIndexed_Illegal_Instruction();
        void _80_NOP_Immediate_Illegal_Instruction();
        void _82_NOP_Immediate_Illegal_Instruction();
        void _89_NOP_Immediate_Illegal_Instruction();
        void _C2_NOP_Immediate_Illegal_Instruction();
        void _D4_NOP_ZeroPage_XIndexed_Illegal_Instruction();
        void _DA_NOP_Implied_Illegal_Instruction();
        void _DC_NOP_Absolute_XIndexed_Illegal_Instruction();
        void _E2_NOP_Immediate_Illegal_Instruction();
        void _EA_NOP_Implied_Instruction();
        void _F4_NOP_ZeroPage_XIndexed_Illegal_Instruction();


        // ========================ILLEGAL========================
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
        void _4B_ALR_Immediate_Illegal_Instruction();

        void _0B_ANC_Immediate_Illegal_Instruction();
        void _2B_ANC_Immediate_Illegal_Instruction(); // ANC 2

        void _8B_ANE_Immediate_Illegal_Instruction();

        void _6B_ARR_Immediate_Illegal_Instruction();

        void _C3_DCP_XIndexed_Indirect_Illegal_Instruction();
        void _C7_DCP_ZeroPage_Illegal_Instruction();
        void _CF_DCP_Absolute_Illegal_Instruction();
        void _D3_DCP_Indirect_YIndexed_Illegal_Instruction();
        void _D7_DCP_ZeroPage_XIndexed_Illegal_Instruction();
        void _DB_DCP_Absolute_YIndexed_Illegal_Instruction();
        void _DF_DCP_Absolute_XIndexed_Illegal_Instruction();

        void _E7_ISC_ZeroPage_Illegal_Instruction();
        void _EF_ISC_Absolute_Illegal_Instruction();
        void _F3_ISC_Indirect_YIndexed_Illegal_Instruction();
        void _F7_ISC_ZeroPage_XIndexed_Illegal_Instruction();
        void _FB_ISC_Absolute_YIndexed_Illegal_Instruction();
        void _FF_ISC_Absolute_XIndexed_Illegal_Instruction();

        void _BB_LAS_Absolute_YIndexed_Illegal_Instruction();

        void _A3_LAX_XIndexed_Indirect_Illegal_Instruction();
        void _A7_LAX_ZeroPage_Illegal_Instruction();
        void _AF_LAX_Absolute_Illegal_Instruction();
        void _B3_LAX_Indirect_YIndexed_Illegal_Instruction();
        void _B7_LAX_ZeroPage_YIndexed_Illegal_Instruction();
        void _BF_LAX_Absolute_YIndexed_Illegal_Instruction();

        void _AB_LXA_Immediate_Illegal_Instruction();

        void _12_JAM_Illegal_Instruction();
        void _22_JAM_Illegal_Instruction();
        void _32_JAM_Illegal_Instruction();
        void _42_JAM_Illegal_Instruction();
        void _52_JAM_Illegal_Instruction();
        void _62_JAM_Illegal_Instruction();
        void _72_JAM_Illegal_Instruction();
        void _92_JAM_Illegal_Instruction();
        void _B2_JAM_Illegal_Instruction();
        void _D2_JAM_Illegal_Instruction();
        void _F2_JAM_Illegal_Instruction();

        void _23_RLA_XIndexed_Indirect_Illegal_Instruction();
        void _27_RLA_ZeroPage_Illegal_Instruction();
        void _2F_RLA_Absolute_Illegal_Instruction();
        void _33_RLA_Indirect_YIndexed_Illegal_Instruction();
        void _37_RLA_ZeroPage_XIndexed_Illegal_Instruction();
        void _3B_RLA_Absolute_YIndexed_Illegal_Instruction();
        void _3F_RLA_Absolute_XIndexed_Illegal_Instruction();

        void _63_RRA_XIndexed_Indirect_Illegal_Instruction();
        void _67_RRA_ZeroPage_Illegal_Instruction();
        void _6F_RRA_Absolute_Illegal_Instruction();
        void _73_RRA_Indirect_YIndexed_Illegal_Instruction();
        void _77_RRA_ZeroPage_XIndexed_Illegal_Instruction();
        void _7B_RRA_Absolute_YIndexed_Illegal_Instruction();
        void _7F_RRA_Absolute_XIndexed_Illegal_Instruction();

        void _83_SAX_XIndexed_Indirect_Illegal_Instruction();
        void _87_SAX_ZeroPage_Illegal_Instruction();
        void _8F_SAX_Absolute_Illegal_Instruction();
        void _97_SAX_ZeroPage_YIndexed_Illegal_Instruction();

        void _CB_SBX_Immediate_Illegal_Instruction();

        void _93_SHA_Indirect_YIndexed_Illegal_Instruction();
        void _9F_SHA_Absolute_YIndexed_Illegal_Instruction();

        void _9E_SHX_Absolute_YIndexed_Illegal_Instruction();

        void _9C_SHY_Absolute_XIndexed_Illegal_Instruction();

        void _03_SLO_XIndexed_Indirect_Illegal_Instruction();
        void _07_SLO_ZeroPage_Illegal_Instruction();
        void _0F_SLO_Absolute_Illegal_Instruction();
        void _13_SLO_Indirect_YIndexed_Illegal_Instruction();
        void _17_SLO_ZeroPage_XIndexed_Illegal_Instruction();
        void _1B_SLO_Absolute_YIndexed_Illegal_Instruction();
        void _1F_SLO_Absolute_XIndexed_Illegal_Instruction();

        void _43_SRE_XIndexed_Indirect_Illegal_Instruction();
        void _47_SRE_ZeroPage_Illegal_Instruction();
        void _4F_SRE_Absolute_Illegal_Instruction();
        void _53_SRE_Indirect_YIndexed_Illegal_Instruction();
        void _57_SRE_ZeroPage_XIndexed_Illegal_Instruction();
        void _5B_SRE_Absolute_YIndexed_Illegal_Instruction();
        void _5F_SRE_Absolute_XIndexed_Illegal_Instruction();

        void _9B_TAS_Absolute_YIndexed_Illegal_Instruction();

        void _EB_USBC_Immediate_Illegal_Instruction();

        

        // =====================ADDRESS=MODES=====================
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
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        
        


        

};

#endif // ALIENCPU6502_H