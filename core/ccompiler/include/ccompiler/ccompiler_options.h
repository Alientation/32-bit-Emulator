#pragma once

#include <stddef.h>

typedef struct CompilerOptions
{
    // These paths should all be relative to the CWD or absolute paths.
    size_t n_src_files;
    const char **src_files;

    size_t n_sys_dirs;
    const char **sys_dirs;

    size_t n_local_dirs;
    const char **local_dirs;
} compiler_options_t;