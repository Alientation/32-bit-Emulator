#include "ccompiler/stringbuffer.h"

#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void stringbuffer_extend (stringbuffer_t *stringbuffer, const size_t target_cap);


static void stringbuffer_extend (stringbuffer_t *stringbuffer, const size_t target_cap)
{
    size_t new_capacity = stringbuffer->capacity * 2;
    if (new_capacity < 2 * target_cap)
    {
        new_capacity = 2 * target_cap;
    }
    stringbuffer->capacity = new_capacity;

    char *old_buf = stringbuffer->buf;
    stringbuffer->buf = calloc (new_capacity + 1, sizeof (char));

    if (!stringbuffer->buf)
    {
        fprintf (stderr, "ERROR: failed to allocate memory\n");
        exit (EXIT_FAILURE);
    }

    if (old_buf)
    {
        memcpy (stringbuffer->buf, old_buf, stringbuffer->length);
        free (old_buf);
    }
    stringbuffer->buf[stringbuffer->length] = '\0';
}


void stringbuffer_init (stringbuffer_t *stringbuffer)
{
    stringbuffer->buf = NULL;
    stringbuffer->capacity = 0;
    stringbuffer->length = 0;
}

void stringbuffer_free (stringbuffer_t *stringbuffer)
{
    free (stringbuffer->buf);
    stringbuffer->buf = NULL;
    stringbuffer->capacity = 0;
    stringbuffer->length = 0;
}

void stringbuffer_appendf (stringbuffer_t *stringbuffer, const char *fmt, ...)
{
    va_list args;
    va_start(args, fmt);

    // find required size
    va_list args_copy;
    va_copy(args_copy, args);
    int size = vsnprintf(NULL, 0, fmt, args_copy);
    va_end(args_copy);

    if (size < 0) {
        va_end(args);

        fprintf (stderr, "ERROR: vsnprintf failed\n");
        exit (EXIT_FAILURE);
    }

    // allocate space for string (+1 for null terminator)
    char* buffer = calloc(size + 1, sizeof (char));
    if (!buffer) {
        va_end(args);

        fprintf (stderr, "ERROR: memory allocation failed\n");
        exit (EXIT_FAILURE);
    }

    // output into buffer
    vsnprintf(buffer, size + 1, fmt, args);
    va_end(args);

    // append to string buffer
    stringbuffer_appendl (stringbuffer, buffer, size);
}

void stringbuffer_append (stringbuffer_t *stringbuffer, const char *str)
{
    int len = strlen (str);
    stringbuffer_appendl (stringbuffer, str, len);
}

void stringbuffer_appendl (stringbuffer_t *stringbuffer, const char *str, const size_t len)
{
    if (stringbuffer->length + len > stringbuffer->capacity)
    {
        stringbuffer_extend (stringbuffer, stringbuffer->length + len);
    }

    memcpy (stringbuffer->buf + stringbuffer->length, str, len);
    stringbuffer->length += len;
    stringbuffer->buf[stringbuffer->length] = '\0';
}

void stringbuffer_clear (stringbuffer_t *stringbuffer)
{
    if (stringbuffer->capacity > 0)
    {
        stringbuffer->buf[0] = '\0';
    }
}