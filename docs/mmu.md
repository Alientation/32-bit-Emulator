# The MMU: page tables that the operating system owns

**Status: implemented** (`VirtualMemory`, `virtual_memory.h`), as an opt-in mode next to the old one. Setting `SCTLR.M` makes the CPU translate every address by walking a two level table in physical memory that the program (the OS) builds and maintains. Page faults are the ordinary [data and instruction aborts](exceptions.md#exception-classes), so a kernel can allocate, swap and share pages itself, and the C++ swapping in `VirtualMemory` is not involved.

## Two modes

| `SCTLR.M` | Translation |
|-----------|-------------|
| 0 (reset) | the original behavior: the emulator pages the virtual pages of a process in and out of RAM itself (LRU, a disk behind it), `emu32 -e` loads programs this way. Nothing below applies |
| 1 | the page tables at `PTBR` |

So existing programs and the toolchain keep working unchanged, and a kernel opts in by writing `PTBR` and then `SCTLR`. `reset()` turns it off. Everything below is about mode 1.

## Addresses and tables

A virtual address is 32 bits with 4 KiB pages:

```
 31        22 21        12 11          0
+------------+------------+-------------+
|  L1 index  |  L2 index  | page offset |
+------------+------------+-------------+
     10 bits     10 bits       12 bits
```

`PTBR` holds the **physical** address of the first level table (a page, so its low 12 bits are 0 when you write it; they read back as 0). Each table is one page of 1024 four byte entries. The walk reads `PTBR + 4 * L1 index`; if that entry is valid, its frame is the second level table, and `4 * L2 index` into it is the page entry.

### Entries

```
 31                      12 11     6  5  4  3  2  1  0
+--------------------------+--------+--+--+--+--+--+--+
|   physical page number   |  free  | D| A| U| X| W| V|
+--------------------------+--------+--+--+--+--+--+--+
```

| Bit | Name | Meaning |
|-----|------|---------|
| 0 | `V` valid | clear: any access is a translation fault. The other bits are then free for the OS (a swap slot, say) |
| 1 | `W` | the page can be written |
| 2 | `X` | code can be fetched from the page |
| 3 | `U` | user mode may use the page |
| 4 | `A` accessed | set by the **hardware** on any access |
| 5 | `D` dirty | set by the hardware on a write |
| 6–11 | | ignored by the hardware, free for the OS |
| 12–31 | | physical page number (the frame) |

In a **first level** entry only `V` and the frame are used (the table it points to is the second level); the permissions are all in the second level entry. A page can be read whenever it is valid and the mode allows it, there is no separate read bit.

### Permissions

| Access | Kernel mode | User mode |
|--------|-------------|-----------|
| read | valid | valid and `U` |
| write | valid and `W` | valid, `W` and `U` |
| fetch | valid and `X`, and **not** `U` | valid, `X` and `U` |

The kernel can read and write user pages, which is what copying a system call's arguments needs, but never runs code from a user page. The kernel's own pages (no `U`) are out of reach for user mode. This is what ties the page attributes to `PSTATE.U`.

## Faults

An access that is not allowed does nothing and raises an abort, with the same retry rules as every exception (nothing was changed, so the handler can fix the table and `ERET` to the same instruction). A fetch raises an instruction abort, a load, store or atomic a data abort. `ESR` has the cause and `FAR` the address (see [exceptions.md](exceptions.md#esr-exception-syndrome)):

| ISS (bits 0-2) | Cause here |
|----------------|------------|
| 1 translation | the first or second level entry is not valid, or the table itself is outside of memory |
| 2 permission | a write to a page without `W`, a fetch from a page without `X`, user mode on a page without `U`, the kernel fetching from a `U` page |

Bit 3 of a data abort's ISS is set for a write. An access that crosses into a second page translates both before anything is written.

## Accessed and dirty

The hardware writes the bits back to the page entry in memory: `A` on any translation, `D` also when the access is a write. An OS uses them for page replacement (clear `A` periodically, evict pages that stay clear) and to know which pages must be written to swap. Because a translation is cached, `A` is set when the entry is first loaded and not on every access; clearing it needs a `TLBI` to be seen again. A write to a page that is cached as clean takes the slow path once, to set `D`.

## The TLB and `TLBI`

Translations are cached in a TLB of 4096 entries, indexed by the page number. The cache is **visible to software**: changing a page entry does not change what a cached translation does until it is forgotten. The kernel must run

| Instruction | Effect |
|-------------|--------|
| `tlbi` | forget all translations |
| `tlbi xn` | forget the translation of the page that holds the address in `xn` |

after it changes or removes an entry that may be cached (revoking permissions, unmapping, remapping). Not needed after making a page *more* permissive or after adding a new mapping: an access that the cached entry does not allow always walks the table again. Writing `PTBR` or `SCTLR` forgets everything. There are no address space identifiers, so a context switch (a new `PTBR`) costs a flush. `TLBI` is privileged. Its encoding is the old reserved one (`imm16` is reserved, write 0).

## Turning it on

The tables must already map the code that is running, since the next fetch uses them. The usual order:

1. Build the tables in memory (identity map the kernel).
2. Install the vector table (`VBAR`), whose pages must be mapped too, or the first fault is a double fault.
3. `msr ptbr, xn` then `msr sctlr, 1`.

A page table walk reads RAM, ROM or the disk; a table outside of those is a translation fault. Only RAM can be written back (accessed and dirty).

A bare metal program that knows where its tables are puts them at physical addresses with a linker script (`@P;`), see `a_program_builds_page_tables_and_turns_the_mmu_on` in the integration tests:

```asm
        adrp    x0, l1
        add     x0, x0, :lo12:l1
        ...                     ; fill the tables
        msr     ptbr, x0
        mov     x7, 1
        msr     sctlr, x7
```

## What this does not have

- Address space identifiers, large pages and read-only-but-not-readable pages.
- Taking the kernel out of the translation: there is no separate kernel table, so a kernel's pages are in every address space (without `U`), as on most simple designs.
- Anything for devices yet: memory mapped devices are the next roadmap item, and will be physical addresses a kernel maps with no `W`/`X` as it likes.
- The old `begin_process`/`add_vpage` interface is still what `emu32 -e` uses with `SCTLR.M` clear. It has its own kernel-only notion (`set_ppage_permissions`) that does not follow `PSTATE.U`; mode 1 replaces it.

## Where it is in the code

`VirtualMemory::walk_translate` (the walk), `translate_address` (the TLB hit check, inline), `SystemBus::fetch_instruction` (fetches with `EXECUTE`), `Emulator32bit::write_sysreg` (`PTBR`, `SCTLR`), `Emulator32bit::_tlbi`. Tests: `mmu_test.cpp`.
