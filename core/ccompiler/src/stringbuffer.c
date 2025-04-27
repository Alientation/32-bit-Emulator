#include "ccompiler/stringbuffer.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void stringbuffer_extend (struct StringBuffer *stringbuffer, int target_cap)
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

    memcpy (stringbuffer->buf, old_buf, stringbuffer->length);
    stringbuffer->buf[stringbuffer->length] = '\0';
}


void stringbuffer_init (struct StringBuffer *stringbuffer)
{
    stringbuffer->buf = NULL;
    stringbuffer->capacity = 0;
    stringbuffer->length = 0;
}

void stringbuffer_free (struct StringBuffer *stringbuffer)
{
    free (stringbuffer->buf);
    stringbuffer->buf = NULL;
    stringbuffer->capacity = 0;
    stringbuffer->length = 0;
}

void stringbuffer_append (struct StringBuffer *stringbuffer, char *str)
{
    int len = strlen (str);
    stringbuffer_appendl (stringbuffer, str, len);
}

void stringbuffer_appendl (struct StringBuffer *stringbuffer, char *str, int len)
{
    if (stringbuffer->length + len > stringbuffer->capacity)
    {
        stringbuffer_extend (stringbuffer, stringbuffer->length + len);
    }

    memcpy (stringbuffer->buf + stringbuffer->length, str, len);
    stringbuffer->length += len;
    stringbuffer->buf[stringbuffer->length] = '\0';
}

void stringbuffer_clear (struct StringBuffer *stringbuffer)
{
    if (stringbuffer->capacity > 0)
    {
        stringbuffer->buf[0] = '\0';
    }
}