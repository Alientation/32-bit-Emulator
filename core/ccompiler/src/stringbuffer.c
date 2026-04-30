#include "ccompiler/stringbuffer.h"

#include "ccompiler/massert.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void sb_extend (stringbuffer_t *sb, const size_t target_cap);

static void sb_extend (stringbuffer_t * const sb, const size_t target_cap)
{
    size_t new_cap = sb->capacity * 2;
    if (new_cap < 2 * target_cap)
    {
        new_cap = 2 * target_cap;
    }
    new_cap++;
    sb->capacity = new_cap;

    char *old_buf = sb->buf;
    sb->buf = calloc (new_cap, sizeof (char));

    if (!sb->buf)
    {
        free (old_buf);
        M_UNREACHABLE ("ERROR: failed to allocate memory.");
    }

    if (old_buf)
    {
        memcpy (sb->buf, old_buf, sb->length);
        free (old_buf);
    }
    sb->buf[sb->length] = '\0';
}


void sb_init (stringbuffer_t * const sb)
{
    sb->buf = NULL;
    sb->capacity = 0;
    sb->length = 0;
}

void sb_reserve (stringbuffer_t * const sb, const size_t target_size)
{
    if (sb->capacity >= target_size)
    {
        return;
    }
    sb_extend (sb, target_size);
}

void sb_free (stringbuffer_t * const sb)
{
    free (sb->buf);
    sb->buf = NULL;
    sb->capacity = 0;
    sb->length = 0;
}

void sb_appendf (stringbuffer_t * const sb, const char * const fmt, ...)
{
    va_list args;
    va_start (args, fmt);
    sb_vappendf (sb, fmt, args);
    va_end (args);
}

void sb_vappendf (stringbuffer_t * const sb, const char * const fmt, va_list args)
{
    // find required size
    va_list args_copy;
    va_copy (args_copy, args);
    int size = vsnprintf (NULL, 0, fmt, args_copy);
    va_end (args_copy);

    if (size < 0)
    {
        va_end (args);
        M_UNREACHABLE ("ERROR: vsnprintf failed.");
    }

    // allocate space for string (+1 for null terminator)
    char* buffer = calloc (size + 1, sizeof (char));
    if (!buffer)
    {
        va_end (args);
        M_UNREACHABLE ("ERROR: memory allocation failed.");
    }

    // output into buffer
    vsnprintf (buffer, size + 1, fmt, args);

    // append to string buffer
    sb_appendl (sb, buffer, size);
    free (buffer);
}

void sb_append (stringbuffer_t * const sb, const char *str)
{
    int len = strlen (str);
    sb_appendl (sb, str, len);
}

void sb_appendc (stringbuffer_t * const sb, const char ch)
{
    char cstr[2] = {ch, '\0'};
    sb_append (sb, cstr);
}

void sb_appendl (stringbuffer_t * const sb, const char * const str, const size_t len)
{
    if (sb->length + len + 1 > sb->capacity)
    {
        sb_extend (sb, sb->length + len);
    }

    memcpy (sb->buf + sb->length, str, len);
    sb->length += len;
    sb->buf[sb->length] = '\0';
}

void sb_appendsb (stringbuffer_t * const dest, const stringbuffer_t * const src)
{
    if (src->length == 0)
    {
        return;
    }

    sb_appendl (dest, src->buf, src->length);
}

void sb_clear (stringbuffer_t * const sb)
{
    if (sb->capacity > 0)
    {
        sb->buf[0] = '\0';
    }
    sb->length = 0;
}