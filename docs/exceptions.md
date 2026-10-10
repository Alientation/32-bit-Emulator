# Exceptions, system registers and privilege

This describes how the CPU handles traps, interrupts and privilege: two privilege levels, a banked stack pointer, system registers, a vector table and `ERET` to return. The instruction encodings are in [isa.md](isa.md#special-instructions-opcode-000000). In assembly a number is written without `#`: `swi 3`, `brk 3`.

- A faulting instruction changes no register, so the kernel can fix the cause (demand paging, copy on write) and retry it.
- A trap needs no free register: the stack pointer is banked, so the handler has a stack of its own from its first instruction.
- A program that never installs a vector table is not affected by any of this, see [Without a vector table](#without-a-vector-table).

## Privilege

PSTATE has two bits besides the flags. Its reset value is `0x20`: kernel mode, IRQs masked.

| Bit | Name | Meaning |
|-----|------|---------|
| 0–3 | N Z C V | the condition flags |
| 4 | `U` | 1 = user mode, 0 = kernel mode. Reset: 0 |
| 5 | `I` | 1 = IRQs masked. Reset: 1 |

In **user mode**:

- `MSR`, `MRS` (except the NZCV bits of PSTATE, and `fpcr` and `fpsr`), `TLBI`, `ERET`, `WFI` and `HLT` raise [undefined instruction](#exception-classes) (ISS 2).
- `sp` is the user stack pointer, see [banked stack pointer](#banked-stack-pointer).
- With the page tables on (`SCTLR.M`), a page without the `U` bit faults (permission), and the kernel never executes a `U` page, see [mmu.md](mmu.md#permissions). The per process MMU that is used while `SCTLR.M` is 0 has its own notion of privilege (`begin_process (kernel_privilege)`) that does not follow `PSTATE.U`.

The CPU **always starts in kernel mode**, with IRQs masked. The only way into user mode is `ERET` with an `SPSR` whose `U` bit is set. A program loaded by `emu32 -e` therefore runs privileged.

`HLT` is privileged. A user program leaves through the exit system call, and the kernel (or `emu32`, for a bare program) stops the machine.

## Exception classes

An exception is **synchronous** (caused by the instruction that is executing) or an **IRQ** (an interrupt, taken between instructions).

| Vector | Class | Cause | `ELR` (resume address) |
|--------|-------|-------|------------------------|
| 0 | reserved | | |
| 1 | undefined instruction | unassigned opcode or extended op, a floating point instruction with a function that is not assigned or a double in a register that cannot start a pair, a privileged instruction in user mode, a system register that does not exist | the instruction itself |
| 2 | supervisor call | `swi` | the next instruction |
| 3 | instruction abort | fetch from an unmapped, non-executable, kernel-only or misaligned address, or from a physical address with no memory | the instruction itself |
| 4 | data abort | load, store or atomic that is unmapped, not permitted or hits a physical address with no memory, or an `ldr`, `ldrh`, `str`, `strh` or a word or half-word atomic at an address that is not a multiple of the size of the access (`ldur`, `ldurh`, `stur`, `sturh` may be unaligned) | the instruction itself |
| 5 | breakpoint | `BRK imm22` | the `BRK` itself |
| 6 | IRQ | the interrupt controller has a pending line, `PSTATE.I` is 0 and `VBAR` is set (ISS 0, FAR 0) | the next instruction to execute |
| 7 | reserved | | |

There is no exception for an arithmetic error: `UDIV`/`SDIV` by zero gives 0 ([abi.md](abi.md#division)).

Some conditions are never an exception: a failing semihosting assertion, a `FatalError` and `HLT` end the run (`status=fault` or `status=halted`, see [debugging.md](debugging.md)).

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
| undefined instruction | 0 unassigned opcode, 1 unassigned extended op or encoding, 2 privileged in user mode, 3 not available (a `swi` with no vector table and no emulator call), 4 bad system register, 5 a floating point instruction with a double in x29, `sp` or `xzr` as the first register of its pair |
| supervisor call | the 22 bit immediate of `swi` |
| instruction abort / data abort | bits 0–2 fault type: 1 translation (unmapped), 2 permission (write to read-only, execute of non-executable, kernel only), 3 alignment (a pc that is not a multiple of 4, or a data access that is not aligned to its size), 4 bus error (no such physical address). Data abort: bit 3 is set for a write |
| breakpoint | the 22 bit immediate of `BRK` |
| IRQ | 0 |

`FAR` holds the faulting virtual address for the two aborts (for an instruction abort, the pc; for an alignment fault, the address of the access). An access that crosses into a page that faults reports the first address of that page. A bus error on a data access reports the address of the access.

## System registers

`MSR sysreg, xn | imm16` and `MRS xn, sysreg` have a 5 bit register field. The numbers:

| # | Name | Access | Description |
|---|------|--------|-------------|
| 0 | | | reads 0, writes are ignored |
| 1 | `PSTATE` | kernel (NZCV also user) | the whole PSTATE. In user mode only NZCV can be written and the other bits read as 0 |
| 2 | `ELR` | kernel | resume address of the last exception |
| 3 | `SPSR` | kernel | PSTATE at the last exception. Only the bits of PSTATE that exist are kept |
| 4 | `ESR` | kernel | syndrome of the last exception |
| 5 | `FAR` | kernel | faulting address of the last abort |
| 6 | `VBAR` | kernel | vector table base, rounded down to a multiple of 16. 0 = no table installed |
| 7 | `USP` | kernel | the stack pointer of the other mode: the user one while in kernel mode |
| 8 | `PTBR` | kernel | physical address of the first level page table (low 12 bits read 0), see [mmu.md](mmu.md) |
| 9 | `SCTLR` | kernel | system control. Bit 0 (`M`) turns on translation by the page tables; the other bits read 0 |
| 10 | `FPCR` | kernel and user | floating point control: the rounding mode, bits 1–0. See [isa.md](isa.md#fpcr-and-fpsr) |
| 11 | `FPSR` | kernel and user | floating point status: the cumulative exception flags, bits 4–0 |
| 12–31 | | | reserved. The devices (timer, interrupt controller) are memory mapped, not system registers |

Reading or writing a number that is not listed is an undefined instruction with ISS 4. In assembly the registers are written by name: `msr vbar, x0`, `mrs x1, ESR`, `msr spsr, 16`.

The emulator shows them: `emu32 --format plain` prints `mode`, `pstate`, `elr`, `spsr`, `esr`, `far`, `vbar`, `fpcr` and `fpsr`, the debugger's `regs` prints them once a vector table is installed, and the trace has a line for each exception taken.

## Taking an exception

This happens as part of the instruction that caused the exception (or between two instructions, for an IRQ):

1. `ELR` = the resume address from the table above.
2. `SPSR` = PSTATE.
3. `ESR` (and `FAR` for an abort) are set.
4. PSTATE: `U` = 0, `I` = 1. NZCV is left as it is. `sp` switches to the kernel stack pointer.
5. PC = `VBAR + 16 * vector`.

Each table entry is 16 bytes, four instructions, usually a branch to the real handler. The general registers are untouched, so a handler saves what it uses on its own stack.

### Banked stack pointer

There are two stack pointers behind register number 30, the user one and the kernel one. Which `sp` an instruction sees depends on `PSTATE.U`. The kernel reads or writes the user one with `MRS`/`MSR USP`. The kernel's `sp` keeps its value between entries, so the handler runs on a stack that was set up when the kernel booted or when it last returned to user mode (the kernel stores its current `sp` itself before `ERET`).

### Returning: `ERET`

PC = `ELR`, PSTATE = `SPSR`. The mode bit comes back with `SPSR`, so the stack pointer switches back automatically. `ERET` is privileged.

### Nested exceptions and double faults

An exception taken in kernel mode is legal. It overwrites `ELR`, `SPSR` and `ESR`, so a handler that can fault again must save them first. `I` is set on entry, so there are no nested IRQs unless the handler clears it.

If an exception is raised again before any instruction ran since the last one was taken (a bad `VBAR`, a vector page that is unmapped, a handler whose first instruction faults), the emulator ends the run with `status=fault` and the message `Double fault: ... raised at <pc> before any instruction of the handler ran`, rather than looping forever. A `swi` counts as an instruction that ran, and so does a `hlt` that stops the machine.

### Interrupts: first come, first served

There are no interrupt priorities. The interrupt controller (a memory mapped device, see [devices.md](devices.md)) keeps the pending lines in a queue **in the order they were raised**, and a line that is already pending is not queued twice. Between two instructions, if the queue is not empty and `PSTATE.I` is 0, the CPU takes the IRQ exception. The handler reads the controller's claim register to get the line at the head of the queue, which removes it, and services it. An interrupt that arrives meanwhile waits behind the ones before it. A handler that clears `I` can be interrupted, but only by the next line in the queue, never by a more important one.

## Instructions

The special group (opcode `000000`) holds `ERET` (`0101`), `WFI` (`0110`) and `BRK imm22` (`0111`), see [isa.md](isa.md#special-instructions-opcode-000000). `WFI` is described in [devices.md](devices.md#wfi): with nothing that could wake it, it ends the run like `HLT`, which keeps a program from hanging. `MSR`, `MRS` and `TLBI` ([mmu.md](mmu.md#the-tlb-and-tlbi)) are in the same group.

## `swi` and the emulator calls

`swi` has the opcode `011111` and uses the otherwise unused 22 bit field of the B1 format as a number: `swi 3`. `swi` alone is `swi 0`, and it takes a condition like a branch: `swi.eq 3`. The number is unsigned and is not an offset.

- `swi 0`, and any number but 1, is a system call of the operating system: it raises the supervisor call exception with the number as the syndrome. The call number is in `x8`, the arguments in `x0`–`x5` and the result in `x0` ([abi.md](abi.md#system-calls)).
- `swi 1` is a semihosting call that the emulator handles itself, in either mode. It does not raise an exception. The call number is in `x8` and the calls (`emu_print`, `emu_assert*`, `emu_log`, `emu_error`) are listed in `software_interrupt.cpp`. They are a debugging aid, not an interface for an operating system. With `emu32 --no-semihosting` (`Emulator32bit::set_semihosting (false)`) `swi 1` is an undefined instruction.

## Without a vector table

While `VBAR` is 0 no table is installed and nothing is turned into an exception:

- What would be an exception ends the run with `status=fault`.
- `swi` and `swi 1` are emulator calls, and any other `swi` number is a fault.
- IRQs are not taken.
- `brk` is a fault, unless a debugger is attached.

A program or kernel installs a table by writing a non-zero `VBAR`, which therefore cannot be at address 0.

## Debugging

- With a debugger attached `BRK` stops the run (`Status::BREAKPOINT`, the pc is the next instruction) instead of raising the exception. `emu32 --debug` does that (`Emulator32bit::set_brk_stops`).
- The trace logs each exception taken as a line, `-- exception: data abort, ESR=0x..., ELR=0x..., FAR=0x... -> 0x<handler>`, so the sequence "fault, handler, `eret`, retry" can be read. An `eret` shows the pc it goes to.
