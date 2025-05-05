#include "ccompiler/stringbuffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void stringbuffer_extend (stringbuffer_t *stringbuffer, int target_cap)
{
    int new_cap = stringbuffer->capacity *= 2;
    if (new_cap < 2 * target_cap)
    {
        new_cap = 2 * target_cap;
    }

    char *old_buf = stringbuffer->buf;
    stringbuffer->buf = calloc (new_cap + 1, sizeof (char));

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

void stringbuffer_append (stringbuffer_t *stringbuffer, char *str)
{
    int len = strlen (str);
    stringbuffer_appendl (stringbuffer, str, len);
}

void stringbuffer_appendl (stringbuffer_t *stringbuffer, char *str, int len)
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