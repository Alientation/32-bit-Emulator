# To do

Work that is not done yet. The other documents describe what exists, so what is missing from it is listed here.

## C compiler

The compiler is in `core/ccompiler` and is currently disabled in `core/CMakeLists.txt`. It is awaiting a full rewrite.

## Instruction set

- Hardware floating point. The `V*` opcodes are reserved and the `fimm` immediate of the F2 format has no layout, see [isa.md](isa.md#f-floating-point-not-implemented). `FPCR`/`FPSR` do not exist.
- Fewer opcodes by aliasing: `cmp` could be `sub` with `xzr` as the destination, and instructions like `hlt` and `nop` could share an encoding. The assembler already injects `xzr` for the comparisons.

## Devices

- DMA of the block device is one contiguous buffer per command, with no scatter/gather, and it moves the data when the command completes ([devices.md](devices.md#block-device-0xf0003000)).
- Console input that arrives while the machine runs (host time). It is queued before the run today.

## Debugger

- Watchpoints on physical addresses, the old value of a write, and conditions (stop only if the value is...).
- Showing the exception state in `bt`. A backtrace through a handler stops at the `ERET` frame.
- A gdb remote stub, for use from an IDE.

## Code and build

- Move the disassembler into a component of its own.
- Turn the debug log calls off outside debug builds. `AEMU_LOG_COMPILE_LEVEL` is 0 in every build today ([logger.h](../core/util/include/util/logger.h)).
- Reduce the compilation time. Start by looking at the preprocessed output ([`-save-temps`](https://gcc.gnu.org/onlinedocs/gcc/Developer-Options.html#index-save-temps)).
- `core/app/src/main.cpp` has a long list of older ideas (multi-core, shared objects, a visualizer, a bootloader) at its top that has not been sorted.
