# BELF: the object, executable and library formats

BELF is the ELF-like binary format of the basm toolchain. One layout is used for relocatable object files (`.bo`) and for executables (`.bexe`); static libraries (`.ba`) are archives of object files. The code is in `core/assembler/src/object_file.cpp` (`ObjectFile`), `static_library.cpp` and `linker.cpp`, the constants in `core/assembler/include/assembler/object_file.h`.

| Extension | What it is | Made by |
|-----------|------------|---------|
| `.bo`     | relocatable object file (`file_type` 1) | assembler |
| `.bexe`   | executable (`file_type` 2), no relocations left | linker |
| `.ba`     | static library: a list of `.bo` files | `basm -ar` |
| `.ld`     | linker script (text, see the end) | written by hand |

All numbers are **little endian**, the instruction words of `.text` too (the same order as in the memory of the emulator). Sizes and offsets are stored in more bytes than they need (8), but the reader keeps 32 bits of most of them. Files written before this was changed (when `.text` was stored most significant byte first) are not detected: they would be read with scrambled instructions, so build them again.

## File layout

```
 offset
 0        header                         24 bytes
          .text                          4 bytes per instruction
          .data                          raw bytes
          .rodata                        raw bytes (read only data)
          .init_array                    raw bytes (words)
          .fini_array                    raw bytes (words)
          .bss                           8 bytes: the size of the section, no contents
          .symtab                        26 bytes per symbol
          .rel.text                      28 bytes per relocation
          .rel.data                      28 bytes per relocation
          .rel.rodata                    28 bytes per relocation
          .rel.init_array                28 bytes per relocation
          .rel.fini_array                28 bytes per relocation
          .rel.bss                       28 bytes per relocation (never used)
          .strtab                        the names, each ended by a 0 byte
 H        section headers                53 bytes per section
 end - 8  H, the offset of the section headers   8 bytes
```

The *contents* are written in this order and always all of them are there (an empty one has size 0). The section *headers* are in the order the assembler added the sections: `.text`, `.data`, `.bss`, `.symtab`, `.rel.text`, `.rel.data`, `.rel.bss`, `.strtab`, then `.rodata`, `.rel.rodata`, `.init_array`, `.rel.init_array`, `.fini_array`, `.rel.fini_array` (these six came later, and were added at the end so the indexes of the older sections, which the symbols hold, did not change). Files from before these sections existed have no `.rodata` and are refused with "has no .rodata section": build them again. To read a file, take the last 8 bytes, go to the section headers, and read each section from the offset and size of its header. A reader fails with a message when the file is too small, the magic is wrong, a section lies outside of the file, or a symbol, name or relocation points to something that does not exist.

### Header (24 bytes)

| Bytes | Field |
|-------|-------|
| 0-3   | magic `BELF` |
| 4-15  | unused, 0 |
| 16-17 | file type: 1 relocatable, 2 executable, 3 shared object (not made yet) |
| 18-19 | target machine: 1 is EMU32 |
| 20-21 | flags (0) |
| 22-23 | number of sections |

### Section header (53 bytes)

| Size | Field | Meaning |
|------|-------|---------|
| 8 | name | index into the string table (the position in the list of names, not a byte offset) |
| 4 | type | 1 `.text`, 2 `.data`, 3 `.bss`, 4 symbol table, 5 `.rel.text`, 6 `.rel.data`, 7 `.rel.bss`, 8 debug (unused), 9 string table, 10 `.rodata`, 11 `.init_array`, 12 `.fini_array`, 13 `.rel.rodata`, 14 `.rel.init_array`, 15 `.rel.fini_array` |
| 8 | start | offset of the section in the file |
| 8 | size | size in bytes (for `.bss`, the size it has in memory) |
| 8 | entry size | size of one entry of the table sections |
| 1 | physical | 1 if the loader puts the section at a physical address (set by the linker, `@P;`) |
| 8 | address | where the linker put the section, 0 in a `.bo` |
| 8 | alignment | what the offset and the address of the section are a multiple of: the largest `.align` of the section, 1 if none |

### Symbol table entry (26 bytes)

| Size | Field | Meaning |
|------|-------|---------|
| 8 | name | index into the string table |
| 8 | value | the offset of the symbol in its section (in an executable, its address) |
| 2 | binding | 0 local, 1 global, 2 weak (an undefined reference), 3 declared weak (`.weak`, `.comm`) |
| 8 | section | index of the section that defines the symbol, `0xFFFFFFFF` if it is not defined in this file |

A label defined in a file is local, or global if `.global` named it. A symbol that is only used (a branch to it, `.word symbol`, `adrp`, ...), or named by `.extern`, is **weak and undefined**. The linker looks every undefined symbol up in the other files. Binding 3 is for `.weak name` and `.comm name, size`: if the symbol is defined in the file it is a *weak definition* (used only if no file has a strong one), and if it is not defined it is a *weak reference* (0 if nothing defines it, instead of the undefined reference error). The symbols of a `.scope` are stored as `name::SCOPE:<id>`; a relocation refers to the symbol of the scope it is in.

`.equ` constants are **not** in the symbol table, they only exist while their file is assembled (see [basm-syntax.md](basm-syntax.md#constants-equ)).

### Relocation entry (28 bytes)

A relocation says: "the instruction (or word) at `offset` in this section needs the address of `symbol` plus `addend`, which is only known when the program is linked".

| Size | Field | Meaning |
|------|-------|---------|
| 8 | offset | from the start of the section; a multiple of 4 in `.text` |
| 8 | symbol | the symbol (its name index, the key of the symbol table) |
| 4 | type | see below |
| 8 | addend | signed number added to the address of the symbol (`table + 8` is the symbol `table` with the addend 8). The reader keeps the low 32 bits, and the arithmetic wraps at 32 bits. |

The patched value is `target = address(symbol) + addend`. In the table below `P` is the address of the instruction.

| # | Type | Where | What goes in |
|---|------|-------|--------------|
| 1 | `R_EMU32_O_LO12` | `add xd, xn, :lo12:sym` and other ALU immediates | the low 12 bits of `target`, in the 14 bit immediate |
| 2 | `R_EMU32_ADRP_HI20` | `adrp xd, sym` | the distance in 4 KiB pages from `P` to `target`, 20 bits plus a sign bit |
| 3 | `R_EMU32_MOV_LO19` | `mov xd, :lo19:sym` | bits 0-18 of `target` |
| 4 | `R_EMU32_MOV_HI13` | `mov xd, :hi13:sym` | bits 19-31 of `target` |
| 5 | `R_EMU32_B_OFFSET22` | `b`, `bl` with a label | `(target - P) / 4` as a signed 22 bit number; `target` has to be 4 byte aligned and within reach |
| 6 | `R_EMU32_ABS32` | `.word sym` in `.data`, `.rodata`, `.init_array`, `.fini_array` | `target` is added to the 32 bit word that is already there |
| 7 | `R_EMU32_ADR_PCREL21` | `adr xd, sym` | `target - P` in bytes, as a signed 21 bit number (20 bits plus a sign bit); within 1 MiB either way |

A relocation of one of the data sections has to be `R_EMU32_ABS32`; the others are for `.text`.

The assembler itself resolves a branch to a label of the same file (a label outside of any `.scope`), because that distance does not depend on where the file ends up. It then writes no relocation. A branch to a label of a `.scope` keeps its relocation, pointed at the symbol of that scope. Everything else is left for the linker, so a `.bo` that came out of the assembler can have relocations of all seven types.

#### The addend

`label + 4`, `label - 4`, `4 + label` and `:lo12:label + 4` (anywhere an address of a symbol is taken) become a relocation of `label` with that addend; the number is not written into the instruction. The symbol can be defined in the file, later in the file, or in another file. Older object files stored 0 in this field and are still valid.

Not representable, so rejected by the assembler: `-label`, `2 * label`, `label + label`, `label >> 2`, and any other use of an address as a number. The difference of two labels of the same section is a plain number and has no relocation.

## What the linker does

1. **Merge the sections.** The `.text`, `.data`, `.rodata`, `.init_array`, `.fini_array` and `.bss` of the object files are put one after another in the order of the files (the files from the command line, then the members of libraries that are needed). Each starts at a multiple of its alignment, and the merged section gets the largest alignment.
2. **Place the sections** at the addresses of the linker script (a section without an address follows the previous one, rounded up to its alignment). Two overlapping sections, or an address that is not aligned, are an error. If the script has a `SECTIONS` command, a section with contents that it does not list is an error too (it would be loaded at address 0).
3. **Merge the symbols.** A symbol defined by two files is an error ("multiple definition"), except for local symbols: those are renamed `name:LOCAL:<file number>`, so two files can have a label of the same name, and except for weak definitions (binding 3): a strong definition replaces a weak one whatever the order, and of two weak ones the first stays. A definition of any kind beats a reference. Each symbol gets its final address (section address + the offset of the file's part in the section + the symbol's value).
4. **Check the entry point:** `_start` (or the symbol of `ENTRY(...)`) has to be defined. Then `__init_array_start`, `__init_array_end`, `__fini_array_start` and `__fini_array_end` are defined as the bounds of those two sections (unless the program defines them itself).
5. **Relocate.** For each relocation, `target = address(symbol) + addend`, patched in with the rule of its type. A symbol that is still undefined is the error `undefined reference to 'name'`, whether it was declared with `.global` or not, except for a symbol that only `.weak` declarations name, which is 0.

The result is written as an executable: the same layout, `file_type` 2, merged sections with their addresses, the merged symbol table, and empty relocation sections. `LoadExecutable` copies the sections to their addresses and refuses a file that still has relocations.

### Default layout

Without `-ld`, the built-in script is used: `.text` at `0x0`, `.rodata` at the first page (4 KiB) after the code with `.init_array` and `.fini_array` directly after it, `.data` at the first page after those, `.bss` directly after `.data`, and `ENTRY(_start)`. So code, read only data and writable data are never on the same page, and the loader gives each page the permissions of its section: `.text` is executable, `.rodata` and the arrays are read only, `.data` and `.bss` are writable. (The page is writable if a custom script lets `.rodata` share a page with `.data`; the linker warns when code and writable data share one.)

## Static libraries (`.ba`)

```
 0   magic "BALB"                      4 bytes
 4   version (1)                       2 bytes
 6   number of members                 8 bytes
     for each member:
       length of the name              8 bytes
       name (the file it was made from)
       length of the object file       8 bytes
       the .bo file, byte for byte
```

`basm -ar` checks that every member is a valid object file when it writes the library. When linking, only the members that define a symbol that is used and not yet defined are linked, repeated until nothing new is needed, so a library can hold more than a program uses.

## Linker scripts (`.ld`)

```
ENTRY(main)               // the entry point is `main`, which is also made `_start`

SECTIONS (
    .text = 0x0;          /* a section, optionally with its address */
    @P;                   // the sections that follow are placed at physical addresses
    .data = 0x2000;
    @V;                   // ... back to virtual addresses
    .bss;                 // no address: right after the previous section
)
```

Both commands are optional, and each statement inside `SECTIONS` ends with `;`. Addresses are numbers that fit in 32 bits (`0x2000` or `8192`). Comments are C style (`//` and `/* */`), not the `;` comments of assembly. A script is selected with `basm -ld script.ld`.

## Looking at a file

`basm -dump` prints an objdump-style listing of every object file and of the executable: the symbols, the `.data` bytes, the disassembled `.text`, and the relocations with their addends (`table+0x8`).

```
Relocations of section .data:
0004: R_EMU32_ABS32       table+0x8

Disassembly of section .text:
   0:   c8000000   adrp   x0, 0
        0: R_EMU32_ADRP_HI20     other+0x4
```
