#ifndef ALIENCPU_H
#define ALIENCPU_H

#include <Types.h>
#include <../src/Motherboard/Memory/RAM.h>
#include <string>
#include <functional>

class AlienCPU; //forward declaration (!!)

// CPU Emulator (not really emulating any specific CPU)
// https://en.wikipedia.org/wiki/Central_processing_unit
//
//
// ---- Interrupt Process ----
// Used to handle events such as receiving data from the internet
// Or user inputs from keyboard or mouse
// Generally also used to cycle between different threads of execution
//  - Context Switch (storing the state of a thread/process)
//      - Saves all registers the process is using (Process Control Block)
//      - Pointer to the saved PCB located in kernel memory or call stack is
//        added to the queue of processes to be ran (ready queue - priority queue)
//
//  - Interrupt Request
//      - Hardware (sent from device to processor)
//      - Processor (sent from another processor)
//      - Software (sent from program to processor)
//  - Processor halts thread execution
//  - Processor saves thread state
//  - Processor executes interrupt handler
//  - Processor resumes thread execution
//
//
// Plan
// - CPU loads up the BIOS and begins the boot process
// - BIOS 
//      - starts up
//      - initiializes and tests hardware components (POST)
//      - loads boot loader program from a storage device (think this is the OS)
//      - Initiializes a kernel
//      - provides BIOS interrupt calls for keyboard, display, storage, other input/output devices
//          - Used to be stored on ROM chip (Motherboard) but now is stored in flash memory
//
// - OS
//      - todo
//
// - CPU
//      - Managed by the OS
//      - Executes instructions in a cyclic manner
//
//
// - Call Stack - used to store variables and save the state of registers
//  - Stored vertically, grows downward in memory
//  - Split into stack frames (function calls)
//
// - Stack Frame
//    [parameters passed to function]
//    [return address] (to the line of code that called the function)
//    [previous value of base pointer] (to restore the base pointer when the function returns)
//    [local variables]
//
// - Stack Frame Layout
//    (high memory address)
//    +-------------+
//    | Parameter 2 |
//    +-------------+ <-- [bp + 12]
//    | Parameter 1 |
//    +-------------+ <-- [bp + 8]
//    | Return Addr |     (memory address of the instruction to return to)
//    +-------------+    
//    | previous bp |
//    +-------------+ <-- base pointer (first byte of the base of the stack frame)
//    | saved eax   | 
//    +-------------+    
//    | saved ebx   |     (saved register values)
//    +-------------+    
//    | saved ecx   |  
//    +-------------+    
//    | saved edx   |
//    +-------------+
//    | local var   |
//    +-------------+ <-- stack pointer (first byte of the top element)
//    (low memory address)
//

// 32 bit CPU
class AlienCPU {
    public:
        static const std::string VERSION;

        // Number of cycles between each Interrupt check
        static const Word INTERRUPT_CHECK_INTERVAL = 0x10;

        // Total Number of instructions supported by the processor
        static const u16 INSTRUCTION_COUNT = 0x0100;
        
        // Max Instruction Bytes Length
        static const u8 MAX_INSTRUCTION_BYTES_LENGTH = 0x02;

        // should never exceed 0x000FFFFF
        static constexpr Word
            PC_INIT = 0x00000000,
            SP_INIT = 0x00010100, // 65536 Bytes of STACK MEMORY
            BP_INIT = SP_INIT,

            SS_INIT = 0x00000100,

            A_INIT = 0x00000000,
            X_INIT = 0x00000000,
            Y_INIT = 0x00000000,

            FLAGS_Z_INIT = 0x00000000,
            FLAGS_C_INIT = 0x00000000,
            FLAGS_H_INIT = 0x00000000,
            FLAGS_P_INIT = 0x00000000,
            FLAGS_I_INIT = 0x00000000,
            FLAGS_N_INIT = 0x00000000,
            FLAGS_V_INIT = 0x00000000,
            FLAGS_S_INIT = 0x00000000;

        // Instruction opcodes
        static constexpr u16 
            INS_LDA_IM = 0x00A9; // Load Accumulator Immediate

    //private:
        
        // Instruction Set
        using Instruction = std::function<void(AlienCPU&)>;
        Instruction instructions[INSTRUCTION_COUNT];

        // System Memory
        RAM RAM;

        // Number of cycles till the next Interrupt should be processed
        Word NextInterruptCheck;

        u64 Cycles;

        // Program Counter Register
        //  - memory address of the next instruction to be executed
        Word PC;

        // Stack Pointer Register
        //  - points to the first byte of the top element of the call stack
        //  - every stack element is 4 bytes big so to add elements to the stack, 
        //    the stack pointer is decremented by 4
        //      - [SP+0] = byte 1
        //      - [SP+1] = byte 2       (high endian)
        //      - [SP+2] = byte 3
        //      - [SP+3] = byte 4
        // https://en.wikipedia.org/wiki/Stack_register
        // https://en.wikipedia.org/wiki/Call_stack
        Word SP;

        // Base Pointer Register (intially the SP value)
        //  - points to the start of the stack frame
        Word BP;

        // Stack Segment Register --TODO MIGHT NOT NEED THIS--
        //  - points to the end of the stack
        //  - [SS----------SP] Stack grows to the left (down)
        Word SS;

        // potentially more segment registers

        // Other General Purpose Registers to hold data
        Word A;
        Word X;
        Word Y;


        // FLAGS TODO not sure if it should be 4 bytes

        // ZERO FLAG (set if result of last operation was zero)
        // https://en.wikipedia.org/wiki/Zero_flag
        Word Z;
        
        // CARRY FLAG (set if last operation resulted in overflow or underflow)
        // overflow: the last bit (bit 31) overflowed
        // underflow: the first bit (bit 0) underflowed
        // https://en.wikipedia.org/wiki/Carry_flag
        Word C;

        // HALF CARRY flag
        // https://en.wikipedia.org/wiki/Half-carry_flag
        Word H;

        // PARITY flag
        // https://en.wikipedia.org/wiki/Parity_flag
        Word P;

        // INTERRUPT flag
        // https://en.wikipedia.org/wiki/Interrupt_flag
        Word I;

        // NEGATIVE flag
        // https://en.wikipedia.org/wiki/Negative_flag
        Word N;

        // OVERFLOW flag
        // https://en.wikipedia.org/wiki/Overflow_flag
        Word V;
        
        // SUPERVISOR falg
        Word S;


        
    public:
        AlienCPU();
        void Start(u64 maxCycles = 0);
    
    private: 
        void Reset();

        Byte FetchNextByte();
        Byte ReadByte(Word Address);

        void ExecuteInstruction(u16 instruction);
        bool ValidInstruction(u16 instruction);

        // Instructions
        void _0000NullInstruction();
        void _00A9_LoadAccumulator_Immediate();
        

};

#endif // ALIENCPU_H