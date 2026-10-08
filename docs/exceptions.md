# Exceptions, system registers and privilege (design draft)

**Status: proposal, nothing here is implemented yet.** It replaces the C++ exception path (`Emulator32bit::Exception`, `VirtualMemory::PageFaultException`) and the `swi` emulator-call hack with what an operating system needs: traps the kernel can handle, a way to return from them, system registers, and two privilege levels. The current behavior is described in [isa.md](isa.md).

Goals:

1. A faulting instruction can be retried after the kernel fixes the cause (demand paging, copy on write). The emulator already guarantees that an instruction that faults changes no register, so this holds.
2. A trap needs no free register: the stack pointer is banked, so the handler has a stack of its own from its first instruction.
3. Existing programs keep working until they opt in (see [Migration](#migration)).
4. Everything fits in the spare encodings: the special group has 10 free extended ops, and no primary opcode is spent.

## Privilege

PSTATE gets two new bits (reset value `0x20`: kernel mode, IRQs masked):

| Bit | Name | Meaning |
|-----|------|---------|
| 0–3 | N Z C V | as now |
| 4 | `U` | 1 = user mode, 0 = kernel mode. Reset: 0 |
| 5 | `I` | 1 = IRQs masked. Reset: 1 |

In **user mode**:

- `MSR`, `MRS` (except the NZCV bits of PSTATE), `TLBI`, `ERET`, `WFI` and `HLT` raise [undefined instruction](#exception-classes) (ISS = privileged).
- Pages the MMU marks kernel only fault (already modeled by `set_ppage_permissions`).
- `sp` is the user stack pointer (see [banked sp](#banked-stack-pointer)).

The emulator **always starts in kernel (privileged) mode**, with IRQs masked. The only way into user mode is `ERET` with a `SPSR` whose `U` bit is set, so the kernel does it. Programs loaded by `emu32 -e` therefore run privileged, as they do now, and nothing changes for them.

`HLT` is privileged (decided). A user program leaves through the exit system call, and the kernel (or `emu32` for a bare program) is what stops the machine.

## Exception classes

An exception is **synchronous** (caused by the instruction that is executing) or an **IRQ** (an interrupt, checked between instructions).

| Vector | Class | Cause | `ELR` (resume address) |
|--------|-------|-------|------------------------|
| 0 | reserved | reset goes to the reset address, not through the table | |
| 1 | undefined instruction | unassigned opcode or extended op, a not implemented instruction (`v*`, `MRS` ...), a privileged instruction in user mode, bad register | the instruction itself |
| 2 | supervisor call | `swi` | the next instruction |
| 3 | instruction abort | fetch from an unmapped, non-executable, kernel-only or misaligned address | the instruction itself |
| 4 | data abort | load, store or atomic that is unmapped, not permitted, misaligned or hits a missing physical address | the instruction itself |
| 5 | breakpoint | `BRK #imm22` | the `BRK` itself |
| 6 | IRQ | the interrupt controller has a pending, unmasked line | the next instruction to execute |
| 7 | reserved | | |

There is no exception for an arithmetic error: `UDIV`/`SDIV` by zero gives 0 (decided, see [abi.md](abi.md#division)).

Conditions that are not a CPU exception stay what they are today: `FatalError` and a failing semihosting assertion end the run with `status=fault`.

### `ESR` (exception syndrome)

```
 31   26 25                    0
+-------+------------------------+
|  EC   |          ISS           |
+-------+------------------------+
```

`EC` is the vector number above. `ISS` depends on the class:

| EC | ISS |
|----|-----|
| undefined instruction | 0 unassigned opcode, 1 unassigned extended op, 2 privileged in user mode, 3 not implemented, 4 bad system register |
| supervisor call | the 22 bit immediate of `swi` |
| instruction abort / data abort | bits 0–2 fault type: 1 translation (unmapped), 2 permission (write to read-only, execute of non-executable, kernel only), 3 alignment, 4 bus error (no such physical address). Data abort: bit 3 is set for a write |
| breakpoint | the 22 bit immediate of `BRK` |

`FAR` holds the faulting virtual address for the two aborts (for an instruction abort, the PC).

## System registers

`MSR sysreg, xn | imm16` and `MRS xn, sysreg` already have a 5 bit register field. The numbers:

| # | Name | Access | Description |
|---|------|--------|-------------|
| 0 | | | reads 0, writes ignored |
| 1 | `PSTATE` | kernel (NZCV also user) | the whole PSTATE (`kSysregId_pstate`). In user mode only NZCV can be written and the other bits read as 0 |
| 2 | `ELR` | kernel | resume address of the last exception |
| 3 | `SPSR` | kernel | PSTATE at the last exception |
| 4 | `ESR` | kernel | syndrome of the last exception |
| 5 | `FAR` | kernel | faulting address of the last abort |
| 6 | `VBAR` | kernel | vector table base. 0 = no table installed (see [Migration](#migration)) |
| 7 | `USP` | kernel | the user mode stack pointer (see below) |
| 8 | `PTBR` | kernel | page table base, reserved for the MMU design |
| 9 | `SCTLR` | kernel | system control, reserved (bit 0: MMU enable) |
| 10–31 | | | reserved. Devices (timer, interrupt controller) are memory mapped, not system registers |

Reading or writing a number that is not listed is an undefined instruction with ISS = 4 (today `MSR` throws `BAD_REG`).

## Taking an exception

Everything that follows happens as part of the instruction that caused it (or between two instructions, for an IRQ):

1. `ELR` = the resume address from the table above.
2. `SPSR` = PSTATE.
3. `ESR` (and `FAR` for an abort) are set.
4. PSTATE: `U` = 0, `I` = 1. NZCV is left as it is. `sp` switches to the kernel stack pointer.
5. PC = `VBAR + 16 * vector`.

Each table entry is 16 bytes, four instructions, usually a branch to the real handler. `VBAR` has to be 16 byte aligned (it is rounded down).

The 31 general registers are untouched, so a handler saves what it uses on its own stack, which is why `sp` is banked.

### Banked stack pointer

There are two stack pointers behind register number 30: the user one and the kernel one. Which `sp` an instruction sees depends on `PSTATE.U`. The kernel reads or writes the user one with `MRS`/`MSR USP`. The kernel's `sp` simply keeps its value between entries, so the handler runs on a stack that was set up when the kernel booted or when it last returned to user mode (it stores the current kernel `sp` itself before `ERET`).

### Returning: `ERET`

PC = `ELR`, PSTATE = `SPSR`. Because the mode bit comes back with `SPSR`, the stack pointer switches back automatically. The encoding is a new extended op of the special group (kernel only).

### Nested exceptions and double faults

An exception taken in kernel mode is legal, and it overwrites `ELR`, `SPSR` and `ESR`, so a handler that can fault again must save them first. `I` is set on entry, so there are no nested IRQs unless the handler clears it.

### Interrupts: first come, first served

There are no interrupt priorities. The interrupt controller (a memory mapped device, specified with the other devices) keeps the pending lines in a queue **in the order they were raised**, and a line that is already pending is not queued twice. Between two instructions, if the queue is not empty and `PSTATE.I` is 0, the CPU takes the IRQ exception. The handler asks the controller for the line at the head of the queue (reading its claim register removes it) and services it. An interrupt that arrives meanwhile waits its turn behind the ones before it. A handler that clears `I` can be interrupted, but only by the next line in the queue, never by a "more important" one.

If an exception is raised again at the same PC with no instruction retired in between (a bad `VBAR`, a vector page that is unmapped), the emulator ends the run with `status=fault`, message `double fault at <pc>`, rather than looping forever.

## New instructions

All of them live in the special group (opcode `000000`), whose extended ops `0101`–`1110` are free:

| ext. op | Instruction | Description |
|---------|-------------|-------------|
| `0101` | `ERET` | return from an exception (kernel only) |
| `0110` | `WFI` | wait for interrupt: the emulator stops fetching until an IRQ is pending (kernel only). With no interrupt source it ends the run, like `HLT`, so a program cannot hang |
| `0111` | `BRK #imm22` | raise the breakpoint exception, see [Debugging](#debugging) |

`swi` keeps its opcode (`110001`) and gets an operand in the otherwise unused `simm22` field: `swi #imm22`. The assembler currently takes no operand, and `imm22` = 0 is what it emits today.

`MSR`/`MRS`/`TLBI` keep their encodings; the only change is that they work. The assembler needs names for the system registers (`msr vbar, x0`, `mrs x1, esr`).

## `swi` and the emulator calls

The `emu_*` calls (`emu_print`, `emu_assert*`, `emu_log`, `emu_error`) are a debugging aid and an OS is not going to use them. They become **semihosting**:

- `swi #0` is the system call of the OS: it raises the supervisor call exception. The call number is in `x8` and the arguments in `x0`–`x5`, the result in `x0` ([abi.md](abi.md#system-calls)).
- `swi #1` is a semihosting call, handled by the emulator itself (the number is in `x8` as today). It does not raise an exception. With `emu32 --no-semihosting`, or when it is not allowed, it is an undefined instruction.

## Migration

An OS-less program keeps working: while `VBAR` is 0 no table is installed, and the emulator behaves as it does now. An exception is a `status=fault` that stops the run, and a plain `swi` (immediate 0) is treated as a semihosting call. A program or kernel opts in by writing a non-zero `VBAR`.

(The vector table can then not be at address 0. That is fine, RAM starts with the reset code anyway.)

## Debugging

- A `BRK` stops the emulator instead of raising the exception when a debugger is attached (`emu32 --debug`, see [debugging.md](debugging.md)); `ELR`-style resume works because the debugger continues at the next instruction.
- The trace and the instruction history log an exception as a line (`-- exception: data abort, FAR=0x..., vector 4`), so the sequence "fault, handler, `ERET`, retry" is readable.

## Implementation steps

1. PSTATE bits `U` and `I`, `ESR/ELR/SPSR/FAR/VBAR/USP` and a real `MSR`/`MRS` (`emulator32bit.h`, `instructions.cpp`), the encoders and disassembler entries for `ERET`, `WFI`, `BRK`.
2. `run()`: catch `Exception` and `PageFaultException` where they are thrown for an instruction, turn them into `take_exception(class, iss, far)` when `VBAR != 0`. `HALT_INSTR` and `FatalError` stay as they are. Keep the "no register changes on a fault" rule for all instruction handlers.
3. Banked `sp`.
4. Privilege checks, with `ISS = 2`.
5. Assembler: `swi #imm`, system register names, the three new mnemonics (`BASM_INSTRUCTION_LIST`).
6. Semihosting switch (`swi #1`, `--no-semihosting`).
7. Tests, one family at a time: entry and `ERET` round trip, each class, privilege, double fault, retry after a page fault. The `basm` integration tests can run a small kernel stub that installs a vector table.
8. Update `isa.md`, `CLAUDE.md`, and keep the `tlbi_test`, `swi_test` and `swp_test` expectations in step.
