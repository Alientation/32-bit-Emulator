# Instruction Set Architecture

A simplified ARM-like 32 bit ISA. Instructions are fixed width (4 bytes, little endian) with a 6 bit opcode. The encodings keep the register fields at the same bit positions across formats, as ARM does.

Exceptions, system registers and the privilege levels are in [exceptions.md](exceptions.md). A calling convention and `UDIV`/`SDIV` are drafted in [abi.md](abi.md) (not implemented). Debugging the emulator: [debugging.md](debugging.md).

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
- **FPCR / FPSR**: not implemented ([FPCR](https://developer.arm.com/documentation/100446/0100/aarch64-register-descriptions/fpcr--floating-point-control-register), [FPSR](https://developer.arm.com/documentation/100446/0100/aarch64-register-descriptions/fpsr--floating-point-status-register)). They are only a TODO in the emulator.

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
| 5 | I | 1 = IRQs masked (there are no interrupts yet) |

`MSR`/`MRS` access it as system register 1, see [exceptions.md](exceptions.md#system-registers). The other bits are not assigned yet. There are two stack pointers behind `sp`, one for each mode.

### Carry flag convention

- **Addition** (`ADD`, `ADC`, `CMN`): C is the carry out.
- **Subtraction** (`SUB`, `RSB`, `SBC`, `RSC`, `CMP`): ARM convention, C = *no borrow* (C = 1 when op1 ≥ op2). This agrees with the `HI`/`LS`/`HS`/`LO` conditions.

## Encoding overview

- Bits 26–31 are the opcode. Opcode `000000` is the [special group](#special-instructions-opcode-000000).
- Bit 25 is the update flags bit (`S`) in the formats that have one.
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
|opcode | S|  xd  |  xn  |?imm| xm | - | imm5 | - |
+-------+--+------+------+--+-----+---+------+---+
```

- `?imm` (bit 14) set: the shift amount is `imm5` (bits 2–6).
- `?imm` clear: the shift amount is the **low 5 bits** of `xm`, so 0–31.
- With `S`: N and Z come from the result, C from the last bit shifted out, V is unchanged. A shift amount of 0 leaves C unchanged.

### O2: long multiply

`OP{S} xlo, xhi, xn, xm`

```
 31   26 25 24  20 19  15 14 13  9 8   4 3  0
+-------+--+------+------+--+-----+-----+----+
|opcode | S| xlo  | xhi  | -|  xn |  xm | -  |
+-------+--+------+------+--+-----+-----+----+
```

`xlo` is bits 20–24, `xhi` 15–19, `xn` 9–13 and `xm` 4–8. Bit 14 and bits 0–3 are unused.

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

`LDR{B|H} xt, mem` / `STR{B|H} xt, mem`

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

- `adr` (bits 0–1): `00` simple offset, `01` pre-indexed, `10` post-indexed. `11` faults with `BAD_INSTR`.
- `simm12` is sign extended. Range −2048 to 2047.
- `?sign` (bit 25) makes `LDRB`/`LDRH` sign extend the loaded value (`LDRSB`/`LDRSH`). It has no effect on `LDR`, and none on a store (the stored bytes are the low bytes of `xt`).
- A load, store or atomic changes no register until its memory access has succeeded, so an instruction that faults did nothing.
- For pre/post-indexed accesses with `xt` equal to the base register, a store writes the old value of `xt` and a load's value replaces the written back base.

### M1: page address

`ADRP xd, symbol`

```
 31   26 25 24  20 19                 0
+-------+--+------+-------------------+
|opcode |?sign|  xd |      imm20       |
+-------+--+------+-------------------+
```

The page offset is a signed 21 bit number of pages. `imm20` holds its low 20 bits and `?sign` (bit 25) is bit 20, set by the linker when the offset is negative. The result is `(address of the ADRP & ~0xFFF) + (offset << 12)`.

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
 31   26 25  22 21  17 16               0
+-------+------+------+------------------+
|opcode | cond |  xd  |        -         |
+-------+------+------+------------------+
```

### F: floating point (not implemented)

The opcodes are reserved and the assembler recognizes the mnemonics, but it reports "Instruction not implemented yet". If such a word is executed anyway, the emulator faults with `BAD_INSTR` ("`<name>` is not implemented.").

| Format | Syntax | Layout |
|--------|--------|--------|
| F | `OP.F32 xd, xn` | `opcode(6) - (1) xd(5) xn(5) ---(15)` |
| F1 | `OP.F32 xd, xn, xm` | `opcode(6) - (1) xd(5) xn(5) -(1) xm(5) ---(9)` |
| F2 | `OP.F32 xn, {xm \| 0}` | `opcode(6) ?fimm(1) xn(5) fimm(20)`, or with `?fimm` clear: `xn(5) xm(5) ---(16)` |

The original notes described `fimm` as a 14 bit significand plus a 7 bit exponent (21 bits), which does not fit in the 20 bits left in a 32 bit word. The immediate layout needs to be decided when floating point is implemented.

## Special instructions (opcode `000000`)

```
 31   26 25  22 21                    0
+-------+------+-----------------------+
|000000 |extop |        imm22          |
+-------+------+-----------------------+
```

The extended op is bits 22–25. An unknown extended op is an [undefined instruction](exceptions.md#exception-classes) (without a vector table: a fault with `BAD_INSTR`).

| ext. op | Instruction | Status |
|---------|-------------|--------|
| `0000` | `HLT` | stops the program. Privileged |
| `0001` | `MSR sysreg, xn \| imm16` | implemented. Privileged, except for the flags of PSTATE |
| `0010` | `MRS xn, sysreg` | implemented. Privileged, except for the flags of PSTATE |
| `0011` | `TLBI flags{, xt}` | privileged, not implemented (undefined instruction) |
| `0100` | atomic operations | implemented |
| `0101` | `ERET` | implemented. Privileged |
| `0110` | `WFI` | privileged. Halts for now: there are no interrupts |
| `0111` | `BRK imm22` | implemented |
| `1111` | `NOP` | does nothing |

**Privileged** instructions are an undefined instruction (ISS 2) in user mode.

### HLT

Encoding `0x00000000`, so running into zeroed memory halts (in kernel mode).

### NOP

`000000 | 1111 | 0…0`

### ERET

`000000 | 0101 | 0…0`. Returns from an exception: PC = `ELR`, PSTATE = `SPSR`, which also switches the mode and the stack pointer.

### WFI

`000000 | 0110 | 0…0`. Waits for an interrupt. With no interrupt source it ends the run like `HLT`.

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

Bit 16 is `?imm`. In the register form `xn` is bits 11–15. Moves a value to a system register, see [exceptions.md](exceptions.md#system-registers) for the registers. A register that does not exist is an undefined instruction (ISS 4, "System register N unimplemented."). In user mode only the flags of PSTATE can be written.

### MRS

```
 31   26 25  22 21  17 16 15   11 10      0
+-------+------+------+--+--------+---------+
|000000 | 0010 |  xn  | -| sysreg |    -    |
+-------+------+------+--+--------+---------+
```

Moves from a system register. In user mode only the flags of PSTATE can be read.

### TLBI (not implemented)

```
 31   26 25  22 21  17 16 15           0
+-------+------+------+--+--------------+
|000000 | 0011 |  xt  |?xt|    imm16    |
+-------+------+------+--+--------------+
```

Flushes the TLB, `imm16` says how. Privileged. Always an undefined instruction (ISS 3, "TLBI unimplemented.").

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
- Other `atop` values fault with `BAD_INSTR`.

## Instructions by opcode

`{S}` updates the flags. `{CD}` is a [condition code](#condition-codes) (default `AL`).

### Arithmetic (9)

| Opcode | Instruction | Format | Operation |
|--------|-------------|--------|-----------|
| `000001` | `ADD{S} xd, xn, arg` | O | `xd = xn + arg` |
| `000010` | `SUB{S} xd, xn, arg` | O | `xd = xn - arg` |
| `000011` | `RSB{S} xd, xn, arg` | O | `xd = arg - xn` |
| `000100` | `ADC{S} xd, xn, arg` | O | `xd = xn + arg + C` |
| `000101` | `SBC{S} xd, xn, arg` | O | `xd = xn - arg - !C` |
| `000110` | `RSC{S} xd, xn, arg` | O | `xd = arg - xn - !C` |
| `000111` | `MUL{S} xd, xn, arg` | O | `xd = low 32 bits of xn * arg`. `S` updates N and Z, C and V are unchanged |
| `001000` | `UMULL{S} xlo, xhi, xn, xm` | O2 | `{xhi, xlo} = xn * xm` unsigned. `S`: N is bit 63, Z is "64 bit result is 0", C and V are unchanged |
| `001001` | `SMULL{S} xlo, xhi, xn, xm` | O2 | the same, signed |

### Floating point (12), not implemented

Reserved opcodes. The handlers fault with `BAD_INSTR`. See [ARM floating point instructions](https://developer.arm.com/documentation/dui0802/b/Advanced-SIMD-and-Floating-point-Programming--32-bit-/Floating-point-instructions).

| Opcode | Instruction | Format |
|--------|-------------|--------|
| `001010` | `VABS.F32 xd, xn` | F |
| `001011` | `VNEG.F32 xd, xn` | F |
| `001100` | `VSQRT.F32 xd, xn` | F |
| `001101` | `VADD.F32 xd, xn, xm` | F1 |
| `001110` | `VSUB.F32 xd, xn, xm` | F1 |
| `001111` | `VDIV.F32 xd, xn, xm` | F1 |
| `010000` | `VMUL.F32 xd, xn, xm` | F1 |
| `010001` | `VCMP.F32 xn, {xm \| 0}` | F2 |
| `010010` | `VSEL.cond.F32 xd, xn, xm` | F1 |
| `010011` | `VCINT.{u32\|s32}.F32 xd, xn` | F |
| `010100` | `VCFLO.{u32\|s32}.F32 xd, xn` | F |
| `010101` | `VMOV.F32 xd, {xn \| #fimm}` | F2 |

### Bitwise (8)

| Opcode | Instruction | Format | Operation |
|--------|-------------|--------|-----------|
| `010110` | `AND{S} xd, xn, arg` | O | `xn & arg` |
| `010111` | `ORR{S} xd, xn, arg` | O | `xn \| arg` |
| `011000` | `EOR{S} xd, xn, arg` | O | `xn ^ arg` |
| `011001` | `BIC{S} xd, xn, arg` | O | `xn & ~arg` |
| `011010` | `LSL{S} xd, xn, {xm \| #imm5}` | O1 | logical shift left |
| `011011` | `LSR{S} xd, xn, {xm \| #imm5}` | O1 | logical shift right |
| `011100` | `ASR{S} xd, xn, {xm \| #imm5}` | O1 | arithmetic shift right |
| `011101` | `ROR{S} xd, xn, {xm \| #imm5}` | O1 | rotate right |

- `AND`/`ORR`/`EOR`/`BIC` with `S` update N and Z. C and V are unchanged (the carry out of the shifted `arg` is ignored).
- The shifts take the amount from `imm5` or the low 5 bits of `xm` (0–31). With `S`, N and Z come from the result, C is the last bit shifted out and V is unchanged. A shift of 0 leaves C unchanged.

### Comparison (4)

They always update NZCV and have no destination register (the assembler encodes `xzr` as `xd`).

| Opcode | Instruction | Format | Like |
|--------|-------------|--------|------|
| `011110` | `CMP xn, arg` | O | `SUBS` |
| `011111` | `CMN xn, arg` | O | `ADDS` |
| `100000` | `TST xn, arg` | O | `ANDS` |
| `100001` | `TEQ xn, arg` | O | `EORS` |

### Data movement (2)

| Opcode | Instruction | Format | Operation |
|--------|-------------|--------|-----------|
| `100010` | `MOV{S} xd, arg` | O3 | `xd = arg` |
| `100011` | `MVN{S} xd, arg` | O3 | `xd = ~arg` |

`S` updates N and Z, C and V are unchanged. `arg` is an `imm19`, or `xn + imm14`.

### Memory access (6)

| Opcode | Instruction | Access |
|--------|-------------|--------|
| `100100` | `LDR xt, mem` | word |
| `100101` | `LDRB xt, mem` | byte (`LDRSB` with `?sign`) |
| `100110` | `LDRH xt, mem` | half-word (`LDRSH` with `?sign`) |
| `100111` | `STR xt, mem` | word |
| `101000` | `STRB xt, mem` | byte |
| `101001` | `STRH xt, mem` | half-word |

All are format M. See [mem](#operands) for the addressing modes.

### Division (2)

| Opcode | Instruction | Format | Operation |
|--------|-------------|--------|-----------|
| `101010` | `UDIV{S} xd, xn, arg` | O | `xd = xn / arg`, unsigned |
| `101011` | `SDIV{S} xd, xn, arg` | O | `xd = xn / arg`, signed, rounded toward zero |

`S` updates N and Z from the result, C and V are unchanged. **Dividing by zero gives 0** and raises nothing, and `INT_MIN / -1` is `INT_MIN`. There is no remainder instruction: `r = n - (n / d) * d` (`sdiv t, n, d` / `mul t, t, d` / `sub r, n, t`), which is `n` for a `d` of 0. See [abi.md](abi.md#division).

### Branching (5)

| Opcode | Instruction | Format | Operation |
|--------|-------------|--------|-----------|
| `101101` | `B{CD} simm22` | B1 | `pc += simm22 * 4` |
| `101110` | `BL{CD} simm22` | B1 | `x29 = address of the next instruction`, then branch |
| `101111` | `BX{CD} xd` | B2 | `pc = xd`. `BX x29` is `ret` |
| `110000` | `BLX{CD} xd` | B2 | `x29 = address of the next instruction`, then `pc = xd` |
| `110001` | `SWI{CD}` | B1 | [software interrupt](#software-interrupts-swi) |

### Addressing (1)

| Opcode | Instruction | Format | Operation |
|--------|-------------|--------|-----------|
| `110010` | `ADRP xd, symbol` | M1 | `xd = (pc & ~0xFFF) + page offset`. The linker computes the page offset from the relocation (`:hi20:`, which the assembler applies implicitly, so `adrp xd, sym` is enough). Add `:lo12:sym` with `add xd, xd, :lo12:sym` for the full address |

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

`simm12` is signed (−2048 to 2047). The assembler accepts a leading `-`, e.g. `[sp, -8]!`. Pre-indexed uses the new address for the access, post-indexed uses the old one, and both write the new address back to the base register.

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

Opcodes that are not assigned fault with `BAD_INSTR` ("Bad opcode N"). They do **not** halt. The free opcodes are:

`101100`, `110011`, `110100`, `110101`, `110110`, `110111`, `111000`, `111001`, `111010`, `111011`, `111100`, `111101`, `111110`, `111111`
