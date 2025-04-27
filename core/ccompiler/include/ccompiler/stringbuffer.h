#pragma once
#ifndef STRINGBUFFER_H
#define STRINGBUFFER_H

struct StringBuffer
{
    char *buf;
    int length;
    int capacity;
};


void stringbuffer_init (struct StringBuffer *stringbuffer);
void stringbuffer_free (struct StringBuffer *stringbuffer);

void stringbuffer_append (struct StringBuffer *stringbuffer, char *str);
void stringbuffer_appendl (struct StringBuffer *stringbuffer, char *str, int len);

void stringbuffer_clear (struct StringBuffer *stringbuffer);


#endif /* STRINGBUFFER_H */