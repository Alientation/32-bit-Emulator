#pragma once

/// Branch prediction hints for a condition.
#define UNLIKELY(cond) __builtin_expect(cond, 0)
#define LIKELY(cond) __builtin_expect(cond, 1)

/// Silences the unused variable warning for `x`.
#define UNUSED(x) (void) (x)

/// The number of elements of a C array.
#define ARRAY_LEN(arr) (sizeof(arr) / sizeof(arr[0]))

/// Marks a declaration that may be unused.
#define MAYBE_UNUSED __attribute_maybe_unused__
