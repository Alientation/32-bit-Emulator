#pragma once

#include <stddef.h>
#include <stdbool.h>

typedef struct Vector
{
    void (*free)(const void *val);
    const void **begin;
    const void **end;
    const void **cap;
} vector_t;

void vector_init (vector_t *v, void (*free)(const void *));
void vector_free (vector_t *v);

size_t vector_size (vector_t *v);
size_t vector_capacity (vector_t *v);
bool vector_empty (vector_t *v);
void vector_reserve (vector_t *v, size_t new_cap);
void vector_shrink_to_fit (vector_t *v);

void *vector_at (vector_t *v, size_t idx);
void *vector_front (vector_t *v);
void *vector_back (vector_t *v);
void **vector_data (vector_t *v);

void vector_assign (vector_t *v, size_t idx, const void *val);
void vector_push_back (vector_t *v, const void *e);
void vector_pop_back (vector_t *v);

void vector_clear (vector_t *v);