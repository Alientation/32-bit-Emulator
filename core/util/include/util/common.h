#pragma once

#define UNLIKELY(cond) __builtin_expect(cond, 0)
#define LIKELY(cond) __builtin_expect(cond, 1)