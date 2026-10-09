#include "tests/test.h"
#include <micaos/common/minmax.h>

static int next_value(int *counter, int value)
{
    (*counter)++;
    return value;
}

static void test_minmax_two_argument_values(void)
{
    TEST_EQ_U32(5U, max(3U, 5U));
    TEST_EQ_U32(5U, max(5U, 3U));
    TEST_EQ_U32(3U, min(3U, 5U));
    TEST_EQ_U32(3U, min(5U, 3U));
}

static void test_minmax_three_argument_values(void)
{
    TEST_EQ_U32(9U, max3(9U, 3U, 7U));
    TEST_EQ_U32(9U, max3(3U, 9U, 7U));
    TEST_EQ_U32(9U, max3(3U, 7U, 9U));
    TEST_EQ_U32(2U, min3(2U, 8U, 5U));
    TEST_EQ_U32(2U, min3(8U, 2U, 5U));
    TEST_EQ_U32(2U, min3(8U, 5U, 2U));
}

static void test_minmax_clamp_values(void)
{
    TEST_EQ_U32(10U, clamp(5U, 10U, 20U));
    TEST_EQ_U32(15U, clamp(15U, 10U, 20U));
    TEST_EQ_U32(20U, clamp(30U, 10U, 20U));
}

static void test_minmax_signed_values(void)
{
    TEST_ASSERT(max(-3, -7) == -3);
    TEST_ASSERT(min(-3, -7) == -7);
    TEST_ASSERT(clamp(-10, -5, 5) == -5);
    TEST_ASSERT(clamp(10, -5, 5) == 5);
    TEST_ASSERT(clamp(3, -5, 5) == 3);
}

static void test_minmax_arguments_are_evaluated_once(void)
{
    int count_a = 0;
    int count_b = 0;
    int count_c = 0;
    int count_v = 0;
    int count_l = 0;
    int count_h = 0;

    TEST_ASSERT(max(next_value(&count_a, 1), next_value(&count_b, 2)) == 2);
    TEST_EQ_U32(1U, (uint32_t)count_a);
    TEST_EQ_U32(1U, (uint32_t)count_b);

    TEST_ASSERT(min3(next_value(&count_a, 3),
                     next_value(&count_b, 2),
                     next_value(&count_c, 1)) == 1);
    TEST_EQ_U32(2U, (uint32_t)count_a);
    TEST_EQ_U32(2U, (uint32_t)count_b);
    TEST_EQ_U32(1U, (uint32_t)count_c);

    TEST_ASSERT(clamp(next_value(&count_v, 9),
                      next_value(&count_l, 0),
                      next_value(&count_h, 5)) == 5);
    TEST_EQ_U32(1U, (uint32_t)count_v);
    TEST_EQ_U32(1U, (uint32_t)count_l);
    TEST_EQ_U32(1U, (uint32_t)count_h);
}

void test_minmax_run(void)
{
    TEST_RUN(test_minmax_two_argument_values);
    TEST_RUN(test_minmax_three_argument_values);
    TEST_RUN(test_minmax_clamp_values);
    TEST_RUN(test_minmax_signed_values);
    TEST_RUN(test_minmax_arguments_are_evaluated_once);
}
