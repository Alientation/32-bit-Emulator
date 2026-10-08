# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

# Agent Guidelines
- DO NOT use Bash commands (`cat`, `grep`, `sed`, `awk`, `heredocs`) to read or write files.
- ALWAYS use your native tools (`Read()`, `Write()`, `Edit()`, `Grep()`) for file operations.

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
- `emulator32bit/emu32`: runs the emulator from the command line (cxxopts). It links `aemu::assembler` for the `.bexe` loader. Run `--help` to see the options.
  - `-e prog.bexe` loads an executable and starts it at `_start`. Without `-e`, the program comes from `--rom-file`/`--pc`.
  - `--reg x0=5,sp=0x2000` and `--flags 0b0100` set the starting state. The default flags are `0b0100` (Z set).
  - `-l N` sets the instruction limit (0 means none). The `--ram-*`/`--rom-*`/`--disk-*` options set the memory layout. Without `--disk-file`, a `MockDisk` is used.
  - `--format plain -o state.txt -m 0x1000:16` writes `key=value` lines: `status=halted|limit|fault`, `instructions`, `message`, `pc`, `x0`..`x29`, `sp`, `N`/`Z`/`C`/`V`, and `mem[0x00001000]=<hex bytes>`. Without `-o`, the dump goes to stdout mixed with the logs.
  - Exit codes: 0 halted, 1 usage/load error, 2 limit reached, 3 fault (e.g. an unmapped access).
  - `pc` is the physical address where execution stopped. On `hlt` it points at the `hlt` itself.
- `assembler/basm`: the toolchain driver (preprocess → assemble → link). Its argument parser is hand-rolled (`assembler/src/build.cpp`, `Process`). Options that take a value need a space before the value, e.g. `-I ./programs/include -o ./programs/build/palindrome ./programs/src/palindrome.basm -outdir ./programs/build`. Useful flags: `-I`, `-l <lib.ba>`, `-o`, `-makedir` (produce a `.ba` library).
- `app/emulator_app`: builds the program and runs it on the emulator. Sample programs are in `core/app/programs/`.

Integration tests receive the binary paths as the `EMULATOR_PATH` / `ASSEMBLER_PATH` compile definitions, and `PROGRAMS_DIR` (core/app/programs). The assembler suite (`integration_tests/assembler_integration/`) works like this:
- It writes `.basm` sources into a per-test scratch dir under `/tmp/aemu_assembler_integration/`. The dir is kept when a test fails, with `basm.log`, `emu32.log` and `state.txt`.
- It shells out to `basm`, then inspects the `.bo`/`.bexe` with `ObjectFile`.
- It runs the `.bexe` with `emu32 --format plain -o state.txt`. The fixture's `run()`, `reg()`, `flag()`, `mem()` and `state()` helpers wrap this.

Add a test by writing a `TEST_F (AssemblerIntegration, ...)` that calls `write_file`, then `build`, then `run`, then asserts.

## Architecture
Static libraries with this dependency chain: `util` ← `emulator32bit` ← `assembler` ← `app`.

- **util**: logging (`logger.h`), `File`/`Directory` helpers, string utilities, and the fixed-width types (`byte`, `hword`, `word`, `U8`, ...).
- **emulator32bit**: the CPU itself is `Emulator32bit` (`emulator32bit.h`).
  - Instructions are fixed-width 4 bytes with a 6-bit opcode, so there are at most 64 instructions. The `_INSTR(name, opcode)` macro list in `emulator32bit.h` is the single source of opcodes.
  - `fill_out_instructions()` builds the `m_instruction_handler[opcode]` dispatch table. It defaults every slot to `_hlt`.
  - Execution handlers live in `src/instructions.cpp`. Each instruction also has a static `asm_<name>()` encoder, which the assembler calls, and a matching disassembler entry in `src/disassembler.cpp`.
  - Memory: `SystemBus` routes accesses to `RAM`/`ROM` (`memory.h`) and to `Disk` (a page cache over a file, `disk.h`). All of them share the `BaseMemory` page-addressed interface, so the disk is memory-mapped too.
  - `VirtualMemory`/MMU, `Timer`, and software interrupts (`software_interrupt.cpp`) are separate files. `src/kernel/` holds the C++-simulated kernel pieces: process, malloc, free block list.
- **assembler**: the pipeline is `Preprocessor` (`#include`, `#define`, macros, conditionals; writes the `.bi`) → `Assembler` (directives in `directives.cpp`, instructions in `instructions.cpp`) → `ObjectFile` (.bo) → `Linker` (symbol resolution and relocation, with `default_linker.ld` as the default script) → `.bexe`, which `load_executable` loads into emulator memory. `StaticLibrary` handles `.ba` archives. `build.cpp` orchestrates all of it.
  - All three of the preprocessor, the assembler and the linker (for `.ld` scripts) read tokens from the shared lexer in `tokenizer.h` (namespace `basm`: `SourceManager`, `lex`, `Token`, `TokenCursor`). The token list is immutable and tokens point into text owned by the `SourceManager`, so keep it alive as long as the tokens. Lexical and syntax errors are reported as `file:line:col: error: ...` with the source line and a caret, and end through `AEMU_FATAL`.
  - The preprocessor keeps a stack of input frames (the file, `#include`s, macro and symbol expansions) instead of editing a token list. A macro `#invoke` substitutes the arguments into the body textually and wraps it in `.scope`/`.scend`.
  - The preprocessor writes the `.bi` text, but `Process` hands the assembler the preprocessor's tokens directly (`Preprocessor::take_result()` → `basm::PreprocessedSource`, the `Assembler` constructor that takes one) instead of lexing the `.bi` again. Tokens keep their original file/line/column, and tokens from a macro or `#define` expansion carry `SourceLocation::expansion`, so errors show the original source line plus `note: in expansion of macro 'x'` lines. The `Assembler(process, file)` constructor still lexes a `.bi` (its errors point into the `.bi`).

**Adding or changing an instruction touches several places:** the `_INSTR` opcode list and the `asm_*` encoder (emulator32bit.h/instructions.cpp), the disassembler, the lexer's keyword table and token list (`assembler/src/tokenizer_v2.cpp`, `BASM_TOKEN_TYPES` in `tokenizer.h`), the assembler handler (`assembler/src/instructions.cpp`), and a gtest in `emulator32bit/tests/instruction_tests/` that is registered in its CMakeLists.

## basm language quick reference
- Every program needs `.global _start` and a `_start:` label (the loader's entry point). Sections are `.text`, `.data` and `.bss`. Data directives (`.word`, `.byte`, `.ascii`, `.asciz`, ...) are only legal in `.data`. Reserve `.bss` space with `.advance N`. Cross-file symbols are exported with `.global`. A symbol that is referenced but not defined becomes a WEAK, section `-1` entry in the `.bo`, and the linker resolves it.
- Comments: `; ...` and `;* ... *;`. Number literals: `42`, `$2A` (hex), `%101` (binary), `@17` (octal).
- Registers: `x0`–`x29`, `sp` (x30), `xzr` (x31). x29 is the link register, x28 the frame pointer, x8 the syscall number, x0–x7 the arguments, and x0 the return value.
- ALU ops are `op xd, xn, <xm[, shift] | imm14>`. The immediate is **unsigned 14-bit**. A trailing `s` sets flags (`adds`). `lsl/lsr/asr/ror` take `xm` or an imm5 and also accept `s` (N, Z and C from the last bit shifted out, V unchanged). `cmp`/`cmn`/`tst`/`teq` are assembled by injecting `xzr` as the destination. Conditional branches look like `b.le label`. `bl` stores the return address in x29, and `ret` is rewritten to `bx x29`.
- Memory: `[xn]`, `[xn, imm12]`, `[xn, xm{, lsl n}]`, `[xn, imm]!` (pre-index), `[xn], imm` (post-index). Immediate offsets are **signed 12-bit** (-2048 to 2047), so `[sp, -4]!` and `[x0], -4` work. `ldrsb`/`ldrsh` sign-extend. Take a symbol's address with `adrp xd, sym` + `add xd, xd, :lo12:sym`. That works for `.text` labels too (`blx xn` for indirect calls).
- `mov`/`mvn` take a register or an unsigned 14-bit immediate. Expressions are numbers combined with `+ - * /`, evaluated left to right. Symbols can't appear in expressions.
- Data directives: `.byte` (1 byte), `.dbyte` (2), `.word` (4), `.dword` (8), all little endian. Also `.char 'a'`, `.ascii`, `.asciz`, `.align N`, `.advance N`, `.org N` (an offset within the section, which can only move forward).
- Preprocessor: `#include "rel/path.binc"` (relative to the source) or `#include <"file.binc">` (searched in `-I` dirs), `#define`, `#macro name(args)`/`#macend` + `#invoke name(...)`, `#ifdef`/`#ifndef`/`#ifequ A B`/`#else`/`#elsedef`/.../`#endif`. Macro parameter names must not be keywords (`b` is the branch instruction).
- `hlt` encodes as `0x00000000`, so running into zeroed memory halts. Unused opcodes also dispatch to `_hlt`.
- Default layout (`default_linker.ld`): `.text` at 0x0, `.data` at 0x1000, `.bss` directly after `.data` (so it can share `.data`'s page). Pages are 4 KiB (`kNumPageOffsetBits = 12`).

## Toolchain/emulator quirks
- `ERROR(...)` (util/logger.h) calls `exit(EXIT_FAILURE)`. An assembler error inside a gtest therefore kills the whole test binary, and `basm` exits nonzero.
- `basm` needs at least one source file even when it's only linking libraries. `-o <name>` takes no extension (it produces `<name>.bexe`, or `<name>.ba` with `-ar`). Object files go to `-outdir` as `<src>.bo`. `-c` stops after the object files. `.bi` intermediates are deleted unless you pass `-kp`. Paths are relative to the cwd, and the default output name is `a`.
- To run a `.bexe` in-process (this is what `emu32 -e` does): build `Emulator32bit (new RAM (16, 0), new ROM (16, 16), new MockDisk ())`, call `system_bus->mmu->begin_process ()`, then `LoadExecutable`, then `run (max_instructions)`. Every vpage gets a backing disk page. Use `MockDisk` when there's no disk file, because a default-constructed `Disk` errors on save.
- `run` returns a `RunResult` (`HALTED`/`LIMIT_REACHED`/`FAULT`, the instruction count, the message). It catches emulator, system bus and virtual memory exceptions and prints e.g. "Caught Emulator Exception: HLT Exception" on a normal halt.
- **Carry flag:** subtraction/`cmp`/`sbc`/`rsc` use the ARM convention (C = *no borrow*), consistent with `HI`/`LS`/`HS`/`LO`. `adc`/`add` carry is the usual carry-out.
- The in-process preprocessor tests in `assembler/tests` share a static `Disk` and fail when run together in one gtest process. ctest runs each test in its own process, so they pass under ctest.
- The `PreprocessorUnit`, `AssemblerUnit` and `LinkerScript` suites (`assembler/tests`) use `ToolchainFixture` (`assembler/tests/include/assembler_test/toolchain_fixture.h`). It needs no emulator and sets `FatalAction::Throw`, so an assembler error is an `aemu::log::FatalError` that the test can catch and check, instead of an `exit`. Use it for new toolchain unit tests.
- Logging is extremely verbose (DBG level) in every build. Pipe through `grep -v DBG`.

## Style
`.clang-format` (repo root) is LLVM-based: 4-space indent, `SpaceBeforeParens: Always` (so `foo (x)`), right-aligned pointers (`int *p`). Filenames are all lowercase.
