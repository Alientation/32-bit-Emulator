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

/// Converts the spelling of a floating constant, decimal or hexadecimal, with `'` between digits
/// and without its suffix, to the nearest `double`, or the nearest `float` (as a `double`), ties to
/// even. It is what strtod and strtof do, with integer arithmetic only, so that the libc of the
/// bootstrap does not need them (docs/ccompiler.md#self-hosting).
///
/// @param[in] start The first character, a valid spelling (the lexer checks it).
/// @param[in] end The character after the last one.
/// @param[in] is_float Whether the value is rounded to a `float`.
/// @param[out] overflow Set when the value is too large for its type; the result is then infinity.
/// @return The value.
double floating_value(const char *start, const char *end, bool is_float, bool *overflow);

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
