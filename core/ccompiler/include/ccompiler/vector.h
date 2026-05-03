/*
 * Usage: before including this file, define:
 *   VEC_TYPE: the element type
 *   VEC_NAME: the name suffix
 *
 * Example:
 *   #define VEC_TYPE int
 *   #define VEC_NAME int
 *   #include "vector_template.h"
 *
 *   #define VEC_TYPE token_t*
 *   #define VEC_NAME token_ptr
 *   #include "vector_template.h"
 */

#ifndef VEC_TYPE
#error "VEC_TYPE must be defined before including vector_template.h"
#endif
#ifndef VEC_NAME
#error "VEC_NAME must be defined before including vector_template.h"
#endif

#define VEC_CONCAT_(a, b) a##b
#define VEC_CONCAT(a, b) VEC_CONCAT_(a, b)
#define VEC_FN(fn) VEC_CONCAT(VEC_CONCAT(vec_, VEC_NAME), VEC_CONCAT(_, fn))
#define VEC_T VEC_CONCAT(VEC_CONCAT(vec_, VEC_NAME), _t)

#include <stddef.h>
#include <stdbool.h>
#include <stdlib.h>
#include <stdio.h>

typedef struct VEC_T
{
    void (*free_elem)(VEC_TYPE val);
    VEC_TYPE *begin;
    VEC_TYPE *end;
    VEC_TYPE *cap;
} VEC_T;

static inline void VEC_FN (init) (VEC_T * const v, void (* const free_elem)(VEC_TYPE))
{
    v->free_elem = free_elem;
    v->begin = NULL;
    v->end   = NULL;
    v->cap   = NULL;
}

static inline void VEC_FN (free) (VEC_T * const v)
{
    if (v->free_elem)
    {
        for (VEC_TYPE *cur = v->begin; cur < v->end; cur++)
        {
            v->free_elem (*cur);
        }
    }
    free (v->begin);
    v->begin = NULL;
    v->end   = NULL;
    v->cap   = NULL;
}

static inline size_t VEC_FN (size) (const VEC_T * const v)
{
    return (size_t) (v->end - v->begin);
}

static inline size_t VEC_FN (capacity) (const VEC_T * const v)
{
    return (size_t) (v->cap - v->begin);
}

static inline bool VEC_FN (empty) (const VEC_T * const v)
{
    return v->begin == v->end;
}

static inline VEC_TYPE *VEC_FN (data) (VEC_T * const v)
{
    return v->begin;
}

static inline VEC_TYPE *VEC_FN (at) (VEC_T * const v, const size_t idx)
{
    if (idx >= VEC_FN (size) (v))
    {
        return NULL;
    }

    return &v->begin[idx];
}

static inline VEC_TYPE *VEC_FN (front) (VEC_T * const v)
{
    return v->begin;
}

static inline VEC_TYPE *VEC_FN (back) (VEC_T * const v)
{
    return v->end - 1;
}

static inline void VEC_FN (reserve) (VEC_T * const v, const size_t new_cap)
{
    if (VEC_FN (capacity) (v) >= new_cap)
    {
        return;
    }

    const size_t sz = VEC_FN (size) (v);
    VEC_TYPE * const new_begin = (VEC_TYPE *) realloc (v->begin, new_cap * sizeof (VEC_TYPE));
    if (!new_begin)
    {
        fprintf (stderr, "vector: allocation failed\n");
        exit (EXIT_FAILURE);
    }
    v->begin = new_begin;
    v->end = new_begin + sz;
    v->cap = new_begin + new_cap;
}

static inline void VEC_FN (shrink_to_fit) (VEC_T * const v)
{
    const size_t sz = VEC_FN (size) (v);
    if (sz == 0)
    {
        free (v->begin);
        v->begin = v->end = v->cap = NULL;
        return;
    }
    if (v->end == v->cap)
    {
        return;
    }

    VEC_TYPE * const new_begin = (VEC_TYPE *) realloc (v->begin, sz * sizeof (VEC_TYPE));
    if (!new_begin && sz != 0)
    {
        fprintf (stderr, "vector: reallocation failed\n");
        exit (EXIT_FAILURE);
    }
    v->begin = new_begin;
    v->end = new_begin + sz;
    v->cap = new_begin + sz;
}

static inline void VEC_FN (push_back) (VEC_T * const v, VEC_TYPE e)
{
    if (v->end == v->cap)
    {
        VEC_FN (reserve)(v, VEC_FN (capacity)(v) * 2 + 16);
    }
    *v->end++ = e;
}

static inline void VEC_FN (pop_back) (VEC_T * const v)
{
    if (v->free_elem)
    {
        v->free_elem (v->end[-1]);
    }
    v->end--;
}

static inline void VEC_FN (assign) (VEC_T * const v, const size_t idx, VEC_TYPE val)
{
    if (v->free_elem)
    {
        v->free_elem (v->begin[idx]);
    }
    v->begin[idx] = val;
}

static inline void VEC_FN (clear) (VEC_T * const v)
{
    while (!VEC_FN (empty) (v))
    {
        VEC_FN (pop_back) (v);
    }
}

#undef VEC_CONCAT_
#undef VEC_CONCAT
#undef VEC_FN
#undef VEC_T
#undef VEC_TYPE
#undef VEC_NAME