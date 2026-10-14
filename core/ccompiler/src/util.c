#include "ccompiler/util.h"

#include <assert.h>
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

/// A binary floating point format: the bits of the significand, the exponent of the largest power
/// of 2, and the exponent of the last bit of the smallest subnormal.
typedef struct
{
    int mant_bits;
    int max_exp;
    int min_lsb;
} FloatFormat;

static const FloatFormat binary32 = {24, 127, -149};
static const FloatFormat binary64 = {53, 1023, -1074};

enum
{
    /// A decimal constant keeps this many significant digits, and the ones after them count only
    /// as a digit 1 after the last one kept. A halfway point between two doubles has at most 767
    /// significant digits, so the rounding is the same.
    MAX_DECIMAL_DIGITS = 800,
    /// The same for a hexadecimal constant: 120 bits, more than twice those of a double.
    MAX_HEX_DIGITS = 30,
    /// The words of a BigInt, enough for the largest number of a conversion: 10^1125 (the
    /// smallest decimal exponent that does not give 0 at once) shifted by 56 bits.
    BIG_WORDS = 128,
};

/// An unsigned integer of up to BIG_WORDS words, the lowest first, without zero words at the top.
typedef struct
{
    unsigned int words[BIG_WORDS];
    size_t count;
} BigInt;

/// `a = a * mul + add`.
static void big_mul_add(BigInt *a, unsigned int mul, unsigned int add)
{
    unsigned long long carry = add;
    for (size_t i = 0; i < a->count; i++)
    {
        carry += (unsigned long long) a->words[i] * mul;
        a->words[i] = (unsigned int) carry;
        carry >>= 32;
    }
    if (carry)
    {
        assert(a->count < BIG_WORDS);
        a->words[a->count++] = (unsigned int) carry;
    }
}

static void big_shl(BigInt *a, int bits)
{
    const size_t word_shift = (size_t) bits / 32;
    const int bit_shift = bits % 32;
    if (a->count == 0)
    {
        return;
    }
    assert(a->count + word_shift + 1 <= BIG_WORDS);
    a->words[a->count + word_shift] = 0;
    for (size_t i = a->count; i-- > 0;)
    {
        const unsigned long long wide = (unsigned long long) a->words[i] << bit_shift;
        a->words[i + word_shift + 1] |= (unsigned int) (wide >> 32);
        a->words[i + word_shift] = (unsigned int) wide;
    }
    for (size_t i = 0; i < word_shift; i++)
    {
        a->words[i] = 0;
    }
    a->count += word_shift + 1;
    while (a->count && a->words[a->count - 1] == 0)
    {
        a->count--;
    }
}

static void big_shr1(BigInt *a)
{
    for (size_t i = 0; i < a->count; i++)
    {
        const unsigned int next = i + 1 < a->count ? a->words[i + 1] : 0;
        a->words[i] = a->words[i] >> 1 | next << 31;
    }
    if (a->count && a->words[a->count - 1] == 0)
    {
        a->count--;
    }
}

/// @return Whether `a >= b`.
static bool big_ge(const BigInt *a, const BigInt *b)
{
    if (a->count != b->count)
    {
        return a->count > b->count;
    }
    for (size_t i = a->count; i-- > 0;)
    {
        if (a->words[i] != b->words[i])
        {
            return a->words[i] > b->words[i];
        }
    }
    return true;
}

/// `a = a - b`, where `a >= b`.
static void big_sub(BigInt *a, const BigInt *b)
{
    unsigned long long borrow = 0;
    for (size_t i = 0; i < a->count; i++)
    {
        const unsigned long long sub = (i < b->count ? b->words[i] : 0) + borrow;
        borrow = a->words[i] < sub;
        a->words[i] = (unsigned int) (a->words[i] - sub);
    }
    while (a->count && a->words[a->count - 1] == 0)
    {
        a->count--;
    }
}

static int big_bit_length(const BigInt *a)
{
    if (a->count == 0)
    {
        return 0;
    }
    int len = (int) (a->count - 1) * 32;
    for (unsigned int top = a->words[a->count - 1]; top; top >>= 1)
    {
        len++;
    }
    return len;
}

static bool big_bit(const BigInt *a, int bit)
{
    const size_t word = (size_t) bit / 32;
    return word < a->count && (a->words[word] >> (bit % 32) & 1);
}

static double double_from_bits(unsigned long long bits)
{
    double value;
    memcpy(&value, &bits, sizeof(value));
    return value;
}

/// Rounds `q * 2^exp2` to the nearest value of a format, ties to even. `sticky` says that the
/// value is a little more than that, by less than one unit of the last bit of `q`.
///
/// @param[in] q At most 62 bits, and at least two more than the format keeps when `sticky` is set.
/// @param[out] overflow Set when the value is too large for the format.
/// @return The value as a double, infinity when it is too large.
static double round_to_format(unsigned long long q, int exp2, bool sticky, const FloatFormat *fmt,
                              bool *overflow)
{
    int len = 0;
    for (unsigned long long rest = q; rest; rest >>= 1)
    {
        len++;
    }
    // The exponent of the last bit the format keeps, which is fixed for a subnormal.
    int lsb = exp2 + len - fmt->mant_bits;
    if (lsb < fmt->min_lsb)
    {
        lsb = fmt->min_lsb;
    }
    const int shift = lsb - exp2;
    unsigned long long m = q;
    int e = exp2;
    if (shift > 0)
    {
        const bool half = shift <= 62 && (q >> (shift - 1) & 1);
        const bool below = sticky || (shift > 62 ? q != 0 : (q & ((1ull << (shift - 1)) - 1)) != 0);
        m = shift > 62 ? 0 : q >> shift;
        e = lsb;
        if (half && (below || (m & 1)))
        {
            m++;
        }
        if (m >> fmt->mant_bits)
        {
            // Rounding up carried into a new bit.
            m >>= 1;
            e++;
        }
    }
    else
    {
        assert(!sticky);
    }
    if (m == 0)
    {
        return 0.0;
    }

    len = 0;
    for (unsigned long long rest = m; rest; rest >>= 1)
    {
        len++;
    }
    const int top = e + len - 1;
    if (top > fmt->max_exp)
    {
        *overflow = true;
        return double_from_bits(0x7FF0000000000000ull);
    }
    // A float is always a normal double; a double below 2^-1022 is a subnormal one.
    if (top < -1022)
    {
        return double_from_bits(m << (e + 1074));
    }
    const unsigned long long fraction = (m << (53 - len)) & ((1ull << 52) - 1);
    return double_from_bits((unsigned long long) (top + 1023) << 52 | fraction);
}

/// Rounds `n * 2^exp2` to a format, from the top 62 bits of `n` and whether any bit below them is
/// set.
static double round_big(const BigInt *n, int exp2, const FloatFormat *fmt, bool *overflow)
{
    const int len = big_bit_length(n);
    const int dropped = len > 62 ? len - 62 : 0;
    unsigned long long q = 0;
    for (int bit = len - 1; bit >= dropped; bit--)
    {
        q = q << 1 | big_bit(n, bit);
    }
    bool sticky = false;
    for (int bit = 0; bit < dropped && !sticky; bit++)
    {
        sticky = big_bit(n, bit);
    }
    return round_to_format(q, exp2 + dropped, sticky, fmt, overflow);
}

/// @return The value of a hexadecimal digit.
static unsigned int hex_digit_value(char c)
{
    if (c >= '0' && c <= '9')
    {
        return (unsigned int) (c - '0');
    }
    return (unsigned int) ((c | 0x20) - 'a' + 10);
}

double floating_value(const char *start, const char *end, bool is_float, bool *overflow)
{
    const FloatFormat *fmt = is_float ? &binary32 : &binary64;
    *overflow = false;
    const char *p = start;
    const bool hex = p[0] == '0' && (p[1] == 'x' || p[1] == 'X');
    p += hex ? 2 : 0;
    const unsigned int base = hex ? 16 : 10;
    const int max_digits = hex ? MAX_HEX_DIGITS : MAX_DECIMAL_DIGITS;
    const char exponent_letter = hex ? 'p' : 'e';

    // The value is digits * base^scale * (10 or 2)^exponent.
    BigInt digits = {{0}, 0};
    int count = 0;
    int scale = 0;
    bool after_point = false;
    bool dropped = false;
    for (; p < end && (*p | 0x20) != exponent_letter; p++)
    {
        if (*p == '\'')
        {
            continue;
        }
        if (*p == '.')
        {
            after_point = true;
            continue;
        }
        const unsigned int digit = hex_digit_value(*p);
        if (count < max_digits && (count > 0 || digit != 0))
        {
            big_mul_add(&digits, base, digit);
            count++;
            scale -= after_point ? 1 : 0;
        }
        else if (count == max_digits)
        {
            dropped = dropped || digit != 0;
            scale += after_point ? 0 : 1;
        }
        else
        {
            // A leading zero.
            scale -= after_point ? 1 : 0;
        }
    }
    if (dropped)
    {
        big_mul_add(&digits, base, 1);
        count++;
        scale--;
    }

    int exponent = 0;
    if (p < end)
    {
        p++;
        const bool negative = *p == '-';
        p += *p == '+' || *p == '-' ? 1 : 0;
        for (; p < end; p++)
        {
            // Far beyond any exponent that does not give 0 or infinity, and no int overflow.
            if (*p != '\'' && exponent < 100000)
            {
                exponent = exponent * 10 + (*p - '0');
            }
        }
        exponent = negative ? -exponent : exponent;
    }
    if (count == 0)
    {
        return 0.0;
    }
    if (hex)
    {
        return round_big(&digits, 4 * scale + exponent, fmt, overflow);
    }

    // 10^(count - 1 + e) <= value < 10^(count + e): at least 10^309 is infinity, less than
    // 10^-324 (below half the smallest subnormal double) is 0.
    const int e = scale + exponent;
    if (count - 1 + e >= 309)
    {
        *overflow = true;
        return double_from_bits(0x7FF0000000000000ull);
    }
    if (count + e < -324)
    {
        return 0.0;
    }
    if (e >= 0)
    {
        for (int i = 0; i < e; i++)
        {
            big_mul_add(&digits, 10, 0);
        }
        return round_big(&digits, 0, fmt, overflow);
    }

    // value = digits / 10^-e. The quotient of digits * 2^k by 10^-e, for a k that makes it
    // mant_bits + 2 or + 3 bits long, and whether there is a remainder, are enough to round.
    BigInt divisor = {{1}, 1};
    for (int i = 0; i < -e; i++)
    {
        big_mul_add(&divisor, 10, 0);
    }
    const int k = fmt->mant_bits + 2 - (big_bit_length(&digits) - big_bit_length(&divisor));
    if (k > 0)
    {
        big_shl(&digits, k);
    }
    else
    {
        big_shl(&divisor, -k);
    }
    big_shl(&divisor, fmt->mant_bits + 2);
    unsigned long long q = 0;
    for (int bit = fmt->mant_bits + 2; bit >= 0; bit--)
    {
        if (big_ge(&digits, &divisor))
        {
            big_sub(&digits, &divisor);
            q |= 1ull << bit;
        }
        big_shr1(&divisor);
    }
    return round_to_format(q, -k, digits.count != 0, fmt, overflow);
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
