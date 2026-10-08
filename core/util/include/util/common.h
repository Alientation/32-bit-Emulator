#pragma once

#define UNLIKELY(cond) __builtin_expect(cond, 0)
#define LIKELY(cond) __builtin_expect(cond, 1)

#define UNUSED(x) (void) (x)

#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

#define MAYBE_UNUSED __attribute_maybe_unused__