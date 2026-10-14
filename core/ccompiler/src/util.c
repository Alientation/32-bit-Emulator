#include "ccompiler/util.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>

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

void error_at(const char *file_name, const char *contents, const char *loc, const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    verror_at(file_name, contents, loc, fmt, ap);
    va_end(ap);
}

void verror_at(const char *file_name, const char *contents, const char *loc, const char *fmt,
               va_list ap)
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
    int line_number = 1;
    for (const char *p = contents; p < line; p++)
    {
        if (*p == '\n')
        {
            line_number++;
        }
    }

    const int indent = fprintf(stderr, "%s:%d: ", file_name, line_number);
    fwrite(line, 1, (size_t) (end - line), stderr);
    fprintf(stderr, "\n");
    for (int i = 0; i < indent + (int) (loc - line); i++)
    {
        fputc(' ', stderr);
    }
    fprintf(stderr, "^ ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}
