# Debugging programs

Seven tools in `emu32`, all off by default and free when unused:

| Option | What it does |
|--------|--------------|
| `--trace <file>` (`-t`) | writes a line per executed instruction (`-` for stdout) |
| `--history N` | keeps the last N instructions and prints them after the run |
| `--break <addr\|symbol>,...` | stops before the instruction at those addresses |
| `--watch <addr\|symbol>[:len][:r\|w\|rw][:p][:<op><value>],...` | stops after an instruction that accesses those bytes (`p`: physical addresses; `<op><value>`: only for that value) |
| `--watch-reg <reg>[=value],...` | stops after an instruction changes a register (to that value) |
| `--debug` | interactive debugger on stdin/stdout |
| `--gdb <port>` | waits for gdb (or an IDE) on `127.0.0.1:<port>`, see [below](#gdb) |

Symbols come from the `.bexe` loaded with `-e`. A local label shows up under its plain name, so a name that two files both use as a local label refers to the first one.

## Trace

```
0x00000000 <_start>: add x0, xzr, 5 ; x0=0x0->0x5
0x00000004 <_start+0x4>: bl 3 ; x29=0x0->0x8 pc=0x10
0x00000010 <double>: add x0, x0, x0 ; x0=0x5->0xa
0x00000014 <double+0x4>: ret ; pc=0x8
0x0000000c <_start+0xc>: hlt ; halt
```

After the `;`: every register whose value changed (`name=old->new`), the flags if they changed (`NZCV=nzcv->nZcv`, upper case is set), `pc=target` for a taken branch, or the reason when the instruction did not complete (`halt`, or the fault message). Stores to memory are not shown, use `mem` in the debugger. A fault while *fetching* an instruction has no instruction to show, it is only in the run result.

## History

`--history 8` prints, after the run (in `--format plain` as `history[i]=<pc> <word> <assembly> <symbol>` lines, oldest first), the last 8 instructions. If the run ended in a fault, the last one is the instruction that faulted. The debugger turns it on (32 entries) when it is off.

## Breakpoints

`--break main,0x40` runs until the PC is at one of the addresses, with `status=breakpoint` and exit code 4. The instruction at the address has not executed. From the library: `Emulator32bit::add_breakpoint`, `remove_breakpoint`, `clear_breakpoints`; `run()` returns `RunResult::Status::BREAKPOINT`. The first instruction of a `run()` never stops, so calling `run()` again continues, and `run(1)` is a single step.

Breakpoints are virtual addresses, like the PC.

## Watchpoints

`--watch counter` (or `--watch 0x1000:4:rw`) runs until an instruction writes the watched bytes, with `status=breakpoint` and exit code 4. After the address come, in any order, an optional length in bytes (default 1), an optional kind, `r` (reads), `w` (writes, the default) or `rw`, `p` for a physical address and a condition on the value. The message says what happened: `Watchpoint 0x00001000: write of 0x5 (4 bytes) at 0x00001000 by the instruction at 0x00000010, the old value was 0x3`. From the library: `Emulator32bit::add_watchpoint (address, length, kind, physical, compare, compare_value)`, `remove_watchpoint`, `clear_watchpoints`; the result is `Status::BREAKPOINT` as for a breakpoint.

- **Condition.** `--watch counter:==5` stops only when the value of the access is 5: the number that was stored, or that was loaded. The operators are `==`, `!=`, `<`, `<=`, `>`, `>=`, compared as unsigned numbers; the value of an access is its bytes, so a `strb` of 0x1FE is the value 0xFE. An access that does not match does nothing, and is written to the memory as usual. In the debugger: `watch counter 4 w == 5`.
- **Physical addresses.** `--watch 0x14000:4:p` watches the memory at that physical address, whatever virtual address (or page table, or process) reaches it, and the address in the message is the virtual one of the access. A symbol is a virtual address, so a physical watch needs a number. In the debugger: `watch 0x14000 4 p` and `unwatch 0x14000 p`.

- The instruction **has completed** (a store has written its memory, a pre/post-indexed base is updated) and the pc is at the next instruction, like a hardware watchpoint. So the first `run()` after a hit goes on without the watchpoint stopping it again.
- Any overlap counts: a `strb` to byte 2 of a watched word is a hit. A watch is on virtual addresses (the address the instruction computed) unless it has `p`; with the MMU on, another virtual address of the same page is not seen by a virtual watch.
- Only loads, stores and atomics count. Instruction fetches, the page table walker, device activity, `swi` emulator calls (which write memory for the host) and the debugger's own `mem` do not. An atomic is a read and a write; when both match, the write is reported.
- An access that faults does not count, since it did nothing.
- A watchpoint at an address that is already watched replaces the old one (one per start address and kind of address, virtual or physical).
- The value reported is the one loaded or stored (the low bytes for `strb`/`strh`). A write also says what the bytes held before, read just before the store (an atomic knows it already). A load has no old value.
- It costs one test per memory instruction while none is set, so it is free when unused. With a watch set the stores take a path of their own (`store_watched`).

### Register watches

`--watch-reg x28` runs until `x28` has a different value, `--watch-reg sp=0x1000` until `sp` becomes 0x1000 (`x0`-`x29` and `sp`). The message is `Register watch x28: 0x2000 -> 0x1ff8, next instruction at 0x00000024`, with `status=breakpoint` and exit code 4. From the library: `add_register_watch (reg, optional value)`, `remove_register_watch`, `clear_register_watches`, `register_watches ()`.

- It watches for a **change**: writing the value the register already has does nothing. There are no read watches for registers.
- The registers are compared between instructions, so an exception entry (which switches the banked `sp`) or an `eret` is noticed too, and the check is free while no register is watched. A change that the debugger itself makes with `set` between runs is not reported.
- If one instruction changes two watched registers, one is reported and the other the next time.

## Debugger

`emu32 -e prog.bexe --debug` (`--limit` is ignored). An address is a number, a symbol, or `symbol+number`.

| Command | |
|---------|-|
| `step [n]` (`s`) | execute n instructions (default 1) and show the next one |
| `continue [limit]` (`c`) | run until a breakpoint, the halt or a fault |
| `break [addr]` (`b`) | breakpoint at the address (default: the current pc) |
| `delete <addr\|all>` (`d`) / `breaks` | remove / list breakpoints |
| `watch <addr> [len] [r\|w\|rw] [p] [<op> <value>]` (`w`) | stop after a write (default), read or either of those bytes; `p`: physical address; `<op> <value>`: only for a value that compares so (`== 5`) |
| `unwatch <addr\|all> [p]` / `watches` | remove / list watchpoints (and register watches) |
| `watchreg <reg> [value]` / `unwatchreg <reg\|all>` | stop after a register changes (to the value) / remove |
| `regs` (`r`) | registers and flags |
| `mem <addr> [len]` (`x`) | hex dump, default 16 bytes. Unreadable bytes are `??` |
| `disasm [addr] [n]` (`dis`) | n instructions (default 8), `=>` marks the pc |
| `history` (`hist`) | the last executed instructions |
| `backtrace` (`bt`) | the call chain, see [below](#backtrace) |
| `set <reg\|pc> <value>` | change a register or the pc (a finished program can run again after `set pc`) |
| `help` (`h`), `quit` (`q`) | |

After a fault the debugger prints the message and the last 8 instructions. The registers and memory can still be inspected. The exit code is 3 if the program faulted and 0 otherwise.

The code is in `emulator32bit/debugger.h`, which takes the input and output streams so it can be tested.

## Backtrace

```
(dbg) bt
#0  0x00000014 <inner+0x4>
#1  0x00000044 <outer+0x4>
#2  0x00000008 <_start+0x8>
```

Frame 0 is the pc. The other frames come from the frame records of the [ABI](abi.md#frame): `x28` points at a record whose lower word is the caller's `x28` and whose upper word is the return address, and the chain ends at `x28 = 0`. It stops with a message, instead of guessing, when a frame pointer is not word aligned, cannot be read, or does not point higher in memory than the one before it (the stack grows down), and after 64 frames.

In an exception handler (between taking an exception and the `ERET`) `bt` says which one, with ESR and, for an abort, FAR, and shows the code that was interrupted as the next frame, from `ELR`. `Emulator32bit::exception_depth ()` counts the exceptions taken and not returned from; `ELR`, `ESR` and `FAR` are those of the last one, so with nested exceptions the outer ones are not shown:

```
(dbg) bt
#0  0x00000824 <handler+0x4>
    in the handler of a data abort: ESR=0x10000001, FAR=0x00002000, the code that was interrupted:
#1  0x00000020 <func+0x8>
#2  0x00000008 <_start+0x8>
```

The handler is assumed not to have a frame record of its own (an assembly stub that leaves `x28` alone), so that the records that follow are those of the interrupted code. A handler that makes its own record shows the interrupted frame before its own callers.

Three limits, the first two from how the records are made:

- A function has no record before its prologue has run. When the pc is exactly at a symbol the debugger assumes that is the first instruction of a function and takes frame 1 from the link register (`x29`). A breakpoint on a label inside a function therefore shows one extra, wrong, frame.
- Hand written leaf functions that skip the record are not shown.
- Exceptions inside exceptions: only the last one is described.

## Limits

- A watchpoint condition looks at the value of one access, not at the memory: a `strb` into a watched word compares the byte. There is no condition on a register, or on several conditions at once.
- Watchpoints stop after the access; there is no stop before it.

## gdb

`emu32 -e prog.bexe --gdb 1234` loads the program, does **not** start it, and waits for a client of the GDB remote serial protocol on `127.0.0.1:1234` (the port can be 0 for any free one: the line `gdb: listening on 127.0.0.1:<port>` says which). It serves one client, and ends when the client detaches (`detach`), kills the program or hangs up. It cannot be combined with `--debug` or `--console-stdin`. The server is `GdbServer` (`gdb_server.h`), whose `handle ()` takes a packet and returns the reply, so it is tested without a socket; the TCP part is POSIX only.

```
$ gdb-multiarch -nx -ex 'target remote 127.0.0.1:1234'
(gdb) break *0x10          # a breakpoint at an address: symbols of the .bexe are not known to gdb
(gdb) continue
(gdb) info registers r0 pc cpsr
(gdb) stepi
(gdb) watch *(int*)0x1000
(gdb) x/4wx 0x1000
(gdb) monitor disasm 0x0 4   # the instructions as the assembler writes them
```

- **Registers.** gdb has no architecture for this processor, so the server describes the ARM core registers, which gdb accepts with more registers added: `r0`-`r12` are `x0`-`x12`, `sp` is `sp`, `lr` is `x29`, `pc` is `pc`, `cpsr` is the flags in the bits of an ARM one (N, Z, C, V in bits 31-28, the mode in bits 4-0, the IRQ mask in bit 7; only the flags can be written), and `x13`-`x28` follow as registers of their own.
- **Not ours: the disassembly and the symbols.** `x/i` and the frames of gdb decode the instructions as ARM and know no symbol of the `.bexe`, so use `monitor disasm [address [count]]` for the instructions and the addresses of `basm -dump`. `bt` of gdb does not follow the frame records either (the `--debug` debugger does).
- **Breakpoints and watchpoints** (`Z0`-`Z4`) are those of `--break` and `--watch`: a breakpoint stops before the instruction, a watchpoint after the access, in virtual addresses, without a condition. gdb takes it that this kind of processor stops *before* the access and steps over it after the hit; the server reports the hit, and the step that follows it does not run anything, because the instruction has run already. `brk` in the program stops it like a breakpoint.
- **Stops.** A breakpoint or a watchpoint or a step is `SIGTRAP`, a fault of the program `SIGSEGV` (the message is not sent), the halt (`hlt`) is the exit of the program (`W00`), Ctrl-C of gdb is `SIGINT`: the program runs in slices of 20000 instructions and looks at the client between them.
- **Memory** is read and written with the permissions of the host: `x` works on any mapped address, and a write to the code of a loaded program (not writable by the program) is refused by the machine, so `set *(int*)0x0 = 1` fails with an error.
- Not supported: threads (there is one), `X` (binary writes, gdb falls back to `M`), `qSymbol`, tracepoints, the registers of the floating point (they are the integer ones), `vRun` (the program is started by `emu32`).

## `brk`

A program can stop itself with the `brk [n]` instruction. Under `--debug` (or when a library user calls `Emulator32bit::set_brk_stops (true)`) the run stops with `Status::BREAKPOINT`, the message `brk n at <pc>`, and the pc at the instruction after the `brk`, so `continue` goes on. Without a debugger it raises the breakpoint exception when a vector table is installed ([exceptions.md](exceptions.md#debugging)), and is a fault otherwise.

## Exceptions

With a vector table installed the trace shows each exception taken (`-- exception: data abort, ESR=0x..., ELR=0x..., FAR=0x... -> 0x<handler>`) and the `pc=` of an `eret`; `regs` shows the mode, whether IRQs are masked, and the exception registers.
