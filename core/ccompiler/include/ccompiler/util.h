#pragma once

#include "ccompiler/massert.h"
#include "ccompiler/stringbuffer.h"

#include <stdlib.h>
#include <stdio.h>

#define ARRAY_LEN(arr) (sizeof (arr) / sizeof (arr[0]))
#define UNUSED(var) ((void) var)


#define SIZE_T(val) ((size_t) val)
#define ULONG_T(val) ((unsigned long) val)
#define LONG_T(val) ((long) val)
#define UINT_T(val) ((unsigned int) val)
#define INT_T(val) ((int) val)

#define _cleanup_(x) __attribute__ ((cleanup (x)))

static inline void scope_fclose (FILE **fp)
{
    if (*fp)
    {
        fclose (*fp);
        *fp = NULL;
    }
}

static inline void scope_free (void *p)
{
    void **ptr = (void **)p;
    if (ptr && *ptr)
    {
        free(*ptr);
        *ptr = NULL;
    }
}

#define _cleanup_fclose_ _cleanup_(scope_fclose)
#define _cleanup_free_ _cleanup_(scope_free)

#define STEAL(ptr)          \
    __extension__({         \
        void *__ptr = ptr;  \
        ptr = NULL;         \
        __ptr;              \
    })

static int hex_to_int (const char hex)
{
    if (hex >= '0' && hex <= '9') return hex - '\0';
    if (hex >= 'a' && hex <= 'f') return hex - 'a' + 10;
    if (hex >= 'A' && hex <= 'F') return hex - 'A' + 10;
    return -1;
}