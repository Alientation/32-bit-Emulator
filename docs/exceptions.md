# Exceptions, system registers and privilege

**Status: implemented**, except what is listed under [Not done yet](#not-done-yet): interrupts (IRQs). The page tables are in [mmu.md](mmu.md). It replaces the C++ exception path (`Emulator32bit::Exception`, `VirtualMemory::PageFaultException`) and the `swi` emulator-call hack with what an operating system needs: traps the kernel can handle, a way to return from them, system registers, and two privilege levels. The instruction encodings are in [isa.md](isa.md#special-instructions-opcode-000000). In assembly a number is written without `#`: `swi 3`, `brk 3`.

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
- `sp` is the user stack pointer (see [banked sp](#banked-stack-pointer)).
- With the page tables on (`SCTLR.M`), a page without the `U` bit faults (permission) in user mode, and the kernel never executes a `U` page, see [mmu.md](mmu.md#permissions). (The older per process MMU, used while `SCTLR.M` is 0, has its own notion of privilege (`begin_process (kernel_privilege)`) that does not follow `PSTATE.U`.)

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
| 4 | data abort | load, store or atomic that is unmapped, not permitted or hits a missing physical address. (Unaligned data accesses are allowed, only the pc has to be aligned) | the instruction itself |
| 5 | breakpoint | `BRK imm22` | the `BRK` itself |
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

`FAR` holds the faulting virtual address for the two aborts (for an instruction abort, the PC). An access that crosses into a page that faults reports the first address of that page. A bus error on a data access reports the address of the access.

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
| 8 | `PTBR` | kernel | physical address of the first level page table (low 12 bits read 0), see [mmu.md](mmu.md) |
| 9 | `SCTLR` | kernel | system control. Bit 0 (`M`) turns on translation by the page tables; the other bits read 0 |
| 10–31 | | | reserved. Devices (timer, interrupt controller) are memory mapped, not system registers |

Reading or writing a number that is not listed is an undefined instruction with ISS = 4. `SPSR` keeps the bits of PSTATE that exist, and `VBAR` is rounded down to a multiple of 16. In assembly the registers are written by name in any case: `msr vbar, x0`, `mrs x1, ESR`, `msr spsr, 16`.

The emulator shows them: `emu32 --format plain` prints `mode`, `pstate`, `elr`, `spsr`, `esr`, `far` and `vbar`, the debugger's `regs` prints them once a vector table is installed, and the trace has a line for each exception taken.

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

If an exception is raised again before any instruction ran since the last one was taken (a bad `VBAR`, a vector page that is unmapped, a handler whose first instruction faults), the emulator ends the run with `status=fault`, message `Double fault: ... raised at <pc> before any instruction of the handler ran`, rather than looping forever. A `swi` counts as an instruction that ran, and so does a `hlt` that stops the machine.

## New instructions

All of them live in the special group (opcode `000000`), whose extended ops `0101`–`1110` are free:

| ext. op | Instruction | Description |
|---------|-------------|-------------|
| `0101` | `ERET` | return from an exception (kernel only) |
| `0110` | `WFI` | wait for interrupt (kernel only). There is no interrupt source yet, so for now it ends the run like `HLT`, which keeps a program from hanging |
| `0111` | `BRK imm22` | raise the breakpoint exception, see [Debugging](#debugging) |

`swi` keeps its opcode (`110001`) and uses the otherwise unused 22 bit field of the B1 format as a number: `swi 3` (`swi` alone is `swi 0`, and it takes a condition like a branch: `swi.eq 3`). The number is unsigned and is not an offset.

`MSR`/`MRS` keep their encodings and work now. `TLBI` works now, see [mmu.md](mmu.md#the-tlb-and-tlbi).

## `swi` and the emulator calls

The `emu_*` calls (`emu_print`, `emu_assert*`, `emu_log`, `emu_error`) are a debugging aid and an OS is not going to use them. They become **semihosting**:

- `swi 0` (and any number but 1) is the system call of the OS: it raises the supervisor call exception, with the number as syndrome. The call number is in `x8` and the arguments in `x0`–`x5`, the result in `x0` ([abi.md](abi.md#system-calls)).
- `swi 1` is a semihosting call, handled by the emulator itself (the number is in `x8`, the calls are listed in `software_interrupt.cpp`). It does not raise an exception, in either mode. With `emu32 --no-semihosting` (`Emulator32bit::set_semihosting (false)`) it is an undefined instruction.

## Migration

An OS-less program keeps working: while `VBAR` is 0 no table is installed, and the emulator behaves as before. An exception is a `status=fault` that stops the run, `swi` and `swi 1` are emulator calls, and any other `swi` number is a fault. A program or kernel opts in by writing a non-zero `VBAR`.

(The vector table can then not be at address 0. That is fine, RAM starts with the reset code anyway.)

## Debugging

- A `BRK` stops the run (`Status::BREAKPOINT`, the pc is the next instruction) instead of raising the exception when a debugger is attached: `emu32 --debug` does that (`Emulator32bit::set_brk_stops`). Without a debugger and without a vector table, `brk` is a fault.
- The trace logs an exception taken as a line (`-- exception: data abort, ESR=0x..., ELR=0x..., FAR=0x... -> 0x<handler>`), so the sequence "fault, handler, `eret`, retry" is readable, and an `eret` shows the pc it goes to.

## Not done yet

- **Interrupts.** Class 6 (IRQ) is reserved. There is no interrupt controller, and `WFI` halts. The first come, first served queue above is the design for it.
- (`TLBI`, `PTBR`, `SCTLR` and kernel-only pages by privilege level are done: see [mmu.md](mmu.md). The old per-process MMU used while `SCTLR.M` is 0 still has its own kernel-only notion.)
- **Alignment faults for data accesses** (only the pc is checked).
