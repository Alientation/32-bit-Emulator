# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository. The details of how the code is put together are in `docs/internals.md`; read the section you are changing before changing it.

## Overview
An ARM-like 32-bit CPU emulator with its own toolchain: a preprocessor, assembler, and linker for **basm** assembly, plus an executable loader. All code lives under `core/`. A C compiler (`core/ccompiler`, written in C11) is in progress but is currently **commented out** of `core/CMakeLists.txt`.

File types: `.basm` source, `.binc` header, `.bi` preprocessed file, `.bo` relocatable object (the custom ELF-like "belf" format), `.ba` static library, `.bexe` executable, `.ld` linker script.

Dependency chain of the static libraries: `util` ← `disassembler` ← `emulator32bit` ← `assembler` ← `app`. Executables (under `build/<config>/`): `emulator32bit/emu32` (runs a `.bexe`), `assembler/basm` (preprocess → assemble → link), `app/emulator_app` (builds and runs).

## Build & test
Run everything from `core/`. You need CMake ≥ 3.15, Ninja, and a C++20 compiler. You also need the cxxopts submodule: `git submodule update --init --recursive`. GoogleTest is fetched by CMake.

```bash
./build.sh            # configure + build debug (build/debug) and release (build/release, RelWithDebInfo), then run ctest on both
./build.sh compile    # build only
./build.sh test       # run tests only (must have built)
./build.sh clean      # rm -rf build, then full build + test
./build.sh coverage   # lcov/genhtml report from build/debug gcov data -> core/coverage/
./build.sh asan       # separate build in build/asan with AddressSanitizer + LeakSanitizer + UBSan, then ctest on it
./build.sh ubsan      # the same with UBSan alone in build/ubsan (much faster)
tools/bench.sh        # times the emulator (MIPS); --baseline <emu32 of another commit> compares two builds
tools/bench_toolchain.sh  # times basm and the loader on generated programs, fails on a quadratic step
tools/mutate.sh FILE LINE # removes a line, runs the tests: do they notice?
```

Run a single test or suite:
```bash
ctest --test-dir build/debug -R 'ShiftTest.'         # regex on discovered test name (Suite.Test)
ctest --test-dir build/debug -L emulator32bit        # by label: emulator32bit | assembler | assembler_integration
build/debug/emulator32bit/tests/emulator32bit_tests --gtest_filter='DataProcessingTest.golden_*'
```

The debug build logs a lot: pipe through `grep -v DBG`. CI (`.github/workflows/ci.yml`) runs `./build.sh` and `./build.sh asan`. **New test files must be added by hand** to the `SOURCES` of the `aemu_add_gtest(...)` call in that module's `tests/CMakeLists.txt` (and `core/integration_tests/CMakeLists.txt`); nothing globs them. Compile flags and the CMake options are in [internals.md](docs/internals.md#build-system).

## Working rules
- Done means the full `./build.sh` (debug and release) passes. Report the final pass/fail line, don't filter it away.
- Before writing a test, read a neighbouring test file and use only its fixture helpers and patterns; never invent helper names.
- Test labels must not be mnemonics or keywords (`b`, `bl`, `cmp`): use `L_loop`. Count instructions from assembler output, not by estimate.
- A test written together with the code is unverified: check that it fails when the line it guards is removed (`tools/mutate.sh`).
- For a convention-wide change (e.g. the carry flag), list the affected cases first and touch only those.
- Quote globs in shell commands (zsh).
- Docs: a change to the ISA, a directive, the `.bo` format or the emulator registers updates the docs in the same change; check each claim against the code.
- Benchmark before and after a change to the hot path (`run()`, `MemoryPort`, `SystemBus`, `VirtualMemory`) with `tools/bench.sh --baseline` and `-n 15`; decide on the best and the median together ([internals.md](docs/internals.md#measuring-a-change)). After a change to the assembler, linker or object file code, run `tools/bench_toolchain.sh`.
- Commits carry no `Co-Authored-By` trailer.

## Invariants (what breaks silently)
Each of these has a test, but only for the way it has broken so far. Details: [internals.md](docs/internals.md#architecture).
- **VirtualMemory:** whatever changes whether a fetch is allowed or where the page is calls `drop_fetch_cache ()`; whatever changes the current process, `m_enabled` or `m_walk` calls `update_swap_key ()`.
- **`Emulator32bit::run ()`:** a new per instruction debugging feature has to be part of the `hooked` local, and is set between runs. The instruction being executed is kept in `m_instr_in_flight` and not in a local (register allocation, see the benchmark notes). A handler that writes the pc itself (`ERET`, `enter_exception`) sets `m_pc_written`; the branches use the `- 4` form.
- **Exceptions:** a `throw Exception (BAD_INSTR/BAD_REG, ...)` passes a `kUndefinedIss_*`. An argument error of an emulator call (`swi 1`) is `PROGRAM_ERROR`, a pc or an access that is not aligned is `MISALIGNED`. Privileged instructions call `require_kernel ()`. `HALT_INSTR`, a failed assertion and a `FatalError` are never exceptions.
- **Memory instructions:** a load, store or atomic changes no register until its access succeeded; a new kind of access is added to `data_address_of` and calls `watch_access ()` when watchpoints are set (the stores test that at their top and go to `store_watched`).
- **Floating point:** the operands and results of the host FPU operations are `volatile` and the guard has signal fences; test the `<cfenv>` path with `-DAEMU_FPU_FENV` after touching `fpu.cpp`.
- **New instruction:** a row in `AEMU_OPCODES` (the next free opcode), the handler, `disassemble_<name>` (`disassembler/src/disassembler.cpp`), `asm_*`, one row of `BASM_INSTRUCTION_LIST` in the same position, a test in `instruction_tests/`, `docs/isa.md`.
- **Errors:** library code reports with `AEMU_FATAL`/`AEMU_CHECK` and never calls `exit`; `main` functions and test fixtures set `FatalAction::Throw`. Constructors only store their inputs; `run ()`, `link ()`, `load ()`, `preprocess ()` and `assemble ()` do the work.
- **Compile time:** a header that many files include (`types.h`, `file.h`, `alu.h`, `emulator32bit.h`) does not include `<iostream>`, `<sstream>`, `<chrono>`, `<filesystem>` or `util/logger.h` (`<format>`): use `<iosfwd>`, `assert` instead of the `AEMU_` macros, and put the code in a `.cpp`.

## Style
`.clang-format` (repo root) is LLVM-based: 4-space indent, `SpaceBeforeParens: Custom` (control statements get a space, `if (x)`; function names do not, `foo(x)`), right-aligned pointers (`int *p`). Filenames are all lowercase. Comments: `///` with `@param`/`@return` in headers, `///` for statics in `.cpp` files, `//` in code, no `/* */`; few comments in `.cpp` files. No function names in log messages (the logger prints the file and line). Few tests: enough for coverage, not one per spelling.
