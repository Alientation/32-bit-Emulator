#!/usr/bin/env python3
"""Writes a large basm program for tools/bench_toolchain.sh.

    gen_basm.py OUTDIR FILES FUNCTIONS_PER_FILE

Every file has FUNCTIONS_PER_FILE functions of about 15 instructions, with a macro use, a call to
the next function (a branch the assembler resolves itself, or a relocation for the first function
of the next file), an adrp/add pair that needs a relocation, a local label, and a table of .word
addresses in .data. File 0 holds _start. The program is not meant to be run, only built.
"""
import os
import sys

out, files, per_file = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
os.makedirs(out, exist_ok=True)
for f in range(files):
    lines = [
        "#macro addpair(va, vb)\n    add x0, x0, va\n    add x1, x1, vb\n#macend",
        f".global f{f}_0",
        ".global _start" if f == 0 else "",
        ".text",
        "_start:\n    bl f0_0\n    hlt" if f == 0 else "",
    ]
    for i in range(per_file):
        lines.append(f"f{f}_{i}:")
        lines.append("    str x29, [sp, -4]!")
        lines.append(
            f"    mov x2, {i % 1000}\n    add x3, x2, {i % 97}\n    sub x4, x3, x2\n"
            f"    ldr x5, [sp]\n    eor x6, x5, x4"
        )
        lines.append(f"    #invoke addpair({i % 13}, {i % 7})")
        lines.append(
            f"    adrp x7, tbl{f}\n    add x7, x7, :lo12:tbl{f}\n    ldr x8, [x7, {(i % 64) * 4}]"
        )
        lines.append(f"    cmp x4, {i % 50}\n    b.eq L_skip{i}\n    add x9, x9, 1\nL_skip{i}:")
        lines.append("    ldr x29, [sp], 4")
        if i + 1 < per_file:
            lines.append(f"    bl f{f}_{i + 1}")
        elif f + 1 < files:
            lines.append(f"    bl f{f + 1}_0")
        lines.append("    ret")
    lines.append(".data")
    lines.append(f"tbl{f}:")
    lines.extend(f"    .word f{f}_{i % per_file}" for i in range(64))
    with open(f"{out}/m{f}.basm", "w") as handle:
        handle.write("\n".join(line for line in lines if line) + "\n")
