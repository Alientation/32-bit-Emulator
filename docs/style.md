# Style guide

How the code is put together; [CLAUDE.md](../CLAUDE.md) has the working rules (build, test, benchmark) and `.clang-format` the formatting. A rule is added when a review says something that should hold everywhere, and the code is brought in line in a sweep ([Applying it](#applying-it)). Each rule gives its reason, so a case it does not cover can be decided by the reason.

## Standard library and templates

Keep compile times down and the code readable without knowing the library. These are preferences to apply with judgment: a standard facility that is shorter *and* clearer than the hand-written code is fine.

- **No standard abstraction that does not earn its place.** `std::exchange` is `old = x; x = y;`, `std::swap` a temporary and two assignments, `std::min`/`std::max`/`std::clamp` a conditional, `std::erase_if` on a small table a swap with the last entry. Write the statements, with a named local for a long expression: they step better and cost the same.
- **A plain array over `std::array`:** `U8 m_ring[kLines] = {};`. A parameter is a reference to the array, which keeps the size (`history (History &out)`, `using History = ExecutedInstruction[kMaxHistory]`), and a function hands one back as a reference to a static one (`ObjectFile::byte_sections ()`). `std::array` only for what a C array cannot do, return by value (a `constexpr` table that a function builds).
- **Avoid lambdas.** One hides what it captures. Write a `static` function that takes what it needs (`fault (result, what, message)`) or a private member. A condition variable waits in a `while` loop, not with a predicate. Tests may use lambdas.
- **Avoid `std::optional` for data:** a `bool` next to the value (`has_value`, `value`), or a flag in a `BitArray<N>` for a table. `std::optional` is for a function that can fail to produce a result (a parser, a lookup), never in a stored struct.
- **No template where a function or class will do.** A template is for what is sized or typed by a parameter and would otherwise be copied (`BitArray<N>`, `bitfield_*`); each instantiation is compiled again in every file that uses it.
- **No unnecessary namespaces.** A namespace encloses a library or a standalone unit of code (`encoding`, `fpu`), not the helpers of one file: those are `static` in the `.cpp`, with a `///` comment. Not a new namespace for a few functions, not an anonymous one.
- **`const` wherever it holds:** locals that do not change, by-value parameters in the *definition* (`const word pc` in the `.cpp`, `word pc` in the header), `constexpr` when known at compile time. A class type is passed as `const T &`. A local that is returned or moved is not `const`, which would stop the move.
- **References over pointers.** A pointer is for what can be absent (`set_trace (nullptr)`, the optional `PhysicalPages`), a buffer (`byte *out`) and the node of a linked structure. What is always there is a reference (`Disk &`); a reference member is set in the constructor and never changes.
- **What a function writes comes first:** a result handed back through a parameter (`out`, an error flag, an advanced counter) is the first parameter, as in `read_page (out, ppage)` and `memcpy (dest, src)`. The object a function works on (`Emulator32bit &emu`) is not a result.
- **Integers are `size_t`, `U8`..`U64`, `S8`..`S64`, not `int`, `unsigned`, `long` or `uint32_t`**, so the width is in the name. `size_t` is a count, size, index or byte offset; `U<n>`/`S<n>` a value of that width (register number `U8`, immediate `S32`, process id `S64`, a file field the width it has in the file). A number that can go below zero (`-1` for "none", a difference of offsets) is signed. A loop down to 0 over a size stays `size_t`: `for (size_t i = n - 1; i != U64(-1); i--)`. "Not found" is `~U64(0)` (`kNotFound`); a function that must return `-1` for invalid input (`digit_value`) is `S32`. `word`, `byte`, `dword` stay. What an outside interface dictates stays: `main`, `argc` and the exit code (`int`), file descriptors and `send` (`int`, `ssize_t`), `std::ptrdiff_t` of an iterator offset, `char` of text. To find what was left: `libclang` over `build/release/compile_commands.json`, listing declarations spelled `int`, `unsigned`, `long`, `short` or `[u]intN_t`.
- **No bare numbers.** A number that means something is a `constexpr` with a comment (`kInstructionSize`, `kMaxExceptionDepth`, the layout in `encoding.h`). Exceptions: bit positions inside the `bitfield_*` and `zero_bits` calls of a decoder or relocation (`docs/isa.md`), 0 and 1, radixes (2, 8, 10, 16), lexer character codes, ranks in a table read as a list (operator precedence), and numbers a `static_assert` pins down. The width of a file field is `sizeof` of its type (`ByteWriter::Data (offset, sizeof (dword))`); bits are `kNumWordBits` and `kNumByteBits`.
- **Name the fields of a braced initializer**, also in `return {...}` and tables: `{.address = address, .kind = kind}`. A field that moves or is added cannot then take another value silently.
- **Headers that many files include** (`types.h`, `file.h`, `alu.h`, `emulator32bit.h`) have no `<iostream>`, `<sstream>`, `<chrono>`, `<filesystem>` or `util/logger.h`: use `<iosfwd>` and `assert`, and put the code in a `.cpp`. No precompiled headers; clean up heavy includes instead.

## Tables

A few records the program keeps for its whole run (watchpoints, register watches):

- **Fixed capacity, named by a global `constexpr`** (`kMaxWatchpoints`). An add that does not fit returns `false` and the caller tells the user (a full table is the user's doing, not fatal). Replacing an existing entry needs no room.
- **Struct of arrays:** one array per field (`m_watchpoints.address[i]`) and a count, in one struct per table; entries are `[0, count)`. A public accessor builds the record by value (`watchpoint (i)`), so the layout is not API.
- **Remove by moving the last entry into the gap** and decrementing the count. The order is not stable, and the documentation says so.
- **Flags are a `BitArray<N>`**, not a `bool` array or an optional per entry.
- **A bounded list is returned by filling the caller's array** and returning the count (`history (History &out)`), not a `std::vector`: no allocation, and the caller sees the bound.
- **Where order matters, a ring buffer** (the history, the interrupt queue): arrays of the maximum size (`kMaxHistory`), a head and a count; the oldest entry is `count` before the head. A size the user chooses is stored and validated against the maximum by the front ends.
- **A table goes at the end of a class that has hot members**, after what every instruction reads, so it does not move them (see [internals.md](internals.md#measuring-a-change)).

## Names and helpers

- **Text built in more than one place is a helper:** a register is named by `register_name (reg)` (`alu.h`), never `"x" + std::to_string (reg)` or a local ternary; likewise conditions, system registers and exception classes: one table.
- **Register names** are `x0`–`x27`, `fp` (x28), `lr` (x29), `sp`, `xzr`. They are what the disassembler, trace, messages and debugger print, and what the assembler and debugger accept (with `x28` and `x29`). Formats for programs (`--output`, the gdb target description) keep the numbers.
- **Name a flag for what it means, not when it is true:** `leaving_start`, not `first`. A flag that guards one check is named for the situation it detects: `m_just_took_exception` (an exception while it is set is a double fault), not `m_retired_since_entry`.
- **Plain words:** an instruction is executed or completed, not "retired".
- Filenames are lowercase; constants `kName`; members `m_name`; the handler of an opcode `_name`.

## Comments and documentation

- **Documentation is in the headers:** `///` with `@param`, `@return`, `@tparam` on every public member, saying what it does and promises (limits, order, what is replaced, what is returned on failure, `@throws` for each exception).
- **Every `@param` has a direction:** `[in]` for what is only read (a value, a `const` reference, a reference or pointer kept for later), `[out]` for what is only written (a buffer, a result, the stream output goes to), `[in,out]` for what is read and changed (an advanced cursor, an appended container). `[in,out]` is a reason to look at the design again: return the result, or make the object a member.
- **A comment is a sentence:** capital letter, period, also after `@param name` and `@return` and in a trailing comment. Not for: a comment that starts with a name of its own case (`x0`, `` `hlt` ``, `m_pc`), rows of an aligned table, `// namespace` at a closing brace, tool directives (`clang-format off`, `TODO`).
- **Reread a comment as someone who has not seen the code:** every name in it is defined next to it, and it states a fact (what is overwritten and when), not that something "is OK".
- **Few comments in a `.cpp`:** `///` for a `static` function, `//` in code, no `/* */`. A comment says why, never what.
- **A flag, special case or workaround gets its reason.** If it reads as a patch, explain the case or change the code. The test: could a reader who did not write this work out what the variable is for?
- No function names in log messages (the logger prints file and line).
- A change to the ISA, a directive, the `.bo` format, the registers or a public interface updates the docs in the same change.

## Tests

- **Enough for coverage, not one per spelling:** one test per behaviour (the limit, the removal, the wrap-around, the error), not per number.
- **Read a neighbouring test file first**; use its fixtures and helpers, never invent them.
- **Labels are not mnemonics** (`L_loop`, not `b`).
- **A test written with the code is unverified** until `tools/mutate.sh` shows it fails when the line it guards is removed. Say in the commit which lines were mutated if it was not obvious.
- A table gets tests for: full, replace when full, remove (every field moves with the last entry), clear.

## C (the compiler)

`core/ccompiler` is C and not C++, and has to compile itself ([ccompiler.md](ccompiler.md#self-hosting)). The rules above that are about the language do not apply; formatting (`.clang-format`), the comment rules, and the rules about names and helpers do.

- **Only what the compiler supports:** no `long`, `float`, `double` or VLAs, no GNU extensions, `_Noreturn` or attributes, and no library beyond the small one in the plan. `int`, `char`, `size_t` and `bool`; `U32` and the like are C++. `tools/check_selfhost_subset.sh` checks the types.
- **Errors:** `error`, `error_at` and `error_tok` print a message and `exit (1)`: a compiler run is one file, and there is nothing to clean up. The compiler does not know they do not return, so write a `return` after the call that is a value of the right kind (`return NULL;`).
- **Memory:** `arena_alloc`, zeroed, never freed. A new kind of allocation needs a reason.
- **Names:** types are `CamelCase` typedefs, functions and variables `snake_case`, enumerators upper case with the prefix of their enum (`TK_`, `ND_`, `TY_`). Everything used in one file only is `static`.
- **The grammar** in [ccompiler.md](ccompiler.md#grammar) changes with the parser, in the same commit, and so does the `///` rule above the function.
- **Tables over chains of `if`:** the keyword list, the punctuators and the binary operator levels are arrays, and a new one is a row.
- **Tests** are end to end, in the integration test: a case needs an expected value worked out by hand and checked with the host compiler. The lexer and the preprocessor have unit tests as well (`core/ccompiler/tests`), whose expected expansions are the standard's examples or the host's `cpp -P`.

## Performance

- A change to the hot path (`run ()`, `MemoryPort`, `SystemBus`, `VirtualMemory`, a load/store handler) is benchmarked with `tools/bench.sh --baseline <old emu32> -n 15` and judged on the plain programs, best and median together.
- Small regressions in the debugger-mode programs (`+break`, `+history`, `+watchreg`, `+watch`, `+trace`) are accepted; they are no reason to complicate the plain path.

## Applying it

The rules hold for all of `core/` but `ccompiler/` (C, see [the section on it](#c-the-compiler)) and `external/`. A new rule is applied in a sweep: one commit per rule, the full `./build.sh` passing, a benchmark if it touches the hot path. Tests keep their lambdas and change only where an API change forces it.

Tools for finding the places (`clang-tidy` and `libclang` from `pip` in a virtual environment, on `build/release/compile_commands.json`, with the standard library's include directories as `-isystem`):

| Rule | Tool |
|------|------|
| `const` on locals | `clang-tidy -checks=-*,misc-const-correctness` |
| A member function that can be `const` | `readability-make-member-function-const` (not through a pointer member: check by hand) |
| A braced initializer names its fields | `modernize-use-designated-initializers` (skips macro expansions) |
| No bare numbers | `readability-magic-numbers` with `IgnoredIntegerValues` `0;1` |
| A by-value parameter that is not `const`, a reference parameter never written | `libclang`: list them, add `const`, build, take back what does not compile |
| Doc comments | `libclang` gives the `///` of each declaration: `@param` names, order and direction against the declaration (`const` or by-value is `[in]`; a non-`const` reference or pointer by hand), `@return` on `void`, text that is not a sentence |

Also check in a sweep: other tables that are a `std::vector` of records with a small bound, other places that build a name by hand, and flags without a comment on why they exist.
