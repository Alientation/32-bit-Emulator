#include <ccompiler_test/ccompiler_test.h>

#define VEC_TYPE int
#define VEC_NAME int
#include "ccompiler/vector.h"

#define VEC_TYPE int*
#define VEC_NAME intptr
#include "ccompiler/vector.h"

TEST (vector, vector_empty)
{
    vec_int_t v{};
    vec_int_init (&v, NULL);

    EXPECT_EQ (vec_int_size (&v), (size_t) 0);
    EXPECT_EQ (vec_int_capacity(&v), (size_t) 0);
    EXPECT_TRUE (vec_int_empty (&v));
    EXPECT_EQ (vec_int_at (&v, 0), nullptr);
    vec_int_clear (&v);

    vec_int_free (&v);
}

TEST (vector, vector_basic)
{
    vec_int_t v{};
    int data[] = {1, 5, 2, 3, 0, 2};

    vec_int_init (&v, NULL);

    for (size_t i = 0; i < ARRAY_LEN (data); i++)
    {
        vec_int_push_back (&v, data[i]);
    }

    EXPECT_EQ (vec_int_size (&v), ARRAY_LEN (data));
    EXPECT_GE (vec_int_capacity (&v), vec_int_size (&v));
    for (size_t i = 0; i < ARRAY_LEN (data); i++)
    {
        EXPECT_EQ (*vec_int_at (&v, i), data[i]);
    }

    for (size_t removed = 0; removed < ARRAY_LEN (data); removed++)
    {
        EXPECT_EQ (*vec_int_front (&v), data[0]);
        EXPECT_EQ (*vec_int_back (&v), data[ARRAY_LEN (data) - removed - 1]);
        vec_int_pop_back (&v);

        EXPECT_EQ (vec_int_size (&v), ARRAY_LEN (data) - removed - 1);
        for (size_t i = 0; i < ARRAY_LEN (data) - removed - 1; i++)
        {
            EXPECT_EQ (*vec_int_at (&v, i), data[i]);
        }
    }

    EXPECT_TRUE (vec_int_empty (&v));
    vec_int_free (&v);
}

TEST (vector, free_nulls_pointers)
{
    vec_int_t v{};
    vec_int_init (&v, NULL);

    vec_int_push_back (&v, 1);
    vec_int_free (&v);

    // After free the internal pointers must not dangle.
    EXPECT_EQ (v.begin, nullptr);
    EXPECT_EQ (v.end,   nullptr);
    EXPECT_EQ (v.cap,   nullptr);
}

TEST (vector, double_free_is_safe)
{
    vec_int_t v{};
    vec_int_init (&v, NULL);

    vec_int_push_back (&v, 42);
    vec_int_free (&v);
    vec_int_free (&v);

    EXPECT_EQ (vec_int_size (&v), 0u);
}

TEST (vector, push_back_triggers_growth)
{
    vec_int_t v{};
    vec_int_init (&v, NULL);

    const int N = 64;
    for (int i = 0; i < N; i++)
    {
        vec_int_push_back (&v, i);
    }

    EXPECT_EQ (vec_int_size (&v), (size_t) N);
    EXPECT_GE (vec_int_capacity (&v), (size_t) N);

    for (int i = 0; i < N; i++)
    {
        EXPECT_EQ (*vec_int_at (&v, i), i);
    }

    vec_int_free (&v);
}

static void free_int (int *i)
{
    free (i);
}

static int *make_int (int i)
{
    int *iptr = (int *) malloc (sizeof (int));
    *iptr = i;
    return iptr;
}

TEST (vector, pop_back_calls_element_free)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);

    vec_intptr_push_back (&v, make_int (5));
    vec_intptr_push_back (&v, make_int (6));
    vec_intptr_pop_back (&v);

    EXPECT_EQ (vec_intptr_size (&v), 1u);
    EXPECT_EQ (**vec_intptr_at (&v, 0), 5);

    vec_intptr_free (&v);
}

TEST (vector, assign_replaces_element)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);

    vec_intptr_push_back (&v, make_int (10));
    vec_intptr_assign (&v, 0, make_int (99));

    EXPECT_EQ (**vec_intptr_at (&v, 0), 99);
    EXPECT_EQ (vec_intptr_size (&v), 1u);

    vec_intptr_free (&v);
}

TEST (vector, assign_middle_element)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);

    for (int i = 0; i < 5; i++)
    {
        vec_intptr_push_back (&v, make_int (i));
    }
    vec_intptr_assign (&v, 2, make_int (42));

    EXPECT_EQ (**vec_intptr_at (&v, 0), 0);
    EXPECT_EQ (**vec_intptr_at (&v, 1), 1);
    EXPECT_EQ (**vec_intptr_at (&v, 2), 42);
    EXPECT_EQ (**vec_intptr_at (&v, 3), 3);
    EXPECT_EQ (**vec_intptr_at (&v, 4), 4);

    vec_intptr_free (&v);
}

TEST (vector, assign_last_element)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);

    vec_intptr_push_back (&v, make_int (1));
    vec_intptr_push_back (&v, make_int (2));
    vec_intptr_assign (&v, 1, make_int (77));

    EXPECT_EQ (**vec_intptr_at (&v, 1), 77);
    EXPECT_EQ (vec_intptr_size (&v), 2u);

    vec_intptr_free (&v);
}

TEST (vector, clear_empties_vector)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);

    for (int i = 0; i < 8; i++)
    {
        vec_intptr_push_back (&v, make_int (i));
    }
    vec_intptr_clear (&v);

    EXPECT_EQ (vec_intptr_size (&v), 0u);
    EXPECT_TRUE (vec_intptr_empty (&v));

    vec_intptr_free (&v);
}

TEST (vector, clear_then_push_back_works)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);

    vec_intptr_push_back (&v, make_int (1));
    vec_intptr_push_back (&v, make_int (2));
    vec_intptr_clear (&v);
    vec_intptr_push_back (&v, make_int (99));

    EXPECT_EQ (vec_intptr_size (&v), 1u);
    EXPECT_EQ (**vec_intptr_at (&v, 0), 99);

    vec_intptr_free (&v);
}

TEST (vector, clear_empty_vector_is_safe)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);

    vec_intptr_clear (&v);

    EXPECT_EQ (vec_intptr_size (&v), 0u);
    EXPECT_TRUE (vec_intptr_empty (&v));

    vec_intptr_free (&v);
}

TEST (vector, reserve_increases_capacity)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);

    vec_intptr_reserve (&v, 100);

    EXPECT_GE (vec_intptr_capacity (&v), 100u);
    EXPECT_EQ (vec_intptr_size (&v), 0u);

    vec_intptr_free (&v);
}

TEST (vector, reserve_does_not_change_size)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);

    vec_intptr_push_back (&v, make_int (1));
    vec_intptr_reserve (&v, 50);

    EXPECT_EQ (vec_intptr_size (&v), 1u);

    vec_intptr_free (&v);
}

TEST (vector, reserve_smaller_than_current_capacity_is_noop)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);

    vec_intptr_reserve (&v, 64);
    const size_t cap_before = vec_intptr_capacity (&v);

    vec_intptr_reserve (&v, 4);

    EXPECT_EQ (vec_intptr_capacity (&v), cap_before);

    vec_intptr_free (&v);
}

TEST (vector, reserve_preserves_existing_elements)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);

    for (int i = 0; i < 5; i++)
    {
        vec_intptr_push_back (&v, make_int (i));
    }
    vec_intptr_reserve (&v, 200);

    for (int i = 0; i < 5; i++)
    {
        EXPECT_EQ (**vec_intptr_at (&v, i), i);
    }

    vec_intptr_free (&v);
}

TEST (vector, shrink_to_fit_capacity_equals_size)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);

    vec_intptr_reserve (&v, 128);
    for (int i = 0; i < 4; i++)
    {
        vec_intptr_push_back (&v, make_int (i));
    }
    vec_intptr_shrink_to_fit (&v);

    EXPECT_EQ (vec_intptr_capacity (&v), vec_intptr_size (&v));

    vec_intptr_shrink_to_fit (&v);

    vec_intptr_free (&v);
}

TEST (vector, shrink_to_fit_preserves_elements)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);

    vec_intptr_reserve (&v, 100);
    vec_intptr_push_back (&v, make_int (11));
    vec_intptr_push_back (&v, make_int (22));
    vec_intptr_shrink_to_fit (&v);

    EXPECT_EQ (vec_intptr_size (&v), 2u);
    EXPECT_EQ (**vec_intptr_at (&v, 0), 11);
    EXPECT_EQ (**vec_intptr_at (&v, 1), 22);

    vec_intptr_free (&v);
}

TEST (vector, shrink_to_fit_on_empty_vector)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);

    vec_intptr_reserve (&v, 64);
    vec_intptr_shrink_to_fit (&v);

    EXPECT_EQ (vec_intptr_size (&v), 0u);

    vec_intptr_free (&v);
}

TEST (vector, data_returns_null_or_valid_on_empty)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);

    // Just must not crash.
    (void) vec_intptr_data (&v);

    vec_intptr_free (&v);
}

TEST (vector, data_elements_match_at_elements)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);

    for (int i = 0; i < 5; i++)
    {
        vec_intptr_push_back (&v, make_int (i * 3));
    }

    int **d = vec_intptr_data (&v);
    for (int i = 0; i < 5; i++)
    {
        EXPECT_EQ (d + i, vec_intptr_at (&v, i));
    }

    vec_intptr_free (&v);
}

TEST (vector, null_free_callback_allowed)
{
    // Elements not heap-allocated; vector must not call free on them.
    vec_intptr_t v{};
    vec_intptr_init (&v, nullptr);

    static int vals[] = { 10, 20, 30 };
    vec_intptr_push_back (&v, &vals[0]);
    vec_intptr_push_back (&v, &vals[1]);
    vec_intptr_push_back (&v, &vals[2]);

    EXPECT_EQ (vec_intptr_size (&v), 3u);
    EXPECT_EQ (**vec_intptr_at (&v, 0), 10);
    EXPECT_EQ (**vec_intptr_at (&v, 2), 30);

    vec_intptr_free (&v);
}

TEST (vector, capacity_always_gte_size)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);

    for (int i = 0; i < 32; i++)
    {
        vec_intptr_push_back (&v, make_int (i));
        EXPECT_GE (vec_intptr_capacity (&v), vec_intptr_size (&v));
    }
    for (int i = 0; i < 16; i++)
    {
        vec_intptr_pop_back (&v);
        EXPECT_GE (vec_intptr_capacity (&v), vec_intptr_size (&v));
    }

    vec_intptr_free (&v);
}

TEST (vector, size_zero_after_init)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);
    EXPECT_EQ (vec_intptr_size (&v), 0u);
    vec_intptr_free (&v);
}

TEST (vector, empty_true_after_init)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);
    EXPECT_TRUE (vec_intptr_empty (&v));
    vec_intptr_free (&v);
}

TEST (vector, empty_false_after_push)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);
    vec_intptr_push_back (&v, make_int (1));
    EXPECT_FALSE (vec_intptr_empty (&v));
    vec_intptr_free (&v);
}

TEST (vector, empty_true_after_clear)
{
    vec_intptr_t v{};
    vec_intptr_init (&v, free_int);
    vec_intptr_push_back (&v, make_int (1));
    vec_intptr_clear (&v);
    EXPECT_TRUE (vec_intptr_empty (&v));
    vec_intptr_free (&v);
}