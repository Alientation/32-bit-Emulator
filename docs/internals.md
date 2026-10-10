# Internals

How the code is put together, and what to check when changing it. The user level descriptions are
the other documents in this directory ([isa.md](isa.md), [basm-syntax.md](basm-syntax.md),
[belf-format.md](belf-format.md), ...); this one is for people (and tools) that change the emulator
or the toolchain. `CLAUDE.md` keeps the rules and the checklist of invariants; the details are here.

## Build system

CI (`.github/workflows/ci.yml`, Ubuntu 24.04, GCC 13) runs `./build.sh` and, as a second job,
`./build.sh asan`.

Compile flags are not set per target. Every target calls `aemu_target_defaults(<tgt>)` from
`core/cmake/AemuHelpers.cmake`, which adds the warnings, `-O3` for Release/RelWithDebInfo (compile
and link), and `--coverage` for Debug. LTO comes from CMake's IPO support. CMake options:
`AEMU_ENABLE_COVERAGE` (ON), `AEMU_WARNINGS_AS_ERRORS` (OFF), `AEMU_ENABLE_LTO` (ON),
`AEMU_LOG_COMPILE_LEVEL` (empty = 0 in Debug and 2 in Release/RelWithDebInfo, or 0-3: log calls
below that level, 0 debug / 1 info / 2 warn / 3 error, are not in the binary; `core/CMakeLists.txt`
passes it to every target, never define it in a source file; the release builds keep warnings
because the assembler's diagnostics and the report of a faulting program are warnings, and
`assembler_test.cpp` captures them), `AEMU_SANITIZE` (empty; a `;` list of `address`, `undefined`,
`leak`, `thread`, which `aemu_target_defaults` turns into `-fsanitize=...` with
`-fno-sanitize-recover=all` so a report fails the test, plus `_GLIBCXX_ASSERTIONS`; `thread` cannot
be combined with `address`/`leak`; use a build directory of its own, `build.sh asan` does),
`BUILD_TESTS` (ON; when OFF, GoogleTest isn't fetched). In-tree code links the libraries through the
`aemu::util`, `aemu::emulator32bit` and `aemu::assembler` aliases.

**New test files must be added by hand** to the `SOURCES` list of the `aemu_add_gtest(...)` call in
that module's `tests/CMakeLists.txt`. Nothing globs them. Integration tests use the same helper in
`core/integration_tests/CMakeLists.txt`.

`version.h` (`AEMU_VERSION`, from `git describe`) is generated on every build into
`build/<config>/app/generated/` and is visible only to `emulator_app`.

## Measuring a change

Benchmark before and after a change to the hot path (`run()`, `MemoryPort`, `SystemBus`,
`VirtualMemory`). The programs are listed in the `BENCHES` table of `tools/bench.sh`, one line each:
label, source, extra `emu32` arguments (the `callbench+history`, `+break`, `+trace`,
`mixbench+watchreg` and `membench+watch` lines are the same programs with a per instruction hook of
`run()` turned on) and the final state the program must reach (`x2=0x...`, computed independently of
the emulator: a run that does not end in that state fails the script, so a faster but wrong change
is not taken for a gain). A new benchmark is a `.basm` that leaves a checksum in a register, its
reference value computed outside the emulator (comment the header with what the result is) and one
line in `BENCHES`; keep it near 30 million instructions (0.1-0.2 s), except for those with
exceptions, devices or page faults, whose MIPS mean little. Two separate runs of the same binary
differ by 5% or more on this machine, so compare with `--baseline` (build the other commit in a `git
worktree`) and `-n 15` or more, and decide on the best and the median together (a new handler can
slow the *other* instructions by changing the register allocation of `run()`: GCC keeps a value that
is live across every handler call in memory once the handlers use all the registers a call may
change, which is why `run()` keeps the instruction in `m_instr_in_flight` and not in a local for the
exception handler; the floating point handlers cost `long_loop` 14% before that, and the benchmark
showed it, not the tests); a profile where the share of a function dropped is not a speedup until
the time is (the time just moves to the next thing that waits). `tools/bench.sh --perf` finds perf
itself: `/usr/bin/perf` is a wrapper that does not work on the WSL2 kernel, so it uses the binary in
`/usr/lib/linux-tools/*/perf` with the software `cpu-clock` event (WSL2 has no hardware counters),
and the release build has debug info so the samples can be attributed to source lines.

- `tools/bench_toolchain.sh` generates programs of 144k to 576k lines (`tools/gen_basm.py`), times
  `basm` and the loader of `emu32`, and fails if doubling one file more than triples the time (a
  quadratic step in the assembler or linker). Run it after a change to the assembler, the linker or
  the object file code.
- `tools/mutate.sh FILE LINE [TEXT]` replaces one line with a comment, builds the debug build, runs
  the tests and restores the file. It is how to check that a test guards a line (above all code
  written together with its tests): `killed` is good, `SURVIVED` means the line is redundant or a
  test is missing.
- `./build.sh coverage` writes `core/lcov.info` and `core/coverage/` (the older `coverage.info` is
  not written any more). To see what a group of tests adds, run the others first, capture with the
  `lcov` options of `build.sh`, run the group and capture again.

## Executables
- `emulator32bit/emu32`: runs the emulator from the command line (cxxopts). It links
  `aemu::assembler` for the `.bexe` loader. Run `--help` to see the options.
  - `-e prog.bexe` loads an executable and starts it at `_start`. Without `-e`, the program comes
    from `--rom-file`/`--pc`.
  - `--reg x0=5,sp=0x2000` and `--flags 0b0100` set the starting state. The default flags are
    `0b0100` (Z set).
  - `-l N` sets the instruction limit (0 means none). The `--ram-*`/`--rom-*`/`--disk-*` options set
    the memory layout. Without `--disk-file`, a `MockDisk` is used.
  - `--format plain -o state.txt -m 0x1000:16` writes `key=value` lines:
    `status=halted|limit|fault`, `instructions`, `message`, `pc`, `x0`..`x29`, `sp`,
    `N`/`Z`/`C`/`V`, and `mem[0x00001000]=<hex bytes>`. Without `-o`, the dump goes to stdout mixed
    with the logs.
  - Exit codes: 0 halted, 1 usage/load error, 2 limit reached, 3 fault (an unmapped or not allowed
    access, running out of pages to swap, a failed assertion, a disk error, ...), 4 stopped at a
    breakpoint (`status=breakpoint`).
  - Debugging (`docs/debugging.md`): `--trace <file|->` (a line per instruction with the registers
    it changed), `--history N` (last N instructions, printed as `history[i]=...` after the run),
    `--break addr|symbol,...`, `--debug` (interactive REPL, `Debugger` in `debugger.h`) and `--gdb PORT`
    (`GdbServer` in `gdb_server.h`: the packet handler `handle ()` is separate from the socket loop
    `serve ()`; it describes the ARM core registers to gdb and absorbs the step that gdb makes
    after a watchpoint hit). Symbols
    come from the `.bexe` (`SymbolMap`, a local label has its `:LOCAL:n` suffix stripped). The
    library side is `Emulator32bit::add_breakpoint`/`set_trace`/`set_history_size`/`set_symbols`;
    `RunResult::Status::BREAKPOINT` is a fourth status, so a `switch` over it needs the case. The
    first instruction of a `run()` never stops at a breakpoint. `Debugger` turns on `set_brk_stops`,
    so a `brk` instruction stops the run too (pc at the next instruction). `--watch
    addr|symbol[:len][:r|w|rw][:p][:<op><value>]` / `add_watchpoint` stop after a load, store or
    atomic touches virtual (or, with `p`, physical: the access is translated again by
    `watch_access`) bytes whose value matches the condition (also `Status::BREAKPOINT`, with the
    instruction finished and the pc at the next one); the hook is `watch_access ()`, called by the
    memory handlers in `instructions.cpp` only when the list is not empty (the stores test it at
    the top and go to `store_watched`, which reads the old value first), and the REPL has `watch`/`unwatch`/`watches`. `--watch-reg
    reg[=value]` / `add_register_watch` stop after a register changes: `run ()` compares the watched
    registers at the top of each iteration (and once after the loop), nothing is hooked into
    `write_reg`. `run ()` decides once, at its start, whether anything looks at each instruction
    (`hooked`: register watches, breakpoints, history, trace, watchpoints) and the loop only tests
    that local, so a new per-instruction debugging feature has to be added to `hooked` or it never
    fires; set the hooks between runs, not inside one. The conditions are one table lookup
    (`check_cond`, `kConditionTable` in `alu.h`: the four flags are the low bits of PSTATE and index
    a 16 bit mask per flag combination).
  - `--no-semihosting` makes `swi 1`, the emulator calls, an undefined instruction. The plain dump
    also has `mode`, `pstate`, `elr`, `spsr`, `esr`, `far` and `vbar`.
  - `pc` is the virtual address where execution stopped (the loader sets it to `_start`, and fetch
    translates it). On `hlt` it points at the `hlt` itself, and on a fault at the instruction that
    faulted. Fetch faults on a misaligned, unmapped, non-executable or non-RAM PC.
- `assembler/basm`: the toolchain driver (preprocess → assemble → link). Its argument parser is
  hand-rolled (`assembler/src/build.cpp`, class `Build`). `main` passes `argv` as it is, so a path
  can have spaces. Options that take a value need a separate argument for the value, e.g. `-I
  ./programs/include -o ./programs/build/palindrome ./programs/src/palindrome.basm -outdir
  ./programs/build`. Useful flags: `-I`, `-l <lib.ba>`, `-o`, `-ar` (produce a `.ba` library), `-ld
  <script.ld>` (linker script; `ENTRY(sym)` aliases `_start` to `sym`), `-D name[=value]` (defines a
  preprocessor symbol, as if the source began with `#define name value`), `-W error` or `-wall` (a
  warning ends the build like an error), `-kp` (write the preprocessed text of each source to a
  `.bi` file; otherwise there is none), `-dump` (prints an objdump-style listing of every object
  file and the executable to stdout). There is no optimizer and no `-O`/`-oall` option (they are an
  unknown flag). `basm` exits 0 on success and 1 on any error, `--help` and `--version` exit 0
  without building.
- `app/emulator_app`: builds the program and runs it on the emulator. Sample programs are in
  `core/app/programs/`.

Integration tests receive the binary paths as the `EMULATOR_PATH` / `ASSEMBLER_PATH` compile
definitions, and `PROGRAMS_DIR` (core/app/programs). The assembler suite
(`integration_tests/assembler_integration/`) works like this:
- It writes `.basm` sources into a per-test scratch dir under `/tmp/aemu_assembler_integration/`.
  The dir is kept when a test fails, with `basm.log`, `emu32.log` and `state.txt`.
- It shells out to `basm`, then inspects the `.bo`/`.bexe` with `ObjectFile`.
- It runs the `.bexe` with `emu32 --format plain -o state.txt`. The fixture's `run()`, `reg()`,
  `flag()`, `mem()` and `state()` helpers wrap this.

Add a test by writing a `TEST_F (AssemblerIntegration, ...)` that calls `write_file`, then `build`,
then `run`, then asserts.

## Architecture
Static libraries with this dependency chain: `util` ← `disassembler` ← `emulator32bit` ← `assembler` ← `app`.

- **util**: logging (`logger.h`), `File`/`Directory` helpers, string utilities, and the fixed-width
  types (`byte`, `hword`, `word`, `U8`, ...).
- **disassembler**: `disassembler::disassemble (word)` (`disassembler/disassembler.h`, `src/disassembler.cpp`), the instruction word as assembly text. It includes only the headers of the encoding from the emulator library, which are constants and inline functions (`encoding.h`: the opcode constants `_op_<name>`, the operations of the special group, `AddrType`, the system register numbers and names; `opcodes.h`, `alu.h`, `fpu.h`), so that nothing of the machine is linked into it. `Emulator32bit` derives from `Encoding`, which is why `Emulator32bit::_op_add` and `Emulator32bit::kSysregId_elr` still exist. `Emulator32bit::disassemble_instr` calls it.
- **emulator32bit**: the CPU itself is `Emulator32bit` (`emulator32bit.h`).
  - Instructions are fixed-width 4 bytes with a 6-bit opcode, so there are at most 64 instructions.
    The `AEMU_OPCODES(X)` list in `opcodes.h` is the single source of opcodes (a variant of an
    opcode, like the four shifts, is not a row: one handler decodes it). The handler declarations
    (`emulator32bit.h`), the `_op_<name>` constants (`encoding.h`), the dispatch switch and the
    disassembler switch are all generated from it, and a `static_assert` rejects a duplicate or out of range opcode.
  - `Emulator32bit::execute()` is a `switch` over the opcode with one `case` per `AEMU_OPCODES` row
    (a direct call to the handler, so the compiler can inline it; it replaced a table of member
    function pointers, which measured 7-17% slower on the memory benchmarks). The default case is
    `_bad_opcode`, which faults with `BAD_INSTR`.
  - Execution handlers live in `src/instructions.cpp`. Each instruction also has a static
    `asm_<name>()` encoder, which the assembler calls, and a matching disassembler entry in
    `disassembler/src/disassembler.cpp`.
  - Memory: `SystemBus` is the physical address space: it decodes an address
    (`find_memory`/`route_memory`) to `RAM`/`ROM` (`memory.h`), to `Disk` (a page cache over a file,
    `disk.h`) or to a device, and owns all of them. Every address on it is physical, it has no MMU.
    All the memories share the `BaseMemory` page-addressed interface, so the disk is memory-mapped
    too. `Emulator32bit` owns three objects, in this order: the bus (`system_bus`), the MMU (`mmu`,
    a `VirtualMemory` over the disk and the frames of the RAM of the bus) and the `MemoryPort`
    (`memory`, `memory_port.h`), the load/store unit that translates through the MMU and then
    accesses the bus: `memory.read_word (vaddr)`, `read<T>`/`write<T>`, `write_block`,
    `fetch_instruction`. A store of a program to the ROM is a bus error (`SystemBus::route_store`, a
    `SystemBus::Exception`); the loader's physical sections and the host (`ROM::write_*`) can still
    write the image. The bus owns the memories and the disk (`unique_ptr`; the `Emulator32bit` and
    `SystemBus` constructors take `std::unique_ptr`s, e.g. `std::make_unique<RAM> (...)`).
    `MockDisk` is a disk in memory (it keeps the pages written to it, and has no addresses), `Disk`
    needs a file. A `ROM` made from a file (`--rom-file`) only reads the image, nothing is written
    back to the file when the ROM is destroyed.
  - `VirtualMemory` (the MMU, `virtual_memory.h`) pages the virtual pages of a process in and out of
    the physical memory. It copies page contents through the `PhysicalPages` interface
    (`physical_pages.h`) that `SystemBus` implements, and keeps swapped-out pages on the `Disk`
    (`Disk::read_page (page, byte *out)`/`write_page (page, const byte *)` copy a page without a
    vector; the vector versions wrap them). `PhysicalPages::direct_page ()` gives the bytes of a RAM
    page (null for anything else, which goes through a buffer), so a page is copied once between the
    RAM and the disk in each direction. The free frames are a `FrameAllocator` (`frame_allocator.h`,
    a bitmap that hands out the lowest free page first and throws `FreeBlockListException` in the
    cases the free block list did, which the tests compare), and the paging functions take the
    `PageTableEntry`/`PhysicalPage` they already have instead of looking them up again (each lookup
    is a hash and a division). The information about the frames (`PhysicalPage`) is in chunks of 512
    made on first use (`m_frame_chunks`, `physical_page ()` is two loads), and only the pages
    outside the frames, which an explicit mapping can reach, are in a hash map; the consistency
    checks (`check_consistency ()`, which the tests call) go through `all_physical_pages ()`. A page is only paged into the *frames*, the pages of
    the RAM (the constructor gets the range), so ROM and the disk are never handed out; an explicit
    mapping (`ensure_physical_page_mapping`, what the loader uses for physical sections) may map to
    any physical page. When the frames are full a swappable frame is evicted by a clock (second
    chance) that approximates least recently used: a translation that fills the TLB sets a
    `referenced` bit of the physical page (`mark_referenced`; a TLB hit does not, that would be work
    in every access), and the hand (`clock_victim`) passes over pages that have it set, clearing it
    and invalidating the TLB entries of that page (`invalidate_tlb`, which also drops the remembered
    fetch page if it is one of them), until it finds one that does not, so that a page in use fills
    its entry and is marked again by the next access (the way the accessed bit of a real MMU works;
    the TLB is not wiped, that cost a fault a 64 KB loop); `set_ppage_permissions` can pin pages and
    mark them kernel only. Instruction fetch keeps the page of the last instruction
    (`translate_fetch`, `m_fetch_vpage`) so that the following ones skip the translation; everything
    that changes whether a fetch is allowed or where the page is calls `drop_fetch_cache ()`
    (`flush_tlb`, `invalidate_tlb`, which the hand calls for each page whose mark it clears, process
    changes, `set_enabled`, `set_user_mode`, the walk's TLB invalidation and fill), so a new place
    that changes the mappings, permissions or mode must do the same. `enabled` is `set_enabled
    ()`/`enabled ()` for that reason. The TLB of the swapping memory (`m_tlb`, 16 byte entries) is
    looked up with one 64 bit key, `tlb_key (pid, vpage)`, compared with `m_swap_key | vpage` in
    `translate_address`, which has no mode checks: `m_swap_key` is the process half of the key, or
    `kNoProcessKey` (which no entry has) when virtual memory is off, the page tables are used or
    there is no process, and then `translate_address_slow` decides (identity, the devices, the
    walk). Whatever changes the current process, `m_enabled` or `m_walk` must call `update_swap_key
    ()`; a missing call is wrong in one direction (swapping entries still hit while the page tables
    are on). An access that is not allowed (unmapped, write to a read-only page, execute of a page
    that is not executable, kernel memory without kernel privilege) throws
    `VirtualMemory::PageFaultException` with the reason, and `run` reports it as a fault. The
    emulator calls are in `software_interrupt.cpp`: `swi 1` (and `swi` without a vector table) with
    the call number in x8 (the table is above `Emulator32bit::_swi`), and `Emulator32bit::set_output
    ()`/`set_error_output ()` choose where their output goes.
  - **Exceptions and privilege** (`docs/exceptions.md`): PSTATE has a mode bit (`U`, kernel = 0, the
    CPU starts in kernel mode) and an IRQ mask. Once `VBAR` (system register, `msr vbar, x0`) is not
    0, `run()` turns what stops an instruction into an exception instead of ending the run:
    `deliver_exception` maps the C++ exceptions (`Emulator32bit::Exception` of type
    `BAD_INSTR`/`BAD_REG`, `PageFaultException`, `SystemBus::Exception`) to undefined instruction /
    instruction abort / data abort, `enter_exception` saves ELR/SPSR/ESR/FAR, goes to kernel mode
    (banked `sp`) and jumps to `VBAR + 16 * class`; `ERET` returns. `HALT_INSTR`, a failed assertion
    and a `FatalError` are never exceptions. With `VBAR == 0` nothing changes (the old fault path).
    An `Exception` carries an ISS (`kUndefinedIss_*`) for the syndrome, so a new `throw Exception
    (BAD_INSTR, ...)` should pass the right one. A handler that sets `m_pc` itself (`ERET`,
    `enter_exception`) sets `m_pc_written` so that `run()` does not add 4. Privileged instructions
    call `require_kernel ()`.
  - **Devices and interrupts** (`docs/devices.md`): `devices.h` has `Device` (a register page, a
    `BaseMemory`) and `InterruptController` (first come, first served queue, 32 lines, ENABLE mask,
    CLAIM register), `Timer` (counts retired instructions, `tick()` is called by `run()` after each
    one), `Console` (stream out, queued bytes in, and the live input of a `HostInput` that another
    thread fills: it is taken on the emulator thread, by `poll_host_input ()` from the register
    reads and from the `hooked` part of `run ()`, which a live input turns on) and `BlockDevice` (512 byte sectors, data
    register, completes after LATENCY ticks, line 2, `--block-file`/`--block-sectors`; DMA commands
    4/5 with DMA_ADDR/DMA_COUNT, and 6/7 with a list of descriptors at DMA_LIST, move whole sectors
    to and from RAM at completion (`set_dma_memory`, wired by `SystemBus`); separate from the memory mapped `Disk` of the swapping MMU). `SystemBus`
    owns them (`intc`, `timer`, `console`, `block`) at `0xF0000000`/`+0x1000`/`+0x2000`/`+0x3000`
    and `route_memory` sends addresses from `kDeviceBase` to them; the swapping MMU leaves those
    addresses untranslated. `run()` takes the IRQ exception between instructions when the queue is
    not empty, `PSTATE.I` is 0 and `VBAR != 0`; `_wfi` returns when something is pending, jumps the
    timer to its match, else waits for the host's input if the program asked to hear of a byte, else
    halts. Instruction fetch also works from the ROM. `emu32
    --console-input FILE` feeds the console, `--console-stdin` hands it what is typed. Memory may not reach `kDeviceBase`.
  - **Page tables** (`docs/mmu.md`): `SCTLR.M = 1` (`msr sctlr, 1`) switches
    `VirtualMemory::translate_address` from the swapping process MMU above to a hardware walk of a
    two level table at `PTBR` (physical): entries `V W X U A D` + frame, `A`/`D` set by the walker,
    a visible 4096-entry TLB (`WalkEntry`) that only `tlbi`/`tlbi xn`, `PTBR` and `SCTLR` writes
    clear, user mode needs `U`, the kernel never fetches from a `U` page. Faults are the normal
    `PageFaultException` → abort. `PhysicalPages` has `read_physical_word`/`write_physical_word` for
    the walker; `MemoryPort::fetch_instruction` fetches with `EXECUTE` in this mode (through
    `VirtualMemory::translate_fetch`, whose remembered page is dropped exactly when the TLB entry
    is); `Emulator32bit::set_user_mode` tells the MMU the mode. With `SCTLR.M = 0` nothing changes.
    Page tables a bare metal program builds itself need physical placement (`@P;` linker script).
  - A load, store or atomic changes no register until its memory access succeeded
    (`decode_mem_operand ()`/`write_back_base ()`), so an instruction that faults did nothing. FAR
    for a data abort relies on that: it is worked out when the fault is delivered (`data_address_of
    (instr)` decodes the instruction again from the unchanged registers), not stored by every
    access, so a new kind of access must keep the registers unchanged until it succeeded and be
    added to `data_address_of`.
  - **Alignment:** the handlers of `ldr`/`ldrh`/`str`/`strh` (`instructions.cpp`) go through
    `load<T>`/`store<T>`: an address that is a multiple of `sizeof(T)` takes
    `MemoryPort::read_aligned<T>`/`write_aligned<T>` (no page-crossing check, it cannot cross),
    otherwise the access is only made if the instruction is the unaligned form
    (`MemOperand::unaligned`, from `decode_mem_operand<kHasUnalignedForm>`; false for the byte
    handlers, where `adr` = `11` is an undefined instruction) and else `throw_misaligned` raises
    `Exception` of type `MISALIGNED`, which `deliver_exception` turns into a data abort with ISS
    `kAbortIss_alignment` (and FAR from `data_address_of`) and `run()` reports as a fault. The word
    and half-word atomics (`_atomic_rmw`) check it too, with no unaligned form (the message says
    "atomic access"). `MemoryPort::read<T>`/`write<T>`, used by the host and the debugger, still
    take any address.
  - **Floating point** (`docs/isa.md`, `fpu.h`/`fpu.cpp`): a `float` is a general register and a
    `double` a pair `(xN, xN+1)`, N at most 28 (a pair that starts elsewhere is an undefined
    instruction, ISS 5, `check_pair`), so there is no register file of its own and the ABI did not
    change. Four opcodes, `fop1` (abs, neg, sqrt, the `frint*` family, conversions), `fop2` (add,
    sub, mul, div, min, max), `fcmp` and `fop3` (fused multiply-add), with the function in bits 4-0
    (1-0 for `fop3`) and the precision in bit 25 (`asm_fop1/2/3`, `asm_fcmp`). The arithmetic is done by the **host FPU**: `HostFpu` (`fpu.cpp`)
    sets the rounding mode of `FPCR` (system register 10, bits 1-0) and reads the host's exception
    flags into `FPSR` (11, cumulative, nothing traps); on x86-64 it writes MXCSR directly, elsewhere
    (or with `-DAEMU_FPU_FENV`) it uses `<cfenv>`. The operands and results of the operations are
    `volatile` and the guard has signal fences, so the compiler cannot move or fold the arithmetic
    across the change of mode: keep it so, and test the `<cfenv>` path with a scratch build that
    defines `AEMU_FPU_FENV` after touching `fpu.cpp`. `HostFpu` is `always_inline`: when it was left to the compiler, adding `fop3` (more callers) made LTO stop inlining it and `fpbench` 6% slower. A new FP handler needs `tools/bench.sh --baseline`. A NaN that an operation produces is always the
    default NaN (`0x7FC00000`) so that the bits do not depend on the host; float to integer
    saturates. `fpcr` and `fpsr` are the system registers that user mode may use. `fcmp` sets NZCV
    like AArch64's `fcmp` (unordered is C and V, so `mi`/`ls` are the conditions for `<`/`<=`). In
    the assembler each mnemonic is a row per precision (`BASM_FP_PAIR` in `instruction_list.h`; `a`
    is the function, `b` the precision), the lexer reads `fcvt.s32.f64` as one word (`lex_word`),
    `fmov.f32/.f64` is a pseudo instruction (`assemble_fmov`, no opcode) and `.float`/`.double`
    define data. `fop3` (`fmadd`...) has four registers, so its function is in bits 1-0 and the
    accumulator in bits 8-4 (`asm_fop3`, format F4).
- **assembler**: the pipeline is `Preprocessor` (`#include`, `#define`, macros, conditionals; writes
  the `.bi` only if asked to) → `Assembler` (directives in `directives.cpp`, instructions in
  `instructions.cpp`) → `ObjectFile` (.bo; the printer behind `-dump` is in `object_file_print.cpp`)
  → `Linker` (symbol resolution and relocation; the default layout is built into `linker.cpp`) →
  `.bexe`, which `LoadExecutable` copies into emulator memory. `static_library.h` handles `.ba`
  archives. `build.cpp` (`Build`) orchestrates all of it.
  - **Relocation:** how each relocation type is encoded lives in one function, `apply_relocation`
    (`relocation.h`). The assembler uses it only for a branch to a label in the same file (the `.bo`
    keeps every other relocation), the linker uses it for all of them, since it knows every final
    address. So a `.bexe` has no relocations and `LoadExecutable` only copies the sections to their
    addresses (it refuses a file that still has relocations, e.g. a `.bo`). `.word symbol` in
    `.data` is a relocation too (`R_EMU32_ABS32`, in `rel_data`), the address is added to what is in
    the word. Every relocation has a signed `addend` that is added to the address of the symbol
    (`symbol + 4`); `Assembler::add_relocation` records it, the linker and `fill_local` pass
    `symbol_value + addend` as the target.
  - **Documentation:** the language and the file formats are described for users in
    `docs/basm-syntax.md` and `docs/belf-format.md`. Keep them in step when the syntax, a relocation
    type or the `.bo` layout changes. `docs/isa.md` is the ISA reference (keep it in step with
    `opcodes.h` and the handlers). `docs/mmu.md` is the page table format and the TLB rules.
    `docs/devices.md` is the memory map, the devices, interrupts and the boot sequence.
    `docs/exceptions.md` describes the exception machinery. `docs/abi.md` is the agreed calling
    convention, data layout and frame record for the planned C compiler and OS (a convention,
    nothing enforces it yet; `bt` follows its frame record), plus `udiv`/`sdiv` (implemented: a
    division by zero is 0, no remainder instruction). Check them before changing `swi`, the MMU, the
    frame layout or the opcode list (29 primary opcodes are free, the range `100011`–`111111`: the
    rows of `AEMU_OPCODES` are kept dense from 0, so a new instruction takes the next one; `100001`
    is `adr`, the pc relative address with the relocation `R_EMU32_ADR_PCREL21`; `100010` is
    `fop3`; `011011` is `csel`,
    and the special group has `sxtb`..`rev16` under extended op `1000`).
  - **Linking:** the sections of the object files are joined in order, each starting at a multiple
    of its alignment (the largest `.align` of the section, `SectionHeader::alignment`),
    `place_sections` puts the sections at the addresses of the script (a section without one follows
    the previous, rounded up to its alignment) and fails if two overlap or an address is not
    aligned, then symbols are merged, the entry point is checked (`_start`, or the symbol of
    `ENTRY`, must be defined) and everything is relocated. A symbol with no section after linking is
    an `undefined reference`, whether it was declared with `.global` or not, unless only `.weak`
    declarations name it (then it is 0). A strong definition replaces a weak one (`.weak`, `.comm`;
    binding `WEAK_DECLARED`), of two weak ones the first stays, two strong ones are a "Multiple
    definition" error. With a `SECTIONS` command, a section with contents that it does not list is
    an error. The byte sections (`.data`, `.rodata`, `.init_array`, `.fini_array`) are described
    once, by `ObjectFile::byte_sections()`, and the object file reader/writer, assembler, linker and
    loader loop over it: a fifth would be one row there plus the token/directive. The linker defines
    `__init_array_start/_end` and `__fini_array_start/_end`. Members of a static library are only
    linked if they define a symbol that is used and not defined yet (`select_library_members`), so
    `-l` can list libraries with things the program does not use.
  - **Construction vs. work:** constructors only store their inputs. `Build::run()`,
    `Linker::link()`, `LoadExecutable::load()`, `Preprocessor::preprocess()` and
    `Assembler::assemble()` do the work. `Preprocessor` takes a `PreprocessorOptions` (include dirs,
    `-D` defines, `write_output`, `options.h`) and `Assembler` needs nothing from `Build`, so both
    can be used without a command line. `Build` links the `ObjectFile`s that the assembler made
    (`Assembler::object ()`), it does not read the `.bo` files again.
  - All three of the preprocessor, the assembler and the linker (for `.ld` scripts) read tokens from
    the shared lexer in `tokenizer.h` (namespace `basm`: `SourceManager`, `lex`, `Token`,
    `TokenCursor`). The token list is immutable and tokens point into text owned by the
    `SourceManager`, so keep it alive as long as the tokens. Lexical and syntax errors are reported
    as `file:line:col: error: ...` with the source line and a caret, and end through `AEMU_FATAL`.
  - The preprocessor keeps a stack of input frames (the file, `#include`s, macro and symbol
    expansions) instead of editing a token list. A macro `#invoke` substitutes the arguments into
    the body textually and wraps it in `.scope`/`.scend`.
  - The preprocessor can write the `.bi` text (`-kp`), but `Build` hands the assembler the
    preprocessor's tokens directly (`Preprocessor::take_result()` → `basm::PreprocessedSource`, the
    `Assembler` constructor that takes one) instead of lexing the `.bi` again. Tokens keep their
    original file/line/column, and tokens from a macro or `#define` expansion carry
    `SourceLocation::expansion`, so errors show the original source line plus `note: in expansion of
    macro 'x'` lines. The `Assembler(file)` constructor still lexes a `.bi` (its errors point into
    the `.bi`).

**Adding or changing an instruction touches a few places.** Emulator side: a row in `AEMU_OPCODES`
(`opcodes.h`), the handler `_<name>` (`instructions.cpp`), a `disassemble_<name>` function
(`disassembler/src/disassembler.cpp`) and, for the assembler to call, an `asm_*` encoder. Assembler side: one row in
`BASM_INSTRUCTION_LIST` (`assembler/include/assembler/instruction_list.h`, in the same position as
in the opcode list, with `HLT` first and `RET` last). That row generates the `INSTRUCTION_<NAME>`
token type, the lexer keyword (with its `s` variant) and the encoding rule: its `InstructionFormat`
says which `parse_format_*` function parses the operands, and a new operand syntax is a new format
(a case of `Assembler::assemble_instruction`) or, if it is one of a kind, a dedicated `_<name>()`
like `_msr`. `assembler/tests/instruction_table_test` and
`emulator32bit/tests/instruction_tests/opcode_table_test.cpp` check that the lists, the assembler
and the disassembler agree. Finally a gtest in `emulator32bit/tests/instruction_tests/` that is
registered in its CMakeLists. The tests there are table-driven per instruction family
(`dataproc_test.cpp`, `shift_test.cpp`, `long_multiply_test.cpp`, `move_test.cpp`,
`memory_test.cpp`), so a new ALU op usually means one more row in that file's op table rather than a
new file. Shared helpers (`step`, `execute`, flag and register-snapshot checks, `ref_shift`) are in
`tests/include/emulator32bit_test/emulator32bit_test.h`.

## basm language notes

The user reference is [basm-syntax.md](basm-syntax.md); these are the points that matter when
changing the assembler.

- Every program needs `.global _start` and a `_start:` label (the loader's entry point). Sections
  are `.text`, `.data`, `.rodata` (read only), `.init_array`/`.fini_array` (tables of function
  addresses, read only) and `.bss`, plus any number of your own: `.section "name"[, "r"|"rw"|"rx"]`
  (default `"rw"`, never writable and executable at once; bytes, with instructions in an `"rx"` one;
  a third operand `"nobits"` (`.section "stack", "rw", "nobits"`) makes it zero filled like `.bss`:
  only a size in the file (`UserSection::nobits`/`zero_size`, header type `USER_BSS`), only labels,
  `.advance`, `.align` and `.org` in it; `ObjectFile::user_sections`, header types
  `USER_R/USER_RW/USER_RX` + `REL_USER` after each; the linker joins them by name and a linker
  script places them by string, `".vectors" = 0x800;`). Data directives (`.word`, `.byte`, `.ascii`,
  `.asciz`, ...) are legal in all of them but `.text` and `.bss`. Reserve `.bss` space with
  `.advance N`. Cross-file symbols are exported with `.global` (which, like `.extern`, can be
  anywhere, also in a macro). `.pushsection "name"[, flags]` / `.popsection` (a stack of sections,
  for a macro that puts data elsewhere and carries on; unclosed is an error) work like `.section`
  and back. `.weak sym` (replaceable definition, or a reference that is 0 when undefined) and `.comm
  sym, size{, align}` (weak definition in `.bss`) also work anywhere. A symbol that is referenced
  but not defined becomes a WEAK, section `-1` entry in the `.bo`, and the linker resolves it. A
  symbol that nothing defines is a link error.
- Comments: `; ...` and `;* ... *;`. Number literals: `42`, `0x2A` (hex), `0b101` (binary), `0o17`
  (octal), as in the linker script and everywhere else in the toolchain (the old `$2A`, `%101`,
  `@17` are lexical errors that say what to write; `%` is only the remainder operator now; `010` is
  decimal 10).
- Registers: `x0`–`x29`, `sp` (x30), `xzr` (x31). x29 is the link register, x28 the frame pointer,
  x8 the syscall number, x0–x7 the arguments, and x0 the return value (x0 and x1 for 64 bits).
- ALU ops are `op xd, xn, <xm[, shift] | imm14>`. The immediate is **unsigned 14-bit**. A trailing
  `s` sets flags (`adds`). `lsl/lsr/asr/ror` take `xm` or an imm5 and also accept `s` (N, Z and C
  from the last bit shifted out, V unchanged). Instructions that share an opcode: `lsl/lsr/asr/ror`
  are `shift` (the `ShiftType` in bits 7-8 of format O1), `umull/smull` are `mull` (bit 0 = signed)
  and `bx/blx` are `bx` (bit 0 = link), so their `BASM_INSTRUCTION_LIST` rows carry the same opcode
  in `a` and the variant in `b`, and `asm_format_o1/o2/b2` take the variant (`ShiftType`,
  `is_signed`, `link`) instead of an opcode. `cmp`/`cmn`/`tst`/`teq` have no opcode: they are
  `subs`/`adds`/`ands`/`eors` with `xzr` as the destination (`O_NO_DEST` rows of
  `BASM_INSTRUCTION_LIST` carry the opcode of the operation, `parse_format_o` forces S, and the
  disassembler names such an instruction `cmp`...). Conditional branches look like `b.le label`.
  `bl` stores the return address in x29, and `ret` is rewritten to `bx x29`.
- Memory: `[xn]`, `[xn, imm12]`, `[xn, xm{, lsl n}]`, `[xn, imm]!` (pre-index), `[xn], imm`
  (post-index). Immediate offsets are **signed 12-bit** (-2048 to 2047), so `[sp, -4]!` and `[x0],
  -4` work. `ldrsb`/`ldrsh` sign-extend. `ldr`/`str`/`ldrh`/`strh` fault on an address that is not a
  multiple of 2 or 4 (a data abort, ISS alignment); `ldur`/`stur`/`ldurh`/`sturh`/`ldursh` are the
  same accesses at any address (same opcodes, `adr` field `11` = `AddrType::ADDR_UNALIGNED`; a plain
  offset only, an error for `!` or post-index). Take a symbol's address with `adrp xd, sym` + `add
  xd, xd, :lo12:sym`. `adrp` uses `:hi20:` (the page distance, `R_EMU32_ADRP_HI20`) implicitly;
  writing `adrp xd, :hi20:sym` is accepted but not needed. `:hi13:`/`:lo19:` are the halves for
  `mov`. The full ISA reference is `docs/isa.md`; keep it in step when an instruction or encoding
  changes. That works for `.text` labels too (`blx xn` for indirect calls).
- `mov`/`mvn` take a register or an unsigned 19-bit immediate. Expressions
  (`Assembler::parse_expression`, `directives.cpp`) have the operators and precedence of C: `( )`,
  unary `- + ~ !`, `* / %`, `+ -`, `<< >>`, `< <= > >=`, `== !=`, `&`, `^`, `|`, `&&`, `||`, on
  number and character literals. Arithmetic is 64 bit and wraps; `/ % >>` and the comparisons are
  signed, comparisons and logic give 1 or 0; division by zero and a shift outside 0-63 are errors.
  `.equ NAME, expr` names a number (a constant of the file or of the open `.scope`, not a symbol of
  the `.bo`, so other files cannot see it); a number needs a constant or a label defined earlier,
  and the only number you can make from labels is the difference of two defined labels of the same
  section (`end - start`). `label + 4`, `4 + label` and `label - 4` are a symbol with an addend:
  they are only accepted where a symbol is taken (`.word`, `adrp`, `:lo12:`/`:hi13:`/`:lo19:`,
  `b`/`bl`) and become a relocation whose `addend` (`RelocationEntry::addend`, the 8 byte field of
  the `.bo`) the linker adds to the symbol's address; the symbol can be defined later or in another
  file. `-label`, `2 * label` and the like are errors. A name alone in `.word` is an address
  (relocation) unless it is a constant, and in a branch it is the target label unless it is a
  constant (then a byte distance). A numeric branch target (`b 8`, `b.ne -2 * 4`) is a signed byte
  distance from the branch itself (4 byte aligned, -8388608 to 8388604).
- Data directives: `.byte` (1 byte), `.dbyte` (2), `.word` (4), `.dword` (8), all little endian. A
  value has to fit (`.byte 256` is an error, `0 - 1` is 255 or -1), and `.word` also takes the name
  of a symbol, which is its address once linked (`table: .word first, second`). A character literal
  is a number (`.byte 'a'`; `.char` and the signed `.sbyte`/`.sword`/... were removed as
  duplicates). Also `.fill count{, size{, value}}` (repeats a 1/2/4/8 byte value), `.ascii`,
  `.asciz`, `.align N` (kept when files are linked), `.advance N`, `.org N` (an offset within the
  section, which can only move forward). A size that is almost certainly a mistake (`.advance` of 16
  MiB or more) is an error.
- Preprocessor: `#include "rel/path.binc"` (relative to the source) or `#include <"file.binc">`
  (searched in `-I` dirs), `#define`, `#macro name(args)`/`#macend` + `#invoke name(...)`,
  `#ifdef`/`#ifndef`/`#ifequ A B`/`#ifless`/`#ifmore`/`#else`/`#elsedef`/.../`#endif`. Those compare
  numbers by value (`9` is less than `10`) and anything else as text. Macro parameter names must not
  be keywords (`b` is the branch instruction).
- `hlt` encodes as `0x00000000`, so running into zeroed memory halts. Unused opcodes fault
  (`status=fault`, exit code 3), they do not halt. `hlt`, `wfi`, `eret`, `tlbi` and `msr`/`mrs`
  (except the flags of `pstate`, `fpcr` and `fpsr`) are privileged: in user mode they are an
  undefined instruction. Other new mnemonics: `swi [n]` (a number, not an offset), `brk [n]`; system
  registers are named after `msr`/`mrs` (`vbar`, `elr`, `spsr`, `esr`, `far`, `usp`, `fpcr`, `fpsr`,
  ...); `fpcr` and `fpsr` are the only ones besides the flags of `pstate` that user mode may access.
  Compiler helpers: `ldr xd, =value|symbol` (a pseudo instruction, no literal pool: `mov`/`mvn`, or
  `mov`+`lsl`+`orr`, or `adrp`+`add`; `Assembler::assemble_load_constant`),
  `csel`/`csinc`/`csinv`/`csneg xd, xn, xm, cond` and the aliases `cset`/`csetm xd, cond`,
  `cinc`/`cinv`/`cneg xd, xn, cond` (opcode `011011`, the condition is a bare last operand lexed by
  `is_select_condition_position`; the aliases store the opposite condition), and `sxtb sxth uxtb
  uxth clz rev rev16 xd, xn` (special group, ext op `1000`). `mov xd, imm` takes 19 bits.
- Default layout (the linker's built-in script): `.text` at 0x0, `.rodata` on the first page after
  the code with `.init_array`/`.fini_array` right after it, `.data` on the first page after those
  (0x1000 for up to 4 KiB of code and no read only data), `.bss` directly after `.data` (so it can
  share `.data`'s page). Pages are 4 KiB (`kNumPageOffsetBits = 12`). The code of a loaded program
  is not writable (a store to it faults), `.rodata` and the arrays are read only, and data is not
  executable. A page that holds sections of both gets both permissions (the linker warns about code
  sharing a page with writable data).

## Toolchain and emulator quirks
- **Errors:** library code reports an error with `AEMU_FATAL`/`AEMU_CHECK` (util/logger.h), and
  never calls `exit` itself. What happens next is the `FatalAction`: `Exit` (the default) ends the
  process, `Throw` throws `aemu::log::FatalError` (the message is already logged). `basm`, `emu32`
  and `emulator_app` set `Throw` in `main`, catch it and return a failure exit code (1), and so do
  the gtest fixtures (`ToolchainFixture`). A program that embeds the libraries should do the same. A
  test binary that does not set `Throw` is killed by the first assembler error.
- **Using the toolchain in-process:** `Build b ("args"); b.run ();` or `Build b
  (std::vector<std::string>{...})` (the constructor only parses, `b.has_work ()` is false after
  `--help`/`--version`), or the pieces: `Preprocessor (file, out_path, options)` + `preprocess ()`,
  `Assembler (bi_file, out_path)` + `assemble ()`, `Linker (objects, exe_file)` + `link ()`.
- `basm` needs at least one source file even when it's only linking libraries. `-o <name>` takes no
  extension (it produces `<name>.bexe`, or `<name>.ba` with `-ar`). Object files go to `-outdir` as
  `<src>.bo`. `-c` stops after the object files. There is no `.bi` unless you pass `-kp`. Paths are
  relative to the cwd, and the default output name is `a`.
- To run a `.bexe` in-process (this is what `emu32 -e` does): build `Emulator32bit
  (std::make_unique<RAM> (16, 0), std::make_unique<ROM> (16, 16), std::make_unique<MockDisk> ())`,
  call `mmu->begin_process ()`, then `LoadExecutable (emu, file).load ()`, then `run
  (max_instructions)`. Every vpage gets a backing disk page. Use `MockDisk` when there's no disk
  file.
- `run` returns a `RunResult` (`HALTED`/`LIMIT_REACHED`/`FAULT`/`BREAKPOINT`, the instruction count,
  the message). Whatever stops a program that is not `hlt` is a `FAULT` with the message: emulator,
  system bus, virtual memory, free block list and disk exceptions, and a `FatalError`. It logs a
  fault as a warning and a halt at debug level.
- **Transfer register == base register with pre/post-index:** a store reads `xt` before the
  writeback (so `str x1, [x1, 8]!` stores the old x1 and still updates x1). A load writes `xt` last,
  so the loaded value replaces the writeback. For pre/post-index accesses, `parse_format_m` warns
  (`Assembler::warn`, which logs and keeps assembling) when: a load targets its own base (writeback
  lost), a store's `xt` is its base (old value stored), the base is `xzr` (writeback discarded), or
  the offset is zero (writeback has no effect). Only the first applicable warning fires per
  instruction.
- **Carry flag:** subtraction/`cmp`/`sbc`/`rsc` use the ARM convention (C = *no borrow*), consistent
  with `HI`/`LS`/`HS`/`LO`. `adc`/`add` carry is the usual carry-out.
- The suites in `assembler/tests` that run a program (`EmulatorFixture`, the
  `preprocessor_test/*.cpp` files) use an in-memory `MockDisk` and a working directory of their own
  per test, so they are safe to run in parallel (`ctest -j`) and together in one gtest process. Keep
  it that way: a file shared by tests (a `disk.bin`, the default `a.bexe`) makes parallel runs
  flaky.
- The `PreprocessorUnit`, `AssemblerUnit`, `LinkerScript`, `ObjectFileUnit`, `BuildProcess`,
  `StaticLibraryBuild`, `InstructionTable` and `Relocation` suites (`assembler/tests`) mostly use
  `ToolchainFixture` (`assembler/tests/include/assembler_test/toolchain_fixture.h`). It needs no
  emulator and sets `FatalAction::Throw`, so an assembler error is an `aemu::log::FatalError` that
  the test can catch and check, instead of an `exit`. It has a scratch dir `m_dir` and `m_options`
  (a `PreprocessorOptions` whose include dir is `<m_dir>/include`). Use it for new toolchain unit
  tests.
- Logging is extremely verbose (DBG level) in the debug build, pipe it through `grep -v DBG`. The
  release builds (`AEMU_LOG_COMPILE_LEVEL` 2) have no debug or info messages at all.
