#ifndef CCOMPILER_UTIL_H
#define CCOMPILER_UTIL_H

#include <stdarg.h>
#include <stddef.h>

/// Allocates zeroed memory for the compiler's data (tokens, nodes, types, the text of the source
/// files). Nothing is freed one by one: the memory lives until the process exits, which is what a
/// compiler that runs once on a file wants, and it keeps the compiler free of ownership rules.
///
/// @param[in] size Number of bytes.
/// @return The start of the memory, aligned to 8 bytes. Exits with an error if the host is out
/// of memory.
void *arena_alloc(size_t size);

/// Reports an error that is not about a place in the source and exits with status 1. The compiler
/// does not know that this function does not return, so the code after a call still returns a
/// value.
///
/// @param[in] fmt A printf format.
void error(const char *fmt, ...);

/// Reports an error at a place in the source (the file name, the line, the line itself with a
/// caret below the place and the message) and exits with status 1.
///
/// @param[in] file_name Name of the file, for the message.
/// @param[in] contents The whole text of the file.
/// @param[in] loc The place, a pointer into `contents`.
/// @param[in] fmt A printf format.
void error_at(const char *file_name, const char *contents, const char *loc, const char *fmt, ...);

/// Like error_at(), with the arguments of the format in a `va_list`.
///
/// @param[in] ap The arguments of `fmt`.
void verror_at(const char *file_name, const char *contents, const char *loc, const char *fmt,
               va_list ap);

#endif
