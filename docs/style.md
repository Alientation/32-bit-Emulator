# Style guide

The rules the code is written and reviewed by. [CLAUDE.md](../CLAUDE.md) has the working rules (build, test, benchmark) and the formatting that `.clang-format` enforces; this file is about how the code is put together. It grows with the review: a rule is added when a review comment says something that should hold everywhere, and the code is brought in line with it in a sweep (see [Applying it](#applying-it)).

Each rule says why, so that a case it does not cover can be decided by the reason.

## Standard library and templates

The aim is to keep compile times down and the code readable without knowing the library. This is a preference to apply with judgment, not a ban: a standard facility that makes the code shorter *and* clearer than the hand-written one is fine.

- **Do not write what two readable lines do.** `std::exchange` is `old = x; x = y;`. `std::erase_if` on a small table is a swap with the last entry. Prefer the plain statements; they are also easier to step through.
- **Avoid `std::optional` for data.** A value that may be absent is a `bool` next to the value (`has_value`, `value`), or a flag in a `BitArray<N>` when there is a table of them. `std::optional` is for a function that can fail to produce a result (a parser, a lookup), where the absence is the answer. Do not put one in a struct that is stored.
- **Do not write a template where a function or a class will do.** A template is for something that is sized or typed by a parameter and would otherwise be copied (`BitArray<N>`, the `bitfield_*` helpers). Each instantiation is compiled again in every file that uses it.
- **Prefer a fixed size where the size is small and known.** See [Tables](#tables).
- **No `<iostream>`, `<sstream>`, `<chrono>`, `<filesystem>` or `util/logger.h` in a header that many files include** (`types.h`, `file.h`, `alu.h`, `emulator32bit.h`): use `<iosfwd>` and `assert`, and put the code in a `.cpp`.
- **No precompiled headers.** Clean up heavy includes instead.

## Tables

For a collection of a few records that the program keeps for its whole run (watchpoints, register watches, and the like):

- **Fixed capacity, named by a global `constexpr`** (`kMaxWatchpoints`). An add that does not fit returns `false`, and the caller reports it to the user; it is not a fatal error, because a full table is the user's doing. Replacing an entry that exists needs no room.
- **Struct of arrays.** One array per field (`m_watch_address[i]`, `m_watch_length[i]`) and a count; the entries are `[0, count)`. A public accessor builds the record by value (`watchpoint (i)`), so the layout is not part of the API.
- **Remove by moving the last entry into the gap** and decrementing the count. The order is not stable, and the documentation says so.
- **Flags are a `BitArray<N>`**, not a `bool` array and not an optional per entry.
- **Where the order matters, a ring buffer** (the instruction history): an array per field, a head and a count, and the oldest entry is `count` before the head. The size of a ring that the user chooses is allocated when it is set, and has a maximum (`kMaxHistory`) that the front ends validate.
- **Put the table at the end of a class that has hot members**, after what every instruction reads, so that it does not move them (see the benchmark notes in [internals.md](internals.md#measuring-a-change)).

## Names and helpers

- **A piece of text that is built in more than one place is a helper.** A register is named by `register_name (reg)` (`alu.h`), never by `"x" + std::to_string (reg)` or a local ternary. The same goes for any other name that is printed (conditions, system registers, exception classes): one table.
- **Register names:** `x0`–`x27`, `fp` (x28), `lr` (x29), `sp`, `xzr`. They are what is printed (disassembler, trace, messages, debugger) and what the assembler and the debugger accept, together with `x28` and `x29`. Formats meant for programs to read (`--output`, the gdb target description) keep the numbers.
- **Name a flag for what it means, not for when it is true.** `leaving_start`, not `first`.
- Filenames are lowercase. Constants are `kName`; members are `m_name`; the handler of an opcode is `_name`.

## Comments and documentation

- **Documentation is in the headers:** `///` with `@param`, `@return`, `@tparam`, on every public member, saying what it does and what it promises (limits, order, what is replaced, what is returned on failure).
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

The rules are partly applied: the debugger tables, `register_name` and `BitArray` follow them. The rest of the code predates them. A sweep goes one rule at a time, one commit per rule, each with the full `./build.sh` passing, and a benchmark when it touches the hot path. Counts of what a sweep would look at (non-test code under `core/`, excluding `external/`, from `grep`; they are places to look at, not violations):

| Pattern | Places |
|---------|--------|
| `std::optional` | ~90 |
| `std::function` | ~60 |
| `std::map` / `std::set` / `std::unordered_map` | ~70 |
| `std::variant` | ~12 |
| `std::to_string` (check for a register or other name that should be a helper) | ~115 |
| `std::stringstream` / `ostringstream` | ~45 |

Other things to check in a sweep: other tables that are a `std::vector` of records with a small bound (breakpoints are a `std::set`), other places that build a name by hand, and flags that have no comment on why they exist.
