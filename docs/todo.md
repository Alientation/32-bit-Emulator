# To do

Work that is not done yet. The other documents describe what exists, so what is missing from it is listed here.

## C compiler

The compiler is in `core/ccompiler` and is currently disabled in `core/CMakeLists.txt`. It is awaiting a full rewrite.

## Instruction set

- `fmadd`, the fused multiply-add (and the negated forms `fmsub`, `fnmadd`, `fnmsub`) for `fmaf`/`fma`. It has four register operands, so it is an opcode of its own with a function field, and it needs a single rounding, which the host's `std::fma` gives. See [isa.md](isa.md#floating-point-3).

## Devices

- DMA of the block device is one contiguous buffer per command, with no scatter/gather, and it moves the data when the command completes ([devices.md](devices.md#block-device-0xf0003000)).
- Console input that arrives while the machine runs (host time). It is queued before the run today.

## Debugger

- Watchpoints on physical addresses, the old value of a write, and conditions (stop only if the value is...).
- Showing the exception state in `bt`. A backtrace through a handler stops at the `ERET` frame.
- A gdb remote stub, for use from an IDE.

## Code and build

- Move the disassembler into a component of its own.
- Reduce the compilation time. Parsing the headers was measured at about 16% of the CPU time of a debug build (`-fsyntax-only` of every file against the whole build); the rest is code generation, and the test files and GoogleTest are about 60% of it. `<iostream>`, `<chrono>` and `<filesystem>` are no longer included by the headers that every file includes. What is left: `<format>` (logger.h, 68 of 75 files), `cli.cpp` (the longest file, cxxopts), and the test files (`-ftime-report` to see what in them is slow).

## Ideas that are not planned

These were the notes at the top of `core/app/src/main.cpp`.

- A multi-core emulator (memory, disk and the devices would have to be shared and synchronized).
- Shared objects and dynamic linking.
- A visualizer of the state of the processor.
- A basm extension for an editor (highlighting, completion).
- Operating system pieces: a bootloader that loads a kernel from the disk, zeroed first pages to catch null pointers, how the stack, the heap and context switching work. See [devices.md](devices.md) and [abi.md](abi.md).
