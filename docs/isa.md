# Instruction Set Architecture

A simplified ARM-like 32 bit ISA. Instructions are fixed width (4 bytes, little endian) with a 6 bit opcode. The encodings keep the register fields at the same bit positions across formats, as ARM does.

Exceptions, system registers and the privilege levels are in [exceptions.md](exceptions.md). The calling convention, data layout and the use of `UDIV`/`SDIV` are in [abi.md](abi.md). The devices are in [devices.md](devices.md) and the MMU in [mmu.md](mmu.md). Debugging the emulator: [debugging.md](debugging.md).

The source of truth for opcodes is `AEMU_OPCODES` in `core/emulator32bit/include/emulator32bit/opcodes.h`. The assembler syntax is in [basm-syntax.md](basm-syntax.md).

- [Registers](#registers)
- [Encoding overview](#encoding-overview)
- [Instruction formats](#instruction-formats)
- [Special instructions](#special-instructions-opcode-000000)
- [Instructions by opcode](#instructions-by-opcode)
- [Operands](#operands)
- [Condition codes](#condition-codes)
- [Software interrupts](#software-interrupts-swi)
- [Unused opcodes](#unused-opcodes)

## Registers

32 register slots:

| Register | Number | Notes |
|----------|--------|-------|
| `x0`–`x29` | 0–29 | general purpose |
| `sp` | 30 | stack pointer |
| `xzr` | 31 | always reads 0, writes are discarded |

Not in the register file:

- **PC**: not addressable. It is 0 after reset, and the emulator adds 4 after each instruction.
- **PSTATE**: see below.
- **FPCR / FPSR**: the floating point control and status registers, see [FPCR and FPSR](#fpcr-and-fpsr). There is no separate floating point register file: a `float` is the 32 bits of a register and a `double` a pair of them, see [Floating point](#floating-point-3).

A register number above 31 reads as 0 and ignores writes (not reachable from a 5 bit field).

### Register conventions

| Registers | Role |
|-----------|------|
| `x0`–`x7` | parameters (`x0` is also the return value) |
| `x8` | system call number |
| `x0`–`x17` | caller saved |
| `x18` | not assigned a role (reserved) |
| `x19`–`x27` | callee saved |
| `x28` | frame pointer (FP) |
| `x29` | link register (LR), written by `BL`/`BLX` |

The stack grows downwards. `BX x29` is the same as `ret`.

### PSTATE

Reset value is `0x20`: kernel mode, IRQs masked.

| Bit | Flag | Meaning |
|-----|------|---------|
| 0 | N | negative |
| 1 | Z | zero |
| 2 | C | carry |
| 3 | V | overflow |
| 4 | U | 1 = user mode, 0 = kernel (privileged) mode. The CPU starts in kernel mode |
| 5 | I | 1 = IRQs masked, see [devices.md](devices.md) |

`MSR`/`MRS` access it as system register 1, see [exceptions.md](exceptions.md#system-registers). The other bits are reserved. There are two stack pointers behind `sp`, one for each mode.

### FPCR and FPSR

System registers 10 (`fpcr`) and 11 (`fpsr`), read and written with `MSR`/`MRS`. Unlike the other system registers they can be used in **user mode**, since a C library changes the rounding mode and tests the flags. Both are 0 after reset. Taking an exception does not save or change them, so an operating system keeps them per thread.

FPCR, bits 1–0, the rounding mode of the floating point instructions (the other bits read 0 and ignore writes):

| `RMode` | Rounds |
|---------|--------|
| `00` | to nearest, ties to even |
| `01` | toward +infinity |
| `10` | toward -infinity |
| `11` | toward zero |

FPSR, bits 4–0, the cumulative exception flags. An instruction sets a flag when it raises the exception and never clears one; a program clears them by writing the register. Nothing traps. The other bits read 0 and ignore writes.

| Bit | Flag | Raised when |
|-----|------|-------------|
| 0 | `IOC` invalid operation | the result has no meaning (`0/0`, `inf - inf`, the square root of a negative number), an operand is a signaling NaN, a float to integer conversion is out of range or a NaN |
| 1 | `DZC` division by zero | a finite non-zero number is divided by zero |
| 2 | `OFC` overflow | the rounded result is too large for the format |
| 3 | `UFC` underflow | the result is tiny and inexact |
| 4 | `IXC` inexact | the result had to be rounded |

### Carry flag convention

- **Addition** (`ADD`, `ADC`, `CMN`): C is the carry out.
- **Subtraction** (`SUB`, `RSB`, `SBC`, `RSC`, `CMP`): ARM convention, C = *no borrow* (C = 1 when op1 ≥ op2). This agrees with the `HI`/`LS`/`HS`/`LO` conditions.

## Encoding overview

- Bits 26–31 are the opcode. Opcode `000000` is the [special group](#special-instructions-opcode-000000).
- Bit 25 is the update flags bit (`S`) in the formats that have one, and the precision bit (`p`) in the floating point formats.
- Every format is 32 bits. The diagrams below go from bit 31 down to bit 0.

## Instruction formats

### O: data processing

`OP{S} xd, xn, arg`

Immediate form (bit 14 set):

```
 31   26 25 24  20 19  15 14 13              0
+-------+--+------+------+--+----------------+
|opcode | S|  xd  |  xn  | 1|     imm14      |
+-------+--+------+------+--+----------------+
```

Register form (bit 14 clear):

```
 31   26 25 24  20 19  15 14 13  9 8   7 6    2 1 0
+-------+--+------+------+--+-----+-----+------+---+
|opcode | S|  xd  |  xn  | 0| xm  |shift| imm5 | - |
+-------+--+------+------+--+-----+-----+------+---+
```

`arg` is `imm14` (unsigned) or `xm` shifted by `imm5` according to `shift` (see [Operands](#operands)).

### O1: shifts

`OP{S} xd, xn, {xm | #imm5}`

```
 31   26 25 24  20 19  15 14 13  9 8 7 6    2 1 0
+-------+--+------+------+--+-----+---+------+---+
|opcode | S|  xd  |  xn  |?imm| xm |type| imm5 | - |
+-------+--+------+------+--+-----+---+------+---+
```

- `type` (bits 7–8) is the shift: `00` `LSL`, `01` `LSR`, `10` `ASR`, `11` `ROR`. The four are one opcode (`010000`).
- `?imm` (bit 14) set: the shift amount is `imm5` (bits 2–6).
- `?imm` clear: the shift amount is the **low 5 bits** of `xm`, so 0–31.
- With `S`: N and Z come from the result, C from the last bit shifted out, V is unchanged. A shift amount of 0 leaves C unchanged.

### O2: long multiply

`OP{S} xlo, xhi, xn, xm`

```
 31   26 25 24  20 19  15 14 13  9 8   4 3 1 0
+-------+--+------+------+--+-----+-----+---+-+
|opcode | S| xlo  | xhi  | -|  xn |  xm | - |g|
+-------+--+------+------+--+-----+-----+---+-+
```

`xlo` is bits 20–24, `xhi` 15–19, `xn` 9–13 and `xm` 4–8. Bit 0 (`g`) is the signed bit: clear is `UMULL`, set is `SMULL`, one opcode (`001000`). Bit 14 and bits 1–3 are unused.

### O3: moves

`OP{S} xd, arg`

Immediate form (bit 19 set), `xd = imm19` (zero extended):

```
 31   26 25 24  20 19 18                  0
+-------+--+------+--+--------------------+
|opcode | S|  xd  | 1|       imm19        |
+-------+--+------+--+--------------------+
```

Register form (bit 19 clear), `xd = xn + imm14` (the immediate is added to the register):

```
 31   26 25 24  20 19 18  14 13            0
+-------+--+------+--+------+--------------+
|opcode | S|  xd  | 0|  xn  |    imm14     |
+-------+--+------+--+------+--------------+
```

### M: load and store

`LDR{B|H} xt, mem` / `STR{B|H} xt, mem` / `LDUR{H} xt, mem` / `STUR{H} xt, mem`

Immediate offset (bit 14 set):

```
 31   26 25 24  20 19  15 14 13        2 1  0
+-------+--+------+------+--+-----------+----+
|opcode |?sign| xt |  xn  | 1|  simm12   |adr |
+-------+--+------+------+--+-----------+----+
```

Register offset (bit 14 clear):

```
 31   26 25 24  20 19  15 14 13  9 8   7 6    2 1  0
+-------+--+------+------+--+-----+-----+------+----+
|opcode |?sign| xt |  xn  | 0| xm  |shift| imm5 |adr |
+-------+--+------+------+--+-----+-----+------+----+
```

- `adr` (bits 0–1): `00` simple offset, `01` pre-indexed, `10` post-indexed, `11` simple offset that may be unaligned (`LDUR`, `LDURH`, `STUR`, `STURH`, see [Memory access](#memory-access-6)). `11` on a byte access faults with `BAD_INSTR`.
- `simm12` is sign extended. Range −2048 to 2047.
- `?sign` (bit 25) makes `LDRB`/`LDRH` sign extend the loaded value (`LDRSB`/`LDRSH`). It has no effect on `LDR`, and none on a store (the stored bytes are the low bytes of `xt`).
- A load, store or atomic changes no register until its memory access has succeeded, so an instruction that faults did nothing.
- For pre/post-indexed accesses with `xt` equal to the base register, a store writes the old value of `xt` and a load's value replaces the written back base.

### M1: page address

`ADRP xd, symbol`, `ADR xd, symbol`

```
 31   26 25 24  20 19                 0
+-------+--+------+-------------------+
|opcode |?sign|  xd |      imm20       |
+-------+--+------+-------------------+
```

The offset is a signed 21 bit number. `imm20` holds its low 20 bits and `?sign` (bit 25) is bit 20, set by the linker when the offset is negative.

- `ADRP`: the offset is in pages. The result is `(address of the ADRP & ~0xFFF) + (offset << 12)`.
- `ADR`: the offset is in bytes. The result is `address of the ADR + offset`, so it reaches 1 MiB back or 1 MiB − 1 forward, and it is the exact address (a label of the code or of any section, the linker works the distance out). A symbol further away is a link error ("cannot reach"); use `adrp` + `add` for it.

### B1: branch with offset

`B{cd} simm22`, `BL{cd} simm22`, `SWI{cd} imm22`

```
 31   26 25  22 21                    0
+-------+------+-----------------------+
|opcode | cond |        simm22         |
+-------+------+-----------------------+
```

`simm22` is a signed offset in words (multiplied by 4), relative to the address of the branch instruction itself. For `SWI` the 22 bits are an unsigned number (`swi 3`, `swi` alone is `swi 0`), see [Software interrupts](#software-interrupts-swi).

### B2: branch to register

`BX{cd} xd`, `BLX{cd} xd`

```
 31   26 25  22 21  17 16             1 0
+-------+------+------+----------------+-+
|opcode | cond |  xd  |       -        |l|
+-------+------+------+----------------+-+
```

Bit 0 (`l`) is the link bit: clear is `BX`, set is `BLX`, one opcode (`011110`). Bits 1–16 are unused.

### V1, V2, V3: floating point

`OP.F32 xd, xn` (V1), `OP.F32 xd, xn, xm` (V2), `OP.F32 xn, xm` (V3). `.F64` for a double.

```
 31   26 25 24  20 19  15 14       5 4    0
+-------+--+------+------+----------+------+
|opcode | p|  xd  |  xn  |     -    |  fn  |   V1
+-------+--+------+------+----------+------+

 31   26 25 24  20 19  15 14 13  9 8  5 4    0
+-------+--+------+------+--+-----+-----+------+
|opcode | p|  xd  |  xn  | -|  xm |  -  |  fn  |   V2
+-------+--+------+------+--+-----+-----+------+

 31   26 25 24 23  20 19  15 14 13  9 8          0
+-------+--+--+------+------+--+-----+-------------+
|opcode | p| e|  -   |  xn  | -|  xm |      -      |   V3
+-------+--+--+------+------+--+-----+-------------+
```

- `p` (bit 25) is the precision: clear is `.F32`, set is `.F64`. For the conversion between the two it is the precision of the source.
- `fn` (bits 0–4) is the function of the opcode, see [Floating point](#floating-point-3). A function that is not assigned is an undefined instruction (ISS 1).
- `e` (bit 24) of V3 is set for `VCMPE`.
- A double names the first register of a pair, which has to be x0–x28 (ISS 5 otherwise).
- The unused bits are 0 and are ignored.

## Special instructions (opcode `000000`)

```
 31   26 25  22 21                    0
+-------+------+-----------------------+
|000000 |extop |        imm22          |
+-------+------+-----------------------+
```

The extended op is bits 22–25. An unknown extended op is an [undefined instruction](exceptions.md#exception-classes) (without a vector table: a fault with `BAD_INSTR`).

| ext. op | Instruction | Description |
|---------|-------------|-------------|
| `0000` | `HLT` | stops the program. Privileged |
| `0001` | `MSR sysreg, xn \| imm16` | Privileged, except for the flags of PSTATE and FPCR and FPSR |
| `0010` | `MRS xn, sysreg` | Privileged, except for the flags of PSTATE and FPCR and FPSR |
| `0011` | `TLBI{ xt}` | Privileged. Forgets cached page table translations |
| `0100` | atomic operations | see [Atomic operations](#atomic-operations) |
| `0101` | `ERET` | Privileged. Returns from an exception |
| `0110` | `WFI` | Privileged. Waits for an interrupt, see [devices.md](devices.md#wfi) |
| `0111` | `BRK imm22` | raises the breakpoint exception |
| `1000` | `SXTB`, `SXTH`, `UXTB`, `UXTH`, `CLZ`, `REV`, `REV16` | see [Unary operations](#unary-operations) |
| `1111` | `NOP` | does nothing |

**Privileged** instructions are an undefined instruction (ISS 2) in user mode.

### HLT

Encoding `0x00000000`, so running into zeroed memory halts (in kernel mode).

### NOP

`000000 | 1111 | 0…0`

### ERET

`000000 | 0101 | 0…0`. Returns from an exception: PC = `ELR`, PSTATE = `SPSR`, which also switches the mode and the stack pointer.

### WFI

`000000 | 0110 | 0…0`. Waits for an interrupt: returns when one is pending, jumping time forward to the next timer or block device event. With nothing that could produce one it ends the run like `HLT`. See [devices.md](devices.md#wfi).

### BRK

`000000 | 0111 | imm22`. Raises the breakpoint exception. With a debugger attached, the run stops instead.

### MSR

```
 31   26 25  22 21  17 16 15          11 10      0
+-------+------+------+--+--------------+---------+
|000000 | 0001 |sysreg| 0|      xn      |    -    |   register form
|000000 | 0001 |sysreg| 1|          imm16          |   immediate form
+-------+------+------+--+--------------+---------+
```

Bit 16 is `?imm`. In the register form `xn` is bits 11–15. Moves a value to a system register, see [exceptions.md](exceptions.md#system-registers) for the registers. A register that does not exist is an undefined instruction (ISS 4, "System register N unimplemented."). In user mode only the flags of PSTATE and FPCR and FPSR can be written.

### MRS

```
 31   26 25  22 21  17 16 15   11 10      0
+-------+------+------+--+--------+---------+
|000000 | 0010 |  xn  | -| sysreg |    -    |
+-------+------+------+--+--------+---------+
```

Moves from a system register. In user mode only the flags of PSTATE and FPCR and FPSR can be read.

### TLBI

```
 31   26 25  22 21  17 16 15           0
+-------+------+------+--+--------------+
|000000 | 0011 |  xt  |?xt|    imm16    |
+-------+------+------+--+--------------+
```

`tlbi` forgets every cached page table translation, `tlbi xt` (`?xt` set) only the one of the page that holds the address in `xt`. `imm16` is reserved, 0. Privileged. See [mmu.md](mmu.md#the-tlb-and-tlbi).

### Atomic operations

```
 31   26 25  22 21  17 16 15  11 10  6 5   4 3    0
+-------+------+------+--+------+-----+-----+------+
|000000 | 0100 |  xt  | -|  xn  | xm  |width| atop |
+-------+------+------+--+------+-----+-----+------+
```

`width` (bits 4–5): `00` word, `01` byte (`B`), `10` half-word (`H`). `atop` (bits 0–3) selects the operation. All of them read the memory value at address `xm` into `xt`, then write a new value back:

| atop | Instruction | New value in memory |
|------|-------------|---------------------|
| `0000` | `SWP{B\|H} xt, xn, [xm]` | `xn` |
| `0001` | `LDADD{B\|H} xt, xn, [xm]` | memory + `xn` |
| `0010` | `LDCLR{B\|H} xt, xn, [xm]` | memory & ~`xn` (clears the bits set in `xn`) |
| `0011` | `LDSET{B\|H} xt, xn, [xm]` | memory \| `xn` |

- `xn` is truncated to the width of the access. A byte or half-word read is zero extended into `xt`.
- `xt` is written only after the memory write succeeded.
- A word or half-word access needs an address that is a multiple of its size, like `LDR` and `STR`, and there is no unaligned form. Otherwise it is a [data abort](exceptions.md#exception-classes) of the alignment type (with the write bit, FAR is the address), and nothing is read or written. A byte is always aligned.
- Other `atop` values fault with `BAD_INSTR`.

### Unary operations

`OP xd, xn`, one extended op (`1000`) for a family of operations on one register. They are what a compiler needs for conversions and byte order and would otherwise take two or more instructions. No flags are changed.

```
 31   26 25  22 21  17 16 15  11 10        4 3    0
+-------+------+------+--+------+-----------+------+
|000000 | 1000 |  xd  | -|  xn  |     -     |  op  |
+-------+------+------+--+------+-----------+------+
```

| op | Instruction | Result |
|----|-------------|--------|
| `0000` | `SXTB xd, xn` | the low byte of `xn`, sign extended |
| `0001` | `SXTH xd, xn` | the low half-word, sign extended |
| `0010` | `UXTB xd, xn` | the low byte, zero extended (`and xd, xn, 255`) |
| `0011` | `UXTH xd, xn` | the low half-word, zero extended |
| `0100` | `CLZ xd, xn` | the number of leading zero bits, 32 for 0 |
| `0101` | `REV xd, xn` | the four bytes in reverse order |
| `0110` | `REV16 xd, xn` | the bytes of each half-word swapped |

Other values of `op` are an undefined instruction (ISS 1). `xd` and `xn` may be the same register.

## Instructions by opcode

`{S}` updates the flags. `{CD}` is a [condition code](#condition-codes) (default `AL`).

### Arithmetic (8)

| Opcode | Instruction | Format | Operation |
|--------|-------------|--------|-----------|
| `000001` | `ADD{S} xd, xn, arg` | O | `xd = xn + arg` |
| `000010` | `SUB{S} xd, xn, arg` | O | `xd = xn - arg` |
| `000011` | `RSB{S} xd, xn, arg` | O | `xd = arg - xn` |
| `000100` | `ADC{S} xd, xn, arg` | O | `xd = xn + arg + C` |
| `000101` | `SBC{S} xd, xn, arg` | O | `xd = xn - arg - !C` |
| `000110` | `RSC{S} xd, xn, arg` | O | `xd = arg - xn - !C` |
| `000111` | `MUL{S} xd, xn, arg` | O | `xd = low 32 bits of xn * arg`. `S` updates N and Z, C and V are unchanged |
| `001000` | `UMULL{S} xlo, xhi, xn, xm`<br>`SMULL{S} xlo, xhi, xn, xm` | O2 | `{xhi, xlo} = xn * xm`, unsigned or signed (the signed bit). `S`: N is bit 63, Z is "64 bit result is 0", C and V are unchanged |

### Floating point (3)

A `float` is the IEEE 754 binary32 value in a register, a `double` the binary64 value in a pair of registers (`xN` holds the low word, `xN+1` the high word). The registers are the integer ones, so `mov`, `ldr`, `str`, `csel` and the bit operations move and test floats, and `xzr` is +0.0. This is how [abi.md](abi.md) passes them. The three opcodes have a function field `fn` and the precision bit `p`.

| Opcode | Instruction | Format | Operation |
|--------|-------------|--------|-----------|
| `001001` | `VABS`, `VNEG`, `VSQRT`, `VRINT*`, `VCVT*`, `VCVTR*` | V1 | `xd = fn(xn)`, see below |
| `001010` | `VADD`, `VSUB`, `VMUL`, `VDIV`, `VMIN`, `VMAX` | V2 | `xd = xn fn xm` |
| `001011` | `VCMP`, `VCMPE xn, xm` | V3 | sets N, Z, C and V from the comparison |

Every mnemonic has the suffix `.f32` or `.f64` (`vadd.f32 x0, x1, x2`, `vadd.f64 x0, x2, x4`).

`VOP2` (opcode `001010`), `fn`: `0` `VADD`, `1` `VSUB`, `2` `VMUL`, `3` `VDIV`, `4` `VMIN`, `5` `VMAX`. The operands and the result have the same precision. `VMIN` and `VMAX` are `fmin` and `fmax` of C: a quiet NaN operand is skipped (two of them give the default NaN), a signaling NaN is an invalid operation, and -0 is smaller than +0.

`VOP1` (opcode `001001`), `fn`. The precision of the instruction is that of the floating point operand:

| `fn` | Instruction | Operation | `xd` | `xn` |
|------|-------------|-----------|------|------|
| 0 | `VABS` | clears the sign bit | float | float |
| 1 | `VNEG` | flips the sign bit | float | float |
| 2 | `VSQRT` | square root | float | float |
| 3 | `VRINT` | rounds to an integral value in the mode of FPCR | float | float |
| 4 | `VRINTZ` | toward zero | float | float |
| 5 | `VRINTM` | toward -infinity (floor) | float | float |
| 6 | `VRINTP` | toward +infinity (ceil) | float | float |
| 7 | `VRINTA` | to nearest, ties away from zero (`round`) | float | float |
| 8 | `VCVT.S32.Fx` | float to signed integer, toward zero (the C cast) | int32 | float |
| 9 | `VCVT.U32.Fx` | float to unsigned integer, toward zero | uint32 | float |
| 10 | `VCVTR.S32.Fx` | float to signed integer in the mode of FPCR (`lrint`) | int32 | float |
| 11 | `VCVTR.U32.Fx` | float to unsigned integer in the mode of FPCR | uint32 | float |
| 12 | `VCVT.Fx.S32` | signed integer to float, rounded as FPCR says | float | int32 |
| 13 | `VCVT.Fx.U32` | unsigned integer to float | float | uint32 |
| 14 | `VCVT.F64.F32`, `VCVT.F32.F64` | float to the other precision (`p` is the source's) | the other | the source |

Where a column says float, a `.f64` instruction uses a register pair. Integers are a single register. So `vcvt.s32.f64 x0, x2` reads the pair (x2, x3), `vcvt.f64.s32 x2, x0` writes the pair (x2, x3), and `vcvt.f64.f32 x2, x0` widens x0 into (x2, x3).

The results:

- **Rounding.** The result of `VADD`, `VSUB`, `VMUL`, `VDIV`, `VSQRT` and the conversions to a float is the exact result rounded as the IEEE 754 standard says, in the mode of FPCR. They raise the flags of [FPSR](#fpcr-and-fpsr). Denormal numbers are fully supported (nothing is flushed to zero). Whether tininess for `UFC` is found before or after rounding is up to the host FPU, which the standard allows.
- **NaN.** An operation never passes a NaN on: its result is the default NaN, quiet, positive and with no payload (`0x7FC00000`, `0x7FF8000000000000`), so the same program gives the same bits on every host. A signaling NaN operand raises `IOC`, a quiet one does not. `VABS` and `VNEG` are bit operations and leave a NaN as it is.
- **`VRINT`** raises `IOC` for a signaling NaN and nothing else, not `IXC`.
- **Float to integer** saturates. A NaN gives 0, a number too large the largest integer, a number too small the smallest one (0 for an unsigned integer), all with `IOC`. Otherwise `IXC` says that the number had a fraction. The conversions with `R` round first and then check the range, so `-0.5` is 0 with `IXC` in the nearest mode and out of range for an unsigned integer in the mode toward -infinity.
- **Integer to float** is exact for a double and rounds for a float larger than 2^24, with `IXC`.
- **Float to double** is exact, double to float rounds and can raise `OFC`, `UFC` and `IXC`.
- **`VCMP`** does not change a register but sets the flags like a comparison of integers: equal is `Z` and `C` (`0110`), less than is `N` (`1000`), greater than is `C` (`0010`), and unordered, when an operand is a NaN, is `C` and `V` (`0011`). `+0` equals `-0`. `VCMP` raises `IOC` for a signaling NaN, `VCMPE` for any NaN. The flags are those of the conditions, but note what they do for a NaN:

| Condition | `a<b` | `a==b` | `a>b` | NaN |
|-----------|-------|--------|-------|-----|
| `EQ` | no | yes | no | no |
| `NE` | yes | no | yes | yes |
| `MI` (less than) | yes | no | no | no |
| `LS` (less or equal) | yes | yes | no | no |
| `GT` | no | no | yes | no |
| `GE` | no | yes | yes | no |
| `VS` (unordered) | no | no | no | yes |
| `LT` (`N != V`) | yes | no | no | yes |
| `LE` | yes | yes | no | yes |

So a compiler uses `MI` and `LS` for `<` and `<=`, as `LT` and `LE` are also true for a NaN.

- **Registers.** An operand read through `xzr` is 0, a result written to it is dropped (its flags are not). The sources are read before the result is written, so a result may overwrite an operand, also a half of an overlapping pair. A double in a register that cannot start a pair (x29, `sp`, `xzr`) is an undefined instruction (ISS 5), as is a `fn` that is not assigned (ISS 1). An instruction that faults changes no register, no flag and not FPSR.
- The integer flags are changed by `VCMP` only.
- The assembler has the pseudo instruction `vmov.f32 xd, xm | float`, `vmov.f64`, see [basm-syntax.md](basm-syntax.md#floating-point). There is no instruction for it, a move is `mov`.
- A fused multiply-add is not there yet, see [todo.md](todo.md).

### Bitwise (5)

| Opcode | Instruction | Format | Operation |
|--------|-------------|--------|-----------|
| `001100` | `AND{S} xd, xn, arg` | O | `xn & arg` |
| `001101` | `ORR{S} xd, xn, arg` | O | `xn \| arg` |
| `001110` | `EOR{S} xd, xn, arg` | O | `xn ^ arg` |
| `001111` | `BIC{S} xd, xn, arg` | O | `xn & ~arg` |
| `010000` | `LSL{S}`, `LSR{S}`, `ASR{S}`, `ROR{S} xd, xn, {xm \| #imm5}` | O1 | logical shift left, logical shift right, arithmetic shift right, rotate right (the type) |

- `AND`/`ORR`/`EOR`/`BIC` with `S` update N and Z. C and V are unchanged (the carry out of the shifted `arg` is ignored).
- The shifts take the amount from `imm5` or the low 5 bits of `xm` (0–31). With `S`, N and Z come from the result, C is the last bit shifted out and V is unchanged. A shift of 0 leaves C unchanged.

### Comparison (aliases)

`CMP`, `CMN`, `TST` and `TEQ` have no opcode of their own. They are the flag setting form of `SUB`, `ADD`, `AND` and `EOR` with `xzr` as the destination, which discards the result and keeps the flags. The assembler writes them that way and the disassembler shows such an instruction (`S` set, `xd` = `xzr`) under the short name.

| Instruction | Is | Format |
|-------------|----|--------|
| `CMP xn, arg` | `SUBS xzr, xn, arg` | O |
| `CMN xn, arg` | `ADDS xzr, xn, arg` | O |
| `TST xn, arg` | `ANDS xzr, xn, arg` | O |
| `TEQ xn, arg` | `EORS xzr, xn, arg` | O |

### Data movement (2)

| Opcode | Instruction | Format | Operation |
|--------|-------------|--------|-----------|
| `010001` | `MOV{S} xd, arg` | O3 | `xd = arg` |
| `010010` | `MVN{S} xd, arg` | O3 | `xd = ~arg` |

`S` updates N and Z, C and V are unchanged. `arg` is an `imm19`, or `xn + imm14`.

### Memory access (6)

| Opcode | Instruction | Access |
|--------|-------------|--------|
| `010011` | `LDR xt, mem` | word |
| `010100` | `LDRB xt, mem` | byte (`LDRSB` with `?sign`) |
| `010101` | `LDRH xt, mem` | half-word (`LDRSH` with `?sign`) |
| `010110` | `STR xt, mem` | word |
| `010111` | `STRB xt, mem` | byte |
| `011000` | `STRH xt, mem` | half-word |

All are format M. See [mem](#operands) for the addressing modes.

**Alignment.** `LDR`, `STR`, `LDRH` and `STRH` need an address that is a multiple of the size of the access (4 and 2). An address that is not is a [data abort](exceptions.md#exception-classes) of the alignment type (without a vector table, a fault: "Misaligned load of 4 bytes at address ..."), and the instruction does nothing. `LDUR`, `LDURH`, `STUR` and `STURH` are the same accesses without that requirement; they are the same opcodes with `adr` = `11` (below), so they take a simple offset, an immediate or a register, and have no pre- or post-indexed form. A byte is always aligned, so there is no unaligned form of `LDRB` and `STRB` and `adr` = `11` on them is an undefined instruction. An unaligned access that crosses a page is translated page by page like any other (and faults as a whole if the second page does).

| `adr` | Mode | Instructions |
|-------|------|--------------|
| `00` | offset | `LDR` `LDRB` `LDRH` `STR` `STRB` `STRH` |
| `01` | pre-indexed | the same |
| `10` | post-indexed | the same |
| `11` | offset, any alignment | `LDUR` `LDURH` `LDURSH` `STUR` `STURH` (the opcodes of `LDR`, `LDRH`, `STR` and `STRH`) |

### Division (2)

| Opcode | Instruction | Format | Operation |
|--------|-------------|--------|-----------|
| `011001` | `UDIV{S} xd, xn, arg` | O | `xd = xn / arg`, unsigned |
| `011010` | `SDIV{S} xd, xn, arg` | O | `xd = xn / arg`, signed, rounded toward zero |

`S` updates N and Z from the result, C and V are unchanged. **Dividing by zero gives 0** and raises nothing, and `INT_MIN / -1` is `INT_MIN`. There is no remainder instruction: `r = n - (n / d) * d` (`sdiv t, n, d` / `mul t, t, d` / `sub r, n, t`), which is `n` for a `d` of 0. See [abi.md](abi.md#division).

### Conditional select (1)

| Opcode | Instruction | Format | Operation |
|--------|-------------|--------|-----------|
| `011011` | `CSEL`, `CSINC`, `CSINV`, `CSNEG xd, xn, xm, cond` | C | `xd = cond ? xn : f(xm)` |

```
 31   26 25  22 21  17 16 15  11 10   6 5  4 3    0
+-------+------+------+--+------+------+----+------+
|opcode | cond |  xd  | -|  xn  |  xm  | v  |  -   |
+-------+------+------+--+------+------+----+------+
```

`cond` is a [condition code](#condition-codes). The variant `v` (bits 4–5) says what is written when the condition is false: `00` `CSEL` writes `xm`, `01` `CSINC` writes `xm + 1`, `10` `CSINV` writes `~xm`, `11` `CSNEG` writes `-xm`. When it is true `xd = xn`. The flags are only read. `xzr` reads as 0.

The assembler writes the common cases as aliases, which store the **opposite** condition:

| Alias | Is | Result |
|-------|----|--------|
| `CSET xd, cond` | `CSINC xd, xzr, xzr, !cond` | 1 if `cond` holds, else 0 |
| `CSETM xd, cond` | `CSINV xd, xzr, xzr, !cond` | all ones if `cond` holds, else 0 |
| `CINC xd, xn, cond` | `CSINC xd, xn, xn, !cond` | `xn + 1` if `cond` holds, else `xn` |
| `CINV xd, xn, cond` | `CSINV xd, xn, xn, !cond` | `~xn` if `cond` holds, else `xn` |
| `CNEG xd, xn, cond` | `CSNEG xd, xn, xn, !cond` | `-xn` if `cond` holds, else `xn` |

`!cond` flips the lowest bit of the code (`EQ`↔`NE`, `LT`↔`GE`, ...). `AL` and `NV` have no opposite, so the aliases do not accept them. The disassembler shows these forms as the aliases. This is what `a < b` as a value, `abs` and the conditional negation of a compiler turn into, without a branch.

### Branching (4)

| Opcode | Instruction | Format | Operation |
|--------|-------------|--------|-----------|
| `011100` | `B{CD} simm22` | B1 | `pc += simm22 * 4` |
| `011101` | `BL{CD} simm22` | B1 | `x29 = address of the next instruction`, then branch |
| `011110` | `BX{CD} xd`<br>`BLX{CD} xd` | B2 | `BX`: `pc = xd`, and `BX x29` is `ret`. `BLX` (the link bit): `x29 = address of the next instruction`, and `pc = xd`; `xd` is read before `x29` is written, so `BLX x29` jumps to the old `x29` |
| `011111` | `SWI{CD}` | B1 | [software interrupt](#software-interrupts-swi) |

### Addressing (1)

| Opcode | Instruction | Format | Operation |
|--------|-------------|--------|-----------|
| `100000` | `ADRP xd, symbol` | M1 | `xd = (pc & ~0xFFF) + page offset`. The linker computes the page offset from the relocation (`:hi20:`, which the assembler applies implicitly, so `adrp xd, sym` is enough). Add `:lo12:sym` with `add xd, xd, :lo12:sym` for the full address |
| `100001` | `ADR xd, symbol` | M1 | `xd = pc + byte offset` (±1 MiB), the full address in one instruction. The linker computes the offset from the relocation (`R_EMU32_ADR_PCREL21`) |

## Operands

### arg

The right-hand operand of the O format:

| Syntax | Meaning |
|--------|---------|
| `#imm14` | unsigned 14 bit immediate |
| `xn` | register |
| `xn, shift` | register shifted by an `imm5` |

### shift

`shift` is the 2 bit field that picks how a register operand is shifted by `#imm5` (0–31):

| Code | Operation |
|------|-----------|
| `00` | `LSL #imm5` |
| `01` | `LSR #imm5` |
| `10` | `ASR #imm5` |
| `11` | `ROR #imm5` |

A shifted operand does not change the carry flag.

### mem

The memory address of the M format:

| Mode | Forms |
|------|-------|
| offset | `[reg, #simm12]`, `[rega, regb]`, `[rega, regb, shift]` |
| pre-indexed | `[reg, #simm12]!`, `[rega, regb]!`, `[rega, regb, shift]!` |
| post-indexed | `[reg], #simm12`, `[rega], regb`, `[rega], regb, shift` |

`simm12` is signed (−2048 to 2047). The assembler accepts a leading `-`, e.g. `[sp, -8]!`. Pre-indexed uses the new address for the access, post-indexed uses the old one, and both write the new address back to the base register. The unaligned forms (`LDUR`, `STUR`, ...) take only the `offset` row.

## Condition codes

4 bit field, `AL` is the default.

| Code | Name | Condition |
|------|------|-----------|
| `0000` | `EQ` | Z = 1 |
| `0001` | `NE` | Z = 0 |
| `0010` | `CS`, `HS` | C = 1 |
| `0011` | `CC`, `LO` | C = 0 |
| `0100` | `MI` | N = 1 |
| `0101` | `PL` | N = 0 |
| `0110` | `VS` | V = 1 |
| `0111` | `VC` | V = 0 |
| `1000` | `HI` | C = 1 and Z = 0 |
| `1001` | `LS` | C = 0 or Z = 1 |
| `1010` | `GE` | N = V |
| `1011` | `LT` | N ≠ V |
| `1100` | `GT` | Z = 0 and N = V |
| `1101` | `LE` | Z = 1 or N ≠ V |
| `1110` | `AL` | always |
| `1111` | `NV` | never |

## Software interrupts (`swi`)

`SWI{cd} number` is conditional, and a condition that is false skips it. What it does depends on whether a vector table is installed (`VBAR` is not 0, see [exceptions.md](exceptions.md)):

- With a vector table, every number but 1 raises the supervisor call exception (the system call of an operating system, the number is the syndrome).
- `swi 1` is an **emulator call**, below. Without a vector table `swi` (number 0) is one too, which is what programs without an operating system use. `emu32 --no-semihosting` turns them off (undefined instruction).

For an emulator call the call number is in `x8`. The arguments are in `x0`–`x4` (no call needs more) and a result, if any, is written to `x0`.

| ID | Name | Arguments | Description |
|----|------|-----------|-------------|
| 1000 | `emu_print` | | prints the registers and flags |
| 1001 | `emu_printr` | `x0` reg_id | prints a register |
| 1002 | `emu_printm` | `x0` addr, `x1` size (1–4), `x2` little_endian | prints a value in memory |
| 1003 | `emu_printp` | | prints the NZCV flags |
| 1010 | `emu_assertr` | `x0` reg_id, `x1` min, `x2` max | faults (`FAILED_ASSERT`) if the register is outside [min, max] |
| 1011 | `emu_assertm` | `x0` addr, `x1` size, `x2` little_endian, `x3` min, `x4` max | the same for a memory value |
| 1012 | `emu_assertp` | `x0` pstate_bit, `x1` expected | faults if the PSTATE bit is not the expected value |
| 1020 | `emu_log` | `x0` address of a zero terminated string | prints the string and a newline |
| 1021 | `emu_error` | `x0` address of a zero terminated string | prints the string to the error output and stops the program with a fault (`PROGRAM_ERROR`) |

Any other number faults with `BAD_INSTR` ("Invalid syscall number N").

## Unused opcodes

Opcodes that are not assigned fault with `BAD_INSTR` ("Bad opcode N"). They do **not** halt. The assigned opcodes are `000000` to `100001`, so the free ones are one range:

`100010` to `111111` (30 opcodes)
