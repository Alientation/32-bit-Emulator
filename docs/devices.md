# Devices, interrupts and the boot sequence

The machine has nine memory mapped devices: an interrupt controller, a timer, a console, a block device, a display, a system controller, a real time clock, a source of random numbers and a keyboard with a mouse. An operating system uses them to take turns between processes (timer), to wait without spinning (`WFI`), to talk to the user (console), to keep data (block device), to show pictures (display, described in [display.md](display.md)), to find out how much memory there is and to turn the machine off or reset it (system controller), to know the time of day (real time clock), for random numbers, and to read the keys and the mouse of the window (input device). They are reached with ordinary loads and stores, so the kernel maps their pages like any other memory (see [mmu.md](mmu.md)).

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
| `0xF0004000` | **display**, one page ([display.md](display.md)) |
| `0xF0005000` | **system controller**, one page |
| `0xF0006000` | **real time clock**, one page |
| `0xF0007000` | **random numbers**, one page |
| `0xF0008000` | **keyboard and mouse** (the input device), one page |

A store by a program to the ROM is a bus error as well (nothing is written; a store that crosses into the ROM from the page before writes nothing either). The host can still write the image (the loader's physical sections, `ROM::write_*` in a test).

Memory may not reach `0xF0000000` (the machine refuses the layout). Another address in the window (`0xF0009000` ...) is a bus error (a data abort, ISS 4, with a vector table). A device register is a word. A byte or half-word **read** gives that part of the word, and a byte or half-word **write** writes the register with the value in that position, so `strb x1, [console]` sends one byte. A register that has a side effect on read (the interrupt controller's CLAIM, the console's DATA, the random numbers' DATA) does it once per access.

Programs without page tables (`SCTLR.M` = 0, what `emu32 -e` runs) reach the devices at these addresses directly, the swapping memory leaves addresses from `0xF0000000` alone.

### Finding the devices

The devices fill the pages from `0xF0000000` on with no gap, and the system controller, at the fixed address `0xF0005000`, says how many there are (DEVICE_COUNT). The **last word of every device page, offset `0xFFC`, is its ID** (read only, a write is ignored):

| Bits | |
|------|---|
| 31-16 | the type of the device |
| 15-8 | its version, 1 for all of them so far. A device that changes what it does gets a higher one |
| 7-0 | the interrupt line it raises, `0xFF` for none |

| Type | Device | Line |
|------|--------|------|
| 1 | interrupt controller | none |
| 2 | timer | 0 |
| 3 | console | 1 |
| 4 | block device | 2 |
| 5 | display | 3 |
| 6 | system controller | none |
| 7 | real time clock | none |
| 8 | random numbers | none |
| 9 | keyboard and mouse | 4 |

So a kernel finds its drivers' devices by reading DEVICE_COUNT and then the ID of each page, rather than knowing the addresses and lines (`0x000101FF` at `0xF0000FFC`, `0x00020100` at `0xF0001FFC`, ...). The table above is the order now; a new device takes the next page and the next free line.

## Interrupts

An IRQ is **first come, first served**, there are no priorities:

1. A device (or software) *raises a line* (0-31). A line that is not enabled is ignored, one that is waiting already keeps its place and is not queued twice.
2. Between two instructions, if the queue is not empty, `PSTATE.I` is 0 and a vector table is installed (`VBAR != 0`), the CPU takes the **IRQ exception**, vector 6, `VBAR + 96`. `ELR` is the instruction that would have run, so `ERET` resumes exactly there. `I` is set on entry, so the handler is not interrupted unless it clears `I`.
3. The handler reads CLAIM, which returns the line at the head of the queue and removes it, and serves that device. If more lines are waiting it may claim again, or return and be interrupted again at once (the queue is still not empty).

Without a vector table nothing is ever delivered (the queue just fills), which keeps programs that do not know about interrupts unchanged. Lines: 0 timer, 1 console receive, 2 block device, 3 display, 4 keyboard and mouse.

### Interrupt controller (`0xF0000000`)

| Offset | Name | Access | |
|--------|------|--------|---|
| `0x00` | CLAIM | read | the line at the head of the queue, and remove it. `0xFFFFFFFF` if the queue is empty |
| `0x04` | COUNT | read | the number of lines in the queue |
| `0x08` | ENABLE | r/w | bit *n*: line *n* can be raised. Reset: 0, so a kernel enables what it uses |
| `0x0C` | RAISE | write | raises the line with that number: a software interrupt |
| `0x10` | PENDING | read | bit *n*: line *n* is in the queue |

### Timer (`0xF0001000`)

The timer counts **executed instructions**, not host time, so a run is the same every time and a debugger session does not change when it fires.

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

Input can also arrive **while the machine runs**: `emu32 --console-stdin` hands the console what is typed (a thread reads the standard input a character at a time; not with `--debug`, which reads it too), so does the window of `emu32 --display` ([display.md](display.md#the-window-emu32---display)), and `Console::attach_host_input` takes a `HostInput` (`host_input.h`) that any thread of the host pushes bytes to. The console takes them on the thread of the machine: when `STATUS` or `DATA` is read, when the device clock looks (at least every 10000 executed instructions, `Console::kHostInputPollInterval`, and at the start of a run), and when `WFI` waits. So a byte that is typed is received, and raises line 1 if that is enabled, at most 10000 instructions later, without the machine looking before each instruction; when it arrives is up to the host anyway. `WFI` with nothing else to wait for (no timer match, no command in progress) and the receive interrupt enabled **blocks until a byte is typed**, which is host time passing, not instructions; when the input has ended (the end of the file on the standard input) it ends the run like without a live input. A machine that never reads the console and does not enable the interrupt only queues what arrives.

### Block device (`0xF0003000`)

A disk of **512 byte sectors** for the operating system. It is used either one sector at a time through a buffer inside the device and a data register (programmed I/O), or by **DMA**, many sectors straight to and from physical RAM. A command takes `LATENCY` executed instructions (per sector for DMA), like the timer counts, and then the status says so and line 2 is raised if enabled.

| Offset | Name | Access | |
|--------|------|--------|---|
| `0x00` | COMMAND | write | 1 read the sector into the buffer, 2 write the buffer to the sector, 3 flush the disk to its file, 4 DMA read, 5 DMA write, 6 scatter/gather read, 7 scatter/gather write |
| `0x20` | DMA_ADDR | r/w | physical RAM address of a DMA transfer, a multiple of 4 |
| `0x24` | DMA_COUNT | r/w | the number of sectors of a DMA transfer, at least 1; for commands 6 and 7 the number of descriptors |
| `0x28` | DMA_LIST | r/w | physical RAM address of the descriptors of a scatter/gather command, a multiple of 4 |
| `0x04` | STATUS | r/w | bit 0 busy, bit 1 done, bit 2 error. A write clears done and error |
| `0x08` | SECTOR | r/w | the sector number of the next command |
| `0x0C` | DATA | r/w | the word of the buffer at CURSOR, and CURSOR moves on by 4. **Use word accesses** |
| `0x10` | CURSOR | r/w | byte offset into the buffer (a multiple of 4, below 512). Set to 0 when a read completes |
| `0x14` | CAPACITY | read | the number of sectors, 0 when there is no disk |
| `0x18` | CTRL | r/w | bit 0: raise line 2 when a command completes |
| `0x1C` | LATENCY | r/w | instructions a command takes, at least 1. Reset: 100 |

A driver reads a sector like this: write SECTOR, write COMMAND = 1, wait (poll STATUS bit 0, or enable the interrupt and `wfi`), write STATUS to acknowledge, then read DATA 128 times. To write: write CURSOR = 0, write DATA 128 times, SECTOR, COMMAND = 2, wait. The buffer is copied when the command is given, so it can be reused at once.

**DMA:** write SECTOR (the first), DMA_ADDR and DMA_COUNT, then COMMAND = 4 (disk to RAM) or 5 (RAM to disk), and wait as above. The sectors `SECTOR .. SECTOR + COUNT - 1` go to or from the `COUNT * 512` bytes at DMA_ADDR, and the buffer and cursor of the data register are not touched. The three registers are copied when the command is given.

**Scatter/gather:** write DMA_LIST (the physical address of the descriptors) and DMA_COUNT (how many, 1 to 1024), then COMMAND = 6 (disk to RAM) or 7 (RAM to disk). A descriptor is three words, `sector`, `count` (sectors) and `address` (RAM, a multiple of 4), and means what a DMA command with those SECTOR, DMA_COUNT and DMA_ADDR means:

```
+0   sector
+4   count
+8   address
```

The list is read **when the command is given**, the data moves when it completes, after `LATENCY` for each sector of all the descriptors together. If any descriptor is not valid (the rules of a DMA transfer, for each), or the list is empty, longer than 1024, not word aligned or not all in RAM, the command completes with error and moves **nothing**. Pieces are done in order, so where two overlap in RAM (a read) or on the disk (a write) the later one wins.

- The data moves **when the command completes** (after `COUNT * LATENCY` instructions), not at the start. So a DMA write reads RAM at that moment, and a DMA read changes RAM then: the kernel has to keep the buffer where it is, and not use it, until done. Since DMA uses physical addresses, a kernel with page tables passes the frame's physical address and must not have the page swapped or moved meanwhile.
- DMA reaches **RAM only** (not ROM, the memory mapped disk or devices). It does not trigger memory watchpoints, a debugger's `mem` sees the result.
- It completes with error, and moves nothing, if COUNT is 0, DMA_ADDR is not word aligned, any byte of the range is outside the RAM, or a sector is past CAPACITY.
- A command given while busy, or one that is not 1-5, is ignored and sets error. A sector past CAPACITY completes with error and changes nothing.
- A reset keeps the contents of the disk (it resets the controller: cursor, sector, control, latency, buffer, DMA registers).
- Without `--block-file` or `--block-sectors` the disk has 0 sectors. `emu32 --block-sectors N` is a disk of N sectors in memory, `--block-file FILE` keeps it in a file (made, with `--block-sectors`, if it does not exist; its size in sectors otherwise, or more if `--block-sectors` says so). The file is written on FLUSH and when the machine ends.
- This is separate from `--disk-file`, the memory mapped disk that the swapping memory (`SCTLR.M` = 0) uses.

### System controller (`0xF0005000`)

| Offset | Name | Access | |
|--------|------|--------|---|
| `0x00` | POWEROFF | write | turns the machine off after this instruction. The value is the exit code |
| `0x04` | RESET | write | resets the machine after this instruction (any value) |
| `0x08` | RAM_START | read | the physical address of the RAM |
| `0x0C` | RAM_SIZE | read | the bytes of RAM |
| `0x10` | ROM_START | read | the physical address of the ROM, 0 when there is none |
| `0x14` | ROM_SIZE | read | the bytes of ROM |
| `0x18` | DEVICE_COUNT | read | the number of devices, in the pages from `0xF0000000` on ([Finding the devices](#finding-the-devices)) |

A store to POWEROFF completes like any store, and the run ends before the next instruction: `run` returns `Status::POWER_OFF` with the message `Power off with exit code N`, and the pc is the instruction after the store. `emu32` says `status=poweroff` and **exits with the low byte of the code** (an exit status is a byte), so a kernel's test can tell `ctest` whether it passed: 0 for a pass, a code of its own (not 1-4, which `emu32` uses itself) for a failure. The debugger says `Power off with exit code N after M instructions.` and ends the program, the gdb server sends `W` with the code (`W05`). The machine stays off: another `run` returns `POWER_OFF` at once, until a reset (`Emulator32bit::reset`).

A store to RESET also completes, and then the machine is **reset within the same run**, as at power on ([the boot sequence](#boot-sequence)): the registers, the RAM and the devices go back to their reset values, the ROM and the disks keep their contents, and the next instruction is the one at the **boot address**: what `emu32 --pc` gave (0 by default, and with `-e`, which loads the program into the RAM that the reset clears), `Emulator32bit::set_boot_pc` from C++. So a reset is for a machine that boots from its ROM. The run goes on counting its instructions.

RAM_START and RAM_SIZE are what the machine was made with (`emu32 --ram-start`, `--ram-pages`), so a kernel knows which frames it has without being built for one size.

### Real time clock (`0xF0006000`)

| Offset | Name | Access | |
|--------|------|--------|---|
| `0x00` | SECONDS | r/w | seconds since 1970-01-01 00:00 UTC. A read keeps the microseconds for MICROS, a write sets the time to that second |
| `0x04` | MICROS | read | the microseconds within the second that the last read of SECONDS gave |

Read SECONDS, then MICROS, for a time to the microsecond that does not tear between the two.

By default the clock keeps the **time of the machine**, which comes from the executed instructions like the timer's: the machine is taken to run 100 million instructions a second (10 ns each, `kInstructionsPerSecond`), about what the emulator does on a desktop. So a run is the same every time, also its dates, and `WFI` makes time pass as it does for the timer. It starts at 0 (1970) or at `emu32 --rtc-time SECONDS` (`RealTimeClock::set_time`). `emu32 --rtc-host` (`use_host_time`) makes it the **host's clock** instead, which a run cannot repeat. A write of SECONDS sets the time in both. A reset of the machine keeps the time, like the battery of a real clock. SECONDS is 32 bits without a sign, which lasts until 2106.

### Random numbers (`0xF0007000`)

| Offset | Name | Access | |
|--------|------|--------|---|
| `0x00` | DATA | read | the next random word |
| `0x04` | SEED | write | the numbers start again from this seed |

For stack canaries, the seed of `rand ()` and addresses that differ from one run to the next. By default the numbers come from a generator (SplitMix64, the upper half of each output) whose seed the host sets, 0 or `emu32 --rng-seed N` (`RandomNumbers::set_seed`), so a run is the same every time. `emu32 --rng-host` (`use_host_entropy`) takes them from the host (`std::random_device`) instead. A program that writes SEED gets the generator from that seed in both cases, which is how a test of a kernel repeats a sequence. A reset goes back to what the host set. Each read of DATA, also of a byte of it, takes a number.

### Keyboard and mouse (`0xF0008000`)

The keys and the mouse of the window of `emu32 --display` ([display.md](display.md#the-window-emu32---display)), for programs that need more than the console's bytes: a key that goes up, the arrows, a key held with a modifier, where the mouse is.

| Offset | Name | Access | |
|--------|------|--------|---|
| `0x00` | KEY | read | take the next key event, 0 if there is none |
| `0x04` | STATUS | r/w | bit 0: a key event is waiting. bit 1: the mouse changed. bit 2: key events were lost. A write clears bits 1 and 2 |
| `0x08` | CTRL | r/w | bit 0: raise line 4 for each key event. bit 1: raise line 4 when the mouse moves, a button changes or the wheel turns |
| `0x0C` | MOUSE_X | read | where the mouse is across, in pixels of the display's picture (0 to WIDTH - 1) |
| `0x10` | MOUSE_Y | read | down (0 to HEIGHT - 1) |
| `0x14` | BUTTONS | read | the buttons that are down. bit 0: left, bit 1: right, bit 2: middle |
| `0x18` | WHEEL | read | the steps the wheel turned since the last read, up positive (a signed word), and set it to 0 |

A **key event** is a word:

| Bits | |
|------|---|
| 15-0 | the key: its USB HID usage, the code of where the key is, not of what it prints (SDL's scancode). 4-29 are A-Z, 30-39 1-9 and 0, 40 Enter, 41 Escape, 42 Backspace, 43 Tab, 44 Space, 58-69 F1-F12, 79-82 right, left, down, up, 224-231 the left and the right Ctrl, Shift, Alt and GUI |
| 16 | 1: the key went down, 0: it went up |
| 17 | 1: the key is held and repeats (a press again) |
| 24-27 | the modifiers that were down: Shift, Ctrl, Alt, GUI |

Key events wait in a queue of 64; one that arrives while it is full is lost, and STATUS bit 2 says so. The mouse is not queued: the registers are where it is now, and STATUS bit 1 says that it changed since STATUS was last written. With CTRL bit 0 each key event raises line 4, with bit 1 each change of the mouse does (and enabling either while something waits raises it). Text typed in the window also goes to the console as bytes, as before, so a program uses one or the other.

Like the console's, the events come from another thread (`InputDevice::attach_host_input`, the same `HostInput` as the console's in `emu32`) and are taken on the thread of the machine: when a register is read, at least every 10000 instructions and when `WFI` waits. `WFI` with nothing else to wait for and CTRL not 0 **blocks until an event arrives**, as it does for the console's receive interrupt; what arrives that the program did not ask to hear of (text, with the console's interrupt off) is taken and the wait goes on. Without the window nothing arrives; `InputDevice::push` gives the device an event from C++.

## `WFI`

`WFI` (privileged) returns as soon as an interrupt is pending, even a masked one: the next instruction runs, and takes the interrupt if it is not masked. Nothing else runs while the CPU waits, so the only things that can produce an interrupt are the timer, a block device command in progress and the end of a frame of the display (when its interrupt is enabled), and `WFI` **jumps** to the nearest of these moments: time passes at once, the device finishes and raises its interrupt. A kernel's idle loop is `wfi` and then the interrupt handling, and costs one instruction however long the wait is. With nothing that could ever wake it (no pending interrupt, the timer off, no command in progress, the display off or without its interrupt), `WFI` ends the run like `HLT` instead of hanging, with the message `WFI with no interrupt source`.

## Boot sequence

At reset (`Emulator32bit::reset`):

| State | Value |
|-------|-------|
| mode | kernel, IRQs masked (`PSTATE = 0x20`) |
| `PC` | the boot address: 0 (RAM) unless set, `emu32 --pc ADDRESS`, `set_boot_pc` (and `set_pc` for the first run) |
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

- A DMA command moves the data when it completes rather than during the wait, and the descriptors of a scatter/gather command are read when it is given: a kernel cannot change them in between.
- A scatter/gather list has no chaining (a descriptor that points to the next list): 1024 descriptors is the most.

## Where it is in the code

`HostInput` (`host_input.h`) is the thread safe queue of the live input: the console's bytes and the input device's `InputEvent`s. `Device` (`devices.h`) is the register page, with `read_register`/`write_register` and the ID register (`device_id`, answered by `Device` itself, `kNumDevices` and `kDeviceEnd` for the size of the window), and `InterruptController`, `Timer`, `Console`, `BlockDevice`, `Display`, `SystemController`, `RealTimeClock`, `RandomNumbers` and `InputDevice` are its subclasses (`devices.cpp`). `SystemBus` owns `intc`, `timer`, `console`, `block`, `display`, `sysctl`, `rtc`, `rng` and `input`, and routes `0xF0000000` and up to them. `Emulator32bit::run` takes the IRQ between instructions.

The timer, the block device, the display and the real time clock count executed instructions, but they do not look at each one. The bus's `DeviceClock` counts down, once per executed instruction in `run`, to the first moment one of them has something to do (`cycles_until_event`, or the end of a frame), and then gives all of them the instructions that passed at once (`advance`). It also brings them up to date before an access to their registers (`catch_up` for a read, which does not change when the next event is), starts the count down again after a write (which can change when the next event is), and does both at the start and the end of `run`. Each time, it takes the live input of the console and the input device, and while there is one it counts down to at most `Console::kHostInputPollInterval`. While the system controller has a request it counts down to 1, so `tick` returns `true` after the instruction that made the request, and `run` serves it (`serve_system_request`): a power off ends the run, a reset calls `reset` and goes on; this costs the loop nothing, the check is on the path where the count down ends. So a device sees the same count, and raises its interrupt after the same instruction, as if it counted them one at a time; a new device that counts instructions joins the clock. `Emulator32bit::_wfi` asks the devices how long until their next interrupt and moves them all on that far (`DeviceClock::advance`). Tests: `devices_test.cpp`, and `console_timer_and_interrupt_controller`, `a_program_turns_the_machine_off_with_an_exit_code`, `the_real_time_clock_starts_at_the_time_emu32_sets`, `random_numbers_follow_the_seed_emu32_sets` and `a_machine_that_boots_from_its_rom_is_reset_by_the_program` in the integration tests.
