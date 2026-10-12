# Style guide

The rules the code is written and reviewed by. [CLAUDE.md](../CLAUDE.md) has the working rules (build, test, benchmark) and the formatting that `.clang-format` enforces; this file is about how the code is put together. It grows with the review: a rule is added when a review comment says something that should hold everywhere, and the code is brought in line with it in a sweep (see [Applying it](#applying-it)).

Each rule says why, so that a case it does not cover can be decided by the reason.

## Standard library and templates

The aim is to keep compile times down and the code readable without knowing the library. This is a preference to apply with judgment, not a ban: a standard facility that makes the code shorter *and* clearer than the hand-written one is fine.

- **Do not write what two readable lines do.** `std::exchange` is `old = x; x = y;`, `std::swap` is a temporary and two assignments, `std::min`, `std::max` and `std::clamp` are a conditional. `std::erase_if` on a small table is a swap with the last entry. Write the plain statements, with a named local if the expression is long: they are easier to step through, and there is no cost, the compiler sees the same code.
- **Avoid lambdas.** A lambda hides what it captures. Write a `static` function (in an anonymous namespace in a `.cpp`) that takes what it needs as parameters, such as `fault (result, what, message)` that takes the `RunResult` it fills, or a private member function if it needs the state of the class. A condition variable waits in a `while` loop, not with a predicate. Tests may use lambdas.
- **Avoid `std::optional` for data.** A value that may be absent is a `bool` next to the value (`has_value`, `value`), or a flag in a `BitArray<N>` when there is a table of them. `std::optional` is for a function that can fail to produce a result (a parser, a lookup), where the absence is the answer. Do not put one in a struct that is stored.
- **Do not write a template where a function or a class will do.** A template is for something that is sized or typed by a parameter and would otherwise be copied (`BitArray<N>`, the `bitfield_*` helpers). Each instantiation is compiled again in every file that uses it.
- **Prefer a fixed size where the size is small and known.** See [Tables](#tables).
- **`const` wherever it holds.** Every local that is not changed, and every by-value parameter in the *definition* (`const word pc` in the `.cpp`, `word pc` in the header, where it is not part of the interface). A class type is passed as `const T &`, not by value. `constexpr` when the value is known at compile time. A local that is returned or moved is left alone, `const` would stop the move.
- **References over pointers.** A pointer is for what can be absent (`set_trace (nullptr)` turns the trace off, the optional `PhysicalPages` of the MMU), for a buffer (`byte *out`) and for the node of a linked structure. What is always there is a reference: `Disk &`, `PageTable &`. A member that is a reference is set in the constructor and never changes.
- **What a function writes comes first.** A result that is handed back through a parameter (`out`, a flag that is set on an error, a counter that is advanced) is the first parameter, and what the function only reads follows: `read_page (out, ppage)`, `translate_watched_byte (physical_address, mmu, virtual_address)`, like `memcpy (dest, src)`. The object that a function works on (`Emulator32bit &emu`) is not a result.
- **Integer types: `size_t`, `U8`..`U64`, `S8`..`S64`, not `int`, `unsigned`, `long` or `uint32_t`.** The width is then in the name and does not depend on the platform. `size_t` is a count, a size, an index or a byte offset (a watchpoint index, `num_bytes`, `BitArray<N>`); `U<n>`/`S<n>` is a value of that width (a register number is `U8`, an immediate is `S32`, a process id is `S64`, a field of a file is the width it has in the file). A number that can go below zero (`-1` for "none", a difference of two offsets, a depth that an unbalanced input makes negative) is signed: an unsigned one would wrap. A loop that counts down to 0 over a size stays `size_t` and ends at the wrapped value: `for (size_t i = n - 1; i != U64(-1); i--)`. "Not found" in a table is `~U64(0)` (`kNotFound`), a function that cannot return that because it must return `-1` for an invalid input (`digit_value`) stays `S32`. `word`, `byte` and `dword` are the names of the machine types and stay. What an outside interface dictates stays as it is: `main` and the `argc` and exit code it passes on (`int`), a file descriptor and the result of `send` (`int`, `ssize_t`), `std::ptrdiff_t` of an iterator offset, `char` of text. A lookup for what was left: `libclang` over `build/release/compile_commands.json`, listing the declarations whose type is spelled `int`, `unsigned`, `long`, `short` or `[u]intN_t`.
- **No bare numbers.** A number that means something is a `constexpr` with a comment, next to what it describes: `kInstructionSize`, `kMaxExceptionDepth`, `kVectorSize`, and the layout of the instruction set and of the system registers in `encoding.h`. The bit positions inside the `bitfield_*` and `zero_bits` calls of an instruction decoder or of a relocation are an exception (`docs/isa.md` has them), and so are 0 and 1, the radix of a number (2, 8, 10, 16), the character codes of the lexer, the ranks of a table that is read as a list (the precedence of the operators), and the numbers that a `static_assert` pins down. The width of a field in a file is `sizeof` of the type it holds (`ByteWriter::Data (offset, sizeof (dword))`), the bits of a word are `kNumWordBits`, the bits of a byte `kNumByteBits`.
- **Name the fields of a braced initializer.** `{.address = address, .kind = kind}`, not `{address, kind}`, also in a `return {...}` and in a table of aggregates. A field that moves or is added then cannot silently take another value.
- **No `<iostream>`, `<sstream>`, `<chrono>`, `<filesystem>` or `util/logger.h` in a header that many files include** (`types.h`, `file.h`, `alu.h`, `emulator32bit.h`): use `<iosfwd>` and `assert`, and put the code in a `.cpp`.
- **No precompiled headers.** Clean up heavy includes instead.

## Tables

For a collection of a few records that the program keeps for its whole run (watchpoints, register watches, and the like):

- **Fixed capacity, named by a global `constexpr`** (`kMaxWatchpoints`). An add that does not fit returns `false`, and the caller reports it to the user; it is not a fatal error, because a full table is the user's doing. Replacing an entry that exists needs no room.
- **Struct of arrays.** One array per field (`m_watchpoints.address[i]`, `m_watchpoints.length[i]`) and a count, together in one struct per table; the entries are `[0, count)`. A public accessor builds the record by value (`watchpoint (i)`), so the layout is not part of the API.
- **Remove by moving the last entry into the gap** and decrementing the count. The order is not stable, and the documentation says so.
- **Flags are a `BitArray<N>`**, not a `bool` array and not an optional per entry.
- **A bounded list is returned by filling the caller's `std::array`** and returning the count (`history (History &out)`), not as a `std::vector`: there is no allocation, and the caller sees the bound.
- **Where the order matters, a ring buffer** (the instruction history): an array per field, a head and a count, and the oldest entry is `count` before the head. The arrays have the maximum size (`kMaxHistory`), the size that the user chooses is stored and the front ends validate it against the maximum.
- **Put the table at the end of a class that has hot members**, after what every instruction reads, so that it does not move them (see the benchmark notes in [internals.md](internals.md#measuring-a-change)).

## Names and helpers

- **A piece of text that is built in more than one place is a helper.** A register is named by `register_name (reg)` (`alu.h`), never by `"x" + std::to_string (reg)` or a local ternary. The same goes for any other name that is printed (conditions, system registers, exception classes): one table.
- **Register names:** `x0`–`x27`, `fp` (x28), `lr` (x29), `sp`, `xzr`. They are what is printed (disassembler, trace, messages, debugger) and what the assembler and the debugger accept, together with `x28` and `x29`. Formats meant for programs to read (`--output`, the gdb target description) keep the numbers.
- **Name a flag for what it means, not for when it is true.** `leaving_start`, not `first`. A flag that guards one check is named for the situation it detects: `m_just_took_exception` (an exception raised while it is set is a double fault), not `m_retired_since_entry`.
- **Plain words, no jargon.** An instruction is executed or completed, not "retired".
- Filenames are lowercase. Constants are `kName`; members are `m_name`; the handler of an opcode is `_name`.

## Comments and documentation

- **Documentation is in the headers:** `///` with `@param`, `@return`, `@tparam`, on every public member, saying what it does and what it promises (limits, order, what is replaced, what is returned on failure, and `@throws` for each exception).
- **Every `@param` has a direction:** `@param[in]` for what the function only reads (a value, a `const` reference, a reference or pointer it keeps for later, such as the stream a device writes to), `@param[out]` for what it only writes (a buffer, a result, a stream the output goes to), `@param[in,out]` for what it reads and changes (a cursor that is advanced, a container that is appended to, an object that is updated in place). `[in,out]` is a reason to look at the design again: return the result, or make the object a member, where that is possible.
- **A comment is a sentence:** it starts with a capital letter and ends with a period, also the text after `@param name` and `@return` (`@param pc The address of the instruction.`) and a trailing comment (`return i; // The count, if there is none.`). Exceptions: a comment that starts with a name that has a case of its own (`x0`, `` `hlt` ``, `m_pc`), the rows of a table whose columns are aligned, `// namespace` at a closing brace, and the directives of tools (`clang-format off`, `TODO`).
- **Reread a comment as someone who has not seen the code.** Every name in it is defined next to it (no stray `k` or `n`), and it states a fact: what is overwritten and when, not that something "is OK". A comment that took a second reading is rewritten.
- **Few comments in a `.cpp`:** `///` for a `static` or an anonymous-namespace function, `//` in code. No `/* */`. A comment says why, never what.
- **A flag, a special case or a workaround always gets the reason.** If it reads as a patch, either the comment explains the case it handles or the code is changed so it is not needed. The test for a comment: could a reader who did not write this work out what the variable is for?
- No function names in log messages (the logger prints the file and line).
- A change to the ISA, a directive, the `.bo` format, the registers or a public interface updates the docs in the same change.

## Tests

- **Enough for coverage, not one per spelling.** One test for each behaviour: the limit, the removal, the wrap-around, the error. Do not repeat a case that only differs in the number it uses.
- **Read a neighbouring test file first** and use its fixtures and helpers; do not invent them.
- **Labels in tests are not mnemonics** (`L_loop`, not `b`).
- **A test written together with the code is unverified** until `tools/mutate.sh` shows that it fails when the line it guards is removed. Say in the commit which lines were mutated if it was not obvious.
- A table gets a test for: full, replace when full, remove (every field moves with the last entry), and clear.

## Performance

- A change to the hot path (`run ()`, `MemoryPort`, `SystemBus`, `VirtualMemory`, a store/load handler) is benchmarked with `tools/bench.sh --baseline <old emu32> -n 15` and judged on the plain programs, best and median together.
- Debugger-mode benchmarks (`+break`, `+history`, `+watchreg`, `+watch`, `+trace`) are reported but small regressions in them are accepted. They are not a reason to complicate the plain path.

## Applying it

The rules hold for all the code under `core/` (the C compiler in `ccompiler/` is not built and was not swept; `external/` is not ours). A rule that is added is applied in a sweep: one commit per rule, each with the full `./build.sh` passing, and a benchmark when it touches the hot path. Tests keep their lambdas and are changed only where an API change forces it.

Tools that find the places, so that a sweep does not depend on `grep` alone (a `clang-tidy` and `libclang` from `pip` in a virtual environment work with `build/release/compile_commands.json`; give them the include directories of the standard library with `-isystem`):

| Rule | Tool |
|------|------|
| `const` wherever it holds (locals) | `clang-tidy -checks=-*,misc-const-correctness` |
| A member function that can be `const` | `readability-make-member-function-const` (it does not look through a pointer member: check those by hand) |
| A braced initializer names its fields | `modernize-use-designated-initializers` (it skips what a macro expands to) |
| No bare numbers | `readability-magic-numbers` with `IgnoredIntegerValues` `0;1` |
| A by-value parameter that is not `const`, a reference parameter that is never written | `libclang`: list the parameters, add `const`, build, and take back what does not compile |
| Doc comments | `libclang` gives the `///` comment of each declaration: its `@param` names, order and direction against the declaration (a `const` or by-value parameter is `[in]`, a non-`const` reference or pointer is looked at by hand), `@return` on `void`, text that is not a sentence |

Counts of what a sweep looks at (non-test code under `core/`, from `grep`; they are places to look at, not violations): `std::optional` 70 (all of them the result of a parser or a lookup), `std::function` 1 (`Sink` of the logger, which the tests give lambdas), `std::map` / `std::set` / `std::unordered_map` 36, `std::variant` 0, `std::to_string` 113, `std::stringstream` 4.

Other things to check in a sweep: other tables that are a `std::vector` of records with a small bound (breakpoints are a `std::set`), other places that build a name by hand, and flags that have no comment on why they exist.
