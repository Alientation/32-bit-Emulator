#include <ccompiler_test/ccompiler_test.h>

static void free_int (void *val)
{
    free (val);
}

static void *make_int (int n)
{
    int *p = (int *) malloc (sizeof (int));
    *p = n;
    return p;
}

static int get_int (vector_t *v, size_t idx)
{
    return *(int *) vector_at (v, idx);
}


TEST (vector, vector_empty)
{
    vector_t v{};
    vector_init (&v, NULL);

    EXPECT_EQ (vector_size (&v), (size_t) 0);
    EXPECT_EQ (vector_capacity(&v), (size_t) 0);
    EXPECT_TRUE (vector_empty (&v));
    EXPECT_EQ (vector_at (&v, 0), nullptr);
    vector_clear (&v);

    vector_free (&v);
}

TEST (vector, vector_basic)
{
    vector_t v{};
    int data[] = {1, 5, 2, 3, 0, 2};

    vector_init (&v, NULL);

    for (size_t i = 0; i < ARRAY_LEN (data); i++)
    {
        vector_push_back (&v, &data[i]);
    }

    EXPECT_EQ (vector_size (&v), ARRAY_LEN (data));
    EXPECT_GE (vector_capacity (&v), vector_size (&v));
    for (size_t i = 0; i < ARRAY_LEN (data); i++)
    {
        EXPECT_EQ (vector_at (&v, i), &data[i]);
        EXPECT_EQ (* (int *) vector_at (&v, i), data[i]);
    }

    for (size_t removed = 0; removed < ARRAY_LEN (data); removed++)
    {
        EXPECT_EQ (* (int *) vector_front (&v), data[0]);
        EXPECT_EQ (vector_front (&v), &data[0]);
        EXPECT_EQ (vector_back (&v), &data[ARRAY_LEN (data) - removed - 1]);
        vector_pop_back (&v);

        EXPECT_EQ (vector_size (&v), ARRAY_LEN (data) - removed - 1);
        for (size_t i = 0; i < ARRAY_LEN (data) - removed - 1; i++)
        {
            EXPECT_EQ (vector_at (&v, i), &data[i]);
            EXPECT_EQ (* (int *) vector_at (&v, i), data[i]);
        }
    }

    EXPECT_TRUE (vector_empty (&v));
    vector_free (&v);
}

TEST (vector, free_nulls_pointers)
{
    vector_t v{};
    vector_init (&v, free_int);

    vector_push_back (&v, make_int (1));
    vector_free (&v);

    // After free the internal pointers must not dangle.
    EXPECT_EQ (v.begin, nullptr);
    EXPECT_EQ (v.end,   nullptr);
    EXPECT_EQ (v.cap,   nullptr);
}

TEST (vector, double_free_is_safe)
{
    vector_t v{};
    vector_init (&v, free_int);

    vector_push_back (&v, make_int (42));
    vector_free (&v);
    vector_free (&v);

    EXPECT_EQ (vector_size (&v), 0u);
}

TEST (vector, push_back_triggers_growth)
{
    vector_t v{};
    vector_init (&v, free_int);

    const int N = 64;
    for (int i = 0; i < N; i++)
    {
        vector_push_back (&v, make_int (i));
    }

    EXPECT_EQ (vector_size (&v), (size_t) N);
    EXPECT_GE (vector_capacity (&v), (size_t) N);

    for (int i = 0; i < N; i++)
    {
        EXPECT_EQ (get_int (&v, i), i);
    }

    vector_free (&v);
}

TEST (vector, pop_back_calls_element_free)
{
    // if pop_back does NOT call the free function, valgrind / sanitizers will report a leak
    vector_t v{};
    vector_init (&v, free_int);

    vector_push_back (&v, make_int (5));
    vector_push_back (&v, make_int (6));
    vector_pop_back (&v);

    EXPECT_EQ (vector_size (&v), 1u);
    EXPECT_EQ (get_int (&v, 0), 5);

    vector_free (&v);
}

TEST (vector, assign_replaces_element)
{
    vector_t v{};
    vector_init (&v, free_int);

    vector_push_back (&v, make_int (10));
    vector_assign (&v, 0, make_int (99));

    EXPECT_EQ (get_int (&v, 0), 99);
    EXPECT_EQ (vector_size (&v), 1u);

    vector_free (&v);
}

TEST (vector, assign_middle_element)
{
    vector_t v{};
    vector_init (&v, free_int);

    for (int i = 0; i < 5; i++)
    {
        vector_push_back (&v, make_int (i));
    }
    vector_assign (&v, 2, make_int (42));

    EXPECT_EQ (get_int (&v, 0), 0);
    EXPECT_EQ (get_int (&v, 1), 1);
    EXPECT_EQ (get_int (&v, 2), 42);
    EXPECT_EQ (get_int (&v, 3), 3);
    EXPECT_EQ (get_int (&v, 4), 4);

    vector_free (&v);
}

TEST (vector, assign_last_element)
{
    vector_t v{};
    vector_init (&v, free_int);

    vector_push_back (&v, make_int (1));
    vector_push_back (&v, make_int (2));
    vector_assign (&v, 1, make_int (77));

    EXPECT_EQ (get_int (&v, 1), 77);
    EXPECT_EQ (vector_size (&v), 2u);

    vector_free (&v);
}

TEST (vector, clear_empties_vector)
{
    vector_t v{};
    vector_init (&v, free_int);

    for (int i = 0; i < 8; i++)
    {
        vector_push_back (&v, make_int (i));
    }
    vector_clear (&v);

    EXPECT_EQ (vector_size (&v), 0u);
    EXPECT_TRUE (vector_empty (&v));

    vector_free (&v);
}

TEST (vector, clear_then_push_back_works)
{
    vector_t v{};
    vector_init (&v, free_int);

    vector_push_back (&v, make_int (1));
    vector_push_back (&v, make_int (2));
    vector_clear (&v);
    vector_push_back (&v, make_int (99));

    EXPECT_EQ (vector_size (&v), 1u);
    EXPECT_EQ (get_int (&v, 0), 99);

    vector_free (&v);
}

TEST (vector, clear_empty_vector_is_safe)
{
    vector_t v{};
    vector_init (&v, free_int);

    vector_clear (&v);

    EXPECT_EQ (vector_size (&v), 0u);
    EXPECT_TRUE (vector_empty (&v));

    vector_free (&v);
}

TEST (vector, reserve_increases_capacity)
{
    vector_t v{};
    vector_init (&v, free_int);

    vector_reserve (&v, 100);

    EXPECT_GE (vector_capacity (&v), 100u);
    EXPECT_EQ (vector_size (&v), 0u);

    vector_free (&v);
}

TEST (vector, reserve_does_not_change_size)
{
    vector_t v{};
    vector_init (&v, free_int);

    vector_push_back (&v, make_int (1));
    vector_reserve (&v, 50);

    EXPECT_EQ (vector_size (&v), 1u);

    vector_free (&v);
}

TEST (vector, reserve_smaller_than_current_capacity_is_noop)
{
    vector_t v{};
    vector_init (&v, free_int);

    vector_reserve (&v, 64);
    const size_t cap_before = vector_capacity (&v);

    vector_reserve (&v, 4);

    EXPECT_EQ (vector_capacity (&v), cap_before);

    vector_free (&v);
}

TEST (vector, reserve_preserves_existing_elements)
{
    vector_t v{};
    vector_init (&v, free_int);

    for (int i = 0; i < 5; i++)
    {
        vector_push_back (&v, make_int (i));
    }
    vector_reserve (&v, 200);

    for (int i = 0; i < 5; i++)
    {
        EXPECT_EQ (get_int (&v, i), i);
    }

    vector_free (&v);
}

TEST (vector, shrink_to_fit_capacity_equals_size)
{
    vector_t v{};
    vector_init (&v, free_int);

    vector_reserve (&v, 128);
    for (int i = 0; i < 4; i++)
    {
        vector_push_back (&v, make_int (i));
    }
    vector_shrink_to_fit (&v);

    EXPECT_EQ (vector_capacity (&v), vector_size (&v));

    vector_shrink_to_fit (&v);

    vector_free (&v);
}

TEST (vector, shrink_to_fit_preserves_elements)
{
    vector_t v{};
    vector_init (&v, free_int);

    vector_reserve (&v, 100);
    vector_push_back (&v, make_int (11));
    vector_push_back (&v, make_int (22));
    vector_shrink_to_fit (&v);

    EXPECT_EQ (vector_size (&v), 2u);
    EXPECT_EQ (get_int (&v, 0), 11);
    EXPECT_EQ (get_int (&v, 1), 22);

    vector_free (&v);
}

TEST (vector, shrink_to_fit_on_empty_vector)
{
    vector_t v{};
    vector_init (&v, free_int);

    vector_reserve (&v, 64);
    vector_shrink_to_fit (&v);

    EXPECT_EQ (vector_size (&v), 0u);

    vector_free (&v);
}

TEST (vector, data_returns_null_or_valid_on_empty)
{
    vector_t v{};
    vector_init (&v, free_int);

    // Just must not crash.
    (void) vector_data (&v);

    vector_free (&v);
}

TEST (vector, data_elements_match_at_elements)
{
    vector_t v{};
    vector_init (&v, free_int);

    for (int i = 0; i < 5; i++)
    {
        vector_push_back (&v, make_int (i * 3));
    }

    void **d = vector_data (&v);
    for (int i = 0; i < 5; i++)
    {
        EXPECT_EQ (d[i], vector_at (&v, i));
    }

    vector_free (&v);
}

TEST (vector, null_free_callback_allowed)
{
    // Elements not heap-allocated; vector must not call free on them.
    vector_t v{};
    vector_init (&v, nullptr);

    static int vals[] = { 10, 20, 30 };
    vector_push_back (&v, &vals[0]);
    vector_push_back (&v, &vals[1]);
    vector_push_back (&v, &vals[2]);

    EXPECT_EQ (vector_size (&v), 3u);
    EXPECT_EQ (*(int *) vector_at (&v, 0), 10);
    EXPECT_EQ (*(int *) vector_at (&v, 2), 30);

    vector_free (&v);
}

TEST (vector, capacity_always_gte_size)
{
    vector_t v{};
    vector_init (&v, free_int);

    for (int i = 0; i < 32; i++)
    {
        vector_push_back (&v, make_int (i));
        EXPECT_GE (vector_capacity (&v), vector_size (&v));
    }
    for (int i = 0; i < 16; i++)
    {
        vector_pop_back (&v);
        EXPECT_GE (vector_capacity (&v), vector_size (&v));
    }

    vector_free (&v);
}

TEST (vector, size_zero_after_init)
{
    vector_t v{};
    vector_init (&v, free_int);
    EXPECT_EQ (vector_size (&v), 0u);
    vector_free (&v);
}

TEST (vector, empty_true_after_init)
{
    vector_t v{};
    vector_init (&v, free_int);
    EXPECT_TRUE (vector_empty (&v));
    vector_free (&v);
}

TEST (vector, empty_false_after_push)
{
    vector_t v{};
    vector_init (&v, free_int);
    vector_push_back (&v, make_int (1));
    EXPECT_FALSE (vector_empty (&v));
    vector_free (&v);
}

TEST (vector, empty_true_after_clear)
{
    vector_t v{};
    vector_init (&v, free_int);
    vector_push_back (&v, make_int (1));
    vector_clear (&v);
    EXPECT_TRUE (vector_empty (&v));
    vector_free (&v);
}