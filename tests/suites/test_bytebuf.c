#include "tests/test.h"
#include <micaos/data_structure/bytebuf.h>

static void test_bytebuf_expect_bytes(const uint8_t *expected,
                                      const uint8_t *actual,
                                      uint32_t size)
{
    uint32_t i;

    for (i = 0U; i < size; i++) {
        TEST_EQ_U32(expected[i], actual[i]);
    }
}

static void test_bytebuf_init_makes_empty_buffer(void)
{
    uint8_t storage[4];
    bytebuf_t bb;

    bytebuf_init(&bb, storage, sizeof(storage));

    TEST_EQ_PTR(storage, bb.buffer);
    TEST_EQ_U32(sizeof(storage), bytebuf_capacity(&bb));
    TEST_EQ_U32(0U, bytebuf_size(&bb));
    TEST_EQ_U32(sizeof(storage), bytebuf_space(&bb));
    TEST_ASSERT(bytebuf_is_empty(&bb));
    TEST_ASSERT(!bytebuf_is_full(&bb));
}

static void test_bytebuf_static_init_makes_empty_buffer(void)
{
    uint8_t storage[3];
    bytebuf_t bb = bytebuf_static_init(storage, sizeof(storage));

    TEST_EQ_PTR(storage, bb.buffer);
    TEST_EQ_U32(sizeof(storage), bytebuf_capacity(&bb));
    TEST_EQ_U32(0U, bytebuf_size(&bb));
    TEST_EQ_U32(sizeof(storage), bytebuf_space(&bb));
    TEST_ASSERT(bytebuf_is_empty(&bb));
    TEST_ASSERT(!bytebuf_is_full(&bb));
}

static void test_bytebuf_reset_drops_buffered_data(void)
{
    uint8_t storage[4];
    uint8_t out;
    const uint8_t data[] = { 1U, 2U, 3U };
    bytebuf_t bb;

    bytebuf_init(&bb, storage, sizeof(storage));
    TEST_EQ_U32(sizeof(data), bytebuf_put(&bb, data, sizeof(data)));

    bytebuf_reset(&bb);

    TEST_EQ_U32(0U, bytebuf_size(&bb));
    TEST_EQ_U32(sizeof(storage), bytebuf_space(&bb));
    TEST_ASSERT(bytebuf_is_empty(&bb));
    TEST_ASSERT(!bytebuf_get_u8(&bb, &out));
}

static void test_bytebuf_put_get_copy_data_in_fifo_order(void)
{
    uint8_t storage[5];
    uint8_t out[4] = { 0U };
    const uint8_t data[] = { 10U, 20U, 30U, 40U };
    bytebuf_t bb;

    bytebuf_init(&bb, storage, sizeof(storage));

    TEST_EQ_U32(sizeof(data), bytebuf_put(&bb, data, sizeof(data)));
    TEST_EQ_U32(sizeof(data), bytebuf_size(&bb));
    TEST_EQ_U32(1U, bytebuf_space(&bb));
    TEST_ASSERT(!bytebuf_is_empty(&bb));
    TEST_ASSERT(!bytebuf_is_full(&bb));

    TEST_EQ_U32(sizeof(out), bytebuf_get(&bb, out, sizeof(out)));
    test_bytebuf_expect_bytes(data, out, sizeof(data));
    TEST_ASSERT(bytebuf_is_empty(&bb));
    TEST_EQ_U32(sizeof(storage), bytebuf_space(&bb));
}

static void test_bytebuf_put_get_limit_to_available_space_and_size(void)
{
    uint8_t storage[4];
    uint8_t out[6] = { 0U };
    const uint8_t data[] = { 1U, 2U, 3U, 4U, 5U, 6U };
    const uint8_t expected[] = { 1U, 2U, 3U, 4U };
    bytebuf_t bb;

    bytebuf_init(&bb, storage, sizeof(storage));

    TEST_EQ_U32(sizeof(storage), bytebuf_put(&bb, data, sizeof(data)));
    TEST_ASSERT(bytebuf_is_full(&bb));
    TEST_EQ_U32(0U, bytebuf_space(&bb));
    TEST_EQ_U32(0U, bytebuf_put(&bb, data, 1U));

    TEST_EQ_U32(sizeof(storage), bytebuf_get(&bb, out, sizeof(out)));
    test_bytebuf_expect_bytes(expected, out, sizeof(expected));
    TEST_ASSERT(bytebuf_is_empty(&bb));
    TEST_EQ_U32(0U, bytebuf_get(&bb, out, sizeof(out)));
}

static void test_bytebuf_u8_fast_path_reports_empty_and_full_boundaries(void)
{
    uint8_t storage[2];
    uint8_t out = 0U;
    bytebuf_t bb;

    bytebuf_init(&bb, storage, sizeof(storage));

    TEST_ASSERT(!bytebuf_get_u8(&bb, &out));
    TEST_ASSERT(bytebuf_put_u8(&bb, 0xA1U));
    TEST_ASSERT(bytebuf_put_u8(&bb, 0xB2U));
    TEST_ASSERT(!bytebuf_put_u8(&bb, 0xC3U));
    TEST_ASSERT(bytebuf_is_full(&bb));

    TEST_ASSERT(bytebuf_get_u8(&bb, &out));
    TEST_EQ_U32(0xA1U, out);
    TEST_ASSERT(bytebuf_get_u8(&bb, &out));
    TEST_EQ_U32(0xB2U, out);
    TEST_ASSERT(!bytebuf_get_u8(&bb, &out));
    TEST_ASSERT(bytebuf_is_empty(&bb));
}

static void test_bytebuf_copy_operations_preserve_order_across_wrap(void)
{
    uint8_t storage[5];
    uint8_t discarded[3];
    uint8_t out[5] = { 0U };
    const uint8_t first[] = { 1U, 2U, 3U, 4U };
    const uint8_t second[] = { 5U, 6U, 7U, 8U };
    const uint8_t expected[] = { 4U, 5U, 6U, 7U, 8U };
    bytebuf_t bb;

    bytebuf_init(&bb, storage, sizeof(storage));

    TEST_EQ_U32(sizeof(first), bytebuf_put(&bb, first, sizeof(first)));
    TEST_EQ_U32(sizeof(discarded), bytebuf_get(&bb, discarded, sizeof(discarded)));
    TEST_EQ_U32(sizeof(second), bytebuf_put(&bb, second, sizeof(second)));
    TEST_ASSERT(bytebuf_is_full(&bb));

    TEST_EQ_U32(sizeof(out), bytebuf_get(&bb, out, sizeof(out)));
    test_bytebuf_expect_bytes(expected, out, sizeof(expected));
    TEST_ASSERT(bytebuf_is_empty(&bb));
}

static void test_bytebuf_peek_skip_and_skip_all_consume_expected_bytes(void)
{
    uint8_t storage[6];
    uint8_t out[3] = { 0U };
    const uint8_t data[] = { 9U, 8U, 7U, 6U, 5U };
    const uint8_t first_expected[] = { 9U, 8U, 7U };
    const uint8_t second_expected[] = { 7U, 6U, 5U };
    bytebuf_t bb;

    bytebuf_init(&bb, storage, sizeof(storage));
    TEST_EQ_U32(sizeof(data), bytebuf_put(&bb, data, sizeof(data)));

    TEST_EQ_U32(sizeof(out), bytebuf_peek(&bb, out, sizeof(out)));
    test_bytebuf_expect_bytes(first_expected, out, sizeof(first_expected));
    TEST_EQ_U32(sizeof(data), bytebuf_size(&bb));

    TEST_EQ_U32(2U, bytebuf_skip(&bb, 2U));
    TEST_EQ_U32(3U, bytebuf_size(&bb));
    TEST_EQ_U32(sizeof(out), bytebuf_get(&bb, out, sizeof(out)));
    test_bytebuf_expect_bytes(second_expected, out, sizeof(second_expected));

    TEST_EQ_U32(sizeof(data), bytebuf_put(&bb, data, sizeof(data)));
    bytebuf_skip_all(&bb);
    TEST_ASSERT(bytebuf_is_empty(&bb));
    TEST_EQ_U32(sizeof(storage), bytebuf_space(&bb));
}

static void test_bytebuf_put_claim_finish_publishes_written_bytes(void)
{
    uint8_t storage[5];
    uint8_t out[3] = { 0U };
    uint8_t *claim = NULL;
    const uint8_t expected[] = { 0x11U, 0x22U, 0x33U };
    bytebuf_t bb;

    bytebuf_init(&bb, storage, sizeof(storage));

    TEST_EQ_U32(4U, bytebuf_put_claim(&bb, &claim, 4U));
    TEST_EQ_PTR(storage, claim);
    claim[0] = expected[0];
    claim[1] = expected[1];
    claim[2] = expected[2];
    TEST_EQ_U32(0U, bytebuf_size(&bb));

    TEST_EQ_U32(0U, bytebuf_put_finish(&bb, 3U));
    TEST_EQ_U32(3U, bytebuf_size(&bb));
    TEST_EQ_U32(sizeof(out), bytebuf_get(&bb, out, sizeof(out)));
    test_bytebuf_expect_bytes(expected, out, sizeof(expected));
}

static void test_bytebuf_put_claim_stops_at_physical_end_before_wrap(void)
{
    uint8_t storage[5];
    uint8_t discarded[3];
    uint8_t out[5] = { 0U };
    uint8_t *claim = NULL;
    const uint8_t first[] = { 1U, 2U, 3U, 4U };
    const uint8_t expected[] = { 4U, 5U, 6U, 7U, 8U };
    bytebuf_t bb;

    bytebuf_init(&bb, storage, sizeof(storage));

    TEST_EQ_U32(sizeof(first), bytebuf_put(&bb, first, sizeof(first)));
    TEST_EQ_U32(sizeof(discarded), bytebuf_get(&bb, discarded, sizeof(discarded)));

    TEST_EQ_U32(1U, bytebuf_put_claim(&bb, &claim, 4U));
    TEST_EQ_PTR(&storage[4], claim);
    claim[0] = 5U;
    TEST_EQ_U32(0U, bytebuf_put_finish(&bb, 1U));

    TEST_EQ_U32(3U, bytebuf_put_claim(&bb, &claim, 4U));
    TEST_EQ_PTR(storage, claim);
    claim[0] = 6U;
    claim[1] = 7U;
    claim[2] = 8U;
    TEST_EQ_U32(0U, bytebuf_put_finish(&bb, 3U));
    TEST_ASSERT(bytebuf_is_full(&bb));

    TEST_EQ_U32(sizeof(out), bytebuf_get(&bb, out, sizeof(out)));
    test_bytebuf_expect_bytes(expected, out, sizeof(expected));
}

static void test_bytebuf_get_claim_finish_consumes_claimed_bytes(void)
{
    uint8_t storage[4];
    uint8_t *claim = NULL;
    const uint8_t data[] = { 0x41U, 0x42U, 0x43U };
    bytebuf_t bb;

    bytebuf_init(&bb, storage, sizeof(storage));
    TEST_EQ_U32(sizeof(data), bytebuf_put(&bb, data, sizeof(data)));

    TEST_EQ_U32(2U, bytebuf_get_claim(&bb, &claim, 2U));
    TEST_EQ_PTR(storage, claim);
    TEST_EQ_U32(0x41U, claim[0]);
    TEST_EQ_U32(0x42U, claim[1]);
    TEST_EQ_U32(sizeof(data), bytebuf_size(&bb));

    TEST_EQ_U32(0U, bytebuf_get_finish(&bb, 2U));
    TEST_EQ_U32(1U, bytebuf_size(&bb));
    TEST_EQ_U32(1U, bytebuf_get_claim(&bb, &claim, 2U));
    TEST_EQ_U32(0x43U, claim[0]);
    TEST_EQ_U32(0U, bytebuf_get_finish(&bb, 1U));
    TEST_ASSERT(bytebuf_is_empty(&bb));
}

static void test_bytebuf_get_claim_stops_at_physical_end_before_wrap(void)
{
    uint8_t storage[5];
    uint8_t discarded[3];
    uint8_t *claim = NULL;
    const uint8_t first[] = { 1U, 2U, 3U, 4U, 5U };
    const uint8_t second[] = { 6U, 7U, 8U };
    bytebuf_t bb;

    bytebuf_init(&bb, storage, sizeof(storage));

    TEST_EQ_U32(sizeof(first), bytebuf_put(&bb, first, sizeof(first)));
    TEST_EQ_U32(sizeof(discarded), bytebuf_get(&bb, discarded, sizeof(discarded)));
    TEST_EQ_U32(sizeof(second), bytebuf_put(&bb, second, sizeof(second)));

    TEST_EQ_U32(2U, bytebuf_get_claim(&bb, &claim, 5U));
    TEST_EQ_PTR(&storage[3], claim);
    TEST_EQ_U32(4U, claim[0]);
    TEST_EQ_U32(5U, claim[1]);
    TEST_EQ_U32(0U, bytebuf_get_finish(&bb, 2U));

    TEST_EQ_U32(3U, bytebuf_get_claim(&bb, &claim, 5U));
    TEST_EQ_PTR(storage, claim);
    TEST_EQ_U32(6U, claim[0]);
    TEST_EQ_U32(7U, claim[1]);
    TEST_EQ_U32(8U, claim[2]);
    TEST_EQ_U32(0U, bytebuf_get_finish(&bb, 3U));
    TEST_ASSERT(bytebuf_is_empty(&bb));
}

static void test_bytebuf_claim_returns_null_when_no_bytes_available(void)
{
    uint8_t storage[2];
    uint8_t *claim = storage;
    const uint8_t data[] = { 1U, 2U };
    bytebuf_t bb;

    bytebuf_init(&bb, storage, sizeof(storage));

    TEST_EQ_U32(0U, bytebuf_get_claim(&bb, &claim, 1U));
    TEST_EQ_PTR(NULL, claim);

    TEST_EQ_U32(sizeof(data), bytebuf_put(&bb, data, sizeof(data)));
    claim = storage;
    TEST_EQ_U32(0U, bytebuf_put_claim(&bb, &claim, 1U));
    TEST_EQ_PTR(NULL, claim);
}

void test_bytebuf_run(void)
{
    TEST_RUN(test_bytebuf_init_makes_empty_buffer);
    TEST_RUN(test_bytebuf_static_init_makes_empty_buffer);
    TEST_RUN(test_bytebuf_reset_drops_buffered_data);
    TEST_RUN(test_bytebuf_put_get_copy_data_in_fifo_order);
    TEST_RUN(test_bytebuf_put_get_limit_to_available_space_and_size);
    TEST_RUN(test_bytebuf_u8_fast_path_reports_empty_and_full_boundaries);
    TEST_RUN(test_bytebuf_copy_operations_preserve_order_across_wrap);
    TEST_RUN(test_bytebuf_peek_skip_and_skip_all_consume_expected_bytes);
    TEST_RUN(test_bytebuf_put_claim_finish_publishes_written_bytes);
    TEST_RUN(test_bytebuf_put_claim_stops_at_physical_end_before_wrap);
    TEST_RUN(test_bytebuf_get_claim_finish_consumes_claimed_bytes);
    TEST_RUN(test_bytebuf_get_claim_stops_at_physical_end_before_wrap);
    TEST_RUN(test_bytebuf_claim_returns_null_when_no_bytes_available);
}
