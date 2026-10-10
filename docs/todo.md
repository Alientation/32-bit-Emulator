# To do

Work that is not done yet. The other documents describe what exists, so what is missing from it is listed here.

## C compiler

The compiler is in `core/ccompiler` and is currently disabled in `core/CMakeLists.txt`. It is awaiting a full rewrite.

## Code and build

- Reduce the compilation time further. Parsing the headers is about 16% of the CPU time of a debug build (`-fsyntax-only` of every file against the whole build); the rest is code generation, and the test files and GoogleTest are about 60% of it. `<iostream>`, `<chrono>`, `<filesystem>` and (through `alu.h`) `<format>` are no longer included by the headers that every file includes, which took the parsing of all files from 65.7 s to 54.9 s of CPU time; `<format>` is now only in the 27 library files and 10 test files that log. What is left: `cli.cpp` (5 s in a debug build, 60% of it code generation for the templates of cxxopts: a hand written parser like the one of `basm` would take that away), and the test files (`assembler_test.cpp` and the other large ones are mostly GoogleTest macros).
