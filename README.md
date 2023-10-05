# ALIEN CPU - A 16 bit 6502-like Processor
## Details
My 16 bit processor emulator attempts to capture the 6502 feel - including behavior, available instructions, and more. My plan is to design a fully emulated computer using the 6502 (with a simple OS and custom programming language)

Like the 6502, there are only a few registers and a limited amount of instructions. The registers are the Accumulator, X index, Y index, Stack Pointer, Program Counter, and Processor flag. 

Unlike the 6502, the Accumulator, X and Y index, Stack Pointer registers are 16 bits (2 bytes) not 8 bits like in the 6502. The Program Counter likewise has been scaled up by a factor of 2 to 32 bits (4 bytes). As a result, the amount of addressable memory is scaled up from 64 Kb to 4 Gb. The Processor flag register remains the same (as only 7 bits are required for all status flags)

The instructions supported by my processor attempts to emulate the behavior of its 6502 counterpart. Cycle usage for instructions have been scaled up in a (hopefully) logical way that matches the 6502. My processor currently completes each instruction fully each emulator tick, but I am considering switching over to a more accurate cycle emulation, which although might lower the speed of the processor, will allow for stepping through the program through each cycle instead of instruction.
