#pragma once

#include <stdbool.h>

void _M_ASSERT (bool cond, const char *cond_str, const char *file, int line, const char *msg_fmt, ...);

#define massert(cond, ...) _M_ASSERT (cond, #cond, __FILE__, __LINE__, __VA_ARGS__);

#define UNREACHABLE() _M_ASSERT (false, "", __FILE__, __LINE__, "reached unreachable guard");

#define M_UNREACHABLE(...) _M_ASSERT (false, "", __FILE__, __LINE__, __VA_ARGS__);
