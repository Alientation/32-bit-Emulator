# To do

Work that is not done yet. The other documents describe what exists, so what is missing from it is listed here.

## C compiler

The compiler is in `core/ccompiler` and is currently disabled in `core/CMakeLists.txt`. It is awaiting a full rewrite.

## Devices

- DMA of the block device is one contiguous buffer per command, with no scatter/gather, and it moves the data when the command completes ([devices.md](devices.md#block-device-0xf0003000)).
- Console input that arrives while the machine runs (host time). It is queued before the run today.

## Debugger

- Watchpoints on physical addresses, the old value of a write, and conditions (stop only if the value is...).
- Showing the exception state in `bt`. A backtrace through a handler stops at the `ERET` frame.
- A gdb remote stub, for use from an IDE.

## Code and build

- Move the disassembler into a component of its own.
- Reduce the compilation time. Parsing the headers was measured at about 16% of the CPU time of a debug build (`-fsyntax-only` of every file against the whole build); the rest is code generation, and the test files and GoogleTest are about 60% of it. `<iostream>`, `<chrono>` and `<filesystem>` are no longer included by the headers that every file includes. That took the parsing of the library and app files from 23.4 s to 19.9 s of CPU time (15%) and of everything from 65.7 s to 60.1 s, so the rest of the time is code generation. What is left: `<format>` (logger.h, 68 of 75 files), `cli.cpp` (the longest file, cxxopts), and the test files (`-ftime-report` to see what in them is slow).
