# basm assembly language

The syntax of the assembly language `basm` assembles (`.basm` sources, `.binc` headers), and of its preprocessor. The binary formats it produces are in [belf-format.md](belf-format.md). The instructions and their encodings are in [isa.md](isa.md), and sample programs are in `core/app/programs/`.

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
* Code is only legal in `.text` and in a section made executable with `.section "name", "rx"`, data directives (`.word`, `.ascii`, ...) only in the sections that hold data (`.data`, `.rodata`, `.init_array`, `.fini_array` and the sections of your own), and `.bss` only reserves space (`.advance`). A label must be inside a section.
* A line holds at most one statement, optionally after a label: `loop: subs x0, x0, 1`.
* `hlt` encodes as `0x00000000`, so running into zeroed memory halts the program. An opcode that is not used faults instead.

## Lexical rules

| | |
|-|-|
| Comments | `; to the end of the line` and `;* a block *;` |
| Line join | a `\` at the end of a line continues it on the next |
| Numbers | `42` decimal, `0x2A` hex, `0b101` binary, `0o17` octal (the prefix can be upper case; a leading zero is not octal, `010` is 10). The old `$2A`, `%101` and `@17` are errors that say what to write instead |
| Characters | `'a'`, with escapes such as `'\n'`; a character is a number |
| Strings | `"text"` with escapes, in `.ascii`, `.asciz` and `#include` |
| Names | letters, digits and `_`, not starting with a digit. A name cannot be a keyword (an instruction, a register, a condition code, `eret`, `wfi`, `brk`; the names of system registers are only special after `msr`/`mrs`): a label or macro parameter called `b` is the branch instruction |

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
* `.weak name` makes the symbol weak. If the file defines it, another file may define it too: a definition without `.weak` wins whatever the order of the files, and of several weak definitions the first one linked is used (a default that can be replaced). If **nothing** defines it, its value is 0 and it is not an error (`adrp x0, name` / `add x0, x0, :lo12:name` gives 0, a test for "is it there"). A weak reference does not pull a member out of a library. A branch to a weak symbol is left to the linker, even to a label of the same file.
* `.comm name, size{, alignment}` reserves `size` zeroed bytes in `.bss` for a symbol that several files may declare, without being in a section when written (it does not change the current section). It is a weak definition in `.bss`: a file that defines `name` for real replaces it, and the files that only reserve it share the space of the first one linked. Give the same size in every file, the linker does not compare them (the others' space is wasted).

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
| `op xd, xn, xm[, shift]` or `op xd, xn, imm14` or `op xd, xn, :lo12:sym` | `add{s}` `sub{s}` `rsb{s}` `adc{s}` `sbc{s}` `rsc{s}` `mul{s}` `udiv{s}` `sdiv{s}` `and{s}` `orr{s}` `eor{s}` `bic{s}` (a division by zero is 0, there is no remainder instruction, see [abi.md](abi.md#division)) |
| `op xd, xn, xm` or `op xd, xn, imm5` | `lsl{s}` `lsr{s}` `asr{s}` `ror{s}` (with `s`: N, Z and C from the last bit shifted out, V unchanged) |
| `op xn, xm[, shift]` or `op xn, imm14` | `cmp` `cmn` `tst` `teq` (flags only; they are `subs`, `adds`, `ands` and `eors` with `xzr` as the destination, so `cmp x1, 5` and `subs xzr, x1, 5` are the same instruction) |
| `mov{s} xd, xm` / `mov{s} xd, imm19` / `mov xd, :hi13:sym` / `mov xd, :lo19:sym` | `mov` `mvn` (`imm19` is an unsigned 19 bit number, 0 to 524287) |
| `ldr xd, =constant` / `ldr xd, =symbol` | a **pseudo instruction** that loads any 32 bit constant or an address, see below |
| `op xlo, xhi, xn, xm` | `umull{s}` `smull{s}` (the 64 bit product: low word in `xlo`, high word in `xhi`) |

A shift is `lsl n`, `lsr n`, `asr n` or `ror n` after the last register: `add x0, x1, x2, lsl 2`.

#### `ldr xd, =value`

Loads a value an instruction cannot hold. There is **no literal pool**: a load cannot reach one (nothing is pc relative), and any constant takes at most three instructions that touch no memory and no other register:

| Value | Code |
|-------|------|
| 0 to 524287 | `mov xd, value` |
| -524288 to -1 (also `0xFFF80000` and up) | `mvn xd, ~value` |
| any other | `mov xd, value >> 14` / `lsl xd, xd, 14` / `orr xd, xd, value & 0x3FFF` (the `orr` is left out when those bits are 0) |
| a symbol (`=table`, `=table + 8`) | `adrp xd, sym` / `add xd, xd, :lo12:sym`, with the relocations of those |

The value is an expression that is a number, -2147483648 to 4294967295, or a symbol with an optional offset. Only `ldr` has this form (not `ldrb`, `ldrh`).

### Conditional select and unary operations

| Form | Instructions |
|------|--------------|
| `op xd, xn, xm, cond` | `csel` (`xd = cond ? xn : xm`), `csinc` (`xm + 1`), `csinv` (`~xm`), `csneg` (`-xm`) |
| `op xd, cond` | `cset` (1 or 0), `csetm` (all ones or 0) |
| `op xd, xn, cond` | `cinc` (`cond ? xn + 1 : xn`), `cinv`, `cneg` |
| `op xd, xn` | `sxtb` `sxth` (sign extend a byte / half-word), `uxtb` `uxth` (zero extend), `clz` (count leading zeros), `rev` (reverse the bytes), `rev16` (swap the bytes of each half-word) |

`cond` is a bare condition code as the last operand (`cset x0, lt`), not `.lt`. None of these changes the flags, see [isa.md](isa.md#conditional-select-1). These mnemonics are keywords like the others, so a symbol cannot have one of these names where it is *used* (`b rev` is not a branch to a label `rev`).

### Memory

| Form | Meaning |
|------|---------|
| `ldr xt, [xn]` | load from `xn` |
| `ldr xt, [xn, imm12]` | load from `xn + imm12` |
| `ldr xt, [xn, xm{, lsl n}]` | load from `xn + (xm shifted)` |
| `ldr xt, [xn, imm12]!` | pre-index: `xn += imm12`, then load |
| `ldr xt, [xn], imm12` | post-index: load from `xn`, then `xn += imm12` |

The same forms work for `str`, and for the byte (`ldrb`, `strb`) and halfword (`ldrh`, `strh`) versions. `ldrsb` and `ldrsh` sign-extend. The offset can be an expression (`[sp, -WORD_SIZE]!`).

**Alignment.** `ldr`, `str`, `ldrh` and `strh` fault when the address is not a multiple of 4 (word) or 2 (halfword): "Misaligned load of 4 bytes at address ..." and, with a vector table, a data abort. `ldur`, `stur`, `ldurh`, `sturh` (and `ldursh`, which sign-extends) are the same accesses at any address, for packed data and unaligned buffers. They take `[xn]`, `[xn, imm12]` and `[xn, xm{, lsl n}]` only: there is no pre- or post-indexed form (an error). A byte is always aligned, so `ldrb` and `strb` have no unaligned form. Use `.align` to put data where `ldr` can read it (`.align 4` before a `.word`).

An access changes no register unless it succeeded. For pre- and post-index accesses a store reads the register before the write-back, a load writes its result last, and the assembler warns when that makes the write-back pointless (a load into its own base, a store of the base, `xzr` as the base, or an offset of 0).

Atomics: `swp`, `ldadd`, `ldclr`, `ldset` with `b` (byte) and `h` (halfword) variants: `swp xt, xn, [xm]`. The word and halfword ones need an address that is a multiple of 4 or 2, like `ldr` (a data abort otherwise: "Misaligned atomic access of 4 bytes at address ..."), and have no unaligned form.

### Branches, calls and system

| Form | Meaning |
|------|---------|
| `b[.cond] target` | branch |
| `bl[.cond] target` | branch and store the return address in `x29` |
| `bx[.cond] xn` | branch to the address in `xn` |
| `blx[.cond] xn` | indirect call: return address in `x29` |
| `ret` | `bx x29` |
| `swi[.cond] [n]` | software interrupt, `n` is a number from 0 to 4194303 (0 if left out), not an offset. With a vector table installed it is a system call of an operating system; `swi 1` and (without a table) `swi` are the emulator calls, with the call number in `x8` and the arguments in `x0`–`x4`. See [exceptions.md](exceptions.md#swi-and-the-emulator-calls) |
| `adrp xd, sym` | page of a symbol, see above |
| `adr xd, sym` | the address of a symbol in one instruction, relative to the instruction: the symbol has to be within 1 MiB (`adr xd, table + 8` works, no `:lo12:` or `:hi20:`). Further away it is a link error, use `adrp` + `add` |
| `mrs xd, sysreg` / `msr sysreg, xn\|imm16` | read/write a system register: `pstate` `elr` `spsr` `esr` `far` `vbar` `usp` `ptbr` `sctlr` (any case). Privileged, except the flags of `pstate`. See [exceptions.md](exceptions.md#system-registers) |
| `tlbi [xn]` | forget the cached page table translation of the page of `xn`, or all of them (privileged), see [mmu.md](mmu.md) |
| `eret` | return from an exception (privileged) |
| `wfi` | wait for an interrupt (privileged; halts while there are no interrupts) |
| `brk [n]` | breakpoint exception, `n` is a number up to 4194303 |
| `hlt` (privileged), `nop` | |

`target` is a label (also `label + 4`), or a number: a numeric target is a **signed byte distance from the branch itself**, 4 byte aligned, from -8388608 to 8388604 (`b 8` skips one instruction, `b.ne -2 * 4` goes back two). A constant is a number, not a label, so `b SKIP_BYTES` is a distance.

Condition codes: `eq` `ne` `cs`/`hs` `cc`/`lo` `mi` `pl` `vs` `vc` `hi` `ls` `ge` `lt` `gt` `le` `al` `nv`.

The floating point instructions (`vadd.f32`, ...) are reserved and cannot be assembled yet.

## Directives

| Directive | Meaning |
|-----------|---------|
| `.text` `.data` `.bss` | switch to the section (code, initialised data, zeroed space). A section can be re-entered later and continues where it stopped |
| `.rodata` | read only data: strings, tables, constants. Same directives as `.data`, but the program cannot write it and it is not executable. Starts on a page of its own |
| `.init_array` / `.fini_array` | tables of function addresses (`.word start_up`) that the program's startup code calls before `main` and after it. The linker joins the arrays of all files and defines the symbols `__init_array_start`, `__init_array_end`, `__fini_array_start` and `__fini_array_end` around them (also when empty). Read only. The emulator does not call the entries, the startup code of the program (`crt0`) does |
| `.global sym` / `.extern sym` / `.weak sym` / `.comm sym, size` | see [Labels and symbols](#labels-and-symbols); allowed anywhere, also in a macro |
| `.equ name, expr` | constant, see above |
| `.scope` / `.scend` | local scope |
| `.byte` `.dbyte` `.word` `.dword` | 1, 2, 4 and 8 byte values, little endian, comma separated: `.word 1, 2, table + 4`. A value has to fit. `.word` also takes an address (`symbol`, `symbol + 4`); the others cannot |
| `.fill count{, size{, value}}` | `count` copies of a `size` byte value (1, 2, 4 or 8; default 1), little endian. The value defaults to 0, has to fit, and with size 4 can be an address like `.word`'s: `.fill 16, 4, 0xDEADBEEF`. For zeros `.advance` is shorter |
| `.ascii "text"` / `.asciz "text"` | the bytes of the string / and a 0 byte at the end. A character is a number, so `.byte 'a', 'b'` is one byte for each character |
| `.advance n` | skip `n` bytes (zeros in the data sections, which is the zero fill of `.data`; reserves space in `.bss`; a multiple of 4 in `.text`). `n` of 16 MiB or more is an error. Zeros in `.data` take room in the file, a large zeroed array belongs in `.bss` or in a `nobits` section |
| `.org n` | move forward to the offset `n` of the section (never backward) |
| `.align n` | pad to a multiple of `n`; the largest alignment of the section is kept in the object file and respected when files are linked |
| `.stop` | stop assembling here and ignore the rest of the file |

### Sections of your own

`.section "name"` or `.section "name", "flags"` makes a section with a name you choose the current one, and creates it the first time. The flags are a string of the letters `r` (read), `w` (write) and `x` (execute):

| Flags | The program can | Like |
|-------|-----------------|------|
| `"r"` | read it | `.rodata` |
| `"rw"` (the default, without flags) | read and write it | `.data` |
| `"rx"` | read it and run code from it | `.text` |

A section is never writable and executable at once (`"rwx"` is an error), and the flags of a section cannot change once it is made. The directives that work in `.data` work in all of them (`.word`, `.ascii`, `.align`, `.advance`, `.org`, ...; the data is in the file, unless the section is `nobits`, below), and **instructions** are legal in an executable one, starting at a multiple of 4 bytes (a `.byte` before an instruction needs `.align 4`; the size has to end up a whole number of instructions). A name can be any text without spaces, `".rel..."`, `".symtab"` and `".strtab"` are taken. The names of the sections of the assembler are those sections: `.section ".data"` is `.data`, and the flags (if given) have to be the ones it has.

```
.section "vectors", "rx"        ; code that a linker script puts at VBAR
vectors:    b       reset
            b       trap
.section "tables", "r"
squares:    .word 0, 1, 4, 9, 16
.section "counters"             ; "rw"
hits:       .word 0
```

The linker joins the sections of the same name of all files (they have to have the same flags), in the order of the files, each at a multiple of its alignment. A branch from or to a section of your own is resolved by the linker, which sees the final addresses. Without a linker script (the default layout) the executable sections come right after `.text`, the read only ones after `.rodata` and the arrays, on the pages after the code, and the writable ones after `.data`. A linker script places them by name, as a string: `".vectors" = 0x800;`, see [belf-format.md](belf-format.md#linker-scripts-ld); a section with contents that the script does not list is an error.

**Zero filled sections.** A third operand, `"nobits"`, makes the section zero filled like `.bss`: `.section "name", "rw", "nobits"` (the flags have to be `"rw"`). It has a size and no contents, so a large one costs nothing in the `.bo` and the `.bexe`; the loader gives the program the zeroed memory. Only labels, `.advance`, `.align` and `.org` are legal in it (no data, no code), and every file that has the section has to say `"nobits"`. Unlike `.bss` there can be any number of them and the linker script can place each one by name. The name `.bss` is the `.bss` (`.section ".bss", "rw", "nobits"`).

```
.section "stack", "rw", "nobits"
.align 16
stack_bottom:   .advance 4096
stack_top:                      ; the address after the last byte
```

**Visiting another section.** `.pushsection "name"[, "flags"[, "nobits"]]` remembers the section you are in and switches to another one (the operands of `.section`, which it makes if it is new); `.popsection` goes back to where you were. They nest, and every `.pushsection` needs its `.popsection`. This is for a macro or a statement that puts something (a string, a table entry) in `.rodata` or a section of its own and then carries on in the code:

```
        adr     x0, message
.pushsection ".rodata", "r"
message:        .asciz "hello"
.popsection
        bl      print                   ; still in .text
```

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

        #invoke add_to_x0(0x12, 4)  ; the body is pasted in a .scope
        #invoke twice(x1) result    ; `result` receives the value of `#macret value`
```

* The comparing conditions compare **numbers by value** (`9` is less than `10`) and anything else as text.
* Macro parameters are replaced textually, so a macro can contain labels (each use is in its own `.scope`) and `#macret` leaves the macro early, optionally with a value.
* Include guards work as in C: `#ifndef X` / `#define X` / `#endif` around a header.
* An error inside a macro or `#define` expansion shows the original source line, then a `note: in expansion of macro 'x'` line.

## Errors and warnings

* An error is reported as `file:line:column: error: message`, with the source line and a caret under the token. The build stops at the first error.
* A **warning** (the pre/post-index cases above, ...) is logged and the build goes on. With `-W error` (or `-wall`) a warning ends the build like an error.
