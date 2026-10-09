# Devices, interrupts and the boot sequence

The machine has four memory mapped devices: an interrupt controller, a timer, a console and a block device. An operating system uses them to take turns between processes (timer), to wait without spinning (`WFI`), to talk to the user (console) and to keep data (block device). They are reached with ordinary loads and stores, so the kernel maps their pages like any other memory (see [mmu.md](mmu.md)).

## Memory map

Physical addresses:

| Range | What |
|-------|------|
| `0x00000000` and up | RAM. By default `emu32` has 32 pages (128 KiB) from page 0 |
| ROM | a read only image, by default 32 pages from page 32 (`0x20000`), `--rom-start`, `--rom-pages`, `--rom-file` |
| disk | optional, `--disk-start`, `--disk-pages`, `--disk-file` |
| `0xF0000000` | **interrupt controller**, one page |
| `0xF0001000` | **timer**, one page |
| `0xF0002000` | **console**, one page |
| `0xF0003000` | **block device**, one page |

A store by a program to the ROM is a bus error as well (nothing is written; a store that crosses into the ROM from the page before writes nothing either). The host can still write the image (the loader's physical sections, `ROM::write_*` in a test).

Memory may not reach `0xF0000000` (the machine refuses the layout). Another address in the window (`0xF0004000` ...) is a bus error (a data abort, ISS 4, with a vector table). A device register is a word. A byte or half-word **read** gives that part of the word, and a byte or half-word **write** writes the register with the value in that position, so `strb x1, [console]` sends one byte. A register that has a side effect on read (the interrupt controller's CLAIM, the console's DATA) does it once per access.

Programs without page tables (`SCTLR.M` = 0, what `emu32 -e` runs) reach the devices at these addresses directly, the swapping memory leaves addresses from `0xF0000000` alone.

## Interrupts

An IRQ is **first come, first served**, there are no priorities:

1. A device (or software) *raises a line* (0-31). A line that is not enabled is ignored, one that is waiting already keeps its place and is not queued twice.
2. Between two instructions, if the queue is not empty, `PSTATE.I` is 0 and a vector table is installed (`VBAR != 0`), the CPU takes the **IRQ exception**, vector 6, `VBAR + 96`. `ELR` is the instruction that would have run, so `ERET` resumes exactly there. `I` is set on entry, so the handler is not interrupted unless it clears `I`.
3. The handler reads CLAIM, which returns the line at the head of the queue and removes it, and serves that device. If more lines are waiting it may claim again, or return and be interrupted again at once (the queue is still not empty).

Without a vector table nothing is ever delivered (the queue just fills), which keeps programs that do not know about interrupts unchanged. Lines: 0 timer, 1 console receive, 2 block device.

### Interrupt controller (`0xF0000000`)

| Offset | Name | Access | |
|--------|------|--------|---|
| `0x00` | CLAIM | read | the line at the head of the queue, and remove it. `0xFFFFFFFF` if the queue is empty |
| `0x04` | COUNT | read | the number of lines in the queue |
| `0x08` | ENABLE | r/w | bit *n*: line *n* can be raised. Reset: 0, so a kernel enables what it uses |
| `0x0C` | RAISE | write | raises the line with that number: a software interrupt |
| `0x10` | PENDING | read | bit *n*: line *n* is in the queue |

### Timer (`0xF0001000`)

The timer counts **retired instructions**, not host time, so a run is the same every time and a debugger session does not change when it fires.

| Offset | Name | Access | |
|--------|------|--------|---|
| `0x00` | COUNT | r/w | instructions since reset (or the value written) |
| `0x04` | COMPARE | r/w | the interrupt is raised when COUNT becomes equal to this |
| `0x08` | CTRL | r/w | bit 0: enabled. bit 1: periodic |
| `0x0C` | INTERVAL | r/w | when periodic, COMPARE grows by this each time. 0 stops it |

A timer that is not periodic disables itself when it fires. The match is for equality, so write COMPARE as COUNT plus the wait. A periodic tick every N instructions: `COMPARE = COUNT + N`, `INTERVAL = N`, `CTRL = 3`.

### Console (`0xF0002000`)

| Offset | Name | Access | |
|--------|------|--------|---|
| `0x00` | DATA | r/w | write: send the low byte. Read: take the next received byte (0 if none) |
| `0x04` | STATUS | read | bit 0: a byte is waiting. bit 1: ready to send (always 1) |
| `0x08` | CTRL | r/w | bit 0: raise line 1 when a byte is received |

Output goes to the console stream (standard output for `emu32`; `Console::set_output` in a test). Input is queued by the host: `emu32 --console-input FILE` queues the bytes of the file at start, `Console::push_input` does it from C++. With CTRL bit 0 set, each byte that arrives raises line 1 (and enabling it while bytes wait raises it).

### Block device (`0xF0003000`)

A disk of **512 byte sectors** for the operating system. It is used either one sector at a time through a buffer inside the device and a data register (programmed I/O), or by **DMA**, many sectors straight to and from physical RAM. A command takes `LATENCY` retired instructions (per sector for DMA), like the timer counts, and then the status says so and line 2 is raised if enabled.

| Offset | Name | Access | |
|--------|------|--------|---|
| `0x00` | COMMAND | write | 1 read the sector into the buffer, 2 write the buffer to the sector, 3 flush the disk to its file, 4 DMA read, 5 DMA write |
| `0x20` | DMA_ADDR | r/w | physical RAM address of a DMA transfer, a multiple of 4 |
| `0x24` | DMA_COUNT | r/w | the number of sectors of a DMA transfer, at least 1 |
| `0x04` | STATUS | r/w | bit 0 busy, bit 1 done, bit 2 error. A write clears done and error |
| `0x08` | SECTOR | r/w | the sector number of the next command |
| `0x0C` | DATA | r/w | the word of the buffer at CURSOR, and CURSOR moves on by 4. **Use word accesses** |
| `0x10` | CURSOR | r/w | byte offset into the buffer (a multiple of 4, below 512). Set to 0 when a read completes |
| `0x14` | CAPACITY | read | the number of sectors, 0 when there is no disk |
| `0x18` | CTRL | r/w | bit 0: raise line 2 when a command completes |
| `0x1C` | LATENCY | r/w | instructions a command takes, at least 1. Reset: 100 |

A driver reads a sector like this: write SECTOR, write COMMAND = 1, wait (poll STATUS bit 0, or enable the interrupt and `wfi`), write STATUS to acknowledge, then read DATA 128 times. To write: write CURSOR = 0, write DATA 128 times, SECTOR, COMMAND = 2, wait. The buffer is copied when the command is given, so it can be reused at once.

**DMA:** write SECTOR (the first), DMA_ADDR and DMA_COUNT, then COMMAND = 4 (disk to RAM) or 5 (RAM to disk), and wait as above. The sectors `SECTOR .. SECTOR + COUNT - 1` go to or from the `COUNT * 512` bytes at DMA_ADDR, and the buffer and cursor of the data register are not touched. The three registers are copied when the command is given.

- The data moves **when the command completes** (after `COUNT * LATENCY` instructions), not at the start. So a DMA write reads RAM at that moment, and a DMA read changes RAM then: the kernel has to keep the buffer where it is, and not use it, until done. Since DMA uses physical addresses, a kernel with page tables passes the frame's physical address and must not have the page swapped or moved meanwhile.
- DMA reaches **RAM only** (not ROM, the memory mapped disk or devices). It does not trigger memory watchpoints, a debugger's `mem` sees the result.
- It completes with error, and moves nothing, if COUNT is 0, DMA_ADDR is not word aligned, any byte of the range is outside the RAM, or a sector is past CAPACITY.
- A command given while busy, or one that is not 1-5, is ignored and sets error. A sector past CAPACITY completes with error and changes nothing.
- A reset keeps the contents of the disk (it resets the controller: cursor, sector, control, latency, buffer, DMA registers).
- Without `--block-file` or `--block-sectors` the disk has 0 sectors. `emu32 --block-sectors N` is a disk of N sectors in memory, `--block-file FILE` keeps it in a file (made, with `--block-sectors`, if it does not exist; its size in sectors otherwise, or more if `--block-sectors` says so). The file is written on FLUSH and when the machine ends.
- This is separate from `--disk-file`, the memory mapped disk that the swapping memory (`SCTLR.M` = 0) uses.

## `WFI`

`WFI` (privileged) returns as soon as an interrupt is pending, even a masked one: the next instruction runs, and takes the interrupt if it is not masked. Nothing else runs while the CPU waits, so the only things that can produce an interrupt are the timer and a block device command in progress, and `WFI` **jumps** to the nearer of the two moments: time passes at once, the device finishes and raises its interrupt. A kernel's idle loop is `wfi` and then the interrupt handling, and costs one instruction however long the wait is. With nothing that could ever wake it (no pending interrupt, the timer off), `WFI` ends the run like `HLT` instead of hanging, with the message `WFI with no interrupt source`.

## Boot sequence

At reset (`Emulator32bit::reset`):

| State | Value |
|-------|-------|
| mode | kernel, IRQs masked (`PSTATE = 0x20`) |
| `PC` | 0 (RAM) unless set: `emu32 --pc ADDRESS`, `set_pc` |
| registers, `sp` | 0 |
| `VBAR`, `PTBR`, `SCTLR` | 0: no vector table, translation off |
| devices | all registers 0, the interrupt queue empty, every line disabled |
| RAM | zeroed. A ROM keeps its image |

Code runs from RAM **or ROM**: a machine boots from its ROM by starting there (`emu32 --rom-file boot.bin --rom-start 32 --pc 0x20000`). A boot ROM does what a loader would:

1. Set `sp`. Nothing is mapped, addresses are physical.
2. Copy the kernel from the ROM or the disk to RAM (or run it in place).
3. Build the page tables for the kernel and install the vector table ([mmu.md](mmu.md#turning-it-on)): `msr vbar`, `msr ptbr`, `msr sctlr, 1`.
4. Enable the timer's line in the interrupt controller, set the timer, clear `PSTATE.I`.
5. `ERET` to the first user process (it sets `SPSR` with the `U` bit and `ELR` itself), the kernel stack in `sp` and the user stack in `USP`.

The linker script places the kernel at physical addresses with `@P;` ([belf-format.md](belf-format.md)). `emu32 -e` is the shortcut that skips steps 1-3: the loader puts the program in RAM and starts it at `_start`, in kernel mode.

## Limits

- A DMA command moves one contiguous buffer (no scatter/gather), and it moves the data when the command completes rather than during the wait.
- Console input is queued by the host before the run. Nothing arrives while the machine runs.

## Where it is in the code

`Device` (`devices.h`) is the register page, with `read_register`/`write_register`, and `InterruptController`, `Timer`, `Console` and `BlockDevice` are its subclasses (`devices.cpp`). `SystemBus` owns `intc`, `timer`, `console` and `block`, and routes `0xF0000000` and up to them. `Emulator32bit::run` takes the IRQ between instructions and ticks the timer and the block device after each retired instruction. `Emulator32bit::_wfi` asks both devices how long until their next event. Tests: `devices_test.cpp`, and `console_timer_and_interrupt_controller` in the integration tests.
