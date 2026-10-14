# Display

The display shows a picture that is in RAM. A program draws with ordinary stores to the picture, and the display reads it at the end of each frame. Its registers are a page of the device window at `0xF0004000`, and its interrupt is line 3 ([devices.md](devices.md)).

## Registers (`0xF0004000`)

| Offset | Name | Access | |
|--------|------|--------|---|
| `0x00` | CTRL | r/w | bit 0: on. bit 1: raise line 3 when a frame ends |
| `0x04` | STATUS | r/w | bit 0: a frame ended. bit 1: error, the picture of a frame was not valid. A write clears both |
| `0x08` | ADDRESS | r/w | physical RAM address of the picture, a multiple of 4 |
| `0x0C` | WIDTH | r/w | pixels across, 1 to 1024. Reset: 320 |
| `0x10` | HEIGHT | r/w | pixels down, 1 to 1024. Reset: 240 |
| `0x14` | FORMAT | r/w | 0: a byte per pixel, an index into the palette. 1: a word per pixel, `0x00RRGGBB` |
| `0x18` | INTERVAL | r/w | instructions a frame takes, at least 1 (0 is taken as 1). Reset: 100000 |
| `0x1C` | FRAMES | read | the frames that ended since reset |
| `0x400` | PALETTE | r/w | 256 words, entry *n* at `0x400 + 4n`, `0x00RRGGBB` (the top byte is not kept) |

The palette after a reset is 3-3-2: the bits of the index are `rrrgggbb`, each part scaled to 0-255 (`0xE0` is red, `0x1C` green, `0x03` blue, `0xFF` white).

## The picture

The picture is `WIDTH * HEIGHT` pixels, row by row from the top left, with no gap between the rows. In format 0 a pixel is a byte, the index of its colour in the palette; in format 1 it is a little endian word `0x00RRGGBB` (the top byte is ignored). It must be **all in RAM**: not the ROM, the disk or the device window. A picture of 320 by 240 is 76800 bytes in format 0 (19 pages) and 307200 in format 1 (75 pages), so the RAM has to be big enough (`emu32 --ram-pages`, 32 pages by default).

ADDRESS is a **physical** address, like the addresses of DMA. A kernel with page tables maps the frames of the picture where it likes and gives the display their physical address. A program of `emu32 -e` (the swapping memory, `SCTLR.M` = 0) puts the picture in a section at a physical address with a linker script that starts with `@P;` ([belf-format.md](belf-format.md)), so that it knows where the picture is:

```
ENTRY(_start)
SECTIONS (
 @P;
 .text = 0x0;
 .bss = 0x8000;
)
```

## Frames

The display counts **executed instructions**, like the timer: a frame is INTERVAL instructions, so a run is the same every time, with or without a window. When a frame ends, the display:

1. adds 1 to FRAMES and sets STATUS bit 0,
2. checks the picture: the size is valid, ADDRESS is a multiple of 4 and the whole picture is in RAM. If not, it sets STATUS bit 1 (and no picture is shown),
3. hands the picture to the window of the host, if there is one,
4. raises line 3 if CTRL bit 1 is set.

A frame starts when CTRL turns the display on and when INTERVAL is written. Turning it off stops the count. The picture is read only at the end of a frame, so a program that draws between two frames is never seen half drawn: draw, then wait for the frame with `WFI` (or poll STATUS bit 0 and write STATUS to acknowledge it).

`WFI` waits for the end of a frame when the display is on and its interrupt is enabled: time jumps to it like it does for the timer ([devices.md](devices.md#wfi)). A display without its interrupt is not something to wait for, so it does not keep `WFI` from ending the run.

A program that draws frames in a loop never halts: run it with a limit (`emu32 -l`).

## In the host

What the program sees does not depend on the host: the registers and the interrupt are on the thread of the emulator, and a frame ends after the same instruction every time. Only the host's view differs:

- `emu32 --display-dump FILE` writes the picture as it is in RAM when the run ends, as a binary PPM image (P6). It fails (exit code 1) if the picture is not valid.
- `emu32 --display` shows the display in a window (below).
- A window on another thread gets each frame through a `HostDisplay` (`host_display.h`), which the display presents to at the end of a frame. The frame is swapped, not copied, between the two threads. With a frame rate, `present` waits so that the machine shows no more frames a second than that, which is what makes a program that animates run at the speed it was written for; the instructions of a frame do not change.

## The window (`emu32 --display`)

The window is an SDL2 window, built only when CMake is given `-DAEMU_GUI=ON` (and `libsdl2-dev` is installed); the default build, and CI, do not need SDL, and an `emu32` without the window refuses `--display`. A build directory of its own keeps the default ones unchanged:

```
cmake -S . -B build/gui -G Ninja -DCMAKE_BUILD_TYPE=RelWithDebInfo -DAEMU_GUI=ON
cmake --build build/gui
build/gui/assembler/basm -o demo app/programs/src/display_demo.basm -ld app/programs/src/display_demo.ld
build/gui/emulator32bit/emu32 -e demo.bexe --display
```

`app/programs/src/display_demo.basm` draws 300 frames of moving colour bands and halts. Under WSL 2 the window opens on the Windows desktop through WSLg (Windows 11), with no X server to install.

- The machine runs on a thread of its own and the window on the main thread. The window is black until the first frame, takes the size of the picture times `--display-scale` (3), and can be resized (the pixels stay square and sharp).
- `--display-fps N` (60) is the most frames a second: the machine waits at the end of a frame for its turn, so a program that waits for each frame runs at that speed. `0` is no limit. A program that does not turn the display on is not slowed down.
- What is typed in the window goes to the **console** as received bytes: the characters, and Enter, Backspace, Tab and Escape as `\n`, `\b`, `\t` and `0x1B`. It is the console's live input, as with `--console-stdin` (which, like `--debug` and `--gdb`, cannot be combined with `--display`): a key reaches the console within 10000 instructions ([devices.md](devices.md#console-0xf0002000)), and the machine runs as fast as without it.
- When the program stops (it halted, faulted, or reached `-l`), the window shows the picture as the program left it, the title says `(stopped)`, and the window stays until it is closed; then emu32 reports the state as usual.
- **Closing the window ends the run**: the machine stops within 100000 instructions, and the state says `status=limit` and `message=The display window was closed` (exit code 2). Ctrl-C in the terminal closes the window the same way.
- Breakpoints, watchpoints, `--trace` and `--history` work as without the window.

## Where it is in the code

`Display` (`devices.h`, `devices.cpp`) is the device; `SystemBus` owns it (`display`) and gives it the RAM (`set_ram`). The `DeviceClock` of the bus moves it on (`advance`) when a frame ends (`cycles_until_frame`) or its registers are accessed, and `_wfi` asks it how long until its interrupt (`cycles_until_event`). `capture` converts the picture to `0x00RRGGBB` pixels in a `Frame`. `HostDisplay` (`host_display.h`) is the handover to a window, `DisplayWindow` (`display_window.h`, `display_window.cpp`, only compiled into `emu32` with `AEMU_GUI`) the SDL window, and `run_with_window`/`run_in_slices` in `cli.cpp` run the machine on its thread in slices. Tests: the `DisplayUnit` tests and `Devices.wfi_waits_for_the_end_of_a_frame` in `devices_test.cpp`, and `a_program_draws_on_the_display`, `sample_programs` (the demo, headless) and, in a build with `AEMU_GUI`, `closing_the_display_window_ends_the_run` (SDL's `dummy` video driver, so it needs no screen) in the integration tests.
