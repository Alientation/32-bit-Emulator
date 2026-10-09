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
- **FPCR / FPSR**: not implemented ([FPCR](https://developer.arm.com/documentation/100446/0100/aarch64-register-descriptions/fpcr--floating-point-control-register), [FPSR](https://developer.arm.com/documentation/100446/0100/aarch64-register-descriptions/fpsr--floating-point-status-register)). The floating point instructions are reserved, see [Floating point](#floating-point-12-not-implemented).

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
|opcode | S|  xd  |  xn  |?imm| xm |type| imm5 | - |
+-------+--+------+------+--+-----+---+------+---+
```

- `type` (bits 7–8) is the shift: `00` `LSL`, `01` `LSR`, `10` `ASR`, `11` `ROR`. The four are one opcode (`011001`).
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

Bit 0 (`l`) is the link bit: clear is `BX`, set is `BLX`, one opcode (`100111`). Bits 1–16 are unused.

### F: floating point (not implemented)

The opcodes are reserved and the assembler recognizes the mnemonics, but it reports "Instruction not implemented yet". If such a word is executed anyway, the emulator faults with `BAD_INSTR` ("`<name>` is not implemented.").

| Format | Syntax | Layout |
|--------|--------|--------|
| F | `OP.F32 xd, xn` | `opcode(6) - (1) xd(5) xn(5) ---(15)` |
| F1 | `OP.F32 xd, xn, xm` | `opcode(6) - (1) xd(5) xn(5) -(1) xm(5) ---(9)` |
| F2 | `OP.F32 xn, {xm \| 0}` | `opcode(6) ?fimm(1) xn(5) fimm(20)`, or with `?fimm` clear: `xn(5) xm(5) ---(16)` |

`fimm` has 20 bits. How they encode a value is not defined, since floating point is not implemented.

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
| `0001` | `MSR sysreg, xn \| imm16` | Privileged, except for the flags of PSTATE |
| `0010` | `MRS xn, sysreg` | Privileged, except for the flags of PSTATE |
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

Bit 16 is `?imm`. In the register form `xn` is bits 11–15. Moves a value to a system register, see [exceptions.md](exceptions.md#system-registers) for the registers. A register that does not exist is an undefined instruction (ISS 4, "System register N unimplemented."). In user mode only the flags of PSTATE can be written.

### MRS

```
 31   26 25  22 21  17 16 15   11 10      0
+-------+------+------+--+--------+---------+
|000000 | 0010 |  xn  | -| sysreg |    -    |
+-------+------+------+--+--------+---------+
```

Moves from a system register. In user mode only the flags of PSTATE can be read.

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

### Floating point (12), not implemented

Reserved opcodes. The handlers fault with `BAD_INSTR`. See [ARM floating point instructions](https://developer.arm.com/documentation/dui0802/b/Advanced-SIMD-and-Floating-point-Programming--32-bit-/Floating-point-instructions).

| Opcode | Instruction | Format |
|--------|-------------|--------|
| `001001` | `VABS.F32 xd, xn` | F |
| `001010` | `VNEG.F32 xd, xn` | F |
| `001011` | `VSQRT.F32 xd, xn` | F |
| `001100` | `VADD.F32 xd, xn, xm` | F1 |
| `001101` | `VSUB.F32 xd, xn, xm` | F1 |
| `001110` | `VDIV.F32 xd, xn, xm` | F1 |
| `001111` | `VMUL.F32 xd, xn, xm` | F1 |
| `010000` | `VCMP.F32 xn, {xm \| 0}` | F2 |
| `010001` | `VSEL.cond.F32 xd, xn, xm` | F1 |
| `010010` | `VCINT.{u32\|s32}.F32 xd, xn` | F |
| `010011` | `VCFLO.{u32\|s32}.F32 xd, xn` | F |
| `010100` | `VMOV.F32 xd, {xn \| #fimm}` | F2 |

### Bitwise (5)

| Opcode | Instruction | Format | Operation |
|--------|-------------|--------|-----------|
| `010101` | `AND{S} xd, xn, arg` | O | `xn & arg` |
| `010110` | `ORR{S} xd, xn, arg` | O | `xn \| arg` |
| `010111` | `EOR{S} xd, xn, arg` | O | `xn ^ arg` |
| `011000` | `BIC{S} xd, xn, arg` | O | `xn & ~arg` |
| `011001` | `LSL{S}`, `LSR{S}`, `ASR{S}`, `ROR{S} xd, xn, {xm \| #imm5}` | O1 | logical shift left, logical shift right, arithmetic shift right, rotate right (the type) |

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
| `011010` | `MOV{S} xd, arg` | O3 | `xd = arg` |
| `011011` | `MVN{S} xd, arg` | O3 | `xd = ~arg` |

`S` updates N and Z, C and V are unchanged. `arg` is an `imm19`, or `xn + imm14`.

### Memory access (6)

| Opcode | Instruction | Access |
|--------|-------------|--------|
| `011100` | `LDR xt, mem` | word |
| `011101` | `LDRB xt, mem` | byte (`LDRSB` with `?sign`) |
| `011110` | `LDRH xt, mem` | half-word (`LDRSH` with `?sign`) |
| `011111` | `STR xt, mem` | word |
| `100000` | `STRB xt, mem` | byte |
| `100001` | `STRH xt, mem` | half-word |

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
| `100010` | `UDIV{S} xd, xn, arg` | O | `xd = xn / arg`, unsigned |
| `100011` | `SDIV{S} xd, xn, arg` | O | `xd = xn / arg`, signed, rounded toward zero |

`S` updates N and Z from the result, C and V are unchanged. **Dividing by zero gives 0** and raises nothing, and `INT_MIN / -1` is `INT_MIN`. There is no remainder instruction: `r = n - (n / d) * d` (`sdiv t, n, d` / `mul t, t, d` / `sub r, n, t`), which is `n` for a `d` of 0. See [abi.md](abi.md#division).

### Conditional select (1)

| Opcode | Instruction | Format | Operation |
|--------|-------------|--------|-----------|
| `100100` | `CSEL`, `CSINC`, `CSINV`, `CSNEG xd, xn, xm, cond` | C | `xd = cond ? xn : f(xm)` |

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
| `100101` | `B{CD} simm22` | B1 | `pc += simm22 * 4` |
| `100110` | `BL{CD} simm22` | B1 | `x29 = address of the next instruction`, then branch |
| `100111` | `BX{CD} xd`<br>`BLX{CD} xd` | B2 | `BX`: `pc = xd`, and `BX x29` is `ret`. `BLX` (the link bit): `x29 = address of the next instruction`, and `pc = xd`; `xd` is read before `x29` is written, so `BLX x29` jumps to the old `x29` |
| `101000` | `SWI{CD}` | B1 | [software interrupt](#software-interrupts-swi) |

### Addressing (1)

| Opcode | Instruction | Format | Operation |
|--------|-------------|--------|-----------|
| `101001` | `ADRP xd, symbol` | M1 | `xd = (pc & ~0xFFF) + page offset`. The linker computes the page offset from the relocation (`:hi20:`, which the assembler applies implicitly, so `adrp xd, sym` is enough). Add `:lo12:sym` with `add xd, xd, :lo12:sym` for the full address |
| `101010` | `ADR xd, symbol` | M1 | `xd = pc + byte offset` (±1 MiB), the full address in one instruction. The linker computes the offset from the relocation (`R_EMU32_ADR_PCREL21`) |

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

Opcodes that are not assigned fault with `BAD_INSTR` ("Bad opcode N"). They do **not** halt. The assigned opcodes are `000000` to `101010`, so the free ones are one range:

`101011` to `111111` (21 opcodes)
