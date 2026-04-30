#include "ccompiler/vector.h"

#include <stdlib.h>
#include <malloc.h>

#define VEC_SIZE(v) ((size_t) ((v).end - (v).begin))
#define VEC_CAP(v) ((size_t) ((v).cap - (v).begin))

void vector_init (vector_t * const v, void (* const free)(void *))
{
    v->free = free;
    v->begin = NULL;
    v->end = NULL;
    v->cap = NULL;
}

void vector_free (vector_t * const v)
{
    if (v->free)
    {
        for (void **cur = v->begin; cur < v->end; cur++)
        {
            v->free (*cur);
        }
    }
    free (v->begin);

    v->begin = NULL;
    v->end = NULL;
    v->cap = NULL;
}

size_t vector_size (vector_t * const v)
{
    return VEC_SIZE (*v);
}

size_t vector_capacity (vector_t * const v)
{
    return VEC_CAP (*v);
}

bool vector_empty (vector_t * const v)
{
    return VEC_SIZE (*v) == 0;
}

void vector_reserve (vector_t * const v, const size_t new_cap)
{
    if (VEC_CAP (*v) >= new_cap)
    {
        return;
    }

    const size_t size = VEC_SIZE (*v);
    void **new_begin = realloc (v->begin, new_cap * sizeof (void *));
    if (new_begin == NULL)
    {
        fprintf (stderr, "failed memory allocation");
        exit (EXIT_FAILURE);
    }

    v->begin = new_begin;
    v->end = new_begin + size;
    v->cap = new_begin + new_cap;
}

void vector_shrink_to_fit (vector_t * const v)
{
    if (v->end == v->cap)
    {
        return;
    }

    const size_t size = VEC_SIZE (*v);
    void **new_begin = realloc (v->begin, size * sizeof (void *));
    if (new_begin == NULL)
    {
        fprintf (stderr, "failed memory reallocation");
        exit (EXIT_FAILURE);
    }

    v->begin = new_begin;
    v->end = new_begin + size;
    v->cap = new_begin + size;
}

void *vector_at (vector_t * const v, const size_t idx)
{
    if (idx >= VEC_SIZE (*v))
    {
        return NULL;
    }

    return v->begin[idx];
}

void *vector_front (vector_t * const v)
{
    return v->begin[0];
}

void *vector_back (vector_t * const v)
{
    return v->end[-1];
}

void **vector_data (vector_t * const v)
{
    return v->begin;
}

void vector_assign (vector_t * const v, const size_t idx, void * const val)
{
    if (v->free)
    {
        v->free (v->begin[idx]);
    }

    v->begin[idx] = val;
}

void vector_push_back (vector_t * const v, void * const e)
{
    if (v->end == v->cap)
    {
        vector_reserve (v, VEC_CAP(*v) * 2 + 16);
    }

    v->end[0] = e;
    v->end++;
}

void vector_pop_back (vector_t * const v)
{
    v->end--;
}

void vector_clear (vector_t * const v)
{
    v->end = v->begin;
}