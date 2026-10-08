# Debugging programs

Four tools in `emu32`, all off by default and free when unused:

| Option | What it does |
|--------|--------------|
| `--trace <file>` (`-t`) | writes a line per executed instruction (`-` for stdout) |
| `--history N` | keeps the last N instructions and prints them after the run |
| `--break <addr\|symbol>,...` | stops before the instruction at those addresses |
| `--debug` | interactive debugger on stdin/stdout |

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

## Debugger

`emu32 -e prog.bexe --debug` (`--limit` is ignored). An address is a number, a symbol, or `symbol+number`.

| Command | |
|---------|-|
| `step [n]` (`s`) | execute n instructions (default 1) and show the next one |
| `continue [limit]` (`c`) | run until a breakpoint, the halt or a fault |
| `break [addr]` (`b`) | breakpoint at the address (default: the current pc) |
| `delete <addr\|all>` (`d`) / `breaks` | remove / list breakpoints |
| `regs` (`r`) | registers and flags |
| `mem <addr> [len]` (`x`) | hex dump, default 16 bytes. Unreadable bytes are `??` |
| `disasm [addr] [n]` (`dis`) | n instructions (default 8), `=>` marks the pc |
| `history` (`hist`) | the last executed instructions |
| `backtrace` (`bt`) | the call chain, see [below](#backtrace) |
| `set <reg\|pc> <value>` | change a register or the pc (a finished program can run again after `set pc`) |
| `help`, `quit` | |

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

Two limits, both from how the records are made:

- A function has no record before its prologue has run. When the pc is exactly at a symbol the debugger assumes that is the first instruction of a function and takes frame 1 from the link register (`x29`). A breakpoint on a label inside a function therefore shows one extra, wrong, frame.
- Hand written leaf functions that skip the record are not shown.

## Not there yet

- Watchpoints (stop when an address is written). They need a hook in `SystemBus`.
- A gdb remote stub, for use from an IDE.
- `BRK` ([exceptions.md](exceptions.md#debugging)), so a program can stop itself.
