#include "ccompiler/stringbuffer.h"

#include "ccompiler/massert.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void stringbuffer_extend (stringbuffer_t *stringbuffer, const size_t target_cap);


static void stringbuffer_extend (stringbuffer_t * const stringbuffer, const size_t target_cap)
{
    size_t new_capacity = stringbuffer->capacity * 2;
    if (new_capacity < 2 * target_cap)
    {
        new_capacity = 2 * target_cap;
    }
    new_capacity++;
    stringbuffer->capacity = new_capacity;

    char *old_buf = stringbuffer->buf;
    stringbuffer->buf = calloc (new_capacity, sizeof (char));

    if (!stringbuffer->buf)
    {
        free (old_buf);
        M_UNREACHABLE ("ERROR: failed to allocate memory.");
    }

    if (old_buf)
    {
        memcpy (stringbuffer->buf, old_buf, stringbuffer->length);
        free (old_buf);
    }
    stringbuffer->buf[stringbuffer->length] = '\0';
}


void sb_init (stringbuffer_t * const stringbuffer)
{
    stringbuffer->buf = NULL;
    stringbuffer->capacity = 0;
    stringbuffer->length = 0;
}

void sb_free (stringbuffer_t * const stringbuffer)
{
    free (stringbuffer->buf);
    stringbuffer->buf = NULL;
    stringbuffer->capacity = 0;
    stringbuffer->length = 0;
}

void sb_appendf (stringbuffer_t * const stringbuffer, const char * const fmt, ...)
{
    va_list args;
    va_start (args, fmt);
    sb_vappendf (stringbuffer, fmt, args);
    va_end (args);
}

void sb_vappendf (stringbuffer_t * const stringbuffer, const char * const fmt, va_list args)
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
    sb_appendl (stringbuffer, buffer, size);
    free (buffer);
}

void sb_append (stringbuffer_t * const stringbuffer, const char *str)
{
    int len = strlen (str);
    sb_appendl (stringbuffer, str, len);
}

void sb_appendc (stringbuffer_t * const stringbuffer, const char ch)
{
    char cstr[2] = {ch, '\0'};
    sb_append (stringbuffer, cstr);
}

void sb_appendl (stringbuffer_t * const stringbuffer, const char * const str, const size_t len)
{
    if (stringbuffer->length + len + 1 > stringbuffer->capacity)
    {
        stringbuffer_extend (stringbuffer, stringbuffer->length + len);
    }

    memcpy (stringbuffer->buf + stringbuffer->length, str, len);
    stringbuffer->length += len;
    stringbuffer->buf[stringbuffer->length] = '\0';
}

void sb_appendsb (stringbuffer_t * const dest, const stringbuffer_t * const src)
{
    if (src->length == 0)
    {
        return;
    }

    sb_appendl (dest, src->buf, src->length);
}

void sb_clear (stringbuffer_t * const stringbuffer)
{
    if (stringbuffer->capacity > 0)
    {
        stringbuffer->buf[0] = '\0';
    }
    stringbuffer->length = 0;
}