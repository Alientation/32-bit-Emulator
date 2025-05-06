#include "ccompiler/massert.h"

#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>

void _M_ASSERT (bool cond, const char *cond_str, const char *file, int line, const char *msg_fmt, ...)
{
    if (!(cond))
    {
        va_list args;
        va_start(args, msg_fmt);

        printf (msg_fmt, args);
        printf ("\nAssert failed condition %s at %s:%d\n", cond_str, file, line);
        exit (EXIT_FAILURE);
    }
}