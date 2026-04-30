#pragma once

#include <stdarg.h>
#include <stddef.h>

typedef struct StringBuffer
{
    char *buf;
    size_t length;
    size_t capacity;
} stringbuffer_t;


void sb_init (stringbuffer_t *stringbuffer);
void sb_reserve (stringbuffer_t *sb, size_t target_size);
void sb_free (stringbuffer_t *stringbuffer);

void sb_appendf (stringbuffer_t *stringbuffer, const char *fmt, ...);
void sb_vappendf (stringbuffer_t *stringbuffer, const char *fmt, va_list args);
void sb_append (stringbuffer_t *stringbuffer, const char *str);
void sb_appendc (stringbuffer_t *stringbuffer, char ch);
void sb_appendl (stringbuffer_t *stringbuffer, const char *str, const size_t len);

void sb_appendsb (stringbuffer_t *dest, const stringbuffer_t *src);

void sb_clear (stringbuffer_t *stringbuffer);
