# ABI and integer division

This is the convention that a C compiler, the runtime library, the operating system and hand written assembly follow: the data layout, the registers, how arguments and results are passed, the frame record and the system call interface. Everything is little endian.

The convention is not enforced by the toolchain. The instruction set support it relies on is implemented (`UDIV`/`SDIV`, see [Division](#division), and the instructions listed under [Toolchain support](#toolchain-support)) and the debugger's `bt` follows the frame record. The C compiler, `crt0` and the runtime library are not written yet; their sections below say what they have to provide. The register roles are the ones in [isa.md](isa.md#register-conventions).

## Data types

| C type | Size | Alignment |
|--------|------|-----------|
| `char`, `signed char`, `unsigned char`, `_Bool` | 1 | 1 |
| `short` | 2 | 2 |
| `int`, `long`, pointers, `enum` | 4 | 4 |
| `long long` | 8 | 4 |
| `float` | 4 | 4 |
| `double` | 8 | 4 |

`char` is **signed**: a plain `char` is loaded with `ldrsb`, `unsigned char` with `ldrb`. Structs and arrays use the natural alignment of their members and are padded to a multiple of their alignment. The largest alignment is **4**: there is no 64 bit load or store, so a 64 bit value is always handled as two words and gains nothing from 8 byte alignment. `ldr`, `str`, `ldrh` and `strh` fault on an address that is not aligned to the access, so a member of a `packed` struct, or a pointer that was cast to a type of a larger alignment, is accessed with `ldur`, `stur`, `ldurh`, `sturh` (see [isa.md](isa.md#memory-access-6)).

`float` and `double` are IEEE 754 binary32/binary64. The CPU has hardware floating point ([isa.md](isa.md#floating-point-3)) that works on the integer registers, in which these types are passed anyway, so the same registers are used whether the compiler emits `fadd.f32` or calls the software routine of the [runtime library](#runtime-library) (for what the instructions do not do: `long long` to float, and the math library).

## Registers

| Registers | Role | Saved by |
|-----------|------|----------|
| `x0`–`x7` | arguments. `x0`, `x1` return value | caller |
| `x8` | system call number | caller |
| `x9`–`x15` | temporaries | caller |
| `x16`, `x17` | scratch for the compiler's own sequences (address and constant building, a long branch) | caller |
| `x18` | reserved for the platform (the OS may keep a pointer to per-CPU or per-thread data here). Compiled code does not touch it | |
| `x19`–`x27` | callee saved | callee |
| `x28` | frame pointer | callee |
| `x29` | link register | callee, if it makes calls |
| `sp` | stack pointer | callee (restored on return) |
| `xzr` | zero | |

The flags NZCV are not preserved across calls and no function expects them set on entry. The floating point control register `fpcr` (the rounding mode) is not changed by a function, except to set it for a computation and put it back (`fesetround`). The status flags `fpsr` accumulate across calls and no function relies on them being clear.

## Calling convention

### Arguments

Arguments are assigned left to right to **words**:

- An integer, pointer or `float` of up to 4 bytes takes the next free register of `x0`–`x7`.
- A `long long` or `double` takes the next two free registers `(xN, xN+1)`, low word in the lower one (no even/odd rule, since nothing needs 8 byte alignment). When fewer than two are left it goes on the stack, and no later argument goes back to a register.
- A struct or union (**any size**) is never split over registers: the caller copies it to a temporary and passes a **pointer to the copy**, which takes one register like any pointer. The callee may modify the copy.
- Once the eight registers are used, the rest goes on the stack.

Passing every aggregate by pointer keeps the rules simple: an aggregate is one word, it never has to be cut into register-sized pieces, and it never straddles the register/stack boundary.

Stack arguments are at `[sp]` upward at the moment of the call, in the order of the argument list, each aligned to 4 bytes and occupying a multiple of 4 bytes. The caller removes them after the call returns.

### Results

- Up to 32 bits: `x0`. A smaller integer type is extended to 32 bits by the callee (sign or zero according to the type), and the caller may rely on it.
- 64 bits (`long long`, `double`): `x0` (low) and `x1` (high).
- A struct or union: the caller passes a pointer to space for the result in `x0` as a hidden first argument, which shifts the other arguments by one register, and the callee returns the same pointer in `x0`.

A `void` function may leave any value in `x0`.

### Stack

- The stack grows downward and `sp` is a multiple of **4** at all times, not only at calls (`ldr` and `str` need a multiple of 4 and nothing needs more). Interrupt entry can rely on that too.
- Nothing below `sp` is live. Interrupts and signals may overwrite it, with no red zone.
- A push is `str xt, [sp, -4]!` and a pop is `ldr xt, [sp], 4`.

### Frame

Every function that compiled code contains sets up a frame record, two words: the **caller's frame pointer in the lower 4 bytes (`[x28]`) and the return address, the link register, in the upper 4 bytes (`[x28, 4]`)**. `x28` points at the record. Following `x28` therefore walks the call chain, which is what the debugger's `bt` command does: the record gives the caller's `x28` and the address to return to, and the first frame has `x28 = 0` (`_start` clears it).

```
func:
    sub     sp, sp, 8            ; room for the frame record
    str     x28, [sp]            ; saved fp (lower word)
    str     x29, [sp, 4]         ; saved lr (upper word)
    mov     x28, sp              ; fp -> the record
    sub     sp, sp, 24           ; locals and callee saved registers, a multiple of 4
    ...
    mov     sp, x28
    ldr     x29, [x28, 4]
    ldr     x28, [x28]
    add     sp, sp, 8
    ret
```

A function written by hand that is a leaf and uses only `x0`–`x17` may skip the record. It then does not appear in a backtrace (its caller does, through the link register, only when it is stopped at its first instruction, see [debugging.md](debugging.md#backtrace)).

### Variadic functions

The prologue of a variadic function stores the argument registers that were not named (`x0`–`x7`) in a block directly below the stack arguments, so all arguments are contiguous in memory. `va_list` is a `char *` into that block: `va_arg` reads the next value at the current pointer, and advances it by the size rounded up to 4 (all values are 4 byte aligned). The caller does nothing special.

### Startup

`_start` (in the C runtime object `crt0`) sets `sp` (the loader or the kernel gives it the stack), clears `x28` and `x29`, calls `main(argc, argv)` and passes the result to the `exit` system call. The loader zeroes `.bss`. Clearing `x28` is what ends the chain of frame records.

## System calls

`swi 0` with the call number in `x8`, arguments in `x0`–`x5` and the result in `x0`. A value from `-4095` to `-1` is an error (the negated error number), anything else is a result. A system call preserves all registers except `x0` and the flags. The numbers are assigned with the kernel (see [exceptions.md](exceptions.md#swi-and-the-emulator-calls) for how `swi` reaches it). The emulator's own debugging calls (`emu_*`) are the separate *semihosting* interface, `swi 1`.

## Division

Division is two instructions, `UDIV` and `SDIV`. (A remainder has no instruction, see below.)

| Opcode | Instruction | Format | Operation |
|--------|-------------|--------|-----------|
| `011001` | `UDIV{S} xd, xn, arg` | O | `xd = xn / arg`, unsigned |
| `011010` | `SDIV{S} xd, xn, arg` | O | `xd = xn / arg`, signed, rounded toward zero |

They take the same `arg` as the other O instructions (a register with a shift, or an unsigned `imm14`), and `S` updates N and Z from the result with C and V unchanged, like `MUL`. They are in [isa.md](isa.md#division-2).

- **Division by zero** gives **0** and raises nothing (the ARM A64 behavior). C says it is undefined and there is no trap for it, so a compiler that wants a check emits one.
- **`INT_MIN / -1`** gives `INT_MIN` (the result wraps, no exception).
- **Remainder** has no instruction: `r = n - (n / d) * d`, i.e. `sdiv t, n, d` / `mul t, t, d` / `sub r, n, t`. With `d = 0` that gives `n`, which is consistent with the quotient being 0.

Where it is: `opcodes.h` (two rows), `alu.h` (`alu_udiv`, `alu_sdiv`), `instructions.cpp` (`_udiv`, `_sdiv`), `disassembler.cpp`, and one row each in `BASM_INSTRUCTION_LIST` (`instruction_list.h`). Tests: `alu_test.cpp` (including the remainder identity), the `kOps` table of `dataproc_test.cpp`, and the two table checks.

## Lowering of C operations

How a compiler uses the instruction set:

| C | Code |
|---|------|
| `a + b`, `a - b`, `a * b`, `a & b`, `a \| b`, `a ^ b` | `add`, `sub`, `mul`, `and`, `orr`, `eor` |
| `~a` | `mvn` |
| `a / b`, `a % b` | `udiv`/`sdiv`, plus `mul` and `sub` for `%` |
| `a << n`, `a >> n` | `lsl`, `lsr` (unsigned) or `asr` (signed). Register shift amounts use the low 5 bits, so a variable shift of 32 or more needs a compare |
| `long long` add/sub | `adds` + `adc`, `subs` + `sbc` |
| `long long` multiply | `umull` for the low parts plus `mul` of the cross terms |
| `long long` shifts, compares | inline sequences, or the runtime library |
| `float`/`double` `+ - * /`, `sqrtf`, `fabs`, `-x` | `fadd`, `fsub`, `fmul`, `fdiv`, `fsqrt`, `fabs`, `fneg` with `.f32`/`.f64` (a double is a register pair) |
| `fmaf`, `fma` (and `a * b + c` where the compiler may contract it) | `fmadd` with `.f32`/`.f64`; `fmsub`, `fnmadd` and `fnmsub` for the other signs |
| `(int) f`, `(float) i`, `(double) f` | `fcvt.s32.f32`, `fcvt.f32.s32`, `fcvt.f64.f32`, ... (the cast rounds toward zero; `lrintf` is `fcvtr`) |
| `f < g`, `f <= g`, `f == g` | `fcmp.f32` then `b.mi`/`b.ls`/`b.eq` (not `lt`/`le`: they are also true for a NaN); `floorf`, `ceilf`, `truncf`, `roundf` are `frintm`, `frintp`, `frintz`, `frinta` |
| a float constant | `fmov.f32 xd, 1.5` (the same instructions as `ldr xd, =bits`) |
| constants up to `0x7FFFF` | `mov xd, imm` (`mvn` for the complement, so −1 to −524288 are one instruction) |
| any other 32 bit constant | `ldr xd, =value`, which the assembler expands to `mov xd, value >> 14` / `lsl xd, xd, 14` / `orr xd, xd, value & 0x3FFF` (two instructions when the low 14 bits are 0). No literal pool, no scratch register |
| address of a global | `adrp xd, sym` + `add xd, xd, :lo12:sym`, or `ldr xd, =sym`; `adr xd, sym` when it is within 1 MiB of the instruction (a static, a string, a function of the same file) |
| a member that may be unaligned (`packed`, a cast pointer) | `ldur`/`stur`, `ldurh`/`sturh` (`ldursh` to sign extend); `ldrb`/`strb` need nothing |
| sign extension of 8/16 bits in a register | `sxtb xd, xn` / `sxth xd, xn`. Loads use `ldrsb`/`ldrsh` |
| zero extension | `uxtb` / `uxth` (or `and xd, xn, 255` for a byte) |
| comparison for a branch | `cmp` + `b.lt`/`b.lo`/... (signed: `lt le gt ge`, unsigned: `lo ls hi hs`) |
| comparison as a value (`a < b`) | `cmp` + `cset xd, lt` (`csetm` for all ones) |
| `c ? a : b`, `abs`, `-x` on a condition | `cmp` + `csel`, `csneg`, `cneg` |
| count leading zeros, byte swap (`__builtin_clz`, `htonl`) | `clz`, `rev` (`rev16` swaps the bytes of each half-word) |
| indirect call | `blx xn` |
| `switch` jump table | `adrp` + `ldr` + `bx` |

## Runtime library

A static library (`libbasmrt.ba`, linked with `-l`) provides what the instructions do not, with the usual names. It is not written yet; this is the interface the compiler expects:

- 64 bit: `__muldi3` (if not inlined), `__divdi3`, `__udivdi3`, `__moddi3`, `__umoddi3`, `__ashldi3`, `__lshrdi3`, `__ashrdi3`, `__cmpdi2`, `__ucmpdi2`.
- Software floating point, for a compiler that does not use the instructions (`-msoft-float`): `__addsf3`, `__subsf3`, `__mulsf3`, `__divsf3`, `__adddf3`, ..., the conversions `__floatsisf`, `__fixsfsi`, `__extendsfdf2`, `__truncdfsf2`, and the comparisons `__ltsf2`, `__gtsf2`, `__eqsf2`, ... With the instructions only `long long` to and from float (`__floatdisf`, `__fixdfdi`, ...) stay.
- `memcpy`, `memmove`, `memset`, `memcmp`, `strlen` (hand written, word at a time).

Members are only linked when used (`select_library_members`), so an unused part costs nothing.

## Toolchain support

These are needed to follow the convention, and all are implemented (see [basm-syntax.md](basm-syntax.md)):

- `.rodata`: string literals, `const` data and jump tables. Read only and not executable, on a page of its own.
- `.weak` and `.comm`: weak symbols (a library function that a program may replace, an optional hook) and common symbols. A C compiler should place a tentative definition (`int x;` at file scope) in `.bss` as an ordinary global (`-fno-common`, the default of modern compilers) so that nothing depends on `.comm`, which cannot compare sizes: `.comm` is for hand written code.
- `.init_array` / `.fini_array` with the `__init_array_start`/`_end` and `__fini_array_start`/`_end` symbols, for `__attribute__((constructor))`. `crt0` calls what is between the bounds before `main` and after it.
- `ldr xd, =value` (no literal pool, see the lowering table), `cset`/`csel` and their family, and `sxtb`/`sxth`/`uxtb`/`uxth`/`clz`/`rev`/`rev16`.
- `mov xd, imm` takes the whole `imm19`.
- `adr xd, sym` (opcode `100001`): a pc relative address within 1 MiB in one instruction instead of the `adrp` pair.
- `.section "name", "flags"`: sections of the program's own (`"r"`, `"rw"`, `"rx"`), for `__attribute__((section("x")))`, a section for each function or object (`-ffunction-sections`, `-fdata-sections`) and the pieces of a kernel (a vector table, a boot stub) that a linker script places by name.
