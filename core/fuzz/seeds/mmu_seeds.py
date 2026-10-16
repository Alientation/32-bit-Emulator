# Writes the seeds of the mmu fuzz target (fuzz/mmu.cpp) to corpus/mmu: a list of its operations
# for each of the two models. Run from core/: python3 fuzz/seeds/mmu_seeds.py
import struct

# Requests (next_request): the op is kind + 3 * size index, the offset an index into kOffsets.
READ_WORD, WRITE_WORD, FETCH, READ_BYTE, WRITE_HWORD = 6, 7, 2, 0, 4
LAST_BYTE, LAST_WORD, START = 8, 5, 0


def access(op, vpage, request, offset, value=None):
    out = [op, vpage, request, offset]
    if value is not None:
        out += list(struct.pack('<I', value))
    return out


def swap_seed():
    ops = [0]  # the swapping memory
    ops += [0, 0]                          # begin_process, user
    # add_vpage (pick pid 0, vpage index, bits: w=1 x=2, length 1 + (bits >> 2) % 3): 10 pages
    # for 4 frames
    for vpage, length in [(0, 3), (3, 1), (4, 3), (5, 3)]:
        ops += [3, 0, vpage, ((length - 1) << 2) | 3]
    ops += [5, 0, 7, 5]                    # 8192 is mapped to the ROM page 5
    ops += [7, 0x21 | 0x10]                # ROM page 5 is kernel only (swappable)
    ops += access(8, 7, READ_WORD, START)  # a user process: kernel only
    ops += [7, 0x11]                       # not any more
    ops += access(8, 7, WRITE_WORD, START, 1)  # the ROM: a bus error
    for round_ in range(3):
        for vpage in range(8):
            ops += access(8, vpage, WRITE_WORD, LAST_BYTE, 0x01020304 * (vpage + 1) + round_)
            ops += access(8, vpage, READ_WORD, LAST_WORD)
            ops += access(8, vpage, FETCH, START)
    ops += [0, 1]                          # a second process, kernel
    ops += [3, 1, 0, 3, 3, 1, 5, 3]
    ops += [5, 1, 2, 1]                    # its page 2 to the frame 1
    ops += access(8, 2, WRITE_HWORD, LAST_BYTE, 0xBEEF)
    ops += [2, 0]                          # back to the first
    ops += [4, 0, 0, 8 | 1]                # pages 0-2 not executable
    ops += access(8, 0, FETCH, START)
    ops += [6, 0, 6, 1]                    # the virtual memory off and on
    ops += access(8, 1, READ_BYTE, LAST_BYTE)
    ops += [1, 1]                          # end the second
    return bytes(ops)


def fetch_cache_seed():
    # The page that is fetched from is remembered; each of these has to forget it.
    ops = [0]
    ops += [0, 0, 3, 0, 0, 3, 3, 0, 4, 8 | 3, 3, 0, 5, 8 | 3]
    ops += access(8, 0, WRITE_WORD, START, 0x12345678)
    ops += access(8, 0, FETCH, START)
    ops += [4, 0, 0, 1]                    # page 0 is not executable any more
    ops += access(8, 0, FETCH, START)
    ops += [4, 0, 0, 3]
    ops += access(8, 0, FETCH, START)
    ops += [0, 0]                          # another process, without page 0
    ops += access(8, 0, FETCH, START)
    ops += [3, 0xFE, 0, 3]                 # it gets a page 0 of its own (zeros)
    ops += access(8, 0, FETCH, START)
    ops += [2, 0]                          # back to the first
    ops += access(8, 0, FETCH, START)
    for vpage in [4, 5, 6, 1, 2, 3]:       # every frame taken by others: page 0 is evicted
        ops += access(8, vpage, WRITE_WORD, START, vpage)
    ops += access(8, 0, FETCH, START)
    ops += [6, 0]                          # off: the address is physical
    ops += access(8, 0, FETCH, START)
    ops += access(8, 0, READ_WORD, LAST_WORD)
    ops += [6, 1]
    ops += access(8, 0, FETCH, START)
    return bytes(ops)


def walk_seed():
    ops = [1]  # the page tables
    ops += [2, 0, 0x80 | 0]                # L1 0 -> table page 1
    ops += [2, 1, 0x80 | 1]                # L1 1 -> table page 2
    ops += [2, 2, 0x80 | 0]                # L1 4 -> table page 1, shared with L1 0
    # set_entry: pick (l1 = pick % 3, l2 index = pick / 3 % 5), frame, flags, tlbi
    v, w, x, u, a, d = 1, 2, 4, 8, 16, 32
    entries = [(0, 0, v | w | x), (3, 1, v | w), (12, 2, v | w | u), (1, 3, v | x | u),
               (13, 13, v), (4, 5, v | w | x | u)]
    for pick, frame, flags in entries:
        ops += [0, pick, frame, flags, 0]
    for pick in [0, 3, 12, 1, 13, 4]:
        ops += access(6, pick, WRITE_WORD, LAST_BYTE, 0xA5A5A5A5 + pick)
        ops += access(6, pick, READ_WORD, LAST_WORD)
        ops += access(6, pick, FETCH, START)
    ops += [3, 1]                          # user mode
    for pick in [0, 12, 1, 4]:
        ops += access(6, pick, WRITE_WORD, START, pick)
        ops += access(6, pick, FETCH, START)
    ops += [0, 12, 2, v | w | x | u | a | d, 0]  # gains X: no TLBI
    ops += access(6, 12, FETCH, START)
    ops += [0, 0, 7, v, 0]                 # loses W and X, a new frame: TLBI
    ops += access(6, 0, WRITE_WORD, START, 1)
    ops += [3, 0, 5, 1, 4, 1]              # kernel mode, flush, walk off and on
    ops += access(6, 3, READ_BYTE, LAST_BYTE)
    return bytes(ops)


for name, data in [('seed_swap', swap_seed()), ('seed_walk', walk_seed()),
                   ('seed_fetch_cache', fetch_cache_seed())]:
    with open('fuzz/corpus/mmu/' + name, 'wb') as f:
        f.write(data)
