#ifndef ALIENCPUX86_H
#define ALIENCPUX86_H

#include <Types.h>
#include <../src/Motherboard/Memory/RAM.h>
#include <string>
#include <functional>

class AlienCPU;

// 32 bit CPU based off of the x86 architecture (TODO: for now, it is simply a copy of AlienCPU6502)
class AlienCPU {
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
            EIP_INIT = 0x00000000;

    //private:
        
        // Instruction Set
        using Instruction = std::function<void(AlienCPU&)>;
        Instruction instructions[INSTRUCTION_COUNT];

        // System Memory
        RAM ram;

        // Number of cycles till the next Interrupt should be processed
        Word nextInterruptCheck;

        u64 cycles;

        // x86 Architecture Registers
        //https://learn.microsoft.com/en-us/windows-hardware/drivers/debugger/x86-architecture
        Word eip; // Instruction pointer, used to point to the next instruction to be executed

        Word eax; // Accumulator, used in arithmetic operations
        Word ebx; // Base register, used as pointer to data 
        Word ecx; // Counter register, used in shift/rotate instructions and loops
        Word edx; // Data register (can be used for I/O port access and arithmetic functions)
        Word esi; // Source index register, used as a pointer to a source in stream operations
        Word edi; // Destination index register, used as a pointer to the destination in stream operations
        Word ebp; // Stack Base pointer register, used to point to the base of the stack
        Word esp; // Stack pointer, used to point to the top of the stack

        // Flags register, used to store the status of the processor and the results of operations
        // https://en.wikibooks.org/wiki/X86_Assembly/X86_Architecture
        // each bit of the 32 bits in the register stores a flag
        // 00 = CF (carry flag) 
        Word eflags;

    public:
        AlienCPU();
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










#endif // ALIENCPUX86_H