#pragma once

#include <stdbool.h>

void _M_ASSERT (bool cond, const char *cond_str, const char *file, int line, const char *msg_fmt, ...);

#define massert(cond, msg_args...) _M_ASSERT (cond, #cond, __FILE__, __LINE__, msg_args);

#define UNREACHABLE() _M_ASSERT (false, "", __FILE__, __LINE__, "reached unreachable guard");

#define M_UNREACHABLE(msg_args...) _M_ASSERT (false, "", __FILE__, __LINE__, msg_args);
