# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Overview
An ARM-like 32-bit CPU emulator with its own toolchain: a preprocessor, assembler, and linker for **basm** assembly, plus an executable loader. All code lives under `core/`. A C compiler (`core/ccompiler`, written in C11) is in progress but is currently **commented out** of `core/CMakeLists.txt`.

File types: `.basm` source, `.binc` header, `.bi` preprocessed file, `.bo` relocatable object (the custom ELF-like "belf" format), `.ba` static library, `.bexe` executable, `.ld` linker script.

## Build & test
Run everything from `core/`. You need CMake ≥ 3.15, Ninja, and a C++20 compiler. You also need the cxxopts submodule: `git submodule update --init --recursive`. GoogleTest is fetched by CMake.

```bash
./build.sh            # configure + build debug (build/debug) and release (build/release, RelWithDebInfo), then run ctest on both
./build.sh compile    # build only
./build.sh test       # run tests only (must have built)
./build.sh clean      # rm -rf build, then full build + test
./build.sh coverage   # lcov/genhtml report from build/debug gcov data -> core/coverage/
```

Run a single test or suite:
```bash
ctest --test-dir build/debug -R 'EmulatorFixture.add_'   # regex on discovered test name (Suite.Test)
ctest --test-dir build/debug -L emulator32bit        # by label: emulator32bit | assembler | assembler_integration
build/debug/emulator32bit/tests/emulator32bit_tests --gtest_filter='EmulatorFixture.add_*'
```

Compile flags are not set per target. Every target calls `aemu_target_defaults(<tgt>)` from `core/cmake/AemuHelpers.cmake`, which adds the warnings, `-O3` for Release/RelWithDebInfo (compile and link), and `--coverage` for Debug. LTO comes from CMake's IPO support. CMake options: `AEMU_ENABLE_COVERAGE` (ON), `AEMU_WARNINGS_AS_ERRORS` (OFF), `AEMU_ENABLE_LTO` (ON), `BUILD_TESTS` (ON; when OFF, GoogleTest isn't fetched). In-tree code links the libraries through the `aemu::util`, `aemu::emulator32bit` and `aemu::assembler` aliases.

**New test files must be added by hand** to the `SOURCES` list of the `aemu_add_gtest(...)` call in that module's `tests/CMakeLists.txt`. Nothing globs them. Integration tests use the same helper in `core/integration_tests/CMakeLists.txt`.

`version.h` (`AEMU_VERSION`, from `git describe`) is generated on every build into `build/<config>/app/generated/` and is visible only to `emulator_app`.

## Executables (under build/<config>/...)
- `emulator32bit/emu32`: runs the emulator from the command line (cxxopts). Options set RAM/ROM/disk page layout, a ROM/disk file, the initial PC, NZCV flags, register values, and an instruction limit. Run `--help` to see them.
- `assembler/basm`: the toolchain driver (preprocess → assemble → link). Its argument parser is hand-rolled (`assembler/src/build.cpp`, `Process`). Options that take a value need a space before the value, e.g. `-I ./programs/include -o ./programs/build/palindrome ./programs/src/palindrome.basm -outdir ./programs/build`. Useful flags: `-I`, `-l <lib.ba>`, `-o`, `-makedir` (produce a `.ba` library).
- `app/emulator_app`: builds the program and runs it on the emulator. Sample programs are in `core/app/programs/`.

Integration tests shell out to `emu32` and `basm`. They receive the binary paths as the `EMULATOR_PATH` / `ASSEMBLER_PATH` compile definitions.

## Architecture
Static libraries with this dependency chain: `util` ← `emulator32bit` ← `assembler` ← `app`.

- **util**: logging (`logger.h`), `File`/`Directory` helpers, string utilities, and the fixed-width types (`byte`, `hword`, `word`, `U8`, ...).
- **emulator32bit**: the CPU itself is `Emulator32bit` (`emulator32bit.h`).
  - Instructions are fixed-width 4 bytes with a 6-bit opcode, so there are at most 64 instructions. The `_INSTR(name, opcode)` macro list in `emulator32bit.h` is the single source of opcodes.
  - `fill_out_instructions()` builds the `m_instruction_handler[opcode]` dispatch table. It defaults every slot to `_hlt`.
  - Execution handlers live in `src/instructions.cpp`. Each instruction also has a static `asm_<name>()` encoder, which the assembler calls, and a matching disassembler entry in `src/disassembler.cpp`.
  - Memory: `SystemBus` routes accesses to `RAM`/`ROM` (`memory.h`) and to `Disk` (a page cache over a file, `disk.h`). All of them share the `BaseMemory` page-addressed interface, so the disk is memory-mapped too.
  - `VirtualMemory`/MMU, `Timer`, and software interrupts (`software_interrupt.cpp`) are separate files. `src/kernel/` holds the C++-simulated kernel pieces: process, malloc, free block list.
- **assembler**: the pipeline is `Preprocessor` (`#include`, `#define`, macros, conditionals) → `Tokenizer` → `Assembler` (directives in `directives.cpp`, instructions in `instructions.cpp`) → `ObjectFile` (.bo) → `Linker` (symbol resolution and relocation, with `default_linker.ld` as the default script) → `.bexe`, which `load_executable` loads into emulator memory. `StaticLibrary` handles `.ba` archives. `build.cpp` orchestrates all of it.

**Adding or changing an instruction touches several places:** the `_INSTR` opcode list and the `asm_*` encoder (emulator32bit.h/instructions.cpp), the disassembler, the assembler tokenizer keyword map (`assembler/src/tokenizer.cpp`), the assembler handler (`assembler/src/instructions.cpp`), and a gtest in `emulator32bit/tests/instruction_tests/` that is registered in its CMakeLists.

## Style
`.clang-format` (repo root) is LLVM-based: 4-space indent, `SpaceBeforeParens: Always` (so `foo (x)`), right-aligned pointers (`int *p`). Filenames are all lowercase.
