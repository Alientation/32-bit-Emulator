#pragma once

#include "ccompiler/massert.h"
#include "ccompiler/stringbuffer.h"

#define ARRAY_LEN(arr) (sizeof (arr) / sizeof (arr[0]))

#define SIZE_T(val) ((size_t) val)
#define ULONG_T(val) ((unsigned long) val)
#define LONG_T(val) ((long) val)
#define UINT_T(val) ((unsigned int) val)
#define INT_T(val) ((int) val)