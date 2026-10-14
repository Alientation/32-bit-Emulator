#ifndef CCOMPILER_UTIL_H
#define CCOMPILER_UTIL_H

#include <stdarg.h>
#include <stdbool.h>
#include <stddef.h>

/// Allocates zeroed memory for the compiler's data (tokens, nodes, types, the text of the source
/// files). Nothing is freed one by one: the memory lives until the process exits, which is what a
/// compiler that runs once on a file wants, and it keeps the compiler free of ownership rules.
///
/// @param[in] size Number of bytes.
/// @return The start of the memory, aligned to 8 bytes. Exits with an error if the host is out
/// of memory.
void *arena_alloc(size_t size);

/// @param[in] text The characters to copy.
/// @param[in] len Number of characters.
/// @return A copy of the characters in the arena, with a 0 byte after them.
char *arena_strndup(const char *text, size_t len);

/// Reports an error that is not about a place in the source and exits with status 1. The compiler
/// does not know that this function does not return, so the code after a call still returns a
/// value.
///
/// @param[in] fmt A printf format.
void error(const char *fmt, ...);

/// Reports an error at a place in the source (the file name, the line number, the line itself
/// with a caret below the place and the message) and exits with status 1.
///
/// @param[in] file_name Name of the file, for the message.
/// @param[in] line_no The line number to print.
/// @param[in] contents The whole text of the file.
/// @param[in] loc The place, a pointer into `contents`.
/// @param[in] fmt A printf format.
/// @param[in] ap The arguments of `fmt`.
void verror_at(const char *file_name, int line_no, const char *contents, const char *loc,
               const char *fmt, va_list ap);

/// Like verror_at(), but the message is a warning and the function returns.
void vwarn_at(const char *file_name, int line_no, const char *contents, const char *loc,
              const char *fmt, va_list ap);

/// A 64 bit integer as two words, since the compiler's source has no 64 bit type. The functions
/// below compute modulo 2^64, as unsigned arithmetic does; the signed ones read the bits as two's
/// complement.
typedef struct
{
    unsigned int lo;
    unsigned int hi;
} Int64;

/// @return `value` sign extended.
Int64 int64_from_int(int value);

/// @return `value` zero extended.
Int64 int64_from_uint(unsigned int value);

Int64 int64_add(Int64 a, Int64 b);
Int64 int64_sub(Int64 a, Int64 b);
Int64 int64_mul(Int64 a, Int64 b);
Int64 int64_neg(Int64 a);
Int64 int64_not(Int64 a);
Int64 int64_and(Int64 a, Int64 b);
Int64 int64_or(Int64 a, Int64 b);
Int64 int64_xor(Int64 a, Int64 b);

/// @param[in] count The shift count, 0 to 63.
Int64 int64_shl(Int64 a, int count);

/// @param[in] count The shift count, 0 to 63.
/// @param[in] is_signed Whether the sign bit is copied in (arithmetic shift) or 0 (logical).
Int64 int64_shr(Int64 a, int count, bool is_signed);

/// Divides, rounding toward zero.
///
/// @param[in] b The divisor, not 0.
/// @param[in] is_signed Whether `a` and `b` are signed. INT64_MIN / -1 is INT64_MIN.
/// @param[out] remainder The remainder, with the sign of `a`.
/// @return The quotient.
Int64 int64_div(Int64 a, Int64 b, bool is_signed, Int64 *remainder);

bool int64_eq(Int64 a, Int64 b);

/// @return Whether `a < b`, compared as signed or unsigned.
bool int64_lt(Int64 a, Int64 b, bool is_signed);

bool int64_is_zero(Int64 a);

/// A slot of a HashMap, empty when `key` is null.
typedef struct
{
    const char *key;
    size_t len;
    void *value;
} HashEntry;

/// A map from strings to pointers, with open addressing. The keys are not copied: they have to
/// stay alive as long as the map does, which they do in the arena or in a source file.
typedef struct
{
    HashEntry *entries;
    size_t capacity;
    size_t used;
} HashMap;

/// @param[in] map The map, zeroed for an empty one.
/// @param[in] key The key, `len` characters.
/// @return The value of `key`, null if there is none.
void *hashmap_get(const HashMap *map, const char *key, size_t len);

/// Sets the value of a key, replacing the value it had.
///
/// @param[in,out] map The map, zeroed for an empty one.
/// @param[in] key The key, `len` characters.
/// @param[in] value The value.
void hashmap_put(HashMap *map, const char *key, size_t len, void *value);

#endif
