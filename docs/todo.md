# To do

Work that is not done yet. The other documents describe what exists, so what is missing from it is listed here.

## C compiler

The compiler (`core/ccompiler`, [ccompiler.md](ccompiler.md)) has a complete lexer and preprocessor, and a first slice of the parser and code generator that compiles `int` expressions. What is left is its [plan](ccompiler.md#plan): core C, aggregates, the freestanding headers and a libc (with semihosting file calls in the emulator), `long long` and floating point (which the bootstrap needs, since the compiler's source uses them), the bootstrap, and optimization.

## Code and build

- Reduce the compilation time further. Parsing the headers is about 16% of the CPU time of a debug build (`-fsyntax-only` of every file against the whole build); the rest is code generation, and the test files and GoogleTest are about 60% of it. `<iostream>`, `<chrono>`, `<filesystem>` and (through `alu.h`) `<format>` are no longer included by the headers that every file includes, which took the parsing of all files from 65.7 s to 54.9 s of CPU time; `<format>` is now only in the 27 library files and 10 test files that log. What is left is the test files (`assembler_test.cpp` and the other large ones are mostly GoogleTest macros).
