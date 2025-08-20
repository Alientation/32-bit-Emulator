#pragma once

#define ARRAY_LENGTH(arr) (sizeof (arr) / sizeof ((arr)[0]))

#define UNLIKELY(cond) __builtin_expect (cond, 0)
#define LIKELY(cond) __builtin_expect (cond, 1)

#define UNUSED(x) (void) (x)