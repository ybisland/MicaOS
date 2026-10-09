#include "tests/test.h"
#include <micaos/data_structure/bitmap.h>

static void test_bitmap_init_clears_storage_and_reports_empty(void)
{
    bitmap_storage(words, 65);
    bitmap_t bm;
    uint32_t bit = 123U;

    words[0] = UINT32_MAX;
    words[1] = UINT32_MAX;
    words[2] = UINT32_MAX;

    bitmap_init(&bm, words, 65U);

    TEST_EQ_PTR(words, bm.words);
    TEST_EQ_U32(65U, bitmap_bit_count(&bm));
    TEST_EQ_U32(0U, words[0]);
    TEST_EQ_U32(0U, words[1]);
    TEST_EQ_U32(0U, words[2]);
    TEST_ASSERT(bitmap_is_empty(&bm));
    TEST_ASSERT(!bitmap_is_full(&bm));
    TEST_EQ_U32(0U, bitmap_count(&bm));
    TEST_ASSERT(!bitmap_find_first_set(&bm, &bit));
    TEST_ASSERT(bitmap_find_first_zero(&bm, &bit));
    TEST_EQ_U32(0U, bit);
}

static void test_bitmap_static_init_uses_declared_storage(void)
{
    bitmap_storage(words, 33) = { 0U };
    bitmap_t bm = bitmap_static_init(words, 33);

    TEST_EQ_SIZE(2U, sizeof(words) / sizeof(words[0]));
    TEST_EQ_PTR(words, bm.words);
    TEST_EQ_U32(33U, bitmap_bit_count(&bm));
    TEST_ASSERT(bitmap_is_empty(&bm));
    TEST_ASSERT(!bitmap_is_full(&bm));
    TEST_EQ_U32(0U, bitmap_count(&bm));
}

static void test_bitmap_zero_bit_bitmap_queries_are_consistent(void)
{
    bitmap_t bm;
    uint32_t bit = 77U;

    bitmap_init(&bm, NULL, 0U);

    TEST_EQ_PTR(NULL, bm.words);
    TEST_EQ_U32(0U, bitmap_bit_count(&bm));
    TEST_ASSERT(bitmap_is_empty(&bm));
    TEST_ASSERT(bitmap_is_full(&bm));
    TEST_EQ_U32(0U, bitmap_count(&bm));
    TEST_ASSERT(!bitmap_find_first_set(&bm, &bit));
    TEST_ASSERT(!bitmap_find_first_zero(&bm, &bit));
}

static void test_bitmap_set_clear_and_test_work_across_word_boundaries(void)
{
    bitmap_storage(words, 65);
    bitmap_t bm;

    bitmap_init(&bm, words, 65U);

    bitmap_set(&bm, 0U);
    bitmap_set(&bm, 31U);
    bitmap_set(&bm, 32U);
    bitmap_set(&bm, 64U);

    TEST_ASSERT(bitmap_test(&bm, 0U));
    TEST_ASSERT(bitmap_test(&bm, 31U));
    TEST_ASSERT(bitmap_test(&bm, 32U));
    TEST_ASSERT(bitmap_test(&bm, 64U));
    TEST_ASSERT(!bitmap_test(&bm, 1U));
    TEST_ASSERT(!bitmap_test(&bm, 63U));
    TEST_EQ_U32(4U, bitmap_count(&bm));
    TEST_ASSERT(!bitmap_is_empty(&bm));
    TEST_ASSERT(!bitmap_is_full(&bm));

    bitmap_clear(&bm, 31U);
    bitmap_clear(&bm, 64U);

    TEST_ASSERT(bitmap_test(&bm, 0U));
    TEST_ASSERT(!bitmap_test(&bm, 31U));
    TEST_ASSERT(bitmap_test(&bm, 32U));
    TEST_ASSERT(!bitmap_test(&bm, 64U));
    TEST_EQ_U32(2U, bitmap_count(&bm));
}

static void test_bitmap_test_and_set_and_clear_return_previous_state(void)
{
    bitmap_storage(words, 40);
    bitmap_t bm;

    bitmap_init(&bm, words, 40U);

    TEST_ASSERT(!bitmap_test_and_set(&bm, 7U));
    TEST_ASSERT(bitmap_test(&bm, 7U));
    TEST_ASSERT(bitmap_test_and_set(&bm, 7U));
    TEST_EQ_U32(1U, bitmap_count(&bm));

    TEST_ASSERT(bitmap_test_and_clear(&bm, 7U));
    TEST_ASSERT(!bitmap_test(&bm, 7U));
    TEST_ASSERT(!bitmap_test_and_clear(&bm, 7U));
    TEST_ASSERT(bitmap_is_empty(&bm));
}

static void test_bitmap_find_first_set_reports_lowest_set_bit(void)
{
    bitmap_storage(words, 70);
    bitmap_t bm;
    uint32_t bit = 0U;

    bitmap_init(&bm, words, 70U);

    TEST_ASSERT(!bitmap_find_first_set(&bm, &bit));

    bitmap_set(&bm, 63U);
    bitmap_set(&bm, 69U);
    TEST_ASSERT(bitmap_find_first_set(&bm, &bit));
    TEST_EQ_U32(63U, bit);

    bitmap_set(&bm, 2U);
    TEST_ASSERT(bitmap_find_first_set(&bm, &bit));
    TEST_EQ_U32(2U, bit);

    bitmap_clear(&bm, 2U);
    bitmap_clear(&bm, 63U);
    TEST_ASSERT(bitmap_find_first_set(&bm, &bit));
    TEST_EQ_U32(69U, bit);
}

static void test_bitmap_find_first_zero_reports_lowest_clear_bit(void)
{
    bitmap_storage(words, 70);
    bitmap_t bm;
    uint32_t bit = 0U;

    bitmap_init(&bm, words, 70U);

    TEST_ASSERT(bitmap_find_first_zero(&bm, &bit));
    TEST_EQ_U32(0U, bit);

    bitmap_set_all(&bm);
    TEST_ASSERT(!bitmap_find_first_zero(&bm, &bit));

    bitmap_clear(&bm, 63U);
    bitmap_clear(&bm, 69U);
    TEST_ASSERT(bitmap_find_first_zero(&bm, &bit));
    TEST_EQ_U32(63U, bit);

    bitmap_clear(&bm, 1U);
    TEST_ASSERT(bitmap_find_first_zero(&bm, &bit));
    TEST_EQ_U32(1U, bit);
}

static void test_bitmap_set_all_and_clear_all_update_state_queries(void)
{
    bitmap_storage(words, 65);
    bitmap_t bm;
    uint32_t bit = 0U;

    bitmap_init(&bm, words, 65U);

    bitmap_set_all(&bm);

    TEST_ASSERT(!bitmap_is_empty(&bm));
    TEST_ASSERT(bitmap_is_full(&bm));
    TEST_EQ_U32(65U, bitmap_count(&bm));
    TEST_ASSERT(bitmap_find_first_set(&bm, &bit));
    TEST_EQ_U32(0U, bit);
    TEST_ASSERT(!bitmap_find_first_zero(&bm, &bit));
    TEST_EQ_U32(0xFFFFFFFFU, words[0]);
    TEST_EQ_U32(0xFFFFFFFFU, words[1]);
    TEST_EQ_U32(0x00000001U, words[2]);

    bitmap_clear_all(&bm);

    TEST_ASSERT(bitmap_is_empty(&bm));
    TEST_ASSERT(!bitmap_is_full(&bm));
    TEST_EQ_U32(0U, bitmap_count(&bm));
    TEST_ASSERT(!bitmap_find_first_set(&bm, &bit));
    TEST_ASSERT(bitmap_find_first_zero(&bm, &bit));
    TEST_EQ_U32(0U, bit);
}

static void test_bitmap_tail_bits_are_ignored_by_query_apis(void)
{
    bitmap_word_t words[2] = { 0U, 0xFFFFFFFEU };
    bitmap_t bm = bitmap_static_init(words, 33);
    uint32_t bit = 0U;

    TEST_ASSERT(bitmap_is_empty(&bm));
    TEST_ASSERT(!bitmap_is_full(&bm));
    TEST_EQ_U32(0U, bitmap_count(&bm));
    TEST_ASSERT(!bitmap_find_first_set(&bm, &bit));
    TEST_ASSERT(bitmap_find_first_zero(&bm, &bit));
    TEST_EQ_U32(0U, bit);

    words[0] = UINT32_MAX;
    words[1] = UINT32_MAX;

    TEST_ASSERT(!bitmap_is_empty(&bm));
    TEST_ASSERT(bitmap_is_full(&bm));
    TEST_EQ_U32(33U, bitmap_count(&bm));
    TEST_ASSERT(bitmap_find_first_set(&bm, &bit));
    TEST_EQ_U32(0U, bit);
    TEST_ASSERT(!bitmap_find_first_zero(&bm, &bit));
}

static void test_bitmap_exact_word_sized_tail_uses_all_bits(void)
{
    bitmap_word_t words[1] = { 0x7FFFFFFFU };
    bitmap_t bm = bitmap_static_init(words, 32);
    uint32_t bit = 0U;

    TEST_ASSERT(!bitmap_is_full(&bm));
    TEST_EQ_U32(31U, bitmap_count(&bm));
    TEST_ASSERT(bitmap_find_first_zero(&bm, &bit));
    TEST_EQ_U32(31U, bit);

    bitmap_set(&bm, 31U);

    TEST_ASSERT(bitmap_is_full(&bm));
    TEST_EQ_U32(32U, bitmap_count(&bm));
    TEST_ASSERT(!bitmap_find_first_zero(&bm, &bit));
}

void test_bitmap_run(void)
{
    TEST_RUN(test_bitmap_init_clears_storage_and_reports_empty);
    TEST_RUN(test_bitmap_static_init_uses_declared_storage);
    TEST_RUN(test_bitmap_zero_bit_bitmap_queries_are_consistent);
    TEST_RUN(test_bitmap_set_clear_and_test_work_across_word_boundaries);
    TEST_RUN(test_bitmap_test_and_set_and_clear_return_previous_state);
    TEST_RUN(test_bitmap_find_first_set_reports_lowest_set_bit);
    TEST_RUN(test_bitmap_find_first_zero_reports_lowest_clear_bit);
    TEST_RUN(test_bitmap_set_all_and_clear_all_update_state_queries);
    TEST_RUN(test_bitmap_tail_bits_are_ignored_by_query_apis);
    TEST_RUN(test_bitmap_exact_word_sized_tail_uses_all_bits);
}
