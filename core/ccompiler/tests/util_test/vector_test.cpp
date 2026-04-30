#include <ccompiler_test/ccompiler_test.h>

TEST (vector, vector_empty)
{
    vector_t v;
    vector_init (&v, NULL);

    EXPECT_EQ (vector_size (&v), (size_t) 0);
    EXPECT_EQ (vector_empty (&v), true);
    EXPECT_EQ (vector_at (&v, 0), nullptr);
    vector_clear (&v);

    vector_free (&v);
}

TEST (vector, vector_basic)
{
    vector_t v;
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

    vector_free (&v);
}
