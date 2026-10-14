#include "ccompiler/util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/// Memory is taken from the host in chunks of this many bytes (or more for one large request).
enum
{
    ARENA_CHUNK_SIZE = 65536,
    ARENA_ALIGN = 8,
};

typedef struct ArenaChunk ArenaChunk;

struct ArenaChunk
{
    ArenaChunk *next;
    char *data;
    size_t size;
    size_t used;
};

/// The chunks, the newest first. The list stays reachable until the process exits, so a leak
/// checker does not report the arena.
static ArenaChunk *arena_chunks;

void *arena_alloc(size_t size)
{
    size = (size + ARENA_ALIGN - 1) / ARENA_ALIGN * ARENA_ALIGN;
    if (!arena_chunks || arena_chunks->size - arena_chunks->used < size)
    {
        const size_t chunk_size = size > ARENA_CHUNK_SIZE ? size : ARENA_CHUNK_SIZE;
        ArenaChunk *chunk = calloc(1, sizeof(ArenaChunk) + chunk_size + ARENA_ALIGN);
        if (!chunk)
        {
            error("out of memory");
        }
        // Align the data, not only the header in front of it.
        const size_t address = (size_t) (chunk + 1);
        chunk->data = (char *) ((address + ARENA_ALIGN - 1) / ARENA_ALIGN * ARENA_ALIGN);
        chunk->size = chunk_size;
        chunk->next = arena_chunks;
        arena_chunks = chunk;
    }
    void *memory = arena_chunks->data + arena_chunks->used;
    arena_chunks->used += size;
    return memory;
}

char *arena_strndup(const char *text, size_t len)
{
    char *copy = arena_alloc(len + 1);
    memcpy(copy, text, len);
    return copy;
}

void error(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    fprintf(stderr, "bcc: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    va_end(ap);
    exit(1);
}

/// Prints the message of verror_at() and vwarn_at(). `kind` is put in front of the message.
static void report_at(const char *file_name, int line_no, const char *contents, const char *loc,
                      const char *kind, const char *fmt, va_list ap)
{
    const char *line = loc;
    while (contents < line && line[-1] != '\n')
    {
        line--;
    }
    const char *end = loc;
    while (*end && *end != '\n')
    {
        end++;
    }

    const int indent = fprintf(stderr, "%s:%d: ", file_name, line_no);
    fwrite(line, 1, (size_t) (end - line), stderr);
    fprintf(stderr, "\n");
    for (int i = 0; i < indent + (int) (loc - line); i++)
    {
        fputc(' ', stderr);
    }
    fprintf(stderr, "^ %s", kind);
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
}

void verror_at(const char *file_name, int line_no, const char *contents, const char *loc,
               const char *fmt, va_list ap)
{
    report_at(file_name, line_no, contents, loc, "", fmt, ap);
    exit(1);
}

void vwarn_at(const char *file_name, int line_no, const char *contents, const char *loc,
              const char *fmt, va_list ap)
{
    report_at(file_name, line_no, contents, loc, "warning: ", fmt, ap);
}

Int64 int64_from_int(int value)
{
    Int64 result = {(unsigned int) value, value < 0 ? ~0u : 0u};
    return result;
}

Int64 int64_from_uint(unsigned int value)
{
    Int64 result = {value, 0};
    return result;
}

Int64 int64_add(Int64 a, Int64 b)
{
    Int64 result = {a.lo + b.lo, a.hi + b.hi};
    if (result.lo < a.lo)
    {
        result.hi++;
    }
    return result;
}

Int64 int64_sub(Int64 a, Int64 b)
{
    return int64_add(a, int64_neg(b));
}

/// @return The 64 bit product of two words, worked out from their 16 bit halves.
static Int64 multiply_words(unsigned int a, unsigned int b)
{
    const unsigned int a_lo = a & 0xFFFF;
    const unsigned int a_hi = a >> 16;
    const unsigned int b_lo = b & 0xFFFF;
    const unsigned int b_hi = b >> 16;
    const unsigned int cross1 = a_hi * b_lo;
    const unsigned int cross2 = a_lo * b_hi;

    Int64 result = int64_from_uint(a_lo * b_lo);
    result.hi = a_hi * b_hi;
    result = int64_add(result, int64_shl(int64_from_uint(cross1), 16));
    return int64_add(result, int64_shl(int64_from_uint(cross2), 16));
}

Int64 int64_mul(Int64 a, Int64 b)
{
    Int64 result = multiply_words(a.lo, b.lo);
    result.hi += a.lo * b.hi + a.hi * b.lo;
    return result;
}

Int64 int64_neg(Int64 a)
{
    Int64 one = {1, 0};
    return int64_add(int64_not(a), one);
}

Int64 int64_not(Int64 a)
{
    Int64 result = {~a.lo, ~a.hi};
    return result;
}

Int64 int64_and(Int64 a, Int64 b)
{
    Int64 result = {a.lo & b.lo, a.hi & b.hi};
    return result;
}

Int64 int64_or(Int64 a, Int64 b)
{
    Int64 result = {a.lo | b.lo, a.hi | b.hi};
    return result;
}

Int64 int64_xor(Int64 a, Int64 b)
{
    Int64 result = {a.lo ^ b.lo, a.hi ^ b.hi};
    return result;
}

Int64 int64_shl(Int64 a, int count)
{
    Int64 result = a;
    if (count >= 32)
    {
        result.hi = a.lo << (count - 32);
        result.lo = 0;
    }
    else if (count > 0)
    {
        result.hi = a.hi << count | a.lo >> (32 - count);
        result.lo = a.lo << count;
    }
    return result;
}

Int64 int64_shr(Int64 a, int count, bool is_signed)
{
    const unsigned int fill = is_signed && (a.hi >> 31) ? ~0u : 0u;
    Int64 result = a;
    if (count >= 32)
    {
        // A shift by 32 of a word is undefined in C, so a count of 32 gets its own case.
        result.lo = count == 32 ? a.hi : (a.hi >> (count - 32) | fill << (64 - count));
        result.hi = fill;
    }
    else if (count > 0)
    {
        result.lo = a.lo >> count | a.hi << (32 - count);
        result.hi = a.hi >> count | fill << (32 - count);
    }
    return result;
}

/// @return Whether the sign bit of `a` is set.
static bool is_negative(Int64 a)
{
    return (a.hi >> 31) != 0;
}

Int64 int64_div(Int64 a, Int64 b, bool is_signed, Int64 *remainder)
{
    const bool negate_quotient = is_signed && is_negative(a) != is_negative(b);
    const bool negate_remainder = is_signed && is_negative(a);
    if (is_signed && is_negative(a))
    {
        a = int64_neg(a);
    }
    if (is_signed && is_negative(b))
    {
        b = int64_neg(b);
    }

    // Long division, one bit of the quotient at a time.
    Int64 quotient = {0, 0};
    Int64 rest = {0, 0};
    for (int bit = 63; bit >= 0; bit--)
    {
        rest = int64_shl(rest, 1);
        rest.lo |= int64_shr(a, bit, false).lo & 1;
        if (!int64_lt(rest, b, false))
        {
            rest = int64_sub(rest, b);
            quotient = int64_or(quotient, int64_shl(int64_from_uint(1), bit));
        }
    }

    *remainder = negate_remainder ? int64_neg(rest) : rest;
    return negate_quotient ? int64_neg(quotient) : quotient;
}

bool int64_eq(Int64 a, Int64 b)
{
    return a.lo == b.lo && a.hi == b.hi;
}

bool int64_lt(Int64 a, Int64 b, bool is_signed)
{
    if (is_signed && is_negative(a) != is_negative(b))
    {
        return is_negative(a);
    }
    return a.hi < b.hi || (a.hi == b.hi && a.lo < b.lo);
}

bool int64_is_zero(Int64 a)
{
    return a.lo == 0 && a.hi == 0;
}

/// @return The FNV-1a hash of a string.
static unsigned int hash_string(const char *key, size_t len)
{
    unsigned int hash = 2166136261u;
    for (size_t i = 0; i < len; i++)
    {
        hash = (hash ^ (unsigned char) key[i]) * 16777619u;
    }
    return hash;
}

/// @return The slot of `key`, or the empty slot where it would go. The map has an empty slot.
static HashEntry *find_entry(const HashMap *map, const char *key, size_t len)
{
    size_t i = hash_string(key, len) & (map->capacity - 1);
    for (;;)
    {
        HashEntry *entry = &map->entries[i];
        if (!entry->key || (entry->len == len && memcmp(entry->key, key, len) == 0))
        {
            return entry;
        }
        i = (i + 1) & (map->capacity - 1);
    }
}

void *hashmap_get(const HashMap *map, const char *key, size_t len)
{
    if (map->capacity == 0)
    {
        return NULL;
    }
    return find_entry(map, key, len)->value;
}

void hashmap_put(HashMap *map, const char *key, size_t len, void *value)
{
    // Grows at 3/4 full; the capacity is a power of 2 so that a slot is the hash masked.
    if ((map->used + 1) * 4 > map->capacity * 3)
    {
        HashMap bigger = {0};
        bigger.capacity = map->capacity ? map->capacity * 2 : 64;
        bigger.entries = arena_alloc(bigger.capacity * sizeof(HashEntry));
        for (size_t i = 0; i < map->capacity; i++)
        {
            if (map->entries[i].key)
            {
                *find_entry(&bigger, map->entries[i].key, map->entries[i].len) = map->entries[i];
                bigger.used++;
            }
        }
        *map = bigger;
    }

    HashEntry *entry = find_entry(map, key, len);
    if (!entry->key)
    {
        entry->key = key;
        entry->len = len;
        map->used++;
    }
    entry->value = value;
}
