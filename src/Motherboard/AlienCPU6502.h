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
            INS_NULL = 0x00, // Null Instruction
            INS_ORA_X_IND = 0x01, // OR Accumulator, indexed by X register, indirect addressing
            INS_BRK_IMPL = 0x02, // Break/Interrupt, implied addressing
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
        // |
        // ========================STACK=========================
        // | PHA    :   Push Accumulator onto stack
        // | PHP    :   Push Processor Status register onto stack (sets break flag)
        // | PLA    :   Pull Accumulator from stack
        // | PLP    :   Pull Processor Status register from stack
        // |
        // ================DECREMENTS=&=INCREMENTS===============
        // | DEC    :   Decrement values stored in memory
        // | DEX    :   Decrement X register
        // | DEY    :   Decrement Y register
        // | INC    :   Increment values stored in memory
        // | INX    :   Increment X register
        // | INY    :   Increment Y register
        // |
        // =================ARITHMETIC=OPERATIONS================
        // | https://www.masswerk.at/6502/6502_instruction_set.html#arithmetic
        // | ADC    :   Add with carry (prepare by CLC)
        // | SBC    :   Subtract with carry (prepare by SEC)
        // |
        // ==================LOGICAL=OPERATIONS==================
        // | AND    :   And (with Accumulator)
        // | EOR    :   Exclusive or (with Accumulator)
        // | ORA    :   Or (with Accumulator)
        // |
        // ====================SHIFT=&=ROTATE====================
        // | ASL    :   Arithmetic shift left (shifts in a zero bit on the right)
        // | LSR    :   Logical shift right (shifts in a zero bit from the left)
        // | ROL    :   Rotate left (shifts in a carry bit on the right)
        // | ROR    :   Rotate right (shifts in a zero bit on the left)
        // | 
        // =========================FLAG==========================
        // | CLC    :   Clear carry flag
        // | CLD    :   Clear decimal flag (Binary Coded Decimal arithmetics disabled)
        // | CLI    :   Clear interrupt disable flag
        // | CLV    :   Clear overflow flag
        // | SEC    :   Set carry flag
        // | SED    :   Set decimal flag (Binary Coded Decimal arithmetics enabled)
        // | SEI    :   Set interrupt disable flag
        // |
        // =====================COMPARISONS======================
        // | CMP    :   Compare (with Accumulator)
        // | CPX    :   Compare with X register
        // | CPY    :   Compare with Y register
        // | 
        // ==================CONDITIONAL=BRANCH==================
        // | BCC    :   Branch if carry clear
        // | BCS    :   Branch if carry set
        // | BEQ    :   Branch on equal (zero set)
        // | BMI    :   Branch on minus (negative set)
        // | BNE    :   Branch on not equal (zero clear)
        // | BPL    :   Branch on plus (negative clear)
        // | BVC    :   Branch on overflow clear
        // | BVS    :   Branch on overflow set
        // |
        // ==================JUMPS=&=SUBROUTINES=================
        // | JMP    :   Jump
        // | JSR    :   Jump to subroutine
        // | RTS    :   Return from subroutine
        // | 
        // ====================INTERRUPTS========================
        // | BRK    :   Break / software interrupt
        // | RTI    :   Return from interrupt
        // |
        // =========================OTHER=========================
        // | BIT    :   Bit test (Accumulator & Memory)
        // | NOP    :   No operation
        // | 
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
        //
        //
        // in the 6502, $00 was the BRK implied instruction, but to be sure to capture errors easily
        // (ie when incorrect memory is accessed) for this CPU, $00 will be a null instruction
        void _00_Null_Instruction();
        void _01_ORAccumulator_XIndexed_Indirect_Instruction();
        void _02_BRK_Implied_Instruction(); // moved from $00 to $02


        void _A9_LoadAccumulator_Immediate_Instruction();

};

#endif // ALIENCPU6502_H