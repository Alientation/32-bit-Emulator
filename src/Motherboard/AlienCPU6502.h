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

// 32 bit CPU
//  - 1 Mb of memory (addressable via 32 bit address bus)
//      - todo memory locations
// 
//
//
// 6502 
//  - 8 bit CPU
//  - 64 Kb of memory (addressable via 16 bit address bus)
//      - 0x0000 to 0x00FF (Zero page, first 256 bytes)
//          - some special addressing modes allow shorter instructions to default access to zero page memory
//      - 0x0100 to 0x01FF (reserved as stack memory)
//      - 0x0200 to 0xFFF9 (general memory)
//      - 0xFFFA to 0xFFFF (special reserved memory)
//          * 0xFFFA/B - interrupt handler
//          * 0xFFFC/D - power on reset location
//          * 0xFFFE/F - BRK / Interrupt Request (IRQ) handler  
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
        static const u16 INSTRUCTION_COUNT = 0x0100;
        
        // Max Instruction Bytes Length
        static const u8 MAX_INSTRUCTION_BYTES_LENGTH = 0x02;
    
        // Instruction opcodes (1 byte)
        // https://en.wikipedia.org/wiki/X86_instruction_listings#Added_as_instruction_set_extensions
        static constexpr u8
            INS_NULL = 0x00; // Null Instruction

        // Instruction opcodes (2 bytes)
        static constexpr u16
            INS_LDA_IM = 0x00A9; // Load Accumulator Immediate



        // Null address
        static const Word NULL_ADDRESS = 0x00000000;


        // Program Stack
        // SP_INIT - STACK_SIZE = End of stack
        static const Word STACK_SIZE = 0x00010000; // 65536 Bytes of STACK MEMORY


        // should never exceed 0x000FFFFF
        static constexpr Word
            PC_INIT = 0x00000000,
            SP_INIT = 0x00010100, // 65536 Bytes of STACK MEMORY

            A_INIT = 0x00000000,
            X_INIT = 0x00000000,
            Y_INIT = 0x00000000;



        static const Byte P_INIT = 0b00100000;

    //private:
        
        // Instruction Set
        using Instruction = std::function<void(AlienCPU6502&)>;
        Instruction instructions[INSTRUCTION_COUNT];

        // System Memory
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
        //    the stack pointer is decremented by 4
        //      - [SP+0] = byte 3
        //      - [SP+1] = byte 2      (little endian)
        //      - [SP+2] = byte 1
        //      - [SP+3] = byte 0
        // https://en.wikipedia.org/wiki/Stack_register
        // https://en.wikipedia.org/wiki/Call_stack
        Word sp;

        // ==============General Purpose Registers==============
        // https://codebase64.org/doku.php?id=base:6502_registers
        Word a; // accumulator, main register for arithmetic and logic operations (Direct connection to ALU)
        Word x; // index register X, addressing data with indices (like arrays)
        Word y; // index register Y, addressing data with indices (like arrays)


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

        // Instructions
        void _0000NullInstruction();
        void _00A9_LoadAccumulator_Immediate();

};

#endif // ALIENCPU6502_H