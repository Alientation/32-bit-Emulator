#pragma once
#ifndef STRINGBUFFER_H
#define STRINGBUFFER_H

typedef struct StringBuffer
{
    char *buf;
    int length;
    int capacity;
} stringbuffer_t;


void stringbuffer_init (stringbuffer_t *stringbuffer);
void stringbuffer_free (stringbuffer_t *stringbuffer);

void stringbuffer_append (stringbuffer_t *stringbuffer, char *str);
void stringbuffer_appendl (stringbuffer_t *stringbuffer, char *str, int len);

void stringbuffer_clear (stringbuffer_t *stringbuffer);


#endif /* STRINGBUFFER_H */