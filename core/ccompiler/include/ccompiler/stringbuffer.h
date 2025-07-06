#pragma once
#ifndef STRINGBUFFER_H
#define STRINGBUFFER_H

#include "inttypes.h"

typedef struct StringBuffer
{
    char *buf;
    size_t length;
    size_t capacity;
} stringbuffer_t;


void stringbuffer_init (stringbuffer_t *stringbuffer);
void stringbuffer_free (stringbuffer_t *stringbuffer);

void stringbuffer_appendf (stringbuffer_t *stringbuffer, const char *fmt, ...);
void stringbuffer_append (stringbuffer_t *stringbuffer, const char *str);
void stringbuffer_appendl (stringbuffer_t *stringbuffer, const char *str, const size_t len);

void stringbuffer_clear (stringbuffer_t *stringbuffer);


#endif /* STRINGBUFFER_H */