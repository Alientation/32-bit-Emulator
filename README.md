# ALIEN CPU - A 16 bit 6502-like Processor
## Inspiration and Purpose
In the summer of 2023 I came across this [fabulous series](https://www.youtube.com/watch?v=qJgsuQoy9bc&list=PLLwK93hM93Z13TRzPx9JqTIn33feefl37) by Dave Poo on emulating a 6502 cpu which really reminded me of this [video](https://www.youtube.com/watch?v=QZwneRb-zqA) by Sebastian Lague. Long had I seen others create computers in games like Minecraft and I once had the same ambition (which never came to fruition because at just 12 years old, I just don't think I had the necessary expertise and knowledge to pull that off). I figured this project would help knock a few of my goals: learn c++, understand how computers work at a lower level, and just simply explore more. I have so much more in vision for this project than just a simple emulator (build an assembler, use the assembler to make an OS, develop a compiler for a high level language, ...)

## Details
My 16 bit processor emulator attempts to capture the 6502 feel - including behavior, available instructions, and more. My plan is to design a fully emulated computer using the 6502 (with a simple OS and custom programming language)

Like the 6502, there are only a few registers and a limited amount of instructions. The registers are the Accumulator, X index, Y index, Stack Pointer, Program Counter, and Processor flag. 

Unlike the 6502, the Accumulator, X and Y index, and Stack Pointer registers are 16 bits (2 bytes) not 8 bits like in the 6502. The Program Counter likewise has been scaled up by a factor of 2 to 32 bits (4 bytes). As a result, the amount of addressable memory is scaled up from 64 Kb to 4 Gb. The Processor flag register remains the same (as only 7 bits are required for all status flags)

The instructions supported by my processor attempts to emulate the behavior of its 6502 counterpart. Cycle usage for instructions have been scaled up in a (hopefully) logical way that matches the 6502. My processor currently completes each instruction fully each emulator tick, but I am considering switching over to a more accurate cycle emulation, which although might lower the speed of the processor, will allow for stepping through the program through each cycle instead of instruction.

## Progress
All legal instructions have been implemented with around half having been thoroughly tested.
Currently working on an assembler, aiming for a wide support of various directives and linking between assembly files. (Assembler is already on its 3rd iteration, though it is looking far more promising than the previous versions). So far have finished developing and testing some preprocessors like #DEFINE and #MACRO.
Planning on working bit by bit on improving test coverage in between development.

## How to build
Run the build.bat or build.sh script located in the app subdirectory which will configure and build the project

