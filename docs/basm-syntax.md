# basm assembly language

The syntax of the assembly language `basm` assembles (`.basm` sources, `.binc` headers), and of its preprocessor. The binary formats it produces are in [belf-format.md](belf-format.md). The instruction encodings are in [`core/emulator32bit/notes.txt`](../core/emulator32bit/notes.txt), and the samples in `core/app/programs/`.

Contents: [Program structure](#program-structure) · [Lexical rules](#lexical-rules) · [Registers](#registers) · [Expressions](#expressions) · [Constants (.equ)](#constants-equ) · [Labels and symbols](#labels-and-symbols) · [Instructions](#instructions) · [Directives](#directives) · [Preprocessor](#preprocessor) · [Errors](#errors-and-warnings)

## Program structure

```asm
.global _start              ; the entry point has to be global

.data
greeting:   .asciz "hi"
count:      .word 3

.text
_start:
            adrp    x1, count               ; x1 = page of count
            add     x1, x1, :lo12:count     ; x1 = &count
            ldr     x0, [x1]
loop:       subs    x0, x0, 1
            b.ne    loop
            hlt
```

* A program needs `.global _start` and a `_start:` label, which is where the loader starts it (`ENTRY(symbol)` in a linker script can pick another one).
* Code is only legal in `.text`, data directives (`.word`, `.ascii`, ...) only in `.data`, and `.bss` only reserves space (`.advance`). A label must be inside a section.
* A line holds at most one statement, optionally after a label: `loop: subs x0, x0, 1`.
* `hlt` encodes as `0x00000000`, so running into zeroed memory halts the program. An opcode that is not used faults instead.

## Lexical rules

| | |
|-|-|
| Comments | `; to the end of the line` and `;* a block *;` |
| Line join | a `\` at the end of a line continues it on the next |
| Numbers | `42` decimal, `$2A` hex, `%101` binary, `@17` octal |
| Characters | `'a'`, with escapes such as `'\n'`; a character is a number |
| Strings | `"text"` with escapes, in `.ascii`, `.asciz` and `#include` |
| Names | letters, digits and `_`, not starting with a digit. A name cannot be a keyword (an instruction, a register, a condition code, `PSTATE`): a label or macro parameter called `b` is the branch instruction |

## Registers

`x0`–`x29`, `sp` (= x30) and `xzr` (= x31, reads as 0, writes are discarded).

By convention: `x0`–`x7` are the arguments, `x0` also the return value, `x8` the system call number, `x28` the frame pointer and `x29` the link register (`bl` stores the return address there).

The flags are N, Z, C and V. Subtraction, `cmp` and `sbc` set C when there is **no borrow** (the ARM convention).

## Expressions

Anywhere a number is expected (an immediate, a data value, a directive argument), an expression can be written. The operators are the ones of C, from the highest precedence down:

| | |
|-|-|
| `( )` | grouping |
| `-  +  ~  !` | unary: negate, plus, bitwise not, logical not |
| `*  /  %` | multiply, divide, remainder |
| `+  -` | add, subtract |
| `<<  >>` | shifts (`>>` is arithmetic) |
| `<  <=  >  >=` | comparisons, giving 1 or 0 |
| `==  !=` | equality, giving 1 or 0 |
| `&` | bitwise and |
| `^` | bitwise xor |
| `\|` | bitwise or |
| `&&` | logical and, giving 1 or 0 |
| `\|\|` | logical or, giving 1 or 0 |

All the binary operators associate to the left. The arithmetic is 64 bit and wraps around; `/`, `%`, `>>` and the comparisons treat the values as signed. Division by zero and a shift by a negative amount or by 64 or more are errors. There is no `?:`. Operands are number and character literals, constants (below) and labels. Expressions can nest 200 levels.

The result is then checked against what it is used for. `.byte 256`, `add x0, x1, 99999` (an unsigned 14 bit immediate) and `[x0, 5000]` (a signed 12 bit offset) are errors, while `.byte 0 - 1` is accepted as 255.

### Labels in expressions

The address of a label is only known when the program is linked, so what an expression can do with one is limited:

| Expression | Meaning |
|------------|---------|
| `end - start` | **a number**: the distance between two labels of the same section that are both already defined above |
| `label + 4`, `4 + label`, `label - 4` | the address of the label plus a number. This is not a number, it can only be used where a **symbol** is taken: `.word`, `adrp`, `:lo12:`, `:hi13:`, `:lo19:`, `b`, `bl`. The assembler writes a relocation with an *addend* (see [belf-format.md](belf-format.md#the-addend)) |
| anything else with a label (`-label`, `2 * label`, `label + label`, `label >> 2`, a label where a plain number is needed such as `mov x0, label`) | an error: "the address of 'label' is not known until linking" |
| labels of two different sections (`code - data`) | an error: "are in different sections" |

```asm
.data
table:      .word 10, 20, 30
table_end:
            .word table_end - table     ; 12, a number
            .word table + 8             ; the address of the third entry
            .word table_end - 4         ; the address of the last entry, later in the file or not
.text
_start:     adrp    x1, table + 4       ; the page of the second entry
            add     x1, x1, :lo12:table + 4
            b       done + 4            ; skip the first instruction of done
done:       nop
            nop
```

A constant or label has to be defined **before** an expression uses it as a number (`end - start`). A symbol that is not defined yet (later in the file, or in another file) can still be used as `symbol`, `symbol + 4` or `symbol - 4`, because the linker resolves it.

## Constants (`.equ`)

```asm
.equ    WORD_SIZE, 4
.equ    TABLE_BYTES, table_end - table
.equ    LAST, TABLE_BYTES - WORD_SIZE

        mov     x0, TABLE_BYTES >> 2
        ldr     x1, [sp, -WORD_SIZE]
        .word   LAST
```

* `.equ NAME, expression` gives a number a name. It can be used anywhere a number can, also in the expression of another `.equ`.
* The value is computed where `.equ` is, so everything it uses has to be defined before it.
* A constant belongs to the file (or to the open `.scope`, shadowing a constant of the same name outside) and **is not a symbol of the object file**. Another file cannot see it, and `.global` cannot export it. To share a constant between files, put it in a `.binc` header that both `#include`, or use `#define`/`-D`.
* A name can be a constant or a label in one scope, not both.
* A name alone as the operand of `.word` or of a branch is a constant's value if it is a constant, and an address (a relocation) otherwise: `.word SIZE` stores the number, `.word table` the address of `table`.

## Labels and symbols

* `name:` defines a label at the current position of the section.
* `.global name` makes the symbol visible to other files. `.extern name` declares a symbol of another file (using a symbol without declaring it works the same way; the linker finds it or reports `undefined reference`).
* `.scope` / `.scend` open and close a local scope. A label defined inside is private to the scope and to the scopes inside it, and can have the same name as a label in another scope. A reference looks in the open scope, then outward, then at the file. Macros are wrapped in a scope, which is what makes a macro with a label usable twice.
* Two labels with the same name in one scope are an error.

Taking the address of a symbol takes two instructions, because an instruction cannot hold 32 bits:

```asm
adrp    x0, sym                 ; x0 = the 4 KiB page of sym
add     x0, x0, :lo12:sym       ; x0 = the address of sym
```

`mov` can also take the two halves of an address, `:lo19:` (bits 0–18) and `:hi13:` (bits 19–31), for code that builds it from pieces. A relocation operator is written before the symbol and can be followed by `+ number` / `- number`.

| Operator | Used in | Gives |
|----------|---------|-------|
| `:hi20:` (the default for `adrp`) | `adrp xd, sym` | the page distance from the instruction to the symbol |
| `:lo12:` | `add xd, xn, :lo12:sym` | the low 12 bits of the address |
| `:lo19:` | `mov xd, :lo19:sym` | bits 0–18 of the address |
| `:hi13:` | `mov xd, :hi13:sym` | bits 19–31 of the address |

## Instructions

`{s}` means that a trailing `s` sets the flags (`adds`). `imm14` is an **unsigned** 14 bit immediate, `imm12` a **signed** 12 bit one.

### Data processing

| Form | Instructions |
|------|--------------|
| `op xd, xn, xm[, shift]` or `op xd, xn, imm14` or `op xd, xn, :lo12:sym` | `add{s}` `sub{s}` `rsb{s}` `adc{s}` `sbc{s}` `rsc{s}` `mul{s}` `and{s}` `orr{s}` `eor{s}` `bic{s}` |
| `op xd, xn, xm` or `op xd, xn, imm5` | `lsl{s}` `lsr{s}` `asr{s}` `ror{s}` (with `s`: N, Z and C from the last bit shifted out, V unchanged) |
| `op xn, xm[, shift]` or `op xn, imm14` | `cmp` `cmn` `tst` `teq` (flags only; assembled with `xzr` as the destination) |
| `mov{s} xd, xm` / `mov{s} xd, imm14` / `mov xd, :hi13:sym` / `mov xd, :lo19:sym` | `mov` `mvn` |
| `op xlo, xhi, xn, xm` | `umull{s}` `smull{s}` (the 64 bit product: low word in `xlo`, high word in `xhi`) |

A shift is `lsl n`, `lsr n`, `asr n` or `ror n` after the last register: `add x0, x1, x2, lsl 2`.

### Memory

| Form | Meaning |
|------|---------|
| `ldr xt, [xn]` | load from `xn` |
| `ldr xt, [xn, imm12]` | load from `xn + imm12` |
| `ldr xt, [xn, xm{, lsl n}]` | load from `xn + (xm shifted)` |
| `ldr xt, [xn, imm12]!` | pre-index: `xn += imm12`, then load |
| `ldr xt, [xn], imm12` | post-index: load from `xn`, then `xn += imm12` |

The same forms work for `str`, and for the byte (`ldrb`, `strb`) and halfword (`ldrh`, `strh`) versions. `ldrsb` and `ldrsh` sign-extend. The offset can be an expression (`[sp, -WORD_SIZE]!`).

An access changes no register unless it succeeded. For pre- and post-index accesses a store reads the register before the write-back, a load writes its result last, and the assembler warns when that makes the write-back pointless (a load into its own base, a store of the base, `xzr` as the base, or an offset of 0).

Atomics: `swp`, `ldadd`, `ldclr`, `ldset` with `b` (byte) and `h` (halfword) variants: `swp xt, xn, [xm]`.

### Branches, calls and system

| Form | Meaning |
|------|---------|
| `b[.cond] target` | branch |
| `bl[.cond] target` | branch and store the return address in `x29` |
| `bx[.cond] xn` | branch to the address in `xn` |
| `blx[.cond] xn` | indirect call: return address in `x29` |
| `ret` | `bx x29` |
| `swi[.cond] n` | software interrupt; the call number is in `x8`, the arguments in `x0`–`x5` (`n` is ignored) |
| `adrp xd, sym` | page of a symbol, see above |
| `mrs xd, PSTATE` / `msr PSTATE, xn\|imm16` | read/write the flags register (they assemble, but the emulator faults when it runs them) |
| `hlt`, `nop` | |

`target` is a label (also `label + 4`), or a number: a numeric target is a **signed byte distance from the branch itself**, 4 byte aligned, from -8388608 to 8388604 (`b 8` skips one instruction, `b.ne -2 * 4` goes back two). A constant is a number, not a label, so `b SKIP_BYTES` is a distance.

Condition codes: `eq` `ne` `cs`/`hs` `cc`/`lo` `mi` `pl` `vs` `vc` `hi` `ls` `ge` `lt` `gt` `le` `al` `nv`.

The floating point instructions (`vadd.f32`, ...) are reserved and cannot be assembled yet.

## Directives

| Directive | Meaning |
|-----------|---------|
| `.text` `.data` `.bss` | switch to the section (code, initialised data, zeroed space). A section can be re-entered later and continues where it stopped |
| `.global sym` / `.extern sym` | see [Labels and symbols](#labels-and-symbols); allowed anywhere, also in a macro |
| `.equ name, expr` | constant, see above |
| `.scope` / `.scend` | local scope |
| `.byte` `.dbyte` `.word` `.dword` | 1, 2, 4 and 8 byte values, little endian, comma separated: `.word 1, 2, table + 4`. A value has to fit. `.word` also takes an address (`symbol`, `symbol + 4`); the others cannot |
| `.sbyte` `.sdbyte` `.sword` `.sdword` | the same as above |
| `.char 'a', 'b'` | one byte for each character |
| `.ascii "text"` / `.asciz "text"` | the bytes of the string / and a 0 byte at the end |
| `.advance n` | skip `n` bytes (zeros in `.data`; reserves space in `.bss`; a multiple of 4 in `.text`). `n` of 16 MiB or more is an error |
| `.org n` | move forward to the offset `n` of the section (never backward) |
| `.align n` | pad to a multiple of `n`; the largest alignment of the section is kept in the object file and respected when files are linked |
| `.stop` | stop assembling here and ignore the rest of the file |

`.section` and `.fill` are reserved but not implemented.

## Preprocessor

Preprocessor lines start with `#` and are handled before assembling.

```asm
#include "relative/path.binc"       ; relative to the including file
#include <"file.binc">              ; searched in the -I directories

#define LIMIT 10                    ; a text replacement (also -D LIMIT=10)
#undef LIMIT

#ifdef LIMIT / #ifndef LIMIT
#ifequ A B / #ifnequ A B / #ifless A B / #ifmore A B
#else / #elsedef X / #elsendef X / #elseequ ... / #elseless ... / #elsemore ... / #elsenequ ...
#endif

#macro add_to_x0(a, b)              ; macros are chosen by name and by number of arguments
        add x0, xzr, a
        add x0, x0, b
#macend

        #invoke add_to_x0($12, 4)   ; the body is pasted in a .scope
        #invoke twice(x1) result    ; `result` receives the value of `#macret value`
```

* The comparing conditions compare **numbers by value** (`9` is less than `10`) and anything else as text.
* Macro parameters are replaced textually, so a macro can contain labels (each use is in its own `.scope`) and `#macret` leaves the macro early, optionally with a value.
* Include guards work as in C: `#ifndef X` / `#define X` / `#endif` around a header.
* An error inside a macro or `#define` expansion shows the original source line, then a `note: in expansion of macro 'x'` line.

## Errors and warnings

* An error is reported as `file:line:column: error: message`, with the source line and a caret under the token. The build stops at the first error.
* A **warning** (the pre/post-index cases above, ...) is logged and the build goes on. With `-W error` (or `-wall`) a warning ends the build like an error.
