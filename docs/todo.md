# To do

Work that is not done yet. The other documents describe what exists, so what is missing from it is listed here.

## C compiler

The compiler is in `core/ccompiler` and is currently disabled in `core/CMakeLists.txt`. It is awaiting a full rewrite.

## Devices

The devices are in [devices.md](devices.md). Two that were left for later, because each changes more than a device does:

- **A network card.** Packets that arrive from the host come whenever they like, so a run that uses it is no longer the same every time (the console and the window's input are like that too, but a program that does not ask for them is not touched). It would be a device with a ring of receive and a ring of send descriptors in physical RAM (like the block device's scatter/gather list) and an interrupt line, and in the host a TAP device or UDP to a port, on a thread like the console's live input. An operating system needs a network stack before it pays off.
- **More than one processor, and interrupts between them.** `Emulator32bit::run` is one processor: more would each need their registers, their exception state and their own MMU view, a way to run them in turn (by instructions, to stay the same every time) or on host threads, atomic instructions that are atomic between them, a TLB shootdown, and an interrupt controller that sends an interrupt to one of them (an interrupt between processors, for the scheduler). It is a redesign of the machine rather than a device.

## Code and build

- Reduce the compilation time further. Parsing the headers is about 16% of the CPU time of a debug build (`-fsyntax-only` of every file against the whole build); the rest is code generation, and the test files and GoogleTest are about 60% of it. `<iostream>`, `<chrono>`, `<filesystem>` and (through `alu.h`) `<format>` are no longer included by the headers that every file includes, which took the parsing of all files from 65.7 s to 54.9 s of CPU time; `<format>` is now only in the 27 library files and 10 test files that log. What is left is the test files (`assembler_test.cpp` and the other large ones are mostly GoogleTest macros).
