#ifndef ALIENCPUV2_H
#define ALIENCPUV2_H

#include "Types.h"

class AlienCPUV2;

/**
 * A cycle ticked version of AlienCPU. To be completed.
 * 
 * Each cycle, check if instruction is complete by checking the instruction
 * cycle mapping table using the IR (current instruction) register. If the
 * instruction is complete, read the next instruction, else, continue executing
 * the current instruction but incrmenet the tick count
 * 
 * Each instruction method call will be fed a tick count which represents the number
 * of cycles spent on the current instruction. The instruction will based on that decide what to do.
 * 
 * Some resources
 * https://www.pagetable.com/?p=410
 * https://c74project.com/microcode/
 * https://c74project.files.wordpress.com/2020/04/c74-6502-microcode.pdf
 * 
 */
class AlienCPUV2 {
    public:
        static const std::string VERSION;
        static const u64 INTERRUPT_CHECK_INTERVAL = 16;
        static const u16 INSTRUCTION_COUNT = 256;

        static const Word NULL_ADDRESS = 0x00000000;
        static const Word INTERRUPT_HANDLER_VECTOR = 0x000FFFF0;
        static const Word POWER_ON_RESET_VECTOR = 0x000FFFF4;
        static const Word BRK_HANDLER_VECTOR = 0x000FFFF;

        enum StatusFlag : Byte {
            C = 0, // Carry
            Z = 1, // Zero
            I = 2, // Interrupt Disable
            D = 3, // Decimal Mode
            B = 4, // Break Command
            UNUSED = 5, // Unused
            V = 6, // Overflow
            N = 7 // Negative
        };


    private:
        bool debugMode; // whether to print out debug info
        u64 cycles; // number of cycles executed

        Byte IR; // Instruction Register, the currently executing instruction
        Word PC; // Program Counter Register, memory address to the next instruction
        Word A; // Accumulator Register
        Word X,Y; // Index Registers 
        Word SP; // Stack Pointer Register (points to the top of the stack)
        Byte P; // Program Status Register (holds the flags)


    public:
        AlienCPUV2(bool debugMode = false);
        void Step(u64 cycles = 1);
        void reset();

    private:
        void clearFlag(StatusFlag bit);
        void setFlag(StatusFlag bit, bool isSet);
        bool getFlag(StatusFlag bit);

};

#endif // ALIENCPUV2_H